#ifndef FAMIGFX_INTERNAL_HPP
#define FAMIGFX_INTERNAL_HPP

#include "famigfx/FamiGFX.hpp"

namespace famigfx {
namespace detail {

using fgfx_handle_t = Handle;
constexpr fgfx_handle_t FGFX_INVALID_HANDLE = InvalidHandle;
using fgfx_color_t = Color;
using fgfx_result_t = Result;
constexpr fgfx_result_t FGFX_OK = Result::Ok;
constexpr fgfx_result_t FGFX_ERR_INVALID_ARG = Result::InvalidArgument;
constexpr fgfx_result_t FGFX_ERR_NO_MEMORY = Result::NoMemory;
constexpr fgfx_result_t FGFX_ERR_NOT_FOUND = Result::NotFound;
constexpr fgfx_result_t FGFX_ERR_UNSUPPORTED = Result::Unsupported;

using fgfx_pixel_format_t = PixelFormat;
constexpr fgfx_pixel_format_t FGFX_PIXEL_MONO1_LSB_VERTICAL = PixelFormat::Mono1;
constexpr fgfx_pixel_format_t FGFX_PIXEL_MONO1 = PixelFormat::Mono1;
constexpr fgfx_pixel_format_t FGFX_PIXEL_GRAY4 = PixelFormat::Gray4;
constexpr fgfx_pixel_format_t FGFX_PIXEL_GRAY8 = PixelFormat::Gray8;

using fgfx_node_kind_t = NodeKind;
constexpr fgfx_node_kind_t FGFX_NODE_LAYER = NodeKind::Layer;
constexpr fgfx_node_kind_t FGFX_NODE_WINDOW = NodeKind::Window;
constexpr fgfx_node_kind_t FGFX_NODE_FLOATING = NodeKind::Floating;
constexpr fgfx_node_kind_t FGFX_NODE_WIDGET = NodeKind::Widget;

using fgfx_window_border_t = WindowBorder;
constexpr fgfx_window_border_t FGFX_WINDOW_BORDER_NONE = WindowBorder::None;
constexpr fgfx_window_border_t FGFX_WINDOW_BORDER_RECT = WindowBorder::Rect;

using fgfx_anim_property_t = AnimationProperty;
constexpr fgfx_anim_property_t FGFX_ANIM_X = AnimationProperty::X;
constexpr fgfx_anim_property_t FGFX_ANIM_Y = AnimationProperty::Y;
constexpr fgfx_anim_property_t FGFX_ANIM_OPACITY = AnimationProperty::Opacity;

using fgfx_anim_curve_t = AnimationCurve;
constexpr fgfx_anim_curve_t FGFX_ANIM_LINEAR = AnimationCurve::Linear;
constexpr fgfx_anim_curve_t FGFX_ANIM_EASE_OUT = AnimationCurve::EaseOut;

using fgfx_event_type_t = EventType;
constexpr fgfx_event_type_t FGFX_EVENT_NONE = EventType::None;
constexpr fgfx_event_type_t FGFX_EVENT_CANCEL = EventType::Cancel;
constexpr fgfx_event_type_t FGFX_EVENT_KEY_DOWN = EventType::KeyDown;
constexpr fgfx_event_type_t FGFX_EVENT_KEY_UP = EventType::KeyUp;

constexpr uint32_t FGFX_KEY_UNKNOWN = static_cast<uint32_t>(Key::Unknown);
constexpr uint32_t FGFX_KEY_UP = static_cast<uint32_t>(Key::Up);
constexpr uint32_t FGFX_KEY_DOWN = static_cast<uint32_t>(Key::Down);
constexpr uint32_t FGFX_KEY_LEFT = static_cast<uint32_t>(Key::Left);
constexpr uint32_t FGFX_KEY_RIGHT = static_cast<uint32_t>(Key::Right);
constexpr uint32_t FGFX_KEY_ENTER = static_cast<uint32_t>(Key::Enter);
constexpr uint32_t FGFX_KEY_BACK = static_cast<uint32_t>(Key::Back);
constexpr uint32_t FGFX_KEY_ESCAPE = static_cast<uint32_t>(Key::Escape);
constexpr uint32_t FGFX_KEY_MENU = static_cast<uint32_t>(Key::Menu);
constexpr uint32_t FGFX_KEY_HOME = static_cast<uint32_t>(Key::Home);
constexpr uint32_t FGFX_KEY_END = static_cast<uint32_t>(Key::End);
constexpr uint32_t FGFX_KEY_PAGE_UP = static_cast<uint32_t>(Key::PageUp);
constexpr uint32_t FGFX_KEY_PAGE_DOWN = static_cast<uint32_t>(Key::PageDown);

constexpr uint32_t FGFX_MOD_NONE = ModNone;
constexpr uint32_t FGFX_MOD_SHIFT = ModShift;
constexpr uint32_t FGFX_MOD_CTRL = ModCtrl;
constexpr uint32_t FGFX_MOD_ALT = ModAlt;
constexpr uint32_t FGFX_MOD_META = ModMeta;

using fgfx_rect_t = Rect;
using fgfx_canvas_t = CanvasBuffer;
using fgfx_glyph_t = Glyph;
using fgfx_font_t = Font;
using fgfx_event_t = Event;
using fgfx_port_t = Port;

struct ContextState;
using fgfx_context_t = ContextState;

typedef void (*fgfx_draw_fn)(fgfx_context_t *ctx,
                             fgfx_handle_t handle,
                             fgfx_canvas_t *canvas,
                             const fgfx_rect_t *dirty,
                             void *user);
typedef uint8_t (*fgfx_event_fn)(fgfx_context_t *ctx,
                                 fgfx_handle_t handle,
                                 const fgfx_event_t *event,
                                 void *user);

struct fgfx_node_desc {
    fgfx_node_kind_t kind;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint8_t opacity;
    uint8_t visible;
    uint8_t owns_canvas;
    fgfx_pixel_format_t canvas_format;
    uint8_t show_title;
    fgfx_window_border_t border;
    const char *title;
    fgfx_draw_fn draw;
    fgfx_event_fn event;
    void *user;
};
using fgfx_node_desc_t = fgfx_node_desc;

struct fgfx_anim_desc {
    fgfx_handle_t handle;
    fgfx_anim_property_t property;
    int32_t from;
    int32_t to;
    uint32_t duration_ms;
    fgfx_anim_curve_t curve;
};
using fgfx_anim_desc_t = fgfx_anim_desc;

size_t fgfx_canvas_required_bytes(uint16_t width, uint16_t height,
                                  fgfx_pixel_format_t format);
fgfx_canvas_t *fgfx_canvas_create(uint16_t width, uint16_t height,
                                  fgfx_pixel_format_t format);
void fgfx_canvas_destroy(fgfx_canvas_t *canvas);
fgfx_result_t fgfx_canvas_wrap(fgfx_canvas_t *canvas, uint16_t width,
                               uint16_t height, uint16_t stride,
                               fgfx_pixel_format_t format, uint8_t *pixels);
void fgfx_canvas_reset_clip(fgfx_canvas_t *canvas);
fgfx_result_t fgfx_canvas_set_clip(fgfx_canvas_t *canvas, fgfx_rect_t rect);
fgfx_result_t fgfx_canvas_intersect_clip(fgfx_canvas_t *canvas, fgfx_rect_t rect);
fgfx_result_t fgfx_canvas_get_clip(const fgfx_canvas_t *canvas, fgfx_rect_t *rect);
fgfx_result_t fgfx_canvas_copy_rect(fgfx_canvas_t *dst, const fgfx_canvas_t *src,
                                    fgfx_rect_t src_rect, int16_t dst_x,
                                    int16_t dst_y);
fgfx_result_t fgfx_canvas_copy(fgfx_canvas_t *dst, const fgfx_canvas_t *src,
                               int16_t dst_x, int16_t dst_y);
fgfx_color_t fgfx_gray(uint8_t value);

fgfx_context_t *fgfx_create(uint16_t width, uint16_t height);
void fgfx_destroy(fgfx_context_t *ctx);
uint16_t fgfx_width(const fgfx_context_t *ctx);
uint16_t fgfx_height(const fgfx_context_t *ctx);
fgfx_result_t fgfx_resize(fgfx_context_t *ctx, uint16_t width, uint16_t height);
fgfx_result_t fgfx_context_attach_port(fgfx_context_t *ctx, fgfx_port_t *port);
fgfx_port_t *fgfx_context_port(fgfx_context_t *ctx);
void fgfx_context_lock(fgfx_context_t *ctx);
void fgfx_context_unlock(fgfx_context_t *ctx);
fgfx_result_t fgfx_daemon_start(fgfx_context_t *ctx);
void fgfx_daemon_stop(fgfx_context_t *ctx);
void fgfx_daemon_run(fgfx_context_t *ctx);
fgfx_result_t fgfx_daemon_step(fgfx_context_t *ctx);
void fgfx_daemon_notify(fgfx_context_t *ctx);

fgfx_result_t fgfx_node_create(fgfx_context_t *ctx, fgfx_handle_t parent,
                               const fgfx_node_desc_t *desc,
                               fgfx_handle_t *out_handle);
fgfx_result_t fgfx_node_destroy(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_canvas_t *fgfx_node_canvas(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_canvas_t *fgfx_node_back_canvas(fgfx_context_t *ctx, fgfx_handle_t handle);
const fgfx_canvas_t *fgfx_node_front_canvas(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_result_t fgfx_node_commit(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_result_t fgfx_node_commit_rect(fgfx_context_t *ctx, fgfx_handle_t handle,
                                    fgfx_rect_t rect);
fgfx_result_t fgfx_node_discard(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_result_t fgfx_node_set_rect(fgfx_context_t *ctx, fgfx_handle_t handle,
                                 fgfx_rect_t rect);
fgfx_result_t fgfx_node_get_rect(fgfx_context_t *ctx, fgfx_handle_t handle,
                                 fgfx_rect_t *rect);
fgfx_result_t fgfx_node_set_visible(fgfx_context_t *ctx, fgfx_handle_t handle,
                                    uint8_t visible);
fgfx_result_t fgfx_node_set_opacity(fgfx_context_t *ctx, fgfx_handle_t handle,
                                    uint8_t opacity);
fgfx_result_t fgfx_node_bring_to_front(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_result_t fgfx_node_send_to_back(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_result_t fgfx_node_set_parent(fgfx_context_t *ctx, fgfx_handle_t handle,
                                   fgfx_handle_t parent);
fgfx_handle_t fgfx_root_handle(fgfx_context_t *ctx);
fgfx_handle_t fgfx_node_parent(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_handle_t fgfx_node_first_child(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_handle_t fgfx_node_next_sibling(fgfx_context_t *ctx, fgfx_handle_t handle);
void *fgfx_node_user(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_result_t fgfx_invalidate(fgfx_context_t *ctx, fgfx_handle_t handle,
                              fgfx_rect_t rect);
uint8_t fgfx_needs_render(const fgfx_context_t *ctx);
fgfx_result_t fgfx_render(fgfx_context_t *ctx, fgfx_canvas_t *target);
fgfx_result_t fgfx_focus_set(fgfx_context_t *ctx, fgfx_handle_t handle);
fgfx_handle_t fgfx_focus_get(fgfx_context_t *ctx);
fgfx_handle_t fgfx_focus_next(fgfx_context_t *ctx);
fgfx_handle_t fgfx_focus_prev(fgfx_context_t *ctx);
uint8_t fgfx_dispatch_to(fgfx_context_t *ctx, fgfx_handle_t handle,
                         const fgfx_event_t *event);
uint8_t fgfx_dispatch_event(fgfx_context_t *ctx, const fgfx_event_t *event,
                            fgfx_handle_t *out_target);
uint8_t fgfx_dispatch_key(fgfx_context_t *ctx, uint32_t key, uint8_t pressed,
                          uint8_t repeat, uint32_t modifiers,
                          fgfx_handle_t *out_target);
fgfx_result_t fgfx_anim_add(fgfx_context_t *ctx, const fgfx_anim_desc_t *desc);
void fgfx_tick(fgfx_context_t *ctx, uint32_t delta_ms);
uint8_t fgfx_animating(const fgfx_context_t *ctx);

void fgfx_draw_pixel(fgfx_canvas_t *canvas, int16_t x, int16_t y,
                     fgfx_color_t color);
fgfx_color_t fgfx_get_pixel(const fgfx_canvas_t *canvas, int16_t x, int16_t y);
void fgfx_clear(fgfx_canvas_t *canvas, fgfx_color_t color);
void fgfx_draw_line(fgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1, fgfx_color_t color);
void fgfx_draw_rect(fgfx_canvas_t *canvas, int16_t x, int16_t y,
                    int16_t w, int16_t h, fgfx_color_t color);
void fgfx_fill_rect(fgfx_canvas_t *canvas, int16_t x, int16_t y,
                    int16_t w, int16_t h, fgfx_color_t color);
void fgfx_draw_circle(fgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                      int16_t r, fgfx_color_t color);
void fgfx_fill_circle(fgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                      int16_t r, fgfx_color_t color);
void fgfx_draw_triangle(fgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        fgfx_color_t color);
void fgfx_fill_triangle(fgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        fgfx_color_t color);
void fgfx_draw_round_rect(fgfx_canvas_t *canvas, int16_t x, int16_t y,
                          int16_t w, int16_t h, int16_t radius,
                          fgfx_color_t color);
void fgfx_fill_round_rect(fgfx_canvas_t *canvas, int16_t x, int16_t y,
                          int16_t w, int16_t h, int16_t radius,
                          fgfx_color_t color);
void fgfx_draw_bitmap_1bpp(fgfx_canvas_t *canvas, int16_t x, int16_t y,
                           const uint8_t *bitmap, int16_t w, int16_t h,
                           fgfx_color_t color, fgfx_color_t bg);
void fgfx_draw_char_5x7(fgfx_canvas_t *canvas, int16_t x, int16_t y, char ch,
                        fgfx_color_t color, fgfx_color_t bg,
                        uint8_t size_x, uint8_t size_y);
void fgfx_draw_text_5x7(fgfx_canvas_t *canvas, int16_t x, int16_t y,
                        const char *text, fgfx_color_t color,
                        fgfx_color_t bg, uint8_t size_x, uint8_t size_y);
void fgfx_draw_char_font(fgfx_canvas_t *canvas, const fgfx_font_t *font,
                         int16_t x, int16_t y, uint16_t ch,
                         fgfx_color_t color, fgfx_color_t bg,
                         uint8_t size_x, uint8_t size_y);
void fgfx_text_bounds_5x7(const char *text, int16_t x, int16_t y,
                          uint8_t size_x, uint8_t size_y, int16_t *x1,
                          int16_t *y1, uint16_t *w, uint16_t *h);
void fgfx_text_bounds_font(const fgfx_font_t *font, const char *text,
                           int16_t x, int16_t y, uint8_t size_x,
                           uint8_t size_y, int16_t *x1, int16_t *y1,
                           uint16_t *w, uint16_t *h);

} // namespace detail

} // namespace famigfx

#endif
