#include <cstdlib>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include <time.h>

#include "thingfx/ThinGFX.hpp"

#include "mod_file.h"
#include "mod_player.hpp"
#include "ThinMix.h"
#include "WAV.h"

#include "audio_type.h"

#define SAMP_RATE 48000
#define BUF_FRAMES 256

MOD_FILE mod;

int count = 0;

thingfx::Window bgLayer;
thingfx::Window win;

thingfx::Rect countRect = {8, 8, 96, 40};

static void redrawBackground()
{
    auto gfx = bgLayer.draw();

    gfx.fillScreen(0);

    gfx.setTextWrap(false);
    gfx.setTextColor(1);

    gfx.setCursor(0, 57);
    gfx.printf("%d x %d", countRect.w, countRect.h);

    bgLayer.commit();
}

static void redrawCounter(thingfx::Window &window, int count)
{
    auto gfx = window.draw();

    gfx.fillScreen(0);

    gfx.setTextWrap(false);

    gfx.setCursor(0, 3);
    gfx.setTextColor(1);
    gfx.printf(" Counter\n count=%d", count);

    window.commit();
}

static bool onCounterEvent(
    thingfx::Window &window,
    const thingfx::Event &event,
    void *user)
{
    (void)user;

    if (event.type != thingfx::EventType::KeyDown) {
        return false;
    }

    if (event.key == 'q') {
        window.destroy();

        countRect = {0, 0, 0, 0};
        redrawBackground();

        return true;
    }

    count++;

    countRect = {
        static_cast<int16_t>(rand() % 32),
        static_cast<int16_t>(rand() % 16),
        static_cast<int16_t>((rand() % 48) + 48),
        static_cast<int16_t>((rand() % 20) + 28)
    };

    win.setRect(countRect);

    redrawBackground();
    redrawCounter(window, count);

    return true;
}

int main()
{
    srand(time(0));
    thingfx::PortConfig port;
    port.title = "Counter Demo";
    port.scale = 4;
    port.format = thingfx::PixelFormat::Mono1;
    port.autoStartDaemon = true;

    thingfx::Gui gui(128, 64, port);

    thingfx::Transition openAnim;
    openAnim.easing = thingfx::Easing::EaseOut;
    openAnim.durationMs = 250;
    openAnim.speed = 1.0f;

    thingfx::Transition closeAnim;
    closeAnim.easing = thingfx::Easing::EaseIn;
    closeAnim.durationMs = 200;
    closeAnim.speed = 1.0f;

    thingfx::WindowConfig bgCfg;
    bgCfg.kind = thingfx::NodeKind::Layer;
    bgCfg.frame = {0, 0, 128, 64};
    bgCfg.ownsCanvas = true;
    bgCfg.canvasFormat = thingfx::PixelFormat::Mono1;
    bgCfg.visible = true;
    bgCfg.opacity = 255;
    bgCfg.showTitle = false;
    bgCfg.border = thingfx::WindowBorder::None;

    bgLayer = gui.createLayer(bgCfg);

    thingfx::WindowConfig cfg;
    cfg.kind = thingfx::NodeKind::Window;
    cfg.frame = countRect;
    cfg.ownsCanvas = true;
    cfg.canvasFormat = thingfx::PixelFormat::Mono1;
    cfg.visible = true;
    cfg.opacity = 255;
    cfg.showTitle = true;
    cfg.title = "COUNT";
    cfg.border = thingfx::WindowBorder::Rect;
    cfg.onEvent = onCounterEvent;

    cfg.createTransition = openAnim;
    cfg.destroyTransition = closeAnim;

    win = gui.createWindow(cfg);

    thingfx::Transition tr;
    tr.easing = thingfx::Easing::EaseOut;
    tr.durationMs = 200;
    tr.speed = 1.0f;
    win.setTransition(tr);

    gui.setFocus(win);

    redrawBackground();
    redrawCounter(win, count);

    while (gui.pump()) {
        usleep(1000 * 15);
    }

    return 0;
}
