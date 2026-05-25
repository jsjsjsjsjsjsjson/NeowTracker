#include "thingfx/ThinGFX.hpp"

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

typedef struct demo_window {
    thingfx::Gui *gui;
    const char *label;
    thingfx::Color fill;
    thingfx::Color accent;
    uint32_t frame;
    uint32_t key_hits;
    uint8_t inverted;
} demo_window_t;

enum {
    DEMO_BORDER_NONE = 0,
    DEMO_BORDER_RECT = 1,
    DEMO_BORDER_ROUND = 2
};

static uint16_t parse_dimension(const char *text, uint16_t fallback)
{
    long value = text ? strtol(text, NULL, 10) : fallback;
    if (value < 64) {
        value = 64;
    }
    if (value > 32767) {
        value = 32767;
    }
    return (uint16_t)value;
}

static uint32_t parse_frame_limit(const char *text)
{
    long value = text ? strtol(text, NULL, 10) : 0;
    if (value < 0) {
        value = 0;
    }
    return (uint32_t)value;
}

static uint8_t parse_border_mode(const char *text)
{
    if (!text) {
        return DEMO_BORDER_RECT;
    }
    if ((text[0] == 'n' || text[0] == 'N') && text[1] == '\0') {
        return DEMO_BORDER_NONE;
    }
    if ((text[0] == 'r' || text[0] == 'R') && text[1] == '\0') {
        return DEMO_BORDER_RECT;
    }
    if ((text[0] == 'o' || text[0] == 'O') && text[1] == '\0') {
        return DEMO_BORDER_ROUND;
    }
    if (text[0] == '0') {
        return DEMO_BORDER_NONE;
    }
    if (text[0] == '2') {
        return DEMO_BORDER_ROUND;
    }
    return DEMO_BORDER_RECT;
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

static void draw_panel(thingfx::Window &window, thingfx::ThinGFX &gfx,
                       const thingfx::Rect &dirty, void *user)
{
    demo_window_t *win = static_cast<demo_window_t *>(user);
    int16_t x;
    bool focused = win->gui && win->gui->focus().handle() == window.handle();
    thingfx::Color fill = win->inverted ? 238 : win->fill;
    thingfx::Color text = win->inverted ? 20
                                      : 255;
    thingfx::Color accent = focused ? 185 : win->accent;
    (void)dirty;

    gfx.fillScreen(fill);
    gfx.fillRect(4, 4, (int16_t)(gfx.width() > 8 ? gfx.width() - 8 : gfx.width()),
                 14, accent);
    gfx.setCursor(8, 8);
    gfx.setTextSize(1);
    gfx.setTextColor(text, accent);
    gfx.print(win->label);
    if (focused) {
        gfx.print(" *");
    }
    if (win->key_hits) {
        gfx.print(" #");
        gfx.print((unsigned long)win->key_hits);
    }

    gfx.setCursor(8, 24);
    gfx.setTextColor(text, fill);
    gfx.print("n/p focus");
    gfx.setCursor(8, 34);
    gfx.print("f/b z-order");
    gfx.setCursor(8, 44);
    gfx.print("enter invert");

    x = (int16_t)(12 + (win->frame % (uint32_t)(gfx.width() > 48 ? gfx.width() - 48 : 1)));
    gfx.fillCircle(x, (int16_t)(gfx.height() - 18), 8,
                   focused ? 185 : 185);
    gfx.drawTriangle((int16_t)(gfx.width() - 34), 44,
                     (int16_t)(gfx.width() - 12), 44,
                     (int16_t)(gfx.width() - 23), 62,
                     focused ? 185 : 185);
}

static bool handle_panel_event(thingfx::Window &window, const thingfx::Event &event,
                               void *user)
{
    demo_window_t *win = static_cast<demo_window_t *>(user);
    if (event.type != thingfx::EventType::KeyDown) {
        return false;
    }

    ++win->key_hits;

    switch (event.key) {
    case 'n':
    case 'N':
        if (win->gui) {
            win->gui->focusNext();
        }
        break;
    case 'p':
    case 'P':
        if (win->gui) {
            win->gui->focusPrevious();
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
    case static_cast<uint32_t>(thingfx::Key::Enter):
    case ' ':
        win->inverted = win->inverted ? 0u : 1u;
        break;
    default:
        break;
    }

    window.invalidate();
    return true;
}

int main(int argc, char **argv)
{
    uint16_t width = argc > 1 ? parse_dimension(argv[1], 320) : 320;
    uint16_t height = argc > 2 ? parse_dimension(argv[2], 240) : 240;
    uint32_t max_frames = argc > 3 ? parse_frame_limit(argv[3]) : 0;
    uint8_t border_mode = argc > 4 ? parse_border_mode(argv[4]) : DEMO_BORDER_RECT;
    uint8_t show_title = argc > 5 ? parse_title_visible(argv[5]) : 1;
    uint32_t frames = 0;
    thingfx::PortConfig port_config;
    port_config.title = "ThinGFX SDL Demo";
    port_config.scale = 2;
    port_config.format = thingfx::PixelFormat::Gray8;
    port_config.autoStartDaemon = true;
    thingfx::Gui gui(width, height, port_config);
    thingfx::Window main_win;
    thingfx::Window floating_win;
    thingfx::WindowConfig config;
    thingfx::WindowBorder border =
        border_mode == DEMO_BORDER_NONE ? thingfx::WindowBorder::None
                                        : thingfx::WindowBorder::Rect;
    demo_window_t main_data;
    demo_window_t float_data;
    uint32_t last_ticks;

    if (!gui.portValid()) {
        fprintf(stderr, "failed to create selected ThinGFX port\n");
        return 1;
    }
    if (gui.port() && !gui.port()->hasSystemBorder()) {
        fprintf(stderr, "selected port window was created without a system border\n");
        return 1;
    }

    if (!gui.valid()) {
        fprintf(stderr, "failed to allocate demo state\n");
        return 1;
    }

    main_data.gui = &gui;
    main_data.label = "CONTENT";
    main_data.fill = 32;
    main_data.accent = 109;
    main_data.frame = 0;
    main_data.key_hits = 0;
    main_data.inverted = 0;

    config.kind = thingfx::NodeKind::Window;
    config.frame = {16, 16, (int16_t)(width - 32), (int16_t)(height - 32)};
    config.opacity = 255;
    config.visible = true;
    config.ownsCanvas = true;
    config.showTitle = show_title != 0;
    config.border = border;
    config.title = show_title ? "Main Window" : NULL;
    config.onDraw = draw_panel;
    config.onEvent = handle_panel_event;
    config.user = &main_data;
    main_win = gui.createWindow(config);
    if (!main_win.valid()) {
        fprintf(stderr, "failed to create main window\n");
        return 1;
    }

    float_data.gui = &gui;
    float_data.label = "FLOAT FB";
    float_data.fill = 32;
    float_data.accent = 112;
    float_data.frame = 0;
    float_data.key_hits = 0;
    float_data.inverted = 0;

    config.kind = thingfx::NodeKind::Floating;
    config.frame = {(int16_t)(width / 2 - 32), (int16_t)(height / 2 - 28), 128, 76};
    config.opacity = 230;
    config.visible = true;
    config.ownsCanvas = true;
    config.showTitle = show_title != 0;
    config.border = border;
    config.title = show_title ? "Floating" : NULL;
    config.onDraw = draw_panel;
    config.onEvent = handle_panel_event;
    config.user = &float_data;
    floating_win = gui.createWindow(config);
    if (!floating_win.valid()) {
        fprintf(stderr, "failed to create floating window\n");
        return 1;
    }

    last_ticks = demo_ticks_ms();
    floating_win.focus();
    while (gui.pump()) {
        uint32_t now = demo_ticks_ms();
        uint32_t dt = now - last_ticks;
        thingfx::Rect rect;
        int16_t range;
        last_ticks = now;

        main_data.frame += dt / 8 + 1;
        float_data.frame += dt / 6 + 1;
        main_win.invalidate();
        floating_win.invalidate();

        range = (int16_t)(width > 180 ? width - 180 : 1);
        rect.x = (int16_t)(24 + ((now / 12) % (uint32_t)range));
        rect.y = (int16_t)(height / 2 - 28);
        rect.w = 128;
        rect.h = 76;
        floating_win.setRect(rect);

        ++frames;
        if (max_frames != 0 && frames >= max_frames) {
            break;
        }
        demo_delay_ms(16);
    }

    (void)main_win;
        return 0;
}
