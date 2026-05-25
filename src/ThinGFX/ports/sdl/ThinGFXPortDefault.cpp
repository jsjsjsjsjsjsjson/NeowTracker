#include "../../src/ThinGFXInternal.hpp"

#include <SDL.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

namespace thingfx {
using namespace detail;

class SDLDefaultPort : public Port {
public:
    struct State {
        SDL_Window *window;
        SDL_Renderer *renderer;
        SDL_Texture *texture;
        uint32_t *scratch;
        uint8_t *target_pixels;
        uint16_t width;
        uint16_t height;
        PixelFormat format;
        bool quit;
        bool daemon_started;
        bool daemon_stop;
        pthread_t daemon_thread;
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        bool pending_commit;
        bool pending_present;
    };

    SDLDefaultPort(uint16_t width, uint16_t height, const PortConfig &config)
        : state_(0)
    {
        uint8_t scale = config.scale ? config.scale : 1;
        PixelFormat format = config.format;
        const char *title = config.title ? config.title : "ThinGFX";
        if (width == 0 || height == 0 || !supportedFormat(format)) return;
        if (SDL_Init(SDL_INIT_VIDEO) != 0) return;

        state_ = static_cast<State *>(calloc(1, sizeof(State)));
        if (!state_) {
            SDL_Quit();
            return;
        }
        state_->width = width;
        state_->height = height;
        state_->format = format;
        pthread_mutex_init(&state_->mutex, 0);
        pthread_cond_init(&state_->cond, 0);

        size_t bytes = framebufferBytes(width, height, format);
        state_->target_pixels = static_cast<uint8_t *>(calloc(1, bytes));
        state_->scratch = static_cast<uint32_t *>(calloc(static_cast<size_t>(width) * height,
                                                         sizeof(*state_->scratch)));
        if (!state_->scratch || !state_->target_pixels) return;

        state_->window = SDL_CreateWindow(title,
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          width * scale, height * scale,
                                          SDL_WINDOW_SHOWN);
        if (state_->window) SDL_SetWindowBordered(state_->window, SDL_TRUE);
        state_->renderer = SDL_CreateRenderer(state_->window, -1, SDL_RENDERER_ACCELERATED);
        if (!state_->renderer) {
            state_->renderer = SDL_CreateRenderer(state_->window, -1, SDL_RENDERER_SOFTWARE);
        }
        if (state_->window && state_->renderer) {
            state_->texture = SDL_CreateTexture(state_->renderer, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_STREAMING, width, height);
        }
        if (!state_->window || !state_->renderer || !state_->texture) return;

        tgfx_canvas_wrap(&target, width, height, strideFor(width, format),
                         format, state_->target_pixels);
    }

    ~SDLDefaultPort() override
    {
        if (!state_) return;
        stopDaemon();
        SDL_DestroyTexture(state_->texture);
        SDL_DestroyRenderer(state_->renderer);
        SDL_DestroyWindow(state_->window);
        SDL_Quit();
        free(state_->scratch);
        free(state_->target_pixels);
        pthread_cond_destroy(&state_->cond);
        pthread_mutex_destroy(&state_->mutex);
        free(state_);
        state_ = 0;
    }

    bool valid() const override
    {
        return state_ && state_->window && state_->renderer && state_->texture && target.pixels;
    }

    bool pump(Gui &gui) override
    {
        if (!state_) return false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            processEvent(gui, event);
        }
        flushPresentation();
        return !state_->quit;
    }

    bool shouldClose() const override
    {
        return state_ ? state_->quit : true;
    }

    bool hasSystemBorder() const override
    {
        if (!state_ || !state_->window) return false;
        uint32_t flags = SDL_GetWindowFlags(state_->window);
        return (flags & SDL_WINDOW_BORDERLESS) == 0;
    }

    Result present(const CanvasBuffer &canvas, const Rect *dirty) override
    {
        (void)dirty;
        if (!valid() || !canvas.pixels || !supportedFormat(canvas.format)) {
            return Result::InvalidArgument;
        }
        if (canvas.width != state_->width || canvas.height != state_->height ||
            canvas.format != state_->format) {
            return Result::InvalidArgument;
        }

        /*
         * SDL rendering APIs are most reliable when called from the thread
         * that owns the event loop.  The ThinGFX daemon may run on a worker
         * thread, so here we only convert the submitted canvas into an ARGB
         * staging buffer.  pump() performs the actual SDL_UpdateTexture /
         * SDL_RenderPresent on the application thread.
         */
        pthread_mutex_lock(&state_->mutex);
        for (uint16_t y = 0; y < canvas.height; ++y) {
            for (uint16_t x = 0; x < canvas.width; ++x) {
                uint8_t g = tgfx_get_pixel(&canvas, static_cast<int16_t>(x), static_cast<int16_t>(y));
                state_->scratch[static_cast<size_t>(y) * canvas.width + x] =
                    0xFF000000u | (static_cast<uint32_t>(g) << 16) |
                    (static_cast<uint32_t>(g) << 8) | g;
            }
        }
        state_->pending_present = true;
        pthread_mutex_unlock(&state_->mutex);
        return Result::Ok;
    }

    Result startDaemon(void (*entry)(void *), void *arg) override
    {
        if (!state_ || !entry || state_->daemon_started) return Result::InvalidArgument;
        DaemonArg *daemon_arg = new DaemonArg;
        daemon_arg->entry = entry;
        daemon_arg->arg = arg;
        state_->daemon_stop = false;
        if (pthread_create(&state_->daemon_thread, 0, daemonTrampoline, daemon_arg) != 0) {
            delete daemon_arg;
            return Result::Unsupported;
        }
        state_->daemon_started = true;
        return Result::Ok;
    }

    void stopDaemon() override
    {
        if (!state_ || !state_->daemon_started) return;
        pthread_mutex_lock(&state_->mutex);
        state_->daemon_stop = true;
        pthread_cond_signal(&state_->cond);
        pthread_mutex_unlock(&state_->mutex);
        pthread_join(state_->daemon_thread, 0);
        state_->daemon_started = false;
    }

    void notifyCommit() override
    {
        if (!state_) return;
        pthread_mutex_lock(&state_->mutex);
        state_->pending_commit = true;
        pthread_cond_signal(&state_->cond);
        pthread_mutex_unlock(&state_->mutex);
    }

    bool waitCommit() override
    {
        if (!state_) return false;
        pthread_mutex_lock(&state_->mutex);
        while (!state_->pending_commit && !state_->daemon_stop) {
            pthread_cond_wait(&state_->cond, &state_->mutex);
        }
        if (state_->daemon_stop) {
            pthread_mutex_unlock(&state_->mutex);
            return false;
        }
        state_->pending_commit = false;
        pthread_mutex_unlock(&state_->mutex);
        return true;
    }

    bool waitCommitTimeout(uint32_t timeoutMs) override
    {
        if (!state_) return false;
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeoutMs / 1000u;
        ts.tv_nsec += static_cast<long>((timeoutMs % 1000u) * 1000000u);
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }
        pthread_mutex_lock(&state_->mutex);
        while (!state_->pending_commit && !state_->daemon_stop) {
            int rc = pthread_cond_timedwait(&state_->cond, &state_->mutex, &ts);
            if (rc == ETIMEDOUT) {
                break;
            }
        }
        if (state_->daemon_stop) {
            pthread_mutex_unlock(&state_->mutex);
            return false;
        }
        state_->pending_commit = false;
        pthread_mutex_unlock(&state_->mutex);
        return true;
    }

    void lock() override
    {
        if (state_) pthread_mutex_lock(&state_->mutex);
    }

    void unlock() override
    {
        if (state_) pthread_mutex_unlock(&state_->mutex);
    }

private:
    void flushPresentation()
    {
        if (!valid()) return;
        pthread_mutex_lock(&state_->mutex);
        if (!state_->pending_present) {
            pthread_mutex_unlock(&state_->mutex);
            return;
        }
        SDL_UpdateTexture(state_->texture, 0, state_->scratch,
                          static_cast<int>(state_->width) * 4);
        SDL_RenderClear(state_->renderer);
        SDL_RenderCopy(state_->renderer, state_->texture, 0, 0);
        SDL_RenderPresent(state_->renderer);
        state_->pending_present = false;
        pthread_mutex_unlock(&state_->mutex);
    }

    struct DaemonArg {
        void (*entry)(void *);
        void *arg;
    };

    static bool supportedFormat(PixelFormat format)
    {
        return format == PixelFormat::Mono1 ||
               format == PixelFormat::Gray4 ||
               format == PixelFormat::Gray8;
    }

    static uint16_t strideFor(uint16_t width, PixelFormat format)
    {
        if (format == PixelFormat::Mono1) return width;
        if (format == PixelFormat::Gray4) return static_cast<uint16_t>((width + 1u) / 2u);
        return width;
    }

    static uint32_t mapKey(SDL_Keycode key)
    {
        if (key >= 32 && key <= 126) return static_cast<uint32_t>(key);
        switch (key) {
        case SDLK_UP: return static_cast<uint32_t>(Key::Up);
        case SDLK_DOWN: return static_cast<uint32_t>(Key::Down);
        case SDLK_LEFT: return static_cast<uint32_t>(Key::Left);
        case SDLK_RIGHT: return static_cast<uint32_t>(Key::Right);
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return static_cast<uint32_t>(Key::Enter);
        case SDLK_BACKSPACE: return static_cast<uint32_t>(Key::Back);
        case SDLK_ESCAPE: return static_cast<uint32_t>(Key::Escape);
        case SDLK_MENU: return static_cast<uint32_t>(Key::Menu);
        case SDLK_HOME: return static_cast<uint32_t>(Key::Home);
        case SDLK_END: return static_cast<uint32_t>(Key::End);
        case SDLK_PAGEUP: return static_cast<uint32_t>(Key::PageUp);
        case SDLK_PAGEDOWN: return static_cast<uint32_t>(Key::PageDown);
        default: return static_cast<uint32_t>(Key::Unknown);
        }
    }

    static uint32_t mapModifiers(SDL_Keymod mods)
    {
        uint32_t out = ModNone;
        if (mods & KMOD_SHIFT) out |= ModShift;
        if (mods & KMOD_CTRL) out |= ModCtrl;
        if (mods & KMOD_ALT) out |= ModAlt;
        if (mods & KMOD_GUI) out |= ModMeta;
        return out;
    }

    void processEvent(Gui &gui, const SDL_Event &event)
    {
        switch (event.type) {
        case SDL_QUIT:
            state_->quit = true;
            notifyCommit();
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            gui.dispatchKey(mapKey(event.key.keysym.sym),
                            event.type == SDL_KEYDOWN,
                            event.key.repeat != 0,
                            mapModifiers(static_cast<SDL_Keymod>(event.key.keysym.mod)));
            break;
        default:
            break;
        }
    }

    static void *daemonTrampoline(void *arg)
    {
        DaemonArg *a = static_cast<DaemonArg *>(arg);
        void (*entry)(void *) = a->entry;
        void *entry_arg = a->arg;
        delete a;
        entry(entry_arg);
        return 0;
    }

    State *state_;
};

Port *createDefaultPort(uint16_t width, uint16_t height, const PortConfig &config)
{
    return new SDLDefaultPort(width, height, config);
}

void destroyDefaultPort(Port *port)
{
    delete port;
}

} // namespace thingfx
