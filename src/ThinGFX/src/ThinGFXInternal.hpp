#ifndef THINGFX_INTERNAL_HPP
#define THINGFX_INTERNAL_HPP

#include "thingfx/ThinGFX.hpp"

namespace thingfx {
namespace detail {

using tgfx_handle_t = Handle;
constexpr tgfx_handle_t TGFX_INVALID_HANDLE = InvalidHandle;
using tgfx_color_t = Color;
using tgfx_result_t = Result;
constexpr tgfx_result_t TGFX_OK = Result::Ok;
constexpr tgfx_result_t TGFX_ERR_INVALID_ARG = Result::InvalidArgument;
constexpr tgfx_result_t TGFX_ERR_NO_MEMORY = Result::NoMemory;
constexpr tgfx_result_t TGFX_ERR_NOT_FOUND = Result::NotFound;
constexpr tgfx_result_t TGFX_ERR_UNSUPPORTED = Result::Unsupported;

using tgfx_pixel_format_t = PixelFormat;
constexpr tgfx_pixel_format_t TGFX_PIXEL_MONO1_LSB_VERTICAL = PixelFormat::Mono1;
constexpr tgfx_pixel_format_t TGFX_PIXEL_MONO1 = PixelFormat::Mono1;
constexpr tgfx_pixel_format_t TGFX_PIXEL_GRAY4 = PixelFormat::Gray4;
constexpr tgfx_pixel_format_t TGFX_PIXEL_GRAY8 = PixelFormat::Gray8;

using tgfx_node_kind_t = NodeKind;
constexpr tgfx_node_kind_t TGFX_NODE_LAYER = NodeKind::Layer;
constexpr tgfx_node_kind_t TGFX_NODE_WINDOW = NodeKind::Window;
constexpr tgfx_node_kind_t TGFX_NODE_FLOATING = NodeKind::Floating;
constexpr tgfx_node_kind_t TGFX_NODE_WIDGET = NodeKind::Widget;

using tgfx_window_border_t = WindowBorder;
constexpr tgfx_window_border_t TGFX_WINDOW_BORDER_NONE = WindowBorder::None;
constexpr tgfx_window_border_t TGFX_WINDOW_BORDER_RECT = WindowBorder::Rect;

using tgfx_anim_property_t = AnimationProperty;
constexpr tgfx_anim_property_t TGFX_ANIM_X = AnimationProperty::X;
constexpr tgfx_anim_property_t TGFX_ANIM_Y = AnimationProperty::Y;
constexpr tgfx_anim_property_t TGFX_ANIM_OPACITY = AnimationProperty::Opacity;

using tgfx_anim_curve_t = AnimationCurve;
constexpr tgfx_anim_curve_t TGFX_ANIM_LINEAR = AnimationCurve::Linear;
constexpr tgfx_anim_curve_t TGFX_ANIM_EASE_OUT = AnimationCurve::EaseOut;

using tgfx_easing_t = Easing;
constexpr tgfx_easing_t TGFX_EASING_NONE = Easing::None;
constexpr tgfx_easing_t TGFX_EASING_LINEAR = Easing::Linear;
constexpr tgfx_easing_t TGFX_EASING_EASE_IN = Easing::EaseIn;
constexpr tgfx_easing_t TGFX_EASING_EASE_OUT = Easing::EaseOut;
constexpr tgfx_easing_t TGFX_EASING_EASE_IN_OUT = Easing::EaseInOut;
constexpr tgfx_easing_t TGFX_EASING_SMOOTH_STEP = Easing::SmoothStep;

using tgfx_event_type_t = EventType;
constexpr tgfx_event_type_t TGFX_EVENT_NONE = EventType::None;
constexpr tgfx_event_type_t TGFX_EVENT_CANCEL = EventType::Cancel;
constexpr tgfx_event_type_t TGFX_EVENT_KEY_DOWN = EventType::KeyDown;
constexpr tgfx_event_type_t TGFX_EVENT_KEY_UP = EventType::KeyUp;

constexpr uint32_t TGFX_KEY_UNKNOWN = static_cast<uint32_t>(Key::Unknown);
constexpr uint32_t TGFX_KEY_UP = static_cast<uint32_t>(Key::Up);
constexpr uint32_t TGFX_KEY_DOWN = static_cast<uint32_t>(Key::Down);
constexpr uint32_t TGFX_KEY_LEFT = static_cast<uint32_t>(Key::Left);
constexpr uint32_t TGFX_KEY_RIGHT = static_cast<uint32_t>(Key::Right);
constexpr uint32_t TGFX_KEY_ENTER = static_cast<uint32_t>(Key::Enter);
constexpr uint32_t TGFX_KEY_BACK = static_cast<uint32_t>(Key::Back);
constexpr uint32_t TGFX_KEY_ESCAPE = static_cast<uint32_t>(Key::Escape);
constexpr uint32_t TGFX_KEY_MENU = static_cast<uint32_t>(Key::Menu);
constexpr uint32_t TGFX_KEY_HOME = static_cast<uint32_t>(Key::Home);
constexpr uint32_t TGFX_KEY_END = static_cast<uint32_t>(Key::End);
constexpr uint32_t TGFX_KEY_PAGE_UP = static_cast<uint32_t>(Key::PageUp);
constexpr uint32_t TGFX_KEY_PAGE_DOWN = static_cast<uint32_t>(Key::PageDown);

constexpr uint32_t TGFX_MOD_NONE = ModNone;
constexpr uint32_t TGFX_MOD_SHIFT = ModShift;
constexpr uint32_t TGFX_MOD_CTRL = ModCtrl;
constexpr uint32_t TGFX_MOD_ALT = ModAlt;
constexpr uint32_t TGFX_MOD_META = ModMeta;

using tgfx_rect_t = Rect;
using tgfx_canvas_t = CanvasBuffer;
using tgfx_glyph_t = Glyph;
using tgfx_font_t = Font;
using tgfx_event_t = Event;
using tgfx_port_t = Port;

struct ContextState;
using tgfx_context_t = ContextState;

typedef void (*tgfx_draw_fn)(tgfx_context_t *ctx,
                             tgfx_handle_t handle,
                             tgfx_canvas_t *canvas,
                             const tgfx_rect_t *dirty,
                             void *user);
typedef uint8_t (*tgfx_event_fn)(tgfx_context_t *ctx,
                                 tgfx_handle_t handle,
                                 const tgfx_event_t *event,
                                 void *user);

struct tgfx_transition {
    tgfx_easing_t easing;
    uint32_t duration_ms;
    uint32_t speed_per_mille;
};
using tgfx_transition_t = tgfx_transition;

struct tgfx_node_desc {
    tgfx_node_kind_t kind;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    uint8_t opacity;
    uint8_t visible;
    uint8_t owns_canvas;
    tgfx_pixel_format_t canvas_format;
    uint8_t show_title;
    tgfx_window_border_t border;
    const char *title;
    tgfx_transition_t transition;
    tgfx_transition_t create_transition;
    tgfx_transition_t destroy_transition;
    tgfx_draw_fn draw;
    tgfx_event_fn event;
    void *user;
};
using tgfx_node_desc_t = tgfx_node_desc;

struct tgfx_anim_desc {
    tgfx_handle_t handle;
    tgfx_anim_property_t property;
    int32_t from;
    int32_t to;
    uint32_t duration_ms;
    tgfx_anim_curve_t curve;
};
using tgfx_anim_desc_t = tgfx_anim_desc;

size_t tgfx_canvas_required_bytes(uint16_t width, uint16_t height,
                                  tgfx_pixel_format_t format);
tgfx_canvas_t *tgfx_canvas_create(uint16_t width, uint16_t height,
                                  tgfx_pixel_format_t format);
void tgfx_canvas_destroy(tgfx_canvas_t *canvas);
tgfx_result_t tgfx_canvas_wrap(tgfx_canvas_t *canvas, uint16_t width,
                               uint16_t height, uint16_t stride,
                               tgfx_pixel_format_t format, uint8_t *pixels);
void tgfx_canvas_reset_clip(tgfx_canvas_t *canvas);
tgfx_result_t tgfx_canvas_set_clip(tgfx_canvas_t *canvas, tgfx_rect_t rect);
tgfx_result_t tgfx_canvas_intersect_clip(tgfx_canvas_t *canvas, tgfx_rect_t rect);
tgfx_result_t tgfx_canvas_get_clip(const tgfx_canvas_t *canvas, tgfx_rect_t *rect);
tgfx_result_t tgfx_canvas_copy_rect(tgfx_canvas_t *dst, const tgfx_canvas_t *src,
                                    tgfx_rect_t src_rect, int16_t dst_x,
                                    int16_t dst_y);
tgfx_result_t tgfx_canvas_copy(tgfx_canvas_t *dst, const tgfx_canvas_t *src,
                               int16_t dst_x, int16_t dst_y);
tgfx_color_t tgfx_gray(uint8_t value);

tgfx_context_t *tgfx_create(uint16_t width, uint16_t height);
void tgfx_destroy(tgfx_context_t *ctx);
uint16_t tgfx_width(const tgfx_context_t *ctx);
uint16_t tgfx_height(const tgfx_context_t *ctx);
tgfx_result_t tgfx_resize(tgfx_context_t *ctx, uint16_t width, uint16_t height);
tgfx_result_t tgfx_context_attach_port(tgfx_context_t *ctx, tgfx_port_t *port);
tgfx_port_t *tgfx_context_port(tgfx_context_t *ctx);
void tgfx_context_lock(tgfx_context_t *ctx);
void tgfx_context_unlock(tgfx_context_t *ctx);
tgfx_result_t tgfx_daemon_start(tgfx_context_t *ctx);
void tgfx_daemon_stop(tgfx_context_t *ctx);
void tgfx_daemon_run(tgfx_context_t *ctx);
tgfx_result_t tgfx_daemon_step(tgfx_context_t *ctx);
void tgfx_daemon_notify(tgfx_context_t *ctx);

tgfx_result_t tgfx_node_create(tgfx_context_t *ctx, tgfx_handle_t parent,
                               const tgfx_node_desc_t *desc,
                               tgfx_handle_t *out_handle);
tgfx_result_t tgfx_node_destroy(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_result_t tgfx_node_destroy_immediate(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_result_t tgfx_node_destroy_animated(tgfx_context_t *ctx, tgfx_handle_t handle,
                                         tgfx_transition_t transition);
uint8_t tgfx_node_exists(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_canvas_t *tgfx_node_canvas(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_canvas_t *tgfx_node_back_canvas(tgfx_context_t *ctx, tgfx_handle_t handle);
const tgfx_canvas_t *tgfx_node_front_canvas(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_result_t tgfx_node_commit(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_result_t tgfx_node_commit_rect(tgfx_context_t *ctx, tgfx_handle_t handle,
                                    tgfx_rect_t rect);
tgfx_result_t tgfx_node_discard(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_result_t tgfx_node_set_rect(tgfx_context_t *ctx, tgfx_handle_t handle,
                                 tgfx_rect_t rect);
tgfx_result_t tgfx_node_set_rect_immediate(tgfx_context_t *ctx,
                                           tgfx_handle_t handle,
                                           tgfx_rect_t rect);
tgfx_result_t tgfx_node_set_rect_animated(tgfx_context_t *ctx,
                                          tgfx_handle_t handle,
                                          tgfx_rect_t rect,
                                          tgfx_transition_t transition);
tgfx_result_t tgfx_node_set_transition(tgfx_context_t *ctx,
                                       tgfx_handle_t handle,
                                       tgfx_transition_t transition);
tgfx_result_t tgfx_node_get_transition(tgfx_context_t *ctx,
                                       tgfx_handle_t handle,
                                       tgfx_transition_t *transition);
tgfx_result_t tgfx_node_set_destroy_transition(tgfx_context_t *ctx,
                                               tgfx_handle_t handle,
                                               tgfx_transition_t transition);
tgfx_result_t tgfx_node_get_destroy_transition(tgfx_context_t *ctx,
                                               tgfx_handle_t handle,
                                               tgfx_transition_t *transition);
tgfx_result_t tgfx_node_get_rect(tgfx_context_t *ctx, tgfx_handle_t handle,
                                 tgfx_rect_t *rect);
tgfx_result_t tgfx_node_set_visible(tgfx_context_t *ctx, tgfx_handle_t handle,
                                    uint8_t visible);
tgfx_result_t tgfx_node_set_opacity(tgfx_context_t *ctx, tgfx_handle_t handle,
                                    uint8_t opacity);
tgfx_result_t tgfx_node_bring_to_front(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_result_t tgfx_node_send_to_back(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_result_t tgfx_node_set_parent(tgfx_context_t *ctx, tgfx_handle_t handle,
                                   tgfx_handle_t parent);
tgfx_handle_t tgfx_root_handle(tgfx_context_t *ctx);
tgfx_handle_t tgfx_node_parent(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_handle_t tgfx_node_first_child(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_handle_t tgfx_node_next_sibling(tgfx_context_t *ctx, tgfx_handle_t handle);
void *tgfx_node_user(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_result_t tgfx_invalidate(tgfx_context_t *ctx, tgfx_handle_t handle,
                              tgfx_rect_t rect);
uint8_t tgfx_needs_render(const tgfx_context_t *ctx);
tgfx_result_t tgfx_render(tgfx_context_t *ctx, tgfx_canvas_t *target);
tgfx_result_t tgfx_focus_set(tgfx_context_t *ctx, tgfx_handle_t handle);
tgfx_handle_t tgfx_focus_get(tgfx_context_t *ctx);
tgfx_handle_t tgfx_focus_next(tgfx_context_t *ctx);
tgfx_handle_t tgfx_focus_prev(tgfx_context_t *ctx);
uint8_t tgfx_dispatch_to(tgfx_context_t *ctx, tgfx_handle_t handle,
                         const tgfx_event_t *event);
uint8_t tgfx_dispatch_event(tgfx_context_t *ctx, const tgfx_event_t *event,
                            tgfx_handle_t *out_target);
uint8_t tgfx_dispatch_key(tgfx_context_t *ctx, uint32_t key, uint8_t pressed,
                          uint8_t repeat, uint32_t modifiers,
                          tgfx_handle_t *out_target);
tgfx_result_t tgfx_anim_add(tgfx_context_t *ctx, const tgfx_anim_desc_t *desc);
void tgfx_tick(tgfx_context_t *ctx, uint32_t delta_ms);
uint8_t tgfx_animating(const tgfx_context_t *ctx);

void tgfx_draw_pixel(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                     tgfx_color_t color);
tgfx_color_t tgfx_get_pixel(const tgfx_canvas_t *canvas, int16_t x, int16_t y);
void tgfx_clear(tgfx_canvas_t *canvas, tgfx_color_t color);
void tgfx_draw_line(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1, tgfx_color_t color);
void tgfx_draw_rect(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                    int16_t w, int16_t h, tgfx_color_t color);
void tgfx_fill_rect(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                    int16_t w, int16_t h, tgfx_color_t color);
void tgfx_draw_circle(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                      int16_t r, tgfx_color_t color);
void tgfx_fill_circle(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                      int16_t r, tgfx_color_t color);
void tgfx_draw_triangle(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        tgfx_color_t color);
void tgfx_fill_triangle(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        tgfx_color_t color);
void tgfx_draw_round_rect(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                          int16_t w, int16_t h, int16_t radius,
                          tgfx_color_t color);
void tgfx_fill_round_rect(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                          int16_t w, int16_t h, int16_t radius,
                          tgfx_color_t color);
void tgfx_draw_bitmap_1bpp(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                           const uint8_t *bitmap, int16_t w, int16_t h,
                           tgfx_color_t color, tgfx_color_t bg);
void tgfx_draw_char_5x7(tgfx_canvas_t *canvas, int16_t x, int16_t y, char ch,
                        tgfx_color_t color, tgfx_color_t bg,
                        uint8_t size_x, uint8_t size_y);
void tgfx_draw_text_5x7(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                        const char *text, tgfx_color_t color,
                        tgfx_color_t bg, uint8_t size_x, uint8_t size_y);
void tgfx_draw_char_font(tgfx_canvas_t *canvas, const tgfx_font_t *font,
                         int16_t x, int16_t y, uint16_t ch,
                         tgfx_color_t color, tgfx_color_t bg,
                         uint8_t size_x, uint8_t size_y);
void tgfx_text_bounds_5x7(const char *text, int16_t x, int16_t y,
                          uint8_t size_x, uint8_t size_y, int16_t *x1,
                          int16_t *y1, uint16_t *w, uint16_t *h);
void tgfx_text_bounds_font(const tgfx_font_t *font, const char *text,
                           int16_t x, int16_t y, uint8_t size_x,
                           uint8_t size_y, int16_t *x1, int16_t *y1,
                           uint16_t *w, uint16_t *h);

} // namespace detail

} // namespace thingfx

#endif
