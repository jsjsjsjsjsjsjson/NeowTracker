#include "famigfx/FamiGFX.hpp"

#include <chrono>
#include <stdio.h>
#include <stdlib.h>
#include <thread>

static uint32_t demo_ticks_ms()
{
    using clock = std::chrono::steady_clock;
    static const clock::time_point start = clock::now();
    return static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count());
}

static void demo_delay_ms(uint32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

static uint16_t parse_dimension(const char *text, uint16_t fallback)
{
    long value = text ? strtol(text, NULL, 10) : fallback;
    if (value < 32) {
        value = 32;
    }
    if (value > 32767) {
        value = 32767;
    }
    return (uint16_t)value;
}

static uint32_t parse_frame_limit(const char *text)
{
    long value = text ? strtol(text, NULL, 10) : 0;
    return value > 0 ? (uint32_t)value : 0;
}

typedef struct mono_node_data {
    famigfx::Gui *gui;
    const char *label;
    uint32_t frame;
    uint32_t key_hits;
    uint8_t invert;
} mono_node_data_t;

enum {
    MONO_BORDER_NONE = 0,
    MONO_BORDER_RECT = 1,
    MONO_BORDER_ROUND = 2
};

static uint8_t parse_border_mode(const char *text)
{
    if (!text) {
        return MONO_BORDER_RECT;
    }
    if ((text[0] == 'n' || text[0] == 'N') && text[1] == '\0') {
        return MONO_BORDER_NONE;
    }
    if ((text[0] == 'r' || text[0] == 'R') && text[1] == '\0') {
        return MONO_BORDER_RECT;
    }
    if ((text[0] == 'o' || text[0] == 'O') && text[1] == '\0') {
        return MONO_BORDER_ROUND;
    }
    if (text[0] == '0') {
        return MONO_BORDER_NONE;
    }
    if (text[0] == '2') {
        return MONO_BORDER_ROUND;
    }
    return MONO_BORDER_RECT;
}

static uint8_t parse_title_visible(const char *text)
{
    if (!text) {
        return 1;
    }
    if ((text[0] == 'n' || text[0] == 'N') && text[1] == '\0') {
    return 0;
    }
    if (text[0] == '0') {
    return 0;
    }
    return 1;
}

static void draw_background(famigfx::Window &window, famigfx::FamiGFX &gfx,
                            const famigfx::Rect &dirty, void *user)
{
    mono_node_data_t *data = static_cast<mono_node_data_t *>(user);
    int16_t w = gfx.width();
    int16_t h = gfx.height();
    int16_t scan = (int16_t)(data->frame % (uint32_t)(w > 1 ? w - 1 : 1));
    int16_t y;
    (void)window;
    (void)dirty;

    gfx.fillScreen(0);
    gfx.drawRect(0, 0, w, h, 255);
    gfx.setCursor(6, 6);
    gfx.setTextSize(1);
    gfx.setTextColor(255);
    gfx.print(data->label);
    gfx.drawLine(0, (int16_t)(h - 1), (int16_t)(w - 1), 0,
                 255);

    for (y = 2; y < h - 2; y += 4) {
        gfx.drawPixel(scan, y, 255);
        if (scan + 1 < w) {
            gfx.drawPixel((int16_t)(scan + 1), y, 255);
        }
    }
}

static void draw_window(famigfx::Window &window, famigfx::FamiGFX &gfx,
                        const famigfx::Rect &dirty, void *user)
{
    mono_node_data_t *data = static_cast<mono_node_data_t *>(user);
    int16_t w = gfx.width();
    int16_t h = gfx.height();
    int16_t ball_x = (int16_t)(12 + (data->frame % (uint32_t)(w > 36 ? w - 36 : 1)));
    famigfx::Color black = 0;
    famigfx::Color white = 255;
    bool focused = data->gui && data->gui->focus().handle() == window.handle();
    (void)dirty;

    gfx.fillScreen(data->invert ? white : black);

    gfx.setCursor(6, 6);
    gfx.setTextSize(1);
    gfx.setTextColor(data->invert ? black : white,
                     data->invert ? white : black);
    gfx.print(data->label);
    if (focused) {
        gfx.print(" *");
    }
    if (data->key_hits) {
        gfx.print(" ");
        gfx.print((unsigned long)data->key_hits);
    }
    gfx.setCursor(6, 16);
    gfx.print("n/p f/b");
    gfx.drawLine(4, 25, (int16_t)(w - 5), 25,
                 data->invert ? black : white);
    gfx.fillCircle(ball_x, (int16_t)(h - 9), 5,
                   data->invert ? black : white);
}

static bool handle_mono_event(famigfx::Window &window, const famigfx::Event &event,
                              void *user)
{
    mono_node_data_t *data = static_cast<mono_node_data_t *>(user);
    if (event.type != famigfx::EventType::KeyDown) {
        return false;
    }

    ++data->key_hits;

    switch (event.key) {
    case 'n':
    case 'N':
        if (data->gui) {
            data->gui->focusNext();
        }
        break;
    case 'p':
    case 'P':
        if (data->gui) {
            data->gui->focusPrevious();
        }
        break;
    case 'f':
    case 'F':
        window.bringToFront();
        break;
    case 'b':
    case 'B':
        window.sendToBack();
        break;
    case static_cast<uint32_t>(famigfx::Key::Enter):
    case ' ':
        data->invert = data->invert ? 0u : 1u;
        break;
    default:
        break;
    }

    window.invalidate();
    return true;
}

int main(int argc, char **argv)
{
    uint16_t width = argc > 1 ? parse_dimension(argv[1], 128) : 128;
    uint16_t height = argc > 2 ? parse_dimension(argv[2], 64) : 64;
    uint32_t max_frames = argc > 3 ? parse_frame_limit(argv[3]) : 0;
    uint8_t border_mode = argc > 4 ? parse_border_mode(argv[4]) : MONO_BORDER_RECT;
    uint8_t show_title = argc > 5 ? parse_title_visible(argv[5]) : 1;
    uint32_t frame_count = 0;
    famigfx::PortConfig port_config;
    port_config.title = "FamiGFX SDL Mono Demo";
    port_config.scale = 4;
    port_config.format = famigfx::PixelFormat::Mono1;
    port_config.autoStartDaemon = true;
    famigfx::Gui gui(width, height, port_config);
    famigfx::Window background;
    famigfx::Window main_win;
    famigfx::Window floating_win;
    famigfx::WindowConfig config;
    famigfx::WindowBorder border =
        border_mode == MONO_BORDER_NONE ? famigfx::WindowBorder::None
                                        : famigfx::WindowBorder::Rect;
    mono_node_data_t bg_data;
    mono_node_data_t main_data;
    mono_node_data_t float_data;

    if (!gui.portValid()) {
        fprintf(stderr, "failed to create selected mono port\n");
        return 1;
    }
    if (gui.port() && !gui.port()->hasSystemBorder()) {
        fprintf(stderr, "selected mono port window was created without a system border\n");
        return 1;
    }

    if (!gui.valid()) {
        fprintf(stderr, "failed to allocate mono state\n");
        return 1;
    }

    bg_data.gui = &gui;
    bg_data.label = "SSD1306 MONO TREE";
    bg_data.frame = 0;
    bg_data.key_hits = 0;
    bg_data.invert = 0;

    config.kind = famigfx::NodeKind::Layer;
    config.frame = {0, 0, (int16_t)width, (int16_t)height};
    config.opacity = 255;
    config.visible = true;
    config.ownsCanvas = false;
    config.showTitle = false;
    config.border = famigfx::WindowBorder::None;
    config.title = NULL;
    config.onDraw = draw_background;
    config.onEvent = NULL;
    config.user = &bg_data;
    background = gui.createLayer(config);
    if (!background.valid()) {
        fprintf(stderr, "failed to create mono background node\n");
        return 1;
    }

    main_data.gui = &gui;
    main_data.label = "MAIN FB";
    main_data.frame = 0;
    main_data.key_hits = 0;
    main_data.invert = 0;

    config.kind = famigfx::NodeKind::Window;
    config.frame = {8, 18, (int16_t)(width > 24 ? width - 24 : width),
                    (int16_t)(height > 30 ? height - 30 : height)};
    config.opacity = 255;
    config.visible = true;
    config.ownsCanvas = true;
    config.canvasFormat = famigfx::PixelFormat::Mono1;
    config.showTitle = show_title != 0;
    config.border = border;
    config.title = show_title ? "MAIN" : NULL;
    config.onDraw = draw_window;
    config.onEvent = handle_mono_event;
    config.user = &main_data;
    main_win = gui.createWindow(config);
    if (!main_win.valid()) {
        fprintf(stderr, "failed to create mono main window\n");
        return 1;
    }

    float_data.gui = &gui;
    float_data.label = "FLOAT FB";
    float_data.frame = 0;
    float_data.key_hits = 0;
    float_data.invert = 1;

    config.kind = famigfx::NodeKind::Floating;
    config.frame = {(int16_t)(width / 2 - 28), (int16_t)(height / 2 - 14), 56, 28};
    config.opacity = 255;
    config.visible = true;
    config.ownsCanvas = true;
    config.canvasFormat = famigfx::PixelFormat::Mono1;
    config.showTitle = show_title != 0;
    config.border = border;
    config.title = show_title ? "FLOAT" : NULL;
    config.onDraw = draw_window;
    config.onEvent = handle_mono_event;
    config.user = &float_data;
    floating_win = gui.createWindow(config);
    if (!floating_win.valid()) {
        fprintf(stderr, "failed to create mono floating window\n");
        return 1;
    }

    floating_win.focus();
    while (gui.pump()) {
        famigfx::Rect float_rect;
        int16_t range = (int16_t)(width > 72 ? width - 72 : 1);

        bg_data.frame = frame_count;
        main_data.frame = frame_count;
        float_data.frame = frame_count * 2u;
        background.invalidate();
        main_win.invalidate();
        floating_win.invalidate();

        float_rect.x = (int16_t)(8 + ((frame_count * 2u) % (uint32_t)range));
        float_rect.y = (int16_t)(height / 2 - 14);
        float_rect.w = 56;
        float_rect.h = 28;
        floating_win.setRect(float_rect);

        ++frame_count;
        if (max_frames != 0 && frame_count >= max_frames) {
            break;
        }
        demo_delay_ms(16);
    }

    (void)background;
    (void)main_win;
    return 0;
}
