#include "thingfx/ThinGFX.hpp"
#include "ThinGFXInternal.hpp"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace thingfx {
using namespace detail;

Port::Port()
{
    target.width = 0;
    target.height = 0;
    target.stride = 0;
    target.format = PixelFormat::Gray8;
    target.pixels = 0;
    target.clip_x = 0;
    target.clip_y = 0;
    target.clip_w = 0;
    target.clip_h = 0;
}

#if !defined(THINGFX_HAS_DEFAULT_PORT)
Port *createDefaultPort(uint16_t width, uint16_t height, const PortConfig &config)
{
    (void)width;
    (void)height;
    (void)config;
    return 0;
}

void destroyDefaultPort(Port *port)
{
    delete port;
}
#endif


static tgfx_color_t invert_color(tgfx_color_t c)
{
    return static_cast<tgfx_color_t>(255u - c);
}

size_t Print::write(const char *text)
{
    if (!text) {
        return 0;
    }
    return write(reinterpret_cast<const uint8_t *>(text), strlen(text));
}

size_t Print::write(const uint8_t *buffer, size_t size)
{
    size_t written = 0;
    if (!buffer) {
        return 0;
    }
    for (size_t i = 0; i < size; ++i) {
        written += write(buffer[i]);
    }
    return written;
}

size_t Print::print(const char *text) { return write(text); }
size_t Print::print(char ch) { return write(static_cast<uint8_t>(ch)); }

size_t Print::printUnsigned(unsigned long value, int base)
{
    char buf[33];
    char *p = &buf[sizeof(buf) - 1];
    const char *digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (base < 2 || base > 36) {
        base = 10;
    }

    *p = '\0';
    do {
        *--p = digits[value % static_cast<unsigned long>(base)];
        value /= static_cast<unsigned long>(base);
    } while (value);

    return write(p);
}

size_t Print::print(int value, int base) { return print(static_cast<long>(value), base); }
size_t Print::print(unsigned int value, int base) { return printUnsigned(value, base); }

size_t Print::print(long value, int base)
{
    if (base == 10 && value < 0) {
        unsigned long magnitude = static_cast<unsigned long>(-(value + 1)) + 1ul;
        return write(static_cast<uint8_t>('-')) + printUnsigned(magnitude, 10);
    }
    return printUnsigned(static_cast<unsigned long>(value), base);
}

size_t Print::print(unsigned long value, int base) { return printUnsigned(value, base); }
size_t Print::print(float value, int digits) { return print(static_cast<double>(value), digits); }

size_t Print::print(double value, int digits)
{
    char format[8];
    char buf[48];

    if (digits < 0) {
        digits = 2;
    }
    if (digits > 9) {
        digits = 9;
    }

    snprintf(format, sizeof(format), "%%.%df", digits);
    snprintf(buf, sizeof(buf), format, value);
    return write(buf);
}

size_t Print::println() { return write("\r\n"); }
size_t Print::println(const char *text) { return print(text) + println(); }
size_t Print::println(int value, int base) { return print(value, base) + println(); }
size_t Print::println(unsigned int value, int base) { return print(value, base) + println(); }
size_t Print::println(long value, int base) { return print(value, base) + println(); }
size_t Print::println(unsigned long value, int base) { return print(value, base) + println(); }
size_t Print::println(float value, int digits) { return print(value, digits) + println(); }
size_t Print::println(double value, int digits) { return print(value, digits) + println(); }

size_t Print::printf(const char *format, ...)
{
    char stack_buf[128];
    va_list args;
    int needed;

    if (!format) {
        return 0;
    }

    va_start(args, format);
    needed = vsnprintf(stack_buf, sizeof(stack_buf), format, args);
    va_end(args);

    if (needed < 0) {
        return 0;
    }
    if (static_cast<size_t>(needed) < sizeof(stack_buf)) {
        return write(stack_buf);
    }

    char *heap_buf = new char[static_cast<size_t>(needed) + 1];
    va_start(args, format);
    vsnprintf(heap_buf, static_cast<size_t>(needed) + 1, format, args);
    va_end(args);
    size_t written = write(heap_buf);
    delete[] heap_buf;
    return written;
}

ThinGFX::ThinGFX(tgfx_canvas_t *canvas)
    : canvas_(0),
      raw_width_(0),
      raw_height_(0),
      width_(0),
      height_(0),
      cursor_x_(0),
      cursor_y_(0),
      text_color_(255),
      text_bg_(255),
      text_size_x_(1),
      text_size_y_(1),
      rotation_(0),
      wrap_(true),
      cp437_(false),
      inverted_(false),
      font_(defaultFont())
{
    setCanvas(canvas);
}

void ThinGFX::setCanvas(tgfx_canvas_t *canvas)
{
    canvas_ = canvas;
    raw_width_ = canvas ? static_cast<int16_t>(canvas->width) : 0;
    raw_height_ = canvas ? static_cast<int16_t>(canvas->height) : 0;
    setRotation(rotation_);
}

bool ThinGFX::resetClip()
{
    if (!canvas_) {
        return false;
    }
    tgfx_canvas_reset_clip(canvas_);
    return true;
}

bool ThinGFX::setClip(const Rect &rect)
{
    tgfx_rect_t native;
    native.x = rect.x;
    native.y = rect.y;
    native.w = rect.w;
    native.h = rect.h;
    return canvas_ && tgfx_canvas_set_clip(canvas_, native) == TGFX_OK;
}

bool ThinGFX::intersectClip(const Rect &rect)
{
    tgfx_rect_t native;
    native.x = rect.x;
    native.y = rect.y;
    native.w = rect.w;
    native.h = rect.h;
    return canvas_ && tgfx_canvas_intersect_clip(canvas_, native) == TGFX_OK;
}

Rect ThinGFX::clip() const
{
    tgfx_rect_t native;
    Rect out;
    native.x = 0;
    native.y = 0;
    native.w = 0;
    native.h = 0;
    if (canvas_) {
        tgfx_canvas_get_clip(canvas_, &native);
    }
    out.x = native.x;
    out.y = native.y;
    out.w = native.w;
    out.h = native.h;
    return out;
}

void ThinGFX::setRotation(uint8_t rotation)
{
    rotation_ = rotation & 3u;
    if (rotation_ & 1u) {
        width_ = raw_height_;
        height_ = raw_width_;
    } else {
        width_ = raw_width_;
        height_ = raw_height_;
    }
}

void ThinGFX::drawPixel(int16_t x, int16_t y, tgfx_color_t color)
{
    if (!canvas_) {
        return;
    }
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    switch (rotation_) {
    case 1:
        {
            int16_t t = x;
            x = y;
            y = static_cast<int16_t>(raw_height_ - 1 - t);
        }
        break;
    case 2:
        x = static_cast<int16_t>(raw_width_ - 1 - x);
        y = static_cast<int16_t>(raw_height_ - 1 - y);
        break;
    case 3:
        {
            int16_t t = x;
            x = static_cast<int16_t>(raw_width_ - 1 - y);
            y = t;
        }
        break;
    default:
        break;
    }

    tgfx_draw_pixel(canvas_, x, y, inverted_ ? invert_color(color) : color);
}

tgfx_color_t ThinGFX::getPixel(int16_t x, int16_t y) const
{
    if (!canvas_) {
        return 0;
    }
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return 0;
    }
    return tgfx_get_pixel(canvas_, x, y);
}

void ThinGFX::writePixel(int16_t x, int16_t y, tgfx_color_t color)
{
    drawPixel(x, y, color);
}

void ThinGFX::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                            tgfx_color_t color)
{
    fillRect(x, y, w, h, color);
}

void ThinGFX::writeFastVLine(int16_t x, int16_t y, int16_t h, tgfx_color_t color)
{
    drawFastVLine(x, y, h, color);
}

void ThinGFX::writeFastHLine(int16_t x, int16_t y, int16_t w, tgfx_color_t color)
{
    drawFastHLine(x, y, w, color);
}

void ThinGFX::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        tgfx_color_t color)
{
    drawLine(x0, y0, x1, y1, color);
}

void ThinGFX::drawFastHLine(int16_t x, int16_t y, int16_t w, tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_fill_rect(canvas_, x, y, w, 1, color);
        return;
    }
    for (int16_t i = 0; i < w; ++i) {
        drawPixel(static_cast<int16_t>(x + i), y, color);
    }
}

void ThinGFX::drawFastVLine(int16_t x, int16_t y, int16_t h, tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_fill_rect(canvas_, x, y, 1, h, color);
        return;
    }
    for (int16_t i = 0; i < h; ++i) {
        drawPixel(x, static_cast<int16_t>(y + i), color);
    }
}

void ThinGFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_fill_rect(canvas_, x, y, w, h, color);
        return;
    }
    for (int16_t iy = 0; iy < h; ++iy) {
        drawFastHLine(x, static_cast<int16_t>(y + iy), w, color);
    }
}

void ThinGFX::fillScreen(tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_clear(canvas_, color);
        return;
    }
    fillRect(0, 0, width_, height_, color);
}

void ThinGFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       tgfx_color_t color)
{
    int16_t dx = static_cast<int16_t>(x1 > x0 ? x1 - x0 : x0 - x1);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = static_cast<int16_t>(-(y1 > y0 ? y1 - y0 : y0 - y1));
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = static_cast<int16_t>(dx + dy);

    for (;;) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int16_t e2 = static_cast<int16_t>(2 * err);
        if (e2 >= dy) {
            err = static_cast<int16_t>(err + dy);
            x0 = static_cast<int16_t>(x0 + sx);
        }
        if (e2 <= dx) {
            err = static_cast<int16_t>(err + dx);
            y0 = static_cast<int16_t>(y0 + sy);
        }
    }
}

void ThinGFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       tgfx_color_t color)
{
    drawFastHLine(x, y, w, color);
    drawFastHLine(x, static_cast<int16_t>(y + h - 1), w, color);
    drawFastVLine(x, y, h, color);
    drawFastVLine(static_cast<int16_t>(x + w - 1), y, h, color);
}

void ThinGFX::drawCircle(int16_t x0, int16_t y0, int16_t r, tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_draw_circle(canvas_, x0, y0, r, color);
        return;
    }
    int16_t f = static_cast<int16_t>(1 - r);
    int16_t ddx = 1;
    int16_t ddy = static_cast<int16_t>(-2 * r);
    int16_t x = 0;
    int16_t y = r;
    drawPixel(x0, static_cast<int16_t>(y0 + r), color);
    drawPixel(x0, static_cast<int16_t>(y0 - r), color);
    drawPixel(static_cast<int16_t>(x0 + r), y0, color);
    drawPixel(static_cast<int16_t>(x0 - r), y0, color);
    while (x < y) {
        if (f >= 0) {
            --y;
            ddy = static_cast<int16_t>(ddy + 2);
            f = static_cast<int16_t>(f + ddy);
        }
        ++x;
        ddx = static_cast<int16_t>(ddx + 2);
        f = static_cast<int16_t>(f + ddx);
        drawPixel(static_cast<int16_t>(x0 + x), static_cast<int16_t>(y0 + y), color);
        drawPixel(static_cast<int16_t>(x0 - x), static_cast<int16_t>(y0 + y), color);
        drawPixel(static_cast<int16_t>(x0 + x), static_cast<int16_t>(y0 - y), color);
        drawPixel(static_cast<int16_t>(x0 - x), static_cast<int16_t>(y0 - y), color);
        drawPixel(static_cast<int16_t>(x0 + y), static_cast<int16_t>(y0 + x), color);
        drawPixel(static_cast<int16_t>(x0 - y), static_cast<int16_t>(y0 + x), color);
        drawPixel(static_cast<int16_t>(x0 + y), static_cast<int16_t>(y0 - x), color);
        drawPixel(static_cast<int16_t>(x0 - y), static_cast<int16_t>(y0 - x), color);
    }
}

void ThinGFX::fillCircle(int16_t x0, int16_t y0, int16_t r, tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_fill_circle(canvas_, x0, y0, r, color);
        return;
    }
    for (int16_t y = static_cast<int16_t>(-r); y <= r; ++y) {
        for (int16_t x = static_cast<int16_t>(-r); x <= r; ++x) {
            if (x * x + y * y <= r * r) {
                drawPixel(static_cast<int16_t>(x0 + x), static_cast<int16_t>(y0 + y), color);
            }
        }
    }
}

void ThinGFX::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2, tgfx_color_t color)
{
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

void ThinGFX::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2, tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_fill_triangle(canvas_, x0, y0, x1, y1, x2, y2, color);
    } else {
        tgfx_fill_triangle(canvas_, x0, y0, x1, y1, x2, y2, color);
    }
}

void ThinGFX::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                            int16_t radius, tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_draw_round_rect(canvas_, x, y, w, h, radius, color);
        return;
    }
    drawRect(x, y, w, h, color);
}

void ThinGFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                            int16_t radius, tgfx_color_t color)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_fill_round_rect(canvas_, x, y, w, h, radius, color);
        return;
    }
    (void)radius;
    fillRect(x, y, w, h, color);
}

void ThinGFX::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                         int16_t w, int16_t h, tgfx_color_t color)
{
    drawBitmap(x, y, bitmap, w, h, color, color);
}

void ThinGFX::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                         int16_t w, int16_t h, tgfx_color_t color,
                         tgfx_color_t bg)
{
    if (rotation_ == 0 && canvas_ && !inverted_) {
        tgfx_draw_bitmap_1bpp(canvas_, x, y, bitmap, w, h, color, bg);
        return;
    }
    if (!bitmap) {
        return;
    }
    for (int16_t yy = 0; yy < h; ++yy) {
        for (int16_t xx = 0; xx < w; ++xx) {
            uint8_t byte = bitmap[static_cast<size_t>(yy) * ((w + 7) / 8) + (xx / 8)];
            if (byte & (0x80u >> (xx & 7))) {
                drawPixel(static_cast<int16_t>(x + xx), static_cast<int16_t>(y + yy), color);
            } else if (bg != color) {
                drawPixel(static_cast<int16_t>(x + xx), static_cast<int16_t>(y + yy), bg);
            }
        }
    }
}

void ThinGFX::drawChar(int16_t x, int16_t y, unsigned char ch,
                       tgfx_color_t color, tgfx_color_t bg, uint8_t size)
{
    drawChar(x, y, ch, color, bg, size, size);
}

void ThinGFX::drawChar(int16_t x, int16_t y, unsigned char ch,
                       tgfx_color_t color, tgfx_color_t bg, uint8_t size_x,
                       uint8_t size_y)
{
    if (!canvas_) {
        return;
    }
    if (font_) {
        tgfx_draw_char_font(canvas_, font_, x, y, ch, color, bg, size_x, size_y);
    } else {
        tgfx_draw_char_5x7(canvas_, x, y, static_cast<char>(ch), color, bg,
                           size_x, size_y);
    }
}

void ThinGFX::getTextBounds(const char *text, int16_t x, int16_t y,
                            int16_t *x1, int16_t *y1, uint16_t *w,
                            uint16_t *h) const
{
    if (font_) {
        tgfx_text_bounds_font(font_, text, x, y, text_size_x_, text_size_y_,
                              x1, y1, w, h);
    } else {
        tgfx_text_bounds_5x7(text, x, y, text_size_x_, text_size_y_, x1, y1,
                             w, h);
    }
}

size_t ThinGFX::write(uint8_t byte)
{
    if (!canvas_) {
        return 0;
    }
    if (byte == '\r') {
        return 1;
    }
    if (byte == '\n') {
        cursor_x_ = 0;
        cursor_y_ = static_cast<int16_t>(cursor_y_ +
                                         text_size_y_ * (font_ ? font_->yAdvance : 8));
        return 1;
    }

    int16_t char_w = static_cast<int16_t>(text_size_x_ * 6);
    if (font_ && byte >= font_->first && byte <= font_->last) {
        char_w = static_cast<int16_t>(font_->glyph[byte - font_->first].xAdvance *
                                      text_size_x_);
    }
    if (wrap_ && cursor_x_ > 0 && cursor_x_ + char_w > width_) {
        cursor_x_ = 0;
        cursor_y_ = static_cast<int16_t>(cursor_y_ +
                                         text_size_y_ * (font_ ? font_->yAdvance : 8));
    }

    drawChar(cursor_x_, cursor_y_, byte, text_color_, text_bg_, text_size_x_,
             text_size_y_);
    cursor_x_ = static_cast<int16_t>(cursor_x_ + char_w);
    return 1;
}

void ThinGFX::setTextSize(uint8_t size_x, uint8_t size_y)
{
    text_size_x_ = size_x ? size_x : 1;
    text_size_y_ = size_y ? size_y : 1;
}

void ThinGFX::setFont(const Font *font)
{
    font_ = font ? font : defaultFont();
}

OwnedCanvas::OwnedCanvas(uint16_t width, uint16_t height, PixelFormat format)
    : ThinGFX(tgfx_canvas_create(width, height, nativePixelFormat(format)))
{
}

OwnedCanvas::~OwnedCanvas()
{
    tgfx_canvas_t *canvas = canvas_;
    canvas_ = 0;
    tgfx_canvas_destroy(canvas);
}

static tgfx_rect_t toNativeRect(const Rect &rect);

static tgfx_rect_t toNativeRect(const Rect &rect)
{
    tgfx_rect_t native;
    native.x = rect.x;
    native.y = rect.y;
    native.w = rect.w;
    native.h = rect.h;
    return native;
}

static Rect fromNativeRect(const tgfx_rect_t &rect)
{
    Rect native;
    native.x = rect.x;
    native.y = rect.y;
    native.w = rect.w;
    native.h = rect.h;
    return native;
}

static uint32_t speedToPerMille(float speed)
{
    if (!(speed > 0.0f)) {
        return 1000u;
    }
    if (speed > 64.0f) {
        speed = 64.0f;
    }
    return static_cast<uint32_t>(speed * 1000.0f + 0.5f);
}

static float speedFromPerMille(uint32_t speed)
{
    return speed ? static_cast<float>(speed) / 1000.0f : 1.0f;
}

static tgfx_transition_t toNativeTransition(const Transition &transition)
{
    tgfx_transition_t native;
    native.easing = static_cast<tgfx_easing_t>(transition.easing);
    native.duration_ms = transition.durationMs;
    native.speed_per_mille = speedToPerMille(transition.speed);
    return native;
}

static Transition fromNativeTransition(const tgfx_transition_t &transition)
{
    Transition out;
    out.easing = static_cast<Easing>(transition.easing);
    out.durationMs = transition.duration_ms;
    out.speed = speedFromPerMille(transition.speed_per_mille);
    return out;
}

Rect Window::rect() const
{
    tgfx_rect_t native;
    native.x = 0;
    native.y = 0;
    native.w = 0;
    native.h = 0;
    if (!valid()) {
        return fromNativeRect(native);
    }
    tgfx_node_get_rect(gui_->ctx_, handle_, &native);
    return fromNativeRect(native);
}

bool Window::setRect(const Rect &rect)
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_set_rect(gui_->ctx_, handle_, toNativeRect(rect)) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

bool Window::setRectImmediate(const Rect &rect)
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_set_rect_immediate(gui_->ctx_, handle_, toNativeRect(rect)) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

bool Window::setRectAnimated(const Rect &rect, const Transition &transition)
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_set_rect_animated(gui_->ctx_, handle_, toNativeRect(rect),
                                          toNativeTransition(transition)) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

bool Window::setTransition(const Transition &transition)
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_set_transition(gui_->ctx_, handle_,
                                       toNativeTransition(transition)) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    return ok;
}

Transition Window::transition() const
{
    tgfx_transition_t native;
    native.easing = TGFX_EASING_NONE;
    native.duration_ms = 0;
    native.speed_per_mille = 1000;
    if (valid()) {
        tgfx_node_get_transition(gui_->ctx_, handle_, &native);
    }
    return fromNativeTransition(native);
}

bool Window::setDestroyTransition(const Transition &transition)
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_set_destroy_transition(gui_->ctx_, handle_,
                                               toNativeTransition(transition)) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    return ok;
}

Transition Window::destroyTransition() const
{
    tgfx_transition_t native;
    native.easing = TGFX_EASING_NONE;
    native.duration_ms = 0;
    native.speed_per_mille = 1000;
    if (valid()) {
        tgfx_node_get_destroy_transition(gui_->ctx_, handle_, &native);
    }
    return fromNativeTransition(native);
}

bool Window::setVisible(bool visible)
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_set_visible(gui_->ctx_, handle_, visible ? 1u : 0u) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

bool Window::setOpacity(uint8_t opacity)
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_set_opacity(gui_->ctx_, handle_, opacity) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

bool Window::bringToFront()
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_bring_to_front(gui_->ctx_, handle_) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

bool Window::sendToBack()
{
    if (!valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_send_to_back(gui_->ctx_, handle_) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

bool Window::setParent(const Window &parent)
{
    if (!valid() || parent.gui_ != gui_ || !parent.valid()) {
        return false;
    }
    tgfx_context_lock(gui_->ctx_);
    bool ok = tgfx_node_set_parent(gui_->ctx_, handle_, parent.handle_) == TGFX_OK;
    tgfx_context_unlock(gui_->ctx_);
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

Window Window::parent() const
{
    if (!valid()) {
        return Window();
    }
    tgfx_handle_t parent = tgfx_node_parent(gui_->ctx_, handle_);
    return parent != TGFX_INVALID_HANDLE ? Window(gui_, parent) : Window();
}

Window Window::firstChild() const
{
    if (!valid()) {
        return Window();
    }
    tgfx_handle_t child = tgfx_node_first_child(gui_->ctx_, handle_);
    return child != TGFX_INVALID_HANDLE ? Window(gui_, child) : Window();
}

Window Window::nextSibling() const
{
    if (!valid()) {
        return Window();
    }
    tgfx_handle_t sibling = tgfx_node_next_sibling(gui_->ctx_, handle_);
    return sibling != TGFX_INVALID_HANDLE ? Window(gui_, sibling) : Window();
}

Window Window::createChild(const WindowConfig &config) const
{
    return valid() ? gui_->createWindow(*this, config) : Window();
}

bool Window::focus()
{
    if (!valid()) {
        return false;
    }
    return gui_->setFocus(*this);
}

bool Window::invalidate(const Rect &rect)
{
    if (!valid()) {
        return false;
    }
    bool ok = tgfx_invalidate(gui_->ctx_, handle_, toNativeRect(rect)) == TGFX_OK;
    if (ok) {
        gui_->notifyDaemon();
    }
    return ok;
}

bool Window::invalidate()
{
    Rect r = rect();
    r.x = 0;
    r.y = 0;
    return invalidate(r);
}

bool Window::dispatch(const tgfx_event_t &event) const
{
    if (!valid()) {
        return false;
    }
    return tgfx_dispatch_to(gui_->ctx_, handle_, &event) != 0;
}

bool Window::destroy()
{
    if (!valid()) {
        return false;
    }
    Gui *gui = gui_;
    tgfx_handle_t handle = handle_;
    tgfx_result_t result;

    gui_ = 0;
    handle_ = TGFX_INVALID_HANDLE;
    tgfx_context_lock(gui->ctx_);
    result = tgfx_node_destroy(gui->ctx_, handle);
    if (result == TGFX_OK && !tgfx_node_exists(gui->ctx_, handle)) {
        gui->removeBindingsRecursive(handle);
    }
    tgfx_context_unlock(gui->ctx_);
    if (result == TGFX_OK) {
        gui->notifyDaemon();
    }
    return result == TGFX_OK;
}

bool Window::destroyImmediate()
{
    if (!valid()) {
        return false;
    }
    Gui *gui = gui_;
    tgfx_handle_t handle = handle_;
    tgfx_result_t result;

    gui_ = 0;
    handle_ = TGFX_INVALID_HANDLE;
    tgfx_context_lock(gui->ctx_);
    result = tgfx_node_destroy_immediate(gui->ctx_, handle);
    if (result == TGFX_OK) {
        gui->removeBindingsRecursive(handle);
    }
    tgfx_context_unlock(gui->ctx_);
    if (result == TGFX_OK) {
        gui->notifyDaemon();
    }
    return result == TGFX_OK;
}

bool Window::destroyAnimated(const Transition &transition)
{
    if (!valid()) {
        return false;
    }
    Gui *gui = gui_;
    tgfx_handle_t handle = handle_;
    tgfx_result_t result;

    gui_ = 0;
    handle_ = TGFX_INVALID_HANDLE;
    tgfx_context_lock(gui->ctx_);
    result = tgfx_node_destroy_animated(gui->ctx_, handle,
                                        toNativeTransition(transition));
    if (result == TGFX_OK && !tgfx_node_exists(gui->ctx_, handle)) {
        gui->removeBindingsRecursive(handle);
    }
    tgfx_context_unlock(gui->ctx_);
    if (result == TGFX_OK) {
        gui->notifyDaemon();
    }
    return result == TGFX_OK;
}

Canvas Window::canvas() const
{
    return back();
}

Canvas Window::back() const
{
    return Canvas(valid() ? tgfx_node_back_canvas(gui_->ctx_, handle_) : 0);
}

Canvas Window::draw() const
{
    return back();
}

bool Window::commit()
{
    return valid() && tgfx_node_commit(gui_->ctx_, handle_) == TGFX_OK;
}

bool Window::commit(const Rect &rect)
{
    return valid() && tgfx_node_commit_rect(gui_->ctx_, handle_, toNativeRect(rect)) == TGFX_OK;
}

bool Window::discard()
{
    return valid() && tgfx_node_discard(gui_->ctx_, handle_) == TGFX_OK;
}

Gui::Gui(uint16_t width, uint16_t height)
    : ctx_(tgfx_create(width, height)), owned_port_(0), owns_port_(false), daemon_started_(false)
{
}

Gui::Gui(uint16_t width, uint16_t height, PixelFormat outputFormat)
    : ctx_(tgfx_create(width, height)), owned_port_(0), owns_port_(false), daemon_started_(false)
{
    PortConfig config;
    config.format = outputFormat;
    config.autoStartDaemon = true;
    owned_port_ = createDefaultPort(width, height, config);
    if (ctx_ && owned_port_ && owned_port_->valid() &&
        tgfx_context_attach_port(ctx_, owned_port_) == TGFX_OK) {
        owns_port_ = true;
        if (config.autoStartDaemon) {
            startDaemon();
        }
    } else if (owned_port_) {
        destroyDefaultPort(owned_port_);
        owned_port_ = 0;
    }
}

Gui::Gui(uint16_t width, uint16_t height, const PortConfig &portConfig)
    : ctx_(tgfx_create(width, height)), owned_port_(0), owns_port_(false), daemon_started_(false)
{
    owned_port_ = createDefaultPort(width, height, portConfig);
    if (ctx_ && owned_port_ && owned_port_->valid() &&
        tgfx_context_attach_port(ctx_, owned_port_) == TGFX_OK) {
        owns_port_ = true;
        if (portConfig.autoStartDaemon) {
            startDaemon();
        }
    } else if (owned_port_) {
        destroyDefaultPort(owned_port_);
        owned_port_ = 0;
    }
}

Gui::Gui(uint16_t width, uint16_t height, Port *externalPort)
    : ctx_(tgfx_create(width, height)), owned_port_(0), owns_port_(false), daemon_started_(false)
{
    if (ctx_ && externalPort) {
        tgfx_context_attach_port(ctx_, externalPort);
    }
}

Gui::~Gui()
{
    stopDaemon();
    for (size_t i = 0; i < bindings_.size(); ++i) {
        delete bindings_[i];
    }
    bindings_.clear();
    tgfx_destroy(ctx_);
    ctx_ = 0;
    if (owns_port_ && owned_port_) {
        destroyDefaultPort(owned_port_);
    }
    owned_port_ = 0;
    owns_port_ = false;
}

uint16_t Gui::width() const
{
    return tgfx_width(ctx_);
}

uint16_t Gui::height() const
{
    return tgfx_height(ctx_);
}

bool Gui::resize(uint16_t width, uint16_t height)
{
    return tgfx_resize(ctx_, width, height) == TGFX_OK;
}

bool Gui::needsRender() const
{
    return tgfx_needs_render(ctx_) != 0;
}

void Gui::tick(uint32_t delta_ms)
{
    tgfx_tick(ctx_, delta_ms);
}

bool Gui::animating() const
{
    return tgfx_animating(ctx_) != 0;
}

void Gui::lock()
{
    if (ctx_) {
        tgfx_context_lock(ctx_);
    }
}

void Gui::unlock()
{
    if (ctx_) {
        tgfx_context_unlock(ctx_);
    }
}

Window Gui::root() const
{
    tgfx_context_t *ctx = const_cast<tgfx_context_t *>(ctx_);
    tgfx_handle_t handle = tgfx_root_handle(ctx);
    return handle != TGFX_INVALID_HANDLE ? Window(const_cast<Gui *>(this), handle)
                                         : Window();
}

Window Gui::createWindow(const WindowConfig &config)
{
    Window root_window = root();
    return root_window.valid() ? createWindow(root_window, config) : Window();
}

Window Gui::createWindow(const Window &parent, const WindowConfig &config)
{
    tgfx_node_desc_t desc;
    tgfx_handle_t handle = TGFX_INVALID_HANDLE;
    NodeBinding *binding;
    tgfx_result_t result;

    if (!ctx_ || parent.gui_ != this || !parent.valid()) {
        return Window();
    }

    binding = new NodeBinding;
    binding->gui = this;
    binding->handle = TGFX_INVALID_HANDLE;
    binding->draw = config.onDraw;
    binding->event = config.onEvent;
    binding->user = config.user;

    desc.kind = static_cast<tgfx_node_kind_t>(config.kind);
    desc.x = config.frame.x;
    desc.y = config.frame.y;
    desc.w = config.frame.w;
    desc.h = config.frame.h;
    desc.opacity = config.opacity;
    desc.visible = config.visible ? 1u : 0u;
    desc.owns_canvas = config.ownsCanvas ? 1u : 0u;
    desc.canvas_format = nativePixelFormat(config.canvasFormat);
    desc.show_title = config.showTitle ? 1u : 0u;
    desc.border = static_cast<tgfx_window_border_t>(config.border);
    desc.title = config.title;
    desc.transition = toNativeTransition(config.transition);
    desc.create_transition = toNativeTransition(config.createTransition);
    desc.destroy_transition = toNativeTransition(config.destroyTransition);
    desc.draw = config.onDraw ? Gui::drawThunk : 0;
    desc.event = config.onEvent ? Gui::eventThunk : 0;
    desc.user = binding;

    tgfx_context_lock(ctx_);
    result = tgfx_node_create(ctx_, parent.handle_, &desc, &handle);
    if (result == TGFX_OK) {
        binding->handle = handle;
        bindings_.push_back(binding);
    }
    tgfx_context_unlock(ctx_);

    if (result != TGFX_OK) {
        delete binding;
        return Window();
    }

    notifyDaemon();
    return Window(this, handle);
}

Window Gui::createLayer(const WindowConfig &config)
{
    WindowConfig layer_config = config;
    layer_config.kind = NodeKind::Layer;
    layer_config.showTitle = false;
    layer_config.border = WindowBorder::None;
    return createWindow(layer_config);
}

Window Gui::createLayer(const Window &parent, const WindowConfig &config)
{
    WindowConfig layer_config = config;
    layer_config.kind = NodeKind::Layer;
    layer_config.showTitle = false;
    layer_config.border = WindowBorder::None;
    return createWindow(parent, layer_config);
}

bool Gui::setFocus(const Window &window)
{
    if (!ctx_ || window.gui_ != this) {
        return false;
    }
    return tgfx_focus_set(ctx_, window.handle_) == TGFX_OK;
}

bool Gui::clearFocus()
{
    return ctx_ && tgfx_focus_set(ctx_, TGFX_INVALID_HANDLE) == TGFX_OK;
}

bool Gui::focusNext()
{
    return ctx_ && tgfx_focus_next(ctx_) != TGFX_INVALID_HANDLE;
}

bool Gui::focusPrevious()
{
    return ctx_ && tgfx_focus_prev(ctx_) != TGFX_INVALID_HANDLE;
}

Window Gui::focus() const
{
    tgfx_context_t *ctx = const_cast<tgfx_context_t *>(ctx_);
    tgfx_handle_t handle = tgfx_focus_get(ctx);
    return handle != TGFX_INVALID_HANDLE ? Window(const_cast<Gui *>(this), handle)
                                         : Window();
}

bool Gui::dispatchTo(const Window &window, const tgfx_event_t &event)
{
    if (!ctx_ || window.gui_ != this || !window.valid()) {
        return false;
    }
    return tgfx_dispatch_to(ctx_, window.handle_, &event) != 0;
}

bool Gui::dispatchEvent(const tgfx_event_t &event, Window *target)
{
    tgfx_handle_t handle = TGFX_INVALID_HANDLE;
    uint8_t handled;
    if (!ctx_) {
        return false;
    }
    handled = tgfx_dispatch_event(ctx_, &event, &handle);
    if (target) {
        *target = handle != TGFX_INVALID_HANDLE ? Window(this, handle) : Window();
    }
    return handled != 0;
}

bool Gui::dispatchKey(uint32_t key, bool pressed, bool repeat,
                      uint32_t modifiers, Window *target)
{
    tgfx_handle_t handle = TGFX_INVALID_HANDLE;
    uint8_t handled;
    if (!ctx_) {
        return false;
    }
    handled = tgfx_dispatch_key(ctx_, key, pressed ? 1u : 0u,
                                repeat ? 1u : 0u, modifiers, &handle);
    if (target) {
        *target = handle != TGFX_INVALID_HANDLE ? Window(this, handle) : Window();
    }
    return handled != 0;
}

bool Gui::render(ThinGFX &target)
{
    return render(target.native());
}

bool Gui::render(tgfx_canvas_t *target)
{
    return tgfx_render(ctx_, target) == TGFX_OK;
}

bool Gui::attachPort(Port *port)
{
    if (!ctx_ || !port || !port->valid()) {
        return false;
    }
    stopDaemon();
    bool ok = tgfx_context_attach_port(ctx_, port) == TGFX_OK;
    if (ok && owns_port_ && owned_port_ && owned_port_ != port) {
        destroyDefaultPort(owned_port_);
        owned_port_ = 0;
        owns_port_ = false;
    }
    return ok;
}

Port *Gui::port()
{
    return ctx_ ? tgfx_context_port(ctx_) : 0;
}

const Port *Gui::port() const
{
    return ctx_ ? tgfx_context_port(const_cast<tgfx_context_t *>(ctx_)) : 0;
}

bool Gui::portValid() const
{
    const Port *p = port();
    return p && p->valid();
}

bool Gui::pump()
{
    Port *p = port();
    bool result = p ? p->pump(*this) : true;
    removeStaleBindings();
    return result;
}

bool Gui::present(const Rect *dirty)
{
    (void)dirty;
    bool ok = ctx_ && tgfx_daemon_step(ctx_) == TGFX_OK;
    removeStaleBindings();
    return ok;
}

bool Gui::startDaemon()
{
    if (!ctx_ || daemon_started_) {
        return ctx_ != 0;
    }
    bool ok = tgfx_daemon_start(ctx_) == TGFX_OK;
    daemon_started_ = ok;
    return ok;
}

void Gui::stopDaemon()
{
    if (ctx_ && daemon_started_) {
        tgfx_daemon_stop(ctx_);
        daemon_started_ = false;
    }
}

bool Gui::daemonStep()
{
    bool ok = ctx_ && tgfx_daemon_step(ctx_) == TGFX_OK;
    removeStaleBindings();
    return ok;
}

void Gui::notifyDaemon()
{
    if (ctx_) {
        tgfx_daemon_notify(ctx_);
    }
}



void Gui::drawThunk(tgfx_context_t *ctx, tgfx_handle_t handle,
                    tgfx_canvas_t *canvas, const tgfx_rect_t *dirty,
                    void *user)
{
    NodeBinding *binding = static_cast<NodeBinding *>(user);
    Rect rect;
    (void)ctx;
    if (!binding || !binding->draw) {
        return;
    }
    rect.x = dirty ? dirty->x : 0;
    rect.y = dirty ? dirty->y : 0;
    rect.w = dirty ? dirty->w : 0;
    rect.h = dirty ? dirty->h : 0;

    Window window(binding->gui, handle);
    Canvas gfx(canvas);
    binding->draw(window, gfx, rect, binding->user);
}

uint8_t Gui::eventThunk(tgfx_context_t *ctx, tgfx_handle_t handle,
                        const tgfx_event_t *event, void *user)
{
    NodeBinding *binding = static_cast<NodeBinding *>(user);
    (void)ctx;
    if (!binding || !binding->event || !event) {
        return 0;
    }
    Window window(binding->gui, handle);
    return binding->event(window, *event, binding->user) ? 1u : 0u;
}

Gui::NodeBinding *Gui::bindingFor(tgfx_handle_t handle)
{
    for (size_t i = 0; i < bindings_.size(); ++i) {
        if (bindings_[i]->handle == handle) {
            return bindings_[i];
        }
    }
    return 0;
}

void Gui::removeBinding(tgfx_handle_t handle)
{
    for (std::vector<NodeBinding *>::iterator it = bindings_.begin();
         it != bindings_.end(); ++it) {
        if ((*it)->handle == handle) {
            delete *it;
            bindings_.erase(it);
            return;
        }
    }
}

void Gui::removeBindingsRecursive(tgfx_handle_t handle)
{
    tgfx_handle_t child;
    if (!ctx_ || handle == TGFX_INVALID_HANDLE) {
        return;
    }
    child = tgfx_node_first_child(ctx_, handle);
    while (child != TGFX_INVALID_HANDLE) {
        tgfx_handle_t next = tgfx_node_next_sibling(ctx_, child);
        removeBindingsRecursive(child);
        child = next;
    }
    removeBinding(handle);
}

void Gui::removeStaleBindings()
{
    if (!ctx_) {
        return;
    }
    for (std::vector<NodeBinding *>::iterator it = bindings_.begin();
         it != bindings_.end(); ) {
        if ((*it)->handle == TGFX_INVALID_HANDLE ||
            !tgfx_node_exists(ctx_, (*it)->handle)) {
            delete *it;
            it = bindings_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace thingfx
