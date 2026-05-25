#include "famigfx/FamiGFX.hpp"

#include <stdio.h>

int main()
{
    const uint16_t screen_w = 160;
    const uint16_t screen_h = 96;

    famigfx::Gui gui(screen_w, screen_h);
    famigfx::OwnedCanvas output(screen_w, screen_h, famigfx::PixelFormat::Gray8);
    if (!gui.valid() || !output.valid()) {
        return 1;
    }

    famigfx::WindowConfig cfg;
    cfg.kind = famigfx::NodeKind::Layer;
    cfg.frame = {0, 0, (int16_t)screen_w, (int16_t)screen_h};
    cfg.ownsCanvas = true;
    cfg.showTitle = false;
    cfg.border = famigfx::WindowBorder::None;

    famigfx::Window layer = gui.createLayer(cfg);
    if (!layer.valid()) {
        return 1;
    }

    famigfx::Canvas gfx = layer.draw();
    gfx.fillScreen(18);
    gfx.drawRect(0, 0, gfx.width(), gfx.height(), 240);
    gfx.setCursor(6, 6);
    gfx.setTextColor(240, 18);
    gfx.setTextSize(2);
    gfx.print("FamiGFX");
    gfx.drawCircle(48, 56, 18, 180);
    gfx.fillTriangle(90, 34, 132, 76, 70, 78, 96);
    gfx.drawLine(0, 95, 159, 0, 220);

    printf("output pixel before layer commit=%u\n",
           (unsigned)output.getPixel(0, 0));

    layer.commit();
    gui.render(output);

    printf("output pixel after layer commit/render=%u\n",
           (unsigned)output.getPixel(0, 0));

    gfx.fillRect(4, 72, 48, 12, 200);
    layer.commit({4, 72, 48, 12});
    gui.render(output);

    famigfx::OwnedCanvas gray4(8, 8, famigfx::PixelFormat::Gray4);
    famigfx::OwnedCanvas mono(8, 8, famigfx::PixelFormat::Mono1);
    if (!gray4.valid() || !mono.valid()) {
        return 1;
    }
    gray4.drawPixel(0, 0, 128);
    mono.drawPixel(0, 0, 1);

    printf("layer back/front surface bytes: %zu each\n",
           famigfx::framebufferBytes(screen_w, screen_h, famigfx::PixelFormat::Gray8));
    printf("formats: Gray4=%zu bytes Mono1=%zu bytes\n",
           famigfx::framebufferBytes(8, 8, famigfx::PixelFormat::Gray4),
           famigfx::framebufferBytes(8, 8, famigfx::PixelFormat::Mono1));
    return 0;
}
