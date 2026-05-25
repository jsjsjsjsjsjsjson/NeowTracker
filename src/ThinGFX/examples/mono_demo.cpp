#include "thingfx/ThinGFX.hpp"

#include <stdio.h>
#include <stdlib.h>

static uint16_t parse_dimension(const char *text, uint16_t fallback)
{
    long value = text ? strtol(text, NULL, 10) : fallback;
    if (value < 16) value = 16;
    if (value > 32767) value = 32767;
    return (uint16_t)value;
}

static int write_pbm(const char *path, const thingfx::CanvasBuffer *canvas)
{
    FILE *fp = fopen(path, "w");
    int16_t y;
    int16_t x;
    if (!fp) return -1;
    thingfx::Canvas view(const_cast<thingfx::CanvasBuffer *>(canvas));
    fprintf(fp, "P1\n%u %u\n", canvas->width, canvas->height);
    for (y = 0; y < (int16_t)canvas->height; ++y) {
        for (x = 0; x < (int16_t)canvas->width; ++x) {
            fputc(view.getPixel(x, y) ? '1' : '0', fp);
            fputc(x == (int16_t)(canvas->width - 1) ? '\n' : ' ', fp);
        }
    }
    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    uint16_t width = argc > 1 ? parse_dimension(argv[1], 128) : 128;
    uint16_t height = argc > 2 ? parse_dimension(argv[2], 64) : 64;
    const char *out_path = argc > 3 ? argv[3] : "mono_demo.pbm";

    thingfx::Gui gui(width, height);
    thingfx::OwnedCanvas output(width, height, thingfx::PixelFormat::Mono1);
    if (!gui.valid() || !output.valid()) {
        fprintf(stderr, "failed to allocate mono demo %ux%u\n", width, height);
        return 1;
    }

    thingfx::WindowConfig cfg;
    cfg.kind = thingfx::NodeKind::Layer;
    cfg.frame = {0, 0, (int16_t)width, (int16_t)height};
    cfg.ownsCanvas = true;
    cfg.canvasFormat = thingfx::PixelFormat::Mono1;
    cfg.showTitle = false;
    cfg.border = thingfx::WindowBorder::None;

    thingfx::Window panel = gui.createLayer(cfg);
    if (!panel.valid()) {
        fprintf(stderr, "failed to create mono panel\n");
        return 1;
    }

    thingfx::Canvas gfx = panel.draw();
    gfx.fillScreen(0);
    gfx.drawRect(0, 0, gfx.width(), gfx.height(), 255);
    gfx.setCursor(6, 6);
    gfx.setTextSize(2);
    gfx.setTextColor(255, 0);
    gfx.print("MONO1");
    gfx.drawLine(0, (int16_t)(gfx.height() - 1), (int16_t)(gfx.width() - 1), 0, 255);
    gfx.fillCircle((int16_t)(gfx.width() - 18), (int16_t)(gfx.height() - 18), 10, 255);
    gfx.drawRect(4, (int16_t)(gfx.height() / 2), (int16_t)(gfx.width() / 2), 18, 255);

    if (!panel.commit() || !gui.render(output) || write_pbm(out_path, output.native()) != 0) {
        fprintf(stderr, "failed to render mono demo\n");
        return 1;
    }

    printf("mono demo rendered %ux%u to %s (%zu bytes output buffer)\n",
           width, height, out_path,
           thingfx::framebufferBytes(width, height, thingfx::PixelFormat::Mono1));
    return 0;
}
