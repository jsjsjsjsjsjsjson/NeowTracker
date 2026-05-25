#include "thingfx/ThinGFX.hpp"
#include "ThinGFXInternal.hpp"

#include <stdlib.h>
#include <string.h>
#include <chrono>

namespace thingfx {
namespace detail {

typedef struct tgfx_node {
    tgfx_handle_t handle;
    tgfx_handle_t parent;
    tgfx_handle_t first_child;
    tgfx_handle_t next_sibling;
    tgfx_node_kind_t kind;
    tgfx_rect_t rect;
    tgfx_rect_t display_rect;
    tgfx_rect_t anim_start_rect;
    tgfx_rect_t anim_target_rect;
    uint32_t transition_duration_ms;
    uint32_t transition_speed_per_mille;
    uint32_t rect_anim_elapsed_ms;
    uint32_t rect_anim_duration_ms;
    uint32_t rect_anim_speed_per_mille;
    tgfx_easing_t transition_easing;
    tgfx_easing_t rect_anim_easing;
    uint8_t rect_anim_active;
    uint8_t rect_anim_destroy_on_finish;
    uint32_t destroy_duration_ms;
    uint32_t destroy_speed_per_mille;
    tgfx_easing_t destroy_easing;
    uint8_t destroy_pending;
    uint8_t opacity;
    uint8_t visible;
    uint8_t owns_canvas;
    tgfx_pixel_format_t canvas_format;
    uint8_t dirty;
    tgfx_rect_t dirty_rect;
    uint8_t show_title;
    tgfx_window_border_t border;
    const char *title;
    tgfx_canvas_t *front_canvas;
    tgfx_canvas_t *back_canvas;
    uint8_t committed_dirty;
    tgfx_rect_t committed_rect;
    tgfx_draw_fn draw;
    tgfx_event_fn event;
    void *user;
} tgfx_node_t;

typedef struct tgfx_anim {
    tgfx_handle_t handle;
    tgfx_anim_property_t property;
    int32_t from;
    int32_t to;
    uint32_t elapsed_ms;
    uint32_t duration_ms;
    tgfx_anim_curve_t curve;
    uint8_t active;
} tgfx_anim_t;

struct ContextState {
    uint16_t width;
    uint16_t height;
    tgfx_handle_t next_handle;
    tgfx_node_t *nodes;
    size_t node_count;
    size_t node_capacity;
    tgfx_anim_t *anims;
    size_t anim_count;
    size_t anim_capacity;
    tgfx_handle_t focus;
    uint8_t scene_dirty;
    tgfx_port_t *port;
    uint8_t daemon_running;
};

static void ctx_lock(tgfx_context_t *ctx)
{
    if (ctx && ctx->port) {
        ctx->port->lock();
    }
}

static void ctx_unlock(tgfx_context_t *ctx)
{
    if (ctx && ctx->port) {
        ctx->port->unlock();
    }
}

static void ctx_notify(tgfx_context_t *ctx)
{
    if (ctx && ctx->port) {
        ctx->port->notifyCommit();
    }
}

static tgfx_rect_t rect_make(int16_t x, int16_t y, int16_t w, int16_t h)
{
    tgfx_rect_t r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return r;
}

static uint8_t rect_empty(tgfx_rect_t r)
{
    return r.w <= 0 || r.h <= 0;
}

static tgfx_rect_t rect_intersection(tgfx_rect_t a, tgfx_rect_t b)
{
    int16_t x1 = a.x > b.x ? a.x : b.x;
    int16_t y1 = a.y > b.y ? a.y : b.y;
    int16_t ax2 = (int16_t)(a.x + a.w);
    int16_t ay2 = (int16_t)(a.y + a.h);
    int16_t bx2 = (int16_t)(b.x + b.w);
    int16_t by2 = (int16_t)(b.y + b.h);
    int16_t x2 = ax2 < bx2 ? ax2 : bx2;
    int16_t y2 = ay2 < by2 ? ay2 : by2;
    if (x2 <= x1 || y2 <= y1) {
        return rect_make(0, 0, 0, 0);
    }
    return rect_make(x1, y1, (int16_t)(x2 - x1), (int16_t)(y2 - y1));
}

static tgfx_rect_t rect_union(tgfx_rect_t a, tgfx_rect_t b)
{
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    if (rect_empty(a)) {
        return b;
    }
    if (rect_empty(b)) {
        return a;
    }
    x1 = a.x < b.x ? a.x : b.x;
    y1 = a.y < b.y ? a.y : b.y;
    x2 = (int16_t)((a.x + a.w) > (b.x + b.w) ? (a.x + a.w) : (b.x + b.w));
    y2 = (int16_t)((a.y + a.h) > (b.y + b.h) ? (a.y + a.h) : (b.y + b.h));
    return rect_make(x1, y1, (int16_t)(x2 - x1), (int16_t)(y2 - y1));
}

static uint32_t monotonic_ms()
{
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count());
}

static tgfx_rect_t canvas_full_rect(const tgfx_canvas_t *canvas)
{
    if (!canvas) {
        return rect_make(0, 0, 0, 0);
    }
    return rect_make(0, 0, (int16_t)canvas->width, (int16_t)canvas->height);
}

static tgfx_rect_t canvas_clip_rect(const tgfx_canvas_t *canvas)
{
    if (!canvas) {
        return rect_make(0, 0, 0, 0);
    }
    if (canvas->clip_w <= 0 || canvas->clip_h <= 0) {
        return rect_make(0, 0, 0, 0);
    }
    return rect_intersection(rect_make(canvas->clip_x, canvas->clip_y,
                                      canvas->clip_w, canvas->clip_h),
                             canvas_full_rect(canvas));
}

tgfx_color_t tgfx_gray(uint8_t value)
{
    return value;
}

size_t tgfx_canvas_required_bytes(uint16_t width, uint16_t height,
                                  tgfx_pixel_format_t format)
{
    switch (format) {
    case TGFX_PIXEL_MONO1_LSB_VERTICAL:
        return (size_t)width * ((height + 7u) / 8u);
    case TGFX_PIXEL_GRAY4:
        return (size_t)((width + 1u) / 2u) * height;
    case TGFX_PIXEL_GRAY8:
        return (size_t)width * height;
    default:
        return 0;
    }
}

tgfx_canvas_t *tgfx_canvas_create(uint16_t width, uint16_t height,
                                  tgfx_pixel_format_t format)
{
    tgfx_canvas_t *canvas = (tgfx_canvas_t *)calloc(1, sizeof(*canvas));
    size_t bytes = tgfx_canvas_required_bytes(width, height, format);

    if (!canvas || bytes == 0) {
        free(canvas);
        return NULL;
    }

    canvas->pixels = (uint8_t *)calloc(1, bytes);
    if (!canvas->pixels) {
        free(canvas);
        return NULL;
    }

    canvas->width = width;
    canvas->height = height;
    canvas->format = format;
    if (format == TGFX_PIXEL_MONO1_LSB_VERTICAL) {
        canvas->stride = width;
    } else if (format == TGFX_PIXEL_GRAY4) {
        canvas->stride = (uint16_t)((width + 1u) / 2u);
    } else {
        canvas->stride = width;
    }
    tgfx_canvas_reset_clip(canvas);
    return canvas;
}

void tgfx_canvas_destroy(tgfx_canvas_t *canvas)
{
    if (canvas) {
        free(canvas->pixels);
        free(canvas);
    }
}

tgfx_result_t tgfx_canvas_wrap(tgfx_canvas_t *canvas, uint16_t width,
                               uint16_t height, uint16_t stride,
                               tgfx_pixel_format_t format, uint8_t *pixels)
{
    size_t min_stride = 0;
    if (format == TGFX_PIXEL_MONO1_LSB_VERTICAL) {
        min_stride = width;
    } else if (format == TGFX_PIXEL_GRAY4) {
        min_stride = (size_t)((width + 1u) / 2u);
    } else if (format == TGFX_PIXEL_GRAY8) {
        min_stride = width;
    } else {
        return TGFX_ERR_INVALID_ARG;
    }
    if (!canvas || !pixels || width == 0 || height == 0 ||
        stride < min_stride) {
        return TGFX_ERR_INVALID_ARG;
    }
    canvas->width = width;
    canvas->height = height;
    canvas->stride = stride;
    canvas->format = format;
    canvas->pixels = pixels;
    tgfx_canvas_reset_clip(canvas);
    return TGFX_OK;
}

void tgfx_canvas_reset_clip(tgfx_canvas_t *canvas)
{
    if (!canvas) {
        return;
    }
    canvas->clip_x = 0;
    canvas->clip_y = 0;
    canvas->clip_w = (int16_t)canvas->width;
    canvas->clip_h = (int16_t)canvas->height;
}

tgfx_result_t tgfx_canvas_set_clip(tgfx_canvas_t *canvas, tgfx_rect_t rect)
{
    tgfx_rect_t clipped;
    if (!canvas) {
        return TGFX_ERR_INVALID_ARG;
    }
    if (rect.w < 0 || rect.h < 0) {
        return TGFX_ERR_INVALID_ARG;
    }
    clipped = rect_intersection(rect, canvas_full_rect(canvas));
    canvas->clip_x = clipped.x;
    canvas->clip_y = clipped.y;
    canvas->clip_w = clipped.w;
    canvas->clip_h = clipped.h;
    return TGFX_OK;
}

tgfx_result_t tgfx_canvas_intersect_clip(tgfx_canvas_t *canvas, tgfx_rect_t rect)
{
    if (!canvas || rect.w < 0 || rect.h < 0) {
        return TGFX_ERR_INVALID_ARG;
    }
    return tgfx_canvas_set_clip(canvas, rect_intersection(canvas_clip_rect(canvas), rect));
}

tgfx_result_t tgfx_canvas_get_clip(const tgfx_canvas_t *canvas, tgfx_rect_t *rect)
{
    if (!canvas || !rect) {
        return TGFX_ERR_INVALID_ARG;
    }
    *rect = canvas_clip_rect(canvas);
    return TGFX_OK;
}

tgfx_result_t tgfx_canvas_copy_rect(tgfx_canvas_t *dst, const tgfx_canvas_t *src,
                                    tgfx_rect_t src_rect, int16_t dst_x,
                                    int16_t dst_y)
{
    tgfx_rect_t src_bounds;
    tgfx_rect_t src_clipped;
    int16_t y;

    if (!dst || !src || !dst->pixels || !src->pixels ||
        dst->format != src->format || src_rect.w < 0 || src_rect.h < 0) {
        return TGFX_ERR_INVALID_ARG;
    }

    src_bounds = canvas_full_rect(src);
    src_clipped = rect_intersection(src_rect, src_bounds);
    if (rect_empty(src_clipped)) {
        return TGFX_OK;
    }

    if (src_clipped.x != src_rect.x) {
        dst_x = (int16_t)(dst_x + (src_clipped.x - src_rect.x));
    }
    if (src_clipped.y != src_rect.y) {
        dst_y = (int16_t)(dst_y + (src_clipped.y - src_rect.y));
    }

    for (y = 0; y < src_clipped.h; ++y) {
        int16_t x;
        int16_t sy = (int16_t)(src_clipped.y + y);
        int16_t dy = (int16_t)(dst_y + y);
        for (x = 0; x < src_clipped.w; ++x) {
            int16_t sx = (int16_t)(src_clipped.x + x);
            int16_t dx = (int16_t)(dst_x + x);
            tgfx_draw_pixel(dst, dx, dy, tgfx_get_pixel(src, sx, sy));
        }
    }
    return TGFX_OK;
}

tgfx_result_t tgfx_canvas_copy(tgfx_canvas_t *dst, const tgfx_canvas_t *src,
                               int16_t dst_x, int16_t dst_y)
{
    if (!src) {
        return TGFX_ERR_INVALID_ARG;
    }
    return tgfx_canvas_copy_rect(dst, src, canvas_full_rect(src), dst_x, dst_y);
}

static tgfx_node_t *find_node(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    size_t i;
    if (!ctx || handle == TGFX_INVALID_HANDLE) {
        return NULL;
    }
    for (i = 0; i < ctx->node_count; ++i) {
        if (ctx->nodes[i].handle == handle) {
            return &ctx->nodes[i];
        }
    }
    return NULL;
}

static tgfx_rect_t node_full_dirty_rect(const tgfx_node_t *node)
{
    if (!node) {
        return rect_make(0, 0, 0, 0);
    }
    return rect_make(0, 0, node->rect.w, node->rect.h);
}

static tgfx_rect_t normalize_node_dirty_rect(const tgfx_node_t *node,
                                             tgfx_rect_t rect)
{
    tgfx_rect_t full = node_full_dirty_rect(node);
    if (!node) {
        return full;
    }
    if (rect.w <= 0 || rect.h <= 0) {
        return full;
    }
    return rect_intersection(rect, full);
}

static void mark_node_dirty_rect(tgfx_context_t *ctx, tgfx_node_t *node,
                                 tgfx_rect_t rect)
{
    if (!ctx || !node) {
        return;
    }
    rect = normalize_node_dirty_rect(node, rect);
    if (rect_empty(rect)) {
        return;
    }
    node->dirty_rect = node->dirty ? rect_union(node->dirty_rect, rect) : rect;
    node->dirty = 1;
    ctx->scene_dirty = 1;
}

static void mark_dirty_upwards(tgfx_context_t *ctx, tgfx_handle_t handle,
                               tgfx_rect_t rect)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!ctx || !node) {
        return;
    }
    mark_node_dirty_rect(ctx, node, rect);
    while (node && node->parent != TGFX_INVALID_HANDLE) {
        tgfx_node_t *parent = find_node(ctx, node->parent);
        if (!parent) {
            break;
        }
        mark_node_dirty_rect(ctx, parent, node_full_dirty_rect(parent));
        node = parent;
    }
}

static void mark_scene_dirty(tgfx_context_t *ctx)
{
    if (!ctx || ctx->node_count == 0) {
        return;
    }
    mark_dirty_upwards(ctx, ctx->nodes[0].handle,
                       rect_make(0, 0, ctx->nodes[0].rect.w, ctx->nodes[0].rect.h));
}

static tgfx_result_t reserve_nodes(tgfx_context_t *ctx, size_t count)
{
    tgfx_node_t *nodes;
    size_t capacity;
    if (ctx->node_capacity >= count) {
        return TGFX_OK;
    }
    capacity = ctx->node_capacity ? ctx->node_capacity * 2u : 8u;
    while (capacity < count) {
        capacity *= 2u;
    }
    nodes = (tgfx_node_t *)realloc(ctx->nodes, capacity * sizeof(*nodes));
    if (!nodes) {
        return TGFX_ERR_NO_MEMORY;
    }
    ctx->nodes = nodes;
    ctx->node_capacity = capacity;
    return TGFX_OK;
}

tgfx_context_t *tgfx_create(uint16_t width, uint16_t height)
{
    tgfx_context_t *ctx = (tgfx_context_t *)calloc(1, sizeof(*ctx));
    tgfx_node_desc_t root;
    tgfx_handle_t ignored;

    if (width == 0 || height == 0 || width > INT16_MAX || height > INT16_MAX) {
        return NULL;
    }
    if (!ctx) {
        return NULL;
    }
    ctx->width = width;
    ctx->height = height;
    ctx->next_handle = 1;

    memset(&root, 0, sizeof(root));
    root.kind = TGFX_NODE_LAYER;
    root.w = (int16_t)width;
    root.h = (int16_t)height;
    root.opacity = 255;
    root.visible = 1;
    root.owns_canvas = 0;
    if (tgfx_node_create(ctx, TGFX_INVALID_HANDLE, &root, &ignored) != TGFX_OK) {
        tgfx_destroy(ctx);
        return NULL;
    }
    return ctx;
}

static void destroy_owned_canvases(tgfx_context_t *ctx)
{
    size_t i;
    for (i = 0; i < ctx->node_count; ++i) {
        if (ctx->nodes[i].owns_canvas) {
            tgfx_canvas_destroy(ctx->nodes[i].front_canvas);
            tgfx_canvas_destroy(ctx->nodes[i].back_canvas);
        }
    }
}

void tgfx_destroy(tgfx_context_t *ctx)
{
    if (ctx) {
        tgfx_daemon_stop(ctx);
        destroy_owned_canvases(ctx);
        free(ctx->nodes);
        free(ctx->anims);
        free(ctx);
    }
}

uint16_t tgfx_width(const tgfx_context_t *ctx)
{
    return ctx ? ctx->width : 0;
}

uint16_t tgfx_height(const tgfx_context_t *ctx)
{
    return ctx ? ctx->height : 0;
}

tgfx_result_t tgfx_resize(tgfx_context_t *ctx, uint16_t width, uint16_t height)
{
    if (!ctx || width == 0 || height == 0 || width > INT16_MAX ||
        height > INT16_MAX || ctx->node_count == 0) {
        return TGFX_ERR_INVALID_ARG;
    }
    ctx->width = width;
    ctx->height = height;
    ctx->nodes[0].rect.w = (int16_t)width;
    ctx->nodes[0].rect.h = (int16_t)height;
    ctx->nodes[0].display_rect = ctx->nodes[0].rect;
    mark_scene_dirty(ctx);
    return TGFX_OK;
}

static void append_child(tgfx_context_t *ctx, tgfx_handle_t parent,
                         tgfx_handle_t child)
{
    tgfx_node_t *p = find_node(ctx, parent);
    tgfx_node_t *tail;
    if (!p) {
        return;
    }
    if (p->first_child == TGFX_INVALID_HANDLE) {
        p->first_child = child;
        return;
    }
    tail = find_node(ctx, p->first_child);
    while (tail && tail->next_sibling != TGFX_INVALID_HANDLE) {
        tail = find_node(ctx, tail->next_sibling);
    }
    if (tail) {
        tail->next_sibling = child;
    }
}

static void prepend_child(tgfx_context_t *ctx, tgfx_handle_t parent,
                          tgfx_handle_t child)
{
    tgfx_node_t *p = find_node(ctx, parent);
    tgfx_node_t *c = find_node(ctx, child);
    if (!p || !c) {
        return;
    }
    c->next_sibling = p->first_child;
    p->first_child = child;
}


static uint32_t sanitize_speed(uint32_t speed_per_mille);
static tgfx_easing_t sanitize_easing(tgfx_easing_t easing);
static tgfx_transition_t sanitize_transition(tgfx_transition_t transition);

static uint8_t node_is_descendant(tgfx_context_t *ctx, tgfx_handle_t node_handle,
                                  tgfx_handle_t ancestor_handle)
{
    tgfx_node_t *node = find_node(ctx, node_handle);
    while (node && node->parent != TGFX_INVALID_HANDLE) {
        if (node->parent == ancestor_handle) {
            return 1;
        }
        node = find_node(ctx, node->parent);
    }
    return 0;
}

tgfx_result_t tgfx_node_create(tgfx_context_t *ctx, tgfx_handle_t parent,
                               const tgfx_node_desc_t *desc,
                               tgfx_handle_t *out_handle)
{
    tgfx_node_t *node;
    tgfx_result_t res;

    if (!ctx || !desc || !out_handle || desc->w < 0 || desc->h < 0) {
        return TGFX_ERR_INVALID_ARG;
    }
    if (ctx->node_count > 0 && parent == TGFX_INVALID_HANDLE) {
        parent = ctx->nodes[0].handle;
    }
    if (ctx->node_count > 0 && !find_node(ctx, parent)) {
        return TGFX_ERR_NOT_FOUND;
    }

    res = reserve_nodes(ctx, ctx->node_count + 1u);
    if (res != TGFX_OK) {
        return res;
    }

    node = &ctx->nodes[ctx->node_count++];
    memset(node, 0, sizeof(*node));
    node->handle = ctx->next_handle++;
    node->parent = parent;
    node->kind = desc->kind;
    node->rect.x = desc->x;
    node->rect.y = desc->y;
    node->rect.w = desc->w;
    node->rect.h = desc->h;
    node->display_rect = node->rect;
    node->anim_start_rect = node->rect;
    node->anim_target_rect = node->rect;
    node->transition_easing = sanitize_easing(desc->transition.easing);
    node->rect_anim_easing = TGFX_EASING_NONE;
    node->transition_duration_ms = desc->transition.duration_ms;
    node->transition_speed_per_mille = sanitize_speed(desc->transition.speed_per_mille);
    if (node->transition_easing == TGFX_EASING_NONE) {
        node->transition_duration_ms = 0;
    }
    node->destroy_easing = sanitize_easing(desc->destroy_transition.easing);
    node->destroy_duration_ms = desc->destroy_transition.duration_ms;
    node->destroy_speed_per_mille = sanitize_speed(desc->destroy_transition.speed_per_mille);
    if (node->destroy_easing == TGFX_EASING_NONE) {
        node->destroy_duration_ms = 0;
    }
    node->rect_anim_elapsed_ms = 0;
    node->rect_anim_duration_ms = 0;
    node->rect_anim_speed_per_mille = 1000;
    node->rect_anim_active = 0;
    node->rect_anim_destroy_on_finish = 0;
    node->destroy_pending = 0;
    node->opacity = desc->opacity;
    node->visible = desc->visible;
    node->owns_canvas = desc->owns_canvas;
    node->canvas_format = desc->canvas_format;
    if (tgfx_canvas_required_bytes(1, 1, node->canvas_format) == 0) {
        node->canvas_format = TGFX_PIXEL_GRAY8;
    }
    node->show_title = desc->show_title && desc->title && desc->title[0];
    node->border = desc->border;
    node->title = desc->title;
    node->draw = desc->draw;
    node->event = desc->event;
    node->user = desc->user;
    node->next_sibling = TGFX_INVALID_HANDLE;
    node->first_child = TGFX_INVALID_HANDLE;
    node->dirty = 0;
    node->dirty_rect = rect_make(0, 0, 0, 0);
    node->committed_dirty = 0;
    node->committed_rect = rect_make(0, 0, 0, 0);

    if (node->owns_canvas && node->rect.w > 0 && node->rect.h > 0) {
        node->front_canvas = tgfx_canvas_create((uint16_t)node->rect.w,
                                                (uint16_t)node->rect.h,
                                                node->canvas_format);
        node->back_canvas = tgfx_canvas_create((uint16_t)node->rect.w,
                                               (uint16_t)node->rect.h,
                                               node->canvas_format);
        if (!node->front_canvas || !node->back_canvas) {
            tgfx_canvas_destroy(node->front_canvas);
            tgfx_canvas_destroy(node->back_canvas);
            --ctx->node_count;
            return TGFX_ERR_NO_MEMORY;
        }
    }

    {
        tgfx_transition_t create_transition = sanitize_transition(desc->create_transition);
        if (create_transition.easing != TGFX_EASING_NONE &&
            create_transition.duration_ms > 0 &&
            node->rect.w > 0 && node->rect.h > 0) {
            tgfx_rect_t collapsed = rect_make((int16_t)(node->rect.x + node->rect.w / 2),
                                              (int16_t)(node->rect.y + node->rect.h / 2),
                                              0, 0);
            node->display_rect = collapsed;
            node->anim_start_rect = collapsed;
            node->anim_target_rect = node->rect;
            node->rect_anim_elapsed_ms = 0;
            node->rect_anim_duration_ms = create_transition.duration_ms;
            node->rect_anim_speed_per_mille = create_transition.speed_per_mille;
            node->rect_anim_easing = create_transition.easing;
            node->rect_anim_active = 1;
            node->rect_anim_destroy_on_finish = 0;
        }
    }

    if (parent != TGFX_INVALID_HANDLE) {
        append_child(ctx, parent, node->handle);
    }
    mark_dirty_upwards(ctx, node->handle, node_full_dirty_rect(node));
    *out_handle = node->handle;
    return TGFX_OK;
}

static void detach_from_parent(tgfx_context_t *ctx, tgfx_node_t *node)
{
    tgfx_node_t *parent = find_node(ctx, node->parent);
    tgfx_node_t *prev = NULL;
    tgfx_node_t *cur;
    if (!parent) {
        return;
    }
    cur = find_node(ctx, parent->first_child);
    while (cur) {
        if (cur->handle == node->handle) {
            if (prev) {
                prev->next_sibling = cur->next_sibling;
            } else {
                parent->first_child = cur->next_sibling;
            }
            cur->next_sibling = TGFX_INVALID_HANDLE;
            return;
        }
        prev = cur;
        cur = find_node(ctx, cur->next_sibling);
    }
}

static tgfx_result_t destroy_node_recursive(tgfx_context_t *ctx,
                                            tgfx_handle_t handle)
{
    size_t i;
    tgfx_node_t *node = find_node(ctx, handle);
    if (!ctx || !node) {
        return TGFX_ERR_INVALID_ARG;
    }

    while (node->first_child != TGFX_INVALID_HANDLE) {
        tgfx_result_t res = destroy_node_recursive(ctx, node->first_child);
        if (res != TGFX_OK) {
            return res;
        }
        node = find_node(ctx, handle);
        if (!node) {
            return TGFX_ERR_NOT_FOUND;
        }
    }

    detach_from_parent(ctx, node);
    if (ctx->focus == handle) {
        ctx->focus = TGFX_INVALID_HANDLE;
    }
    if (node->owns_canvas) {
        tgfx_canvas_destroy(node->front_canvas);
        tgfx_canvas_destroy(node->back_canvas);
    }
    i = (size_t)(node - ctx->nodes);
    memmove(&ctx->nodes[i], &ctx->nodes[i + 1u],
            (ctx->node_count - i - 1u) * sizeof(ctx->nodes[0]));
    --ctx->node_count;
    return TGFX_OK;
}

static tgfx_rect_t collapsed_center_rect(tgfx_rect_t rect)
{
    return rect_make((int16_t)(rect.x + rect.w / 2),
                     (int16_t)(rect.y + rect.h / 2),
                     0, 0);
}

static tgfx_result_t start_node_destroy_animation_locked(tgfx_context_t *ctx,
                                                         tgfx_node_t *node,
                                                         tgfx_transition_t transition)
{
    if (!ctx || !node || node->parent == TGFX_INVALID_HANDLE) {
        return TGFX_ERR_INVALID_ARG;
    }
    transition = sanitize_transition(transition);
    if (transition.easing == TGFX_EASING_NONE || transition.duration_ms == 0) {
        tgfx_handle_t parent = node->parent;
        tgfx_handle_t handle = node->handle;
        tgfx_result_t res = destroy_node_recursive(ctx, handle);
        if (res == TGFX_OK && parent != TGFX_INVALID_HANDLE) {
            tgfx_node_t *parent_node = find_node(ctx, parent);
            if (parent_node) {
                mark_dirty_upwards(ctx, parent, node_full_dirty_rect(parent_node));
            } else {
                ctx->scene_dirty = 1;
            }
        }
        return res;
    }

    node->destroy_pending = 1;
    node->anim_start_rect = node->display_rect;
    node->anim_target_rect = collapsed_center_rect(node->display_rect);
    node->rect_anim_elapsed_ms = 0;
    node->rect_anim_duration_ms = transition.duration_ms;
    node->rect_anim_speed_per_mille = transition.speed_per_mille;
    node->rect_anim_easing = transition.easing;
    node->rect_anim_active = 1;
    node->rect_anim_destroy_on_finish = 1;
    ctx->scene_dirty = 1;
    return TGFX_OK;
}

tgfx_result_t tgfx_node_destroy_immediate(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_handle_t parent;
    tgfx_result_t res;
    if (!ctx || ctx->node_count == 0 || handle == ctx->nodes[0].handle) {
        return TGFX_ERR_INVALID_ARG;
    }
    parent = tgfx_node_parent(ctx, handle);
    res = destroy_node_recursive(ctx, handle);
    if (res == TGFX_OK && parent != TGFX_INVALID_HANDLE) {
        tgfx_node_t *parent_node = find_node(ctx, parent);
        if (parent_node) {
            mark_dirty_upwards(ctx, parent, node_full_dirty_rect(parent_node));
        } else {
            ctx->scene_dirty = 1;
        }
    }
    return res;
}

tgfx_result_t tgfx_node_destroy_animated(tgfx_context_t *ctx, tgfx_handle_t handle,
                                         tgfx_transition_t transition)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!ctx || !node || ctx->node_count == 0 || handle == ctx->nodes[0].handle) {
        return TGFX_ERR_INVALID_ARG;
    }
    return start_node_destroy_animation_locked(ctx, node, transition);
}

tgfx_result_t tgfx_node_destroy(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    tgfx_transition_t transition;
    if (!ctx || !node || ctx->node_count == 0 || handle == ctx->nodes[0].handle) {
        return TGFX_ERR_INVALID_ARG;
    }
    transition.easing = node->destroy_easing;
    transition.duration_ms = node->destroy_duration_ms;
    transition.speed_per_mille = node->destroy_speed_per_mille;
    return start_node_destroy_animation_locked(ctx, node, transition);
}

uint8_t tgfx_node_exists(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    return find_node(ctx, handle) ? 1u : 0u;
}

tgfx_canvas_t *tgfx_node_canvas(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    return tgfx_node_back_canvas(ctx, handle);
}

tgfx_canvas_t *tgfx_node_back_canvas(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    return node ? node->back_canvas : NULL;
}

const tgfx_canvas_t *tgfx_node_front_canvas(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    return node ? node->front_canvas : NULL;
}

static tgfx_result_t resize_owned_node_canvas(tgfx_node_t *node,
                                              int16_t width,
                                              int16_t height)
{
    tgfx_canvas_t *new_front = NULL;
    tgfx_canvas_t *new_back = NULL;
    tgfx_rect_t old_front_clip;
    tgfx_rect_t old_back_clip;
    tgfx_rect_t copy_rect;
    if (!node || !node->owns_canvas) {
        return TGFX_OK;
    }
    if (width == 0 || height == 0) {
        tgfx_canvas_destroy(node->front_canvas);
        tgfx_canvas_destroy(node->back_canvas);
        node->front_canvas = NULL;
        node->back_canvas = NULL;
        return TGFX_OK;
    }
    if (node->front_canvas && node->back_canvas &&
        node->front_canvas->width == (uint16_t)width &&
        node->front_canvas->height == (uint16_t)height) {
        return TGFX_OK;
    }
    new_front = tgfx_canvas_create((uint16_t)width, (uint16_t)height,
                                   node->canvas_format);
    new_back = tgfx_canvas_create((uint16_t)width, (uint16_t)height,
                                  node->canvas_format);
    if (!new_front || !new_back) {
        tgfx_canvas_destroy(new_front);
        tgfx_canvas_destroy(new_back);
        return TGFX_ERR_NO_MEMORY;
    }

    if (node->front_canvas && node->front_canvas->pixels) {
        copy_rect = rect_intersection(canvas_full_rect(node->front_canvas),
                                      canvas_full_rect(new_front));
        tgfx_canvas_get_clip(new_front, &old_front_clip);
        tgfx_canvas_reset_clip(new_front);
        (void)tgfx_canvas_copy_rect(new_front, node->front_canvas, copy_rect, 0, 0);
        tgfx_canvas_set_clip(new_front, old_front_clip);
    }
    if (node->back_canvas && node->back_canvas->pixels) {
        copy_rect = rect_intersection(canvas_full_rect(node->back_canvas),
                                      canvas_full_rect(new_back));
        tgfx_canvas_get_clip(new_back, &old_back_clip);
        tgfx_canvas_reset_clip(new_back);
        (void)tgfx_canvas_copy_rect(new_back, node->back_canvas, copy_rect, 0, 0);
        tgfx_canvas_set_clip(new_back, old_back_clip);
    }

    tgfx_canvas_destroy(node->front_canvas);
    tgfx_canvas_destroy(node->back_canvas);
    node->front_canvas = new_front;
    node->back_canvas = new_back;
    node->committed_dirty = 0;
    node->committed_rect = rect_make(0, 0, 0, 0);
    return TGFX_OK;
}

static void mark_node_committed_dirty(tgfx_context_t *ctx, tgfx_node_t *node,
                                           tgfx_rect_t rect)
{
    if (!ctx || !node) {
        return;
    }
    rect = normalize_node_dirty_rect(node, rect);
    if (rect_empty(rect)) {
        return;
    }
    node->committed_rect = node->committed_dirty ?
                               rect_union(node->committed_rect, rect) : rect;
    node->committed_dirty = 1;
    ctx->scene_dirty = 1;
}

static tgfx_result_t commit_node_rect_locked(tgfx_context_t *ctx, tgfx_node_t *node,
                                                tgfx_rect_t rect)
{
    tgfx_rect_t saved_clip;
    tgfx_result_t res;
    if (!ctx || !node || !node->owns_canvas || !node->front_canvas ||
        !node->back_canvas) {
        return TGFX_ERR_INVALID_ARG;
    }
    rect = normalize_node_dirty_rect(node, rect);
    if (rect_empty(rect)) {
        return TGFX_OK;
    }
    tgfx_canvas_get_clip(node->front_canvas, &saved_clip);
    tgfx_canvas_reset_clip(node->front_canvas);
    res = tgfx_canvas_copy_rect(node->front_canvas, node->back_canvas,
                                rect, rect.x, rect.y);
    tgfx_canvas_set_clip(node->front_canvas, saved_clip);
    if (res == TGFX_OK) {
        mark_node_committed_dirty(ctx, node, rect);
    }
    return res;
}

tgfx_result_t tgfx_node_commit_rect(tgfx_context_t *ctx, tgfx_handle_t handle,
                                    tgfx_rect_t rect)
{
    tgfx_node_t *node;
    tgfx_result_t res;
    if (!ctx) {
        return TGFX_ERR_INVALID_ARG;
    }
    ctx_lock(ctx);
    node = find_node(ctx, handle);
    res = commit_node_rect_locked(ctx, node, rect);
    ctx_unlock(ctx);
    if (res == TGFX_OK) {
        ctx_notify(ctx);
    }
    return res;
}

tgfx_result_t tgfx_node_commit(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node;
    tgfx_result_t res;
    if (!ctx) {
        return TGFX_ERR_INVALID_ARG;
    }
    ctx_lock(ctx);
    node = find_node(ctx, handle);
    if (!node) {
        ctx_unlock(ctx);
        return TGFX_ERR_NOT_FOUND;
    }
    res = commit_node_rect_locked(ctx, node, node_full_dirty_rect(node));
    ctx_unlock(ctx);
    if (res == TGFX_OK) {
        ctx_notify(ctx);
    }
    return res;
}

tgfx_result_t tgfx_node_discard(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node;
    tgfx_rect_t saved_clip;
    tgfx_result_t res;
    if (!ctx) {
        return TGFX_ERR_INVALID_ARG;
    }
    ctx_lock(ctx);
    node = find_node(ctx, handle);
    if (!node || !node->owns_canvas || !node->front_canvas || !node->back_canvas) {
        ctx_unlock(ctx);
        return TGFX_ERR_INVALID_ARG;
    }
    tgfx_canvas_get_clip(node->back_canvas, &saved_clip);
    tgfx_canvas_reset_clip(node->back_canvas);
    res = tgfx_canvas_copy_rect(node->back_canvas, node->front_canvas,
                                node_full_dirty_rect(node), 0, 0);
    tgfx_canvas_set_clip(node->back_canvas, saved_clip);
    ctx_unlock(ctx);
    return res;
}

static uint32_t sanitize_speed(uint32_t speed_per_mille)
{
    return speed_per_mille == 0 ? 1000u : speed_per_mille;
}

static tgfx_easing_t sanitize_easing(tgfx_easing_t easing)
{
    switch (easing) {
    case TGFX_EASING_NONE:
    case TGFX_EASING_LINEAR:
    case TGFX_EASING_EASE_IN:
    case TGFX_EASING_EASE_OUT:
    case TGFX_EASING_EASE_IN_OUT:
    case TGFX_EASING_SMOOTH_STEP:
        return easing;
    default:
        return TGFX_EASING_LINEAR;
    }
}

static tgfx_transition_t sanitize_transition(tgfx_transition_t transition)
{
    transition.easing = sanitize_easing(transition.easing);
    transition.speed_per_mille = sanitize_speed(transition.speed_per_mille);
    if (transition.easing == TGFX_EASING_NONE) {
        transition.duration_ms = 0;
    }
    return transition;
}

static int32_t lerp_i32(int32_t from, int32_t to, uint32_t k)
{
    int32_t delta = to - from;
    return from + (int32_t)((delta * (int32_t)k) / 1024);
}

static tgfx_result_t set_node_rect_immediate_locked(tgfx_context_t *ctx,
                                                    tgfx_node_t *node,
                                                    tgfx_rect_t rect)
{
    tgfx_result_t res;
    if (!ctx || !node || rect.w < 0 || rect.h < 0) {
        return TGFX_ERR_INVALID_ARG;
    }
    res = resize_owned_node_canvas(node, rect.w, rect.h);
    if (res != TGFX_OK) {
        return res;
    }
    node->rect = rect;
    node->display_rect = rect;
    node->anim_start_rect = rect;
    node->anim_target_rect = rect;
    node->rect_anim_elapsed_ms = 0;
    node->rect_anim_duration_ms = 0;
    node->rect_anim_speed_per_mille = 1000;
    node->rect_anim_active = 0;
    mark_dirty_upwards(ctx, node->handle, node_full_dirty_rect(node));
    ctx->scene_dirty = 1;
    return TGFX_OK;
}

static tgfx_result_t set_node_rect_animated_locked(tgfx_context_t *ctx,
                                                   tgfx_node_t *node,
                                                   tgfx_rect_t rect,
                                                   tgfx_transition_t transition)
{
    tgfx_result_t res;
    if (!ctx || !node || rect.w < 0 || rect.h < 0) {
        return TGFX_ERR_INVALID_ARG;
    }
    transition = sanitize_transition(transition);
    if (transition.easing == TGFX_EASING_NONE || transition.duration_ms == 0) {
        return set_node_rect_immediate_locked(ctx, node, rect);
    }

    /* The logical/buffer size is updated immediately.  The animated value is
     * only the displayed/clipped rectangle used by the compositor. */
    tgfx_rect_t current_display = node->display_rect;
    res = resize_owned_node_canvas(node, rect.w, rect.h);
    if (res != TGFX_OK) {
        return res;
    }
    node->rect = rect;
    node->anim_start_rect = current_display;
    node->anim_target_rect = rect;
    node->display_rect = current_display;
    node->rect_anim_elapsed_ms = 0;
    node->rect_anim_duration_ms = transition.duration_ms;
    node->rect_anim_speed_per_mille = transition.speed_per_mille;
    node->rect_anim_easing = transition.easing;
    node->rect_anim_active = 1;

    /* Let callback-driven windows redraw once at the new backing size; manual
     * surface users can simply draw/commit whenever they want. */
    if (node->draw) {
        mark_node_dirty_rect(ctx, node, node_full_dirty_rect(node));
    }
    ctx->scene_dirty = 1;
    return TGFX_OK;
}

tgfx_result_t tgfx_node_set_rect_immediate(tgfx_context_t *ctx,
                                           tgfx_handle_t handle,
                                           tgfx_rect_t rect)
{
    tgfx_node_t *node = find_node(ctx, handle);
    return set_node_rect_immediate_locked(ctx, node, rect);
}

tgfx_result_t tgfx_node_set_rect_animated(tgfx_context_t *ctx,
                                          tgfx_handle_t handle,
                                          tgfx_rect_t rect,
                                          tgfx_transition_t transition)
{
    tgfx_node_t *node = find_node(ctx, handle);
    return set_node_rect_animated_locked(ctx, node, rect, transition);
}

tgfx_result_t tgfx_node_set_rect(tgfx_context_t *ctx, tgfx_handle_t handle,
                                 tgfx_rect_t rect)
{
    tgfx_node_t *node = find_node(ctx, handle);
    tgfx_transition_t transition;
    if (!node) {
        return TGFX_ERR_NOT_FOUND;
    }
    transition.easing = node->transition_easing;
    transition.duration_ms = node->transition_duration_ms;
    transition.speed_per_mille = node->transition_speed_per_mille;
    return set_node_rect_animated_locked(ctx, node, rect, transition);
}

tgfx_result_t tgfx_node_set_transition(tgfx_context_t *ctx,
                                       tgfx_handle_t handle,
                                       tgfx_transition_t transition)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!ctx || !node) {
        return TGFX_ERR_NOT_FOUND;
    }
    transition = sanitize_transition(transition);
    node->transition_easing = transition.easing;
    node->transition_duration_ms = transition.duration_ms;
    node->transition_speed_per_mille = transition.speed_per_mille;
    return TGFX_OK;
}

tgfx_result_t tgfx_node_get_transition(tgfx_context_t *ctx,
                                       tgfx_handle_t handle,
                                       tgfx_transition_t *transition)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!ctx || !node || !transition) {
        return TGFX_ERR_INVALID_ARG;
    }
    transition->easing = node->transition_easing;
    transition->duration_ms = node->transition_duration_ms;
    transition->speed_per_mille = node->transition_speed_per_mille;
    return TGFX_OK;
}

tgfx_result_t tgfx_node_set_destroy_transition(tgfx_context_t *ctx,
                                               tgfx_handle_t handle,
                                               tgfx_transition_t transition)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!ctx || !node) {
        return TGFX_ERR_NOT_FOUND;
    }
    transition = sanitize_transition(transition);
    node->destroy_easing = transition.easing;
    node->destroy_duration_ms = transition.duration_ms;
    node->destroy_speed_per_mille = transition.speed_per_mille;
    return TGFX_OK;
}

tgfx_result_t tgfx_node_get_destroy_transition(tgfx_context_t *ctx,
                                               tgfx_handle_t handle,
                                               tgfx_transition_t *transition)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!ctx || !node || !transition) {
        return TGFX_ERR_INVALID_ARG;
    }
    transition->easing = node->destroy_easing;
    transition->duration_ms = node->destroy_duration_ms;
    transition->speed_per_mille = node->destroy_speed_per_mille;
    return TGFX_OK;
}

tgfx_result_t tgfx_node_get_rect(tgfx_context_t *ctx, tgfx_handle_t handle,
                                 tgfx_rect_t *rect)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!node || !rect) {
        return TGFX_ERR_INVALID_ARG;
    }
    *rect = node->rect;
    return TGFX_OK;
}

tgfx_result_t tgfx_node_set_visible(tgfx_context_t *ctx, tgfx_handle_t handle,
                                    uint8_t visible)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!node) {
        return TGFX_ERR_NOT_FOUND;
    }
    node->visible = visible ? 1u : 0u;
    mark_dirty_upwards(ctx, handle, node_full_dirty_rect(node));
    return TGFX_OK;
}

tgfx_result_t tgfx_node_set_opacity(tgfx_context_t *ctx, tgfx_handle_t handle,
                                    uint8_t opacity)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!node) {
        return TGFX_ERR_NOT_FOUND;
    }
    node->opacity = opacity;
    mark_dirty_upwards(ctx, handle, node_full_dirty_rect(node));
    return TGFX_OK;
}

tgfx_result_t tgfx_node_bring_to_front(tgfx_context_t *ctx,
                                       tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    tgfx_handle_t parent;
    if (!node || node->parent == TGFX_INVALID_HANDLE) {
        return TGFX_ERR_INVALID_ARG;
    }
    parent = node->parent;
    detach_from_parent(ctx, node);
    node = find_node(ctx, handle);
    if (!node) {
        return TGFX_ERR_NOT_FOUND;
    }
    node->parent = parent;
    append_child(ctx, parent, handle);
    mark_dirty_upwards(ctx, parent, node_full_dirty_rect(find_node(ctx, parent)));
    return TGFX_OK;
}

tgfx_result_t tgfx_node_send_to_back(tgfx_context_t *ctx,
                                     tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    tgfx_handle_t parent;
    if (!node || node->parent == TGFX_INVALID_HANDLE) {
        return TGFX_ERR_INVALID_ARG;
    }
    parent = node->parent;
    detach_from_parent(ctx, node);
    node = find_node(ctx, handle);
    if (!node) {
        return TGFX_ERR_NOT_FOUND;
    }
    node->parent = parent;
    prepend_child(ctx, parent, handle);
    mark_dirty_upwards(ctx, parent, node_full_dirty_rect(find_node(ctx, parent)));
    return TGFX_OK;
}

tgfx_result_t tgfx_node_set_parent(tgfx_context_t *ctx, tgfx_handle_t handle,
                                   tgfx_handle_t parent)
{
    tgfx_node_t *node = find_node(ctx, handle);
    tgfx_node_t *new_parent;
    tgfx_handle_t old_parent;
    if (!ctx || !node || handle == TGFX_INVALID_HANDLE ||
        handle == tgfx_root_handle(ctx) || parent == TGFX_INVALID_HANDLE ||
        handle == parent) {
        return TGFX_ERR_INVALID_ARG;
    }
    new_parent = find_node(ctx, parent);
    if (!new_parent) {
        return TGFX_ERR_NOT_FOUND;
    }
    if (node_is_descendant(ctx, parent, handle)) {
        return TGFX_ERR_INVALID_ARG;
    }
    old_parent = node->parent;
    detach_from_parent(ctx, node);
    node = find_node(ctx, handle);
    if (!node) {
        return TGFX_ERR_NOT_FOUND;
    }
    node->parent = parent;
    node->next_sibling = TGFX_INVALID_HANDLE;
    append_child(ctx, parent, handle);
    if (old_parent != TGFX_INVALID_HANDLE) {
        mark_dirty_upwards(ctx, old_parent,
                           node_full_dirty_rect(find_node(ctx, old_parent)));
    }
    mark_dirty_upwards(ctx, parent, node_full_dirty_rect(new_parent));
    return TGFX_OK;
}

tgfx_handle_t tgfx_root_handle(tgfx_context_t *ctx)
{
    return (ctx && ctx->node_count > 0) ? ctx->nodes[0].handle : TGFX_INVALID_HANDLE;
}

tgfx_handle_t tgfx_node_parent(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    return node ? node->parent : TGFX_INVALID_HANDLE;
}

tgfx_handle_t tgfx_node_first_child(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    return node ? node->first_child : TGFX_INVALID_HANDLE;
}

tgfx_handle_t tgfx_node_next_sibling(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    return node ? node->next_sibling : TGFX_INVALID_HANDLE;
}

void *tgfx_node_user(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    tgfx_node_t *node = find_node(ctx, handle);
    return node ? node->user : NULL;
}

tgfx_result_t tgfx_invalidate(tgfx_context_t *ctx, tgfx_handle_t handle,
                              tgfx_rect_t rect)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!node) {
        return TGFX_ERR_NOT_FOUND;
    }
    mark_dirty_upwards(ctx, handle, rect);
    return TGFX_OK;
}

uint8_t tgfx_needs_render(const tgfx_context_t *ctx)
{
    return ctx ? ctx->scene_dirty : 0;
}


static uint8_t gray4_pack(uint8_t color)
{
    uint8_t nibble = (uint8_t)(color >> 4);
    return (uint8_t)((nibble << 4) | nibble);
}

void tgfx_draw_pixel(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                     tgfx_color_t color)
{
    tgfx_rect_t clip;
    if (!canvas || !canvas->pixels || x < 0 || y < 0 ||
        x >= (int16_t)canvas->width || y >= (int16_t)canvas->height) {
        return;
    }
    clip = canvas_clip_rect(canvas);
    if (x < clip.x || y < clip.y || x >= (int16_t)(clip.x + clip.w) ||
        y >= (int16_t)(clip.y + clip.h)) {
        return;
    }

    if (canvas->format == TGFX_PIXEL_MONO1_LSB_VERTICAL) {
        size_t index = (size_t)x + (size_t)(y / 8) * canvas->stride;
        uint8_t mask = (uint8_t)(1u << (y & 7));
        if (color) {
            canvas->pixels[index] |= mask;
        } else {
            canvas->pixels[index] &= (uint8_t)~mask;
        }
    } else if (canvas->format == TGFX_PIXEL_GRAY4) {
        uint8_t *p = canvas->pixels + (size_t)y * canvas->stride + (size_t)(x / 2);
        uint8_t n = (uint8_t)(color >> 4);
        if ((x & 1) == 0) {
            *p = (uint8_t)((*p & 0x0Fu) | (uint8_t)(n << 4));
        } else {
            *p = (uint8_t)((*p & 0xF0u) | n);
        }
    } else if (canvas->format == TGFX_PIXEL_GRAY8) {
        canvas->pixels[(size_t)y * canvas->stride + (size_t)x] = color;
    }
}

tgfx_color_t tgfx_get_pixel(const tgfx_canvas_t *canvas, int16_t x, int16_t y)
{
    if (!canvas || !canvas->pixels || x < 0 || y < 0 ||
        x >= (int16_t)canvas->width || y >= (int16_t)canvas->height) {
        return 0;
    }
    if (canvas->format == TGFX_PIXEL_MONO1_LSB_VERTICAL) {
        size_t index = (size_t)x + (size_t)(y / 8) * canvas->stride;
        uint8_t mask = (uint8_t)(1u << (y & 7));
        return (canvas->pixels[index] & mask) ? 255u : 0u;
    }
    if (canvas->format == TGFX_PIXEL_GRAY4) {
        uint8_t packed = canvas->pixels[(size_t)y * canvas->stride + (size_t)(x / 2)];
        uint8_t n = (x & 1) ? (uint8_t)(packed & 0x0Fu) : (uint8_t)(packed >> 4);
        return (tgfx_color_t)((n << 4) | n);
    }
    if (canvas->format == TGFX_PIXEL_GRAY8) {
        return canvas->pixels[(size_t)y * canvas->stride + (size_t)x];
    }
    return 0;
}

void tgfx_fill_rect(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                    int16_t w, int16_t h, tgfx_color_t color)
{
    tgfx_rect_t r;
    int16_t iy;
    if (!canvas || !canvas->pixels || w <= 0 || h <= 0) {
        return;
    }
    r = rect_intersection(rect_make(x, y, w, h), canvas_clip_rect(canvas));
    if (rect_empty(r)) {
        return;
    }

    if (canvas->format == TGFX_PIXEL_GRAY8) {
        for (iy = 0; iy < r.h; ++iy) {
            memset(canvas->pixels + (size_t)(r.y + iy) * canvas->stride + (size_t)r.x,
                   color, (size_t)r.w);
        }
        return;
    }

    if (canvas->format == TGFX_PIXEL_GRAY4) {
        uint8_t n = (uint8_t)(color >> 4);
        uint8_t packed = (uint8_t)((n << 4) | n);
        for (iy = 0; iy < r.h; ++iy) {
            int16_t start = r.x;
            int16_t end = (int16_t)(r.x + r.w);
            uint8_t *row = canvas->pixels + (size_t)(r.y + iy) * canvas->stride;
            if (start & 1) {
                uint8_t *p = row + (size_t)(start / 2);
                *p = (uint8_t)((*p & 0xF0u) | n);
                ++start;
            }
            if (end > start) {
                size_t full_bytes = (size_t)((end - start) / 2);
                if (full_bytes) {
                    memset(row + (size_t)(start / 2), packed, full_bytes);
                    start = (int16_t)(start + full_bytes * 2u);
                }
            }
            if (start < end) {
                uint8_t *p = row + (size_t)(start / 2);
                *p = (uint8_t)((*p & 0x0Fu) | (uint8_t)(n << 4));
            }
        }
        return;
    }

    if (canvas->format == TGFX_PIXEL_MONO1_LSB_VERTICAL) {
        uint8_t on = color ? 1u : 0u;
        int16_t ix;
        for (iy = 0; iy < r.h; ++iy) {
            int16_t yy = (int16_t)(r.y + iy);
            uint8_t mask = (uint8_t)(1u << (yy & 7));
            uint8_t *base = canvas->pixels + (size_t)(yy / 8) * canvas->stride;
            for (ix = 0; ix < r.w; ++ix) {
                uint8_t *p = base + (size_t)(r.x + ix);
                if (on) {
                    *p |= mask;
                } else {
                    *p &= (uint8_t)~mask;
                }
            }
        }
    }
}

void tgfx_clear(tgfx_canvas_t *canvas, tgfx_color_t color)
{
    tgfx_rect_t clip;
    if (!canvas || !canvas->pixels) {
        return;
    }
    clip = canvas_clip_rect(canvas);
    if (rect_empty(clip)) {
        return;
    }
    if (clip.x == 0 && clip.y == 0 && clip.w == (int16_t)canvas->width &&
        clip.h == (int16_t)canvas->height) {
        if (canvas->format == TGFX_PIXEL_MONO1_LSB_VERTICAL) {
            memset(canvas->pixels, color ? 0xFF : 0x00,
                   tgfx_canvas_required_bytes(canvas->width, canvas->height,
                                              canvas->format));
            return;
        }
        if (canvas->format == TGFX_PIXEL_GRAY4) {
            memset(canvas->pixels, gray4_pack(color),
                   tgfx_canvas_required_bytes(canvas->width, canvas->height,
                                              canvas->format));
            return;
        }
        if (canvas->format == TGFX_PIXEL_GRAY8) {
            memset(canvas->pixels, color,
                   tgfx_canvas_required_bytes(canvas->width, canvas->height,
                                              canvas->format));
            return;
        }
    }
    tgfx_fill_rect(canvas, 0, 0, (int16_t)canvas->width,
                   (int16_t)canvas->height, color);
}

void tgfx_draw_line(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                    int16_t x1, int16_t y1, tgfx_color_t color)
{
    int16_t dx = (int16_t)abs(x1 - x0);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t dy = (int16_t)-abs(y1 - y0);
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = (int16_t)(dx + dy);

    for (;;) {
        int16_t e2;
        tgfx_draw_pixel(canvas, x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = (int16_t)(2 * err);
        if (e2 >= dy) {
            err = (int16_t)(err + dy);
            x0 = (int16_t)(x0 + sx);
        }
        if (e2 <= dx) {
            err = (int16_t)(err + dx);
            y0 = (int16_t)(y0 + sy);
        }
    }
}

void tgfx_draw_rect(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                    int16_t w, int16_t h, tgfx_color_t color)
{
    tgfx_fill_rect(canvas, x, y, w, 1, color);
    tgfx_fill_rect(canvas, x, (int16_t)(y + h - 1), w, 1, color);
    tgfx_fill_rect(canvas, x, y, 1, h, color);
    tgfx_fill_rect(canvas, (int16_t)(x + w - 1), y, 1, h, color);
}

void tgfx_draw_circle(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                      int16_t r, tgfx_color_t color)
{
    int16_t f = 1 - r;
    int16_t ddx = 1;
    int16_t ddy = (int16_t)(-2 * r);
    int16_t x = 0;
    int16_t y = r;
    tgfx_draw_pixel(canvas, x0, (int16_t)(y0 + r), color);
    tgfx_draw_pixel(canvas, x0, (int16_t)(y0 - r), color);
    tgfx_draw_pixel(canvas, (int16_t)(x0 + r), y0, color);
    tgfx_draw_pixel(canvas, (int16_t)(x0 - r), y0, color);
    while (x < y) {
        if (f >= 0) {
            --y;
            ddy = (int16_t)(ddy + 2);
            f = (int16_t)(f + ddy);
        }
        ++x;
        ddx = (int16_t)(ddx + 2);
        f = (int16_t)(f + ddx);
        tgfx_draw_pixel(canvas, (int16_t)(x0 + x), (int16_t)(y0 + y), color);
        tgfx_draw_pixel(canvas, (int16_t)(x0 - x), (int16_t)(y0 + y), color);
        tgfx_draw_pixel(canvas, (int16_t)(x0 + x), (int16_t)(y0 - y), color);
        tgfx_draw_pixel(canvas, (int16_t)(x0 - x), (int16_t)(y0 - y), color);
        tgfx_draw_pixel(canvas, (int16_t)(x0 + y), (int16_t)(y0 + x), color);
        tgfx_draw_pixel(canvas, (int16_t)(x0 - y), (int16_t)(y0 + x), color);
        tgfx_draw_pixel(canvas, (int16_t)(x0 + y), (int16_t)(y0 - x), color);
        tgfx_draw_pixel(canvas, (int16_t)(x0 - y), (int16_t)(y0 - x), color);
    }
}

void tgfx_fill_circle(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                      int16_t r, tgfx_color_t color)
{
    int16_t y;
    for (y = (int16_t)-r; y <= r; ++y) {
        int16_t x;
        for (x = (int16_t)-r; x <= r; ++x) {
            if (x * x + y * y <= r * r) {
                tgfx_draw_pixel(canvas, (int16_t)(x0 + x), (int16_t)(y0 + y), color);
            }
        }
    }
}

void tgfx_draw_triangle(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        tgfx_color_t color)
{
    tgfx_draw_line(canvas, x0, y0, x1, y1, color);
    tgfx_draw_line(canvas, x1, y1, x2, y2, color);
    tgfx_draw_line(canvas, x2, y2, x0, y0, color);
}

static void swap_i16(int16_t *a, int16_t *b)
{
    int16_t t = *a;
    *a = *b;
    *b = t;
}

void tgfx_fill_triangle(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                        int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                        tgfx_color_t color)
{
    int16_t a;
    int16_t b;
    int16_t y;
    int16_t last;
    int32_t dx01;
    int32_t dy01;
    int32_t dx02;
    int32_t dy02;
    int32_t dx12;
    int32_t dy12;
    int32_t sa = 0;
    int32_t sb = 0;

    if (y0 > y1) {
        swap_i16(&y0, &y1);
        swap_i16(&x0, &x1);
    }
    if (y1 > y2) {
        swap_i16(&y2, &y1);
        swap_i16(&x2, &x1);
    }
    if (y0 > y1) {
        swap_i16(&y0, &y1);
        swap_i16(&x0, &x1);
    }

    if (y0 == y2) {
        a = b = x0;
        if (x1 < a) {
            a = x1;
        } else if (x1 > b) {
            b = x1;
        }
        if (x2 < a) {
            a = x2;
        } else if (x2 > b) {
            b = x2;
        }
        tgfx_fill_rect(canvas, a, y0, (int16_t)(b - a + 1), 1, color);
        return;
    }

    dx01 = x1 - x0;
    dy01 = y1 - y0;
    dx02 = x2 - x0;
    dy02 = y2 - y0;
    dx12 = x2 - x1;
    dy12 = y2 - y1;
    last = y1 == y2 ? y1 : (int16_t)(y1 - 1);

    for (y = y0; y <= last; ++y) {
        a = (int16_t)(x0 + sa / dy01);
        b = (int16_t)(x0 + sb / dy02);
        sa += dx01;
        sb += dx02;
        if (a > b) {
            swap_i16(&a, &b);
        }
        tgfx_fill_rect(canvas, a, y, (int16_t)(b - a + 1), 1, color);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; ++y) {
        a = (int16_t)(x1 + (dy12 ? sa / dy12 : 0));
        b = (int16_t)(x0 + (dy02 ? sb / dy02 : 0));
        sa += dx12;
        sb += dx02;
        if (a > b) {
            swap_i16(&a, &b);
        }
        tgfx_fill_rect(canvas, a, y, (int16_t)(b - a + 1), 1, color);
    }
}

static void draw_circle_corners(tgfx_canvas_t *canvas, int16_t x0, int16_t y0,
                                int16_t r, uint8_t corners, tgfx_color_t color)
{
    int16_t f = 1 - r;
    int16_t ddx = 1;
    int16_t ddy = (int16_t)(-2 * r);
    int16_t x = 0;
    int16_t y = r;

    while (x < y) {
        if (f >= 0) {
            --y;
            ddy = (int16_t)(ddy + 2);
            f = (int16_t)(f + ddy);
        }
        ++x;
        ddx = (int16_t)(ddx + 2);
        f = (int16_t)(f + ddx);

        if (corners & 0x1u) {
            tgfx_draw_pixel(canvas, (int16_t)(x0 - y), (int16_t)(y0 - x), color);
            tgfx_draw_pixel(canvas, (int16_t)(x0 - x), (int16_t)(y0 - y), color);
        }
        if (corners & 0x2u) {
            tgfx_draw_pixel(canvas, (int16_t)(x0 + x), (int16_t)(y0 - y), color);
            tgfx_draw_pixel(canvas, (int16_t)(x0 + y), (int16_t)(y0 - x), color);
        }
        if (corners & 0x4u) {
            tgfx_draw_pixel(canvas, (int16_t)(x0 + x), (int16_t)(y0 + y), color);
            tgfx_draw_pixel(canvas, (int16_t)(x0 + y), (int16_t)(y0 + x), color);
        }
        if (corners & 0x8u) {
            tgfx_draw_pixel(canvas, (int16_t)(x0 - y), (int16_t)(y0 + x), color);
            tgfx_draw_pixel(canvas, (int16_t)(x0 - x), (int16_t)(y0 + y), color);
        }
    }
}

void tgfx_draw_round_rect(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                          int16_t w, int16_t h, int16_t radius,
                          tgfx_color_t color)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    if (radius < 0) {
        radius = 0;
    }
    if (radius * 2 > w) {
        radius = (int16_t)(w / 2);
    }
    if (radius * 2 > h) {
        radius = (int16_t)(h / 2);
    }
    tgfx_fill_rect(canvas, (int16_t)(x + radius), y, (int16_t)(w - 2 * radius), 1, color);
    tgfx_fill_rect(canvas, (int16_t)(x + radius), (int16_t)(y + h - 1),
                   (int16_t)(w - 2 * radius), 1, color);
    tgfx_fill_rect(canvas, x, (int16_t)(y + radius), 1, (int16_t)(h - 2 * radius), color);
    tgfx_fill_rect(canvas, (int16_t)(x + w - 1), (int16_t)(y + radius), 1,
                   (int16_t)(h - 2 * radius), color);
    draw_circle_corners(canvas, (int16_t)(x + radius), (int16_t)(y + radius),
                        radius, 0x1u, color);
    draw_circle_corners(canvas, (int16_t)(x + w - radius - 1), (int16_t)(y + radius),
                        radius, 0x2u, color);
    draw_circle_corners(canvas, (int16_t)(x + w - radius - 1),
                        (int16_t)(y + h - radius - 1), radius, 0x4u, color);
    draw_circle_corners(canvas, (int16_t)(x + radius),
                        (int16_t)(y + h - radius - 1), radius, 0x8u, color);
}

void tgfx_fill_round_rect(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                          int16_t w, int16_t h, int16_t radius,
                          tgfx_color_t color)
{
    int16_t iy;
    if (w <= 0 || h <= 0) {
        return;
    }
    if (radius < 0) {
        radius = 0;
    }
    if (radius * 2 > w) {
        radius = (int16_t)(w / 2);
    }
    if (radius * 2 > h) {
        radius = (int16_t)(h / 2);
    }
    for (iy = 0; iy < h; ++iy) {
        int16_t left = 0;
        int16_t right = (int16_t)(w - 1);
        if (iy < radius || iy >= h - radius) {
            int16_t cy = iy < radius ? (int16_t)(radius - iy) : (int16_t)(iy - (h - radius - 1));
            while (left < radius && left * left + cy * cy > radius * radius) {
                ++left;
            }
            right = (int16_t)(w - 1 - left);
        }
        tgfx_fill_rect(canvas, (int16_t)(x + left), (int16_t)(y + iy),
                       (int16_t)(right - left + 1), 1, color);
    }
}

void tgfx_draw_bitmap_1bpp(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                           const uint8_t *bitmap, int16_t w, int16_t h,
                           tgfx_color_t color, tgfx_color_t bg)
{
    int16_t yy;
    int16_t xx;
    if (!canvas || !bitmap || w <= 0 || h <= 0) {
        return;
    }
    for (yy = 0; yy < h; ++yy) {
        for (xx = 0; xx < w; ++xx) {
            uint8_t byte = bitmap[(size_t)yy * ((w + 7) / 8) + (xx / 8)];
            uint8_t on = (uint8_t)(byte & (0x80u >> (xx & 7)));
            if (on) {
                tgfx_draw_pixel(canvas, (int16_t)(x + xx), (int16_t)(y + yy), color);
            } else if (bg != color) {
                tgfx_draw_pixel(canvas, (int16_t)(x + xx), (int16_t)(y + yy), bg);
            }
        }
    }
}

static void glyph_5x7(char ch, uint8_t out[5])
{
    static const uint8_t fallback[5] = {0x7F, 0x41, 0x5D, 0x41, 0x7F};
    static const uint8_t space[5] = {0, 0, 0, 0, 0};
    static const uint8_t digits[10][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
        {0x62, 0x51, 0x49, 0x49, 0x46}, {0x22, 0x49, 0x49, 0x49, 0x36},
        {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x2F, 0x49, 0x49, 0x49, 0x31},
        {0x3E, 0x49, 0x49, 0x49, 0x32}, {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36}, {0x26, 0x49, 0x49, 0x49, 0x3E}
    };
    static const uint8_t letters[26][5] = {
        {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
        {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
        {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
        {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
        {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},
        {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
        {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
        {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
        {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
        {0x26,0x49,0x49,0x49,0x32},{0x01,0x01,0x7F,0x01,0x01},
        {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
        {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
        {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
    };
    const uint8_t *src = fallback;

    if (ch == ' ') {
        src = space;
    } else if (ch >= '0' && ch <= '9') {
        src = digits[ch - '0'];
    } else {
        if (ch >= 'a' && ch <= 'z') {
            ch = (char)(ch - 'a' + 'A');
        }
        if (ch >= 'A' && ch <= 'Z') {
            src = letters[ch - 'A'];
        } else {
            switch (ch) {
            case '.': { static const uint8_t g[5] = {0,0x60,0x60,0,0}; src = g; break; }
            case ',': { static const uint8_t g[5] = {0,0x80,0x60,0,0}; src = g; break; }
            case ':': { static const uint8_t g[5] = {0,0x36,0x36,0,0}; src = g; break; }
            case ';': { static const uint8_t g[5] = {0,0x80,0x76,0,0}; src = g; break; }
            case '!': { static const uint8_t g[5] = {0,0,0x5F,0,0}; src = g; break; }
            case '?': { static const uint8_t g[5] = {0x02,0x01,0x59,0x09,0x06}; src = g; break; }
            case '-': { static const uint8_t g[5] = {0x08,0x08,0x08,0x08,0x08}; src = g; break; }
            case '+': { static const uint8_t g[5] = {0x08,0x08,0x3E,0x08,0x08}; src = g; break; }
            case '/': { static const uint8_t g[5] = {0x20,0x10,0x08,0x04,0x02}; src = g; break; }
            case '\\': { static const uint8_t g[5] = {0x02,0x04,0x08,0x10,0x20}; src = g; break; }
            case '_': { static const uint8_t g[5] = {0x40,0x40,0x40,0x40,0x40}; src = g; break; }
            case '=': { static const uint8_t g[5] = {0x14,0x14,0x14,0x14,0x14}; src = g; break; }
            case '(': { static const uint8_t g[5] = {0,0x1C,0x22,0x41,0}; src = g; break; }
            case ')': { static const uint8_t g[5] = {0,0x41,0x22,0x1C,0}; src = g; break; }
            case '[': { static const uint8_t g[5] = {0,0x7F,0x41,0x41,0}; src = g; break; }
            case ']': { static const uint8_t g[5] = {0,0x41,0x41,0x7F,0}; src = g; break; }
            case '<': { static const uint8_t g[5] = {0x08,0x14,0x22,0x41,0}; src = g; break; }
            case '>': { static const uint8_t g[5] = {0x41,0x22,0x14,0x08,0}; src = g; break; }
            case '"': { static const uint8_t g[5] = {0,0x07,0,0x07,0}; src = g; break; }
            case '\'': { static const uint8_t g[5] = {0,0,0x07,0,0}; src = g; break; }
            default: break;
            }
        }
    }
    memcpy(out, src, 5);
}

void tgfx_draw_char_5x7(tgfx_canvas_t *canvas, int16_t x, int16_t y, char ch,
                        tgfx_color_t color, tgfx_color_t bg,
                        uint8_t size_x, uint8_t size_y)
{
    uint8_t glyph[5];
    uint8_t col;
    if (!canvas) {
        return;
    }
    if (size_x == 0) {
        size_x = 1;
    }
    if (size_y == 0) {
        size_y = 1;
    }
    glyph_5x7(ch, glyph);
    for (col = 0; col < 6; ++col) {
        uint8_t bits = col < 5 ? glyph[col] : 0;
        uint8_t row;
        for (row = 0; row < 8; ++row) {
            if (bits & (1u << row)) {
                tgfx_fill_rect(canvas, (int16_t)(x + col * size_x),
                               (int16_t)(y + row * size_y), size_x, size_y,
                               color);
            } else if (bg != color) {
                tgfx_fill_rect(canvas, (int16_t)(x + col * size_x),
                               (int16_t)(y + row * size_y), size_x, size_y,
                               bg);
            }
        }
    }
}

void tgfx_draw_text_5x7(tgfx_canvas_t *canvas, int16_t x, int16_t y,
                        const char *text, tgfx_color_t color,
                        tgfx_color_t bg, uint8_t size_x, uint8_t size_y)
{
    while (text && *text) {
        tgfx_draw_char_5x7(canvas, x, y, *text++, color, bg, size_x, size_y);
        x = (int16_t)(x + (size_x ? size_x : 1) * 6);
    }
}

void tgfx_draw_char_font(tgfx_canvas_t *canvas, const tgfx_font_t *font,
                         int16_t x, int16_t y, uint16_t ch,
                         tgfx_color_t color, tgfx_color_t bg,
                         uint8_t size_x, uint8_t size_y)
{
    const tgfx_glyph_t *glyph;
    uint16_t bo;
    uint8_t bits = 0;
    uint8_t bit = 0;
    uint8_t yy;
    uint8_t xx;

    if (!canvas || !font || !font->bitmap || !font->glyph ||
        ch < font->first || ch > font->last) {
        return;
    }
    if (size_x == 0) {
        size_x = 1;
    }
    if (size_y == 0) {
        size_y = 1;
    }

    glyph = &font->glyph[ch - font->first];
    bo = glyph->bitmapOffset;

    if (bg != color) {
        tgfx_fill_rect(canvas, x, y,
                       static_cast<int16_t>(glyph->xAdvance * size_x),
                       static_cast<int16_t>(font->yAdvance * size_y), bg);
    }

    for (yy = 0; yy < glyph->height; ++yy) {
        for (xx = 0; xx < glyph->width; ++xx) {
            if ((bit++ & 7u) == 0) {
                bits = font->bitmap[bo++];
            }
            if (bits & 0x80u) {
                tgfx_fill_rect(canvas,
                               (int16_t)(x + glyph->xOffset * size_x + xx * size_x),
                               (int16_t)(y + glyph->yOffset * size_y + yy * size_y),
                               size_x, size_y, color);
            }
            bits <<= 1;
        }
    }
}

void tgfx_text_bounds_5x7(const char *text, int16_t x, int16_t y,
                          uint8_t size_x, uint8_t size_y, int16_t *x1,
                          int16_t *y1, uint16_t *w, uint16_t *h)
{
    int16_t min_x;
    int16_t min_y;
    int16_t max_x;
    int16_t max_y;
    int16_t cursor_x = x;
    int16_t cursor_y = y;

    if (!text || !x1 || !y1 || !w || !h) {
        return;
    }
    if (size_x == 0) {
        size_x = 1;
    }
    if (size_y == 0) {
        size_y = 1;
    }

    min_x = x;
    min_y = y;
    max_x = (int16_t)(x - 1);
    max_y = (int16_t)(y - 1);

    while (*text) {
        if (*text == '\n') {
            cursor_x = x;
            cursor_y = (int16_t)(cursor_y + size_y * 8);
        } else if (*text != '\r') {
            int16_t char_w = (int16_t)(size_x * 6);
            int16_t char_h = (int16_t)(size_y * 8);
            if (cursor_x < min_x) {
                min_x = cursor_x;
            }
            if (cursor_y < min_y) {
                min_y = cursor_y;
            }
            if (cursor_x + char_w - 1 > max_x) {
                max_x = (int16_t)(cursor_x + char_w - 1);
            }
            if (cursor_y + char_h - 1 > max_y) {
                max_y = (int16_t)(cursor_y + char_h - 1);
            }
            cursor_x = (int16_t)(cursor_x + char_w);
        }
        ++text;
    }

    *x1 = min_x;
    *y1 = min_y;
    *w = max_x >= min_x ? (uint16_t)(max_x - min_x + 1) : 0;
    *h = max_y >= min_y ? (uint16_t)(max_y - min_y + 1) : 0;
}

void tgfx_text_bounds_font(const tgfx_font_t *font, const char *text,
                           int16_t x, int16_t y, uint8_t size_x,
                           uint8_t size_y, int16_t *x1, int16_t *y1,
                           uint16_t *w, uint16_t *h)
{
    int16_t min_x;
    int16_t min_y;
    int16_t max_x;
    int16_t max_y;
    int16_t cursor_x = x;
    int16_t cursor_y = y;

    if (!font || !text || !x1 || !y1 || !w || !h) {
        return;
    }
    if (size_x == 0) {
        size_x = 1;
    }
    if (size_y == 0) {
        size_y = 1;
    }

    min_x = x;
    min_y = y;
    max_x = (int16_t)(x - 1);
    max_y = (int16_t)(y - 1);

    while (*text) {
        uint8_t c = (uint8_t)*text++;
        if (c == '\n') {
            cursor_x = x;
            cursor_y = (int16_t)(cursor_y + font->yAdvance * size_y);
        } else if (c != '\r' && c >= font->first && c <= font->last) {
            const tgfx_glyph_t *glyph = &font->glyph[c - font->first];
            int16_t gx1 = (int16_t)(cursor_x + glyph->xOffset * size_x);
            int16_t gy1 = (int16_t)(cursor_y + glyph->yOffset * size_y);
            int16_t gx2 = (int16_t)(gx1 + glyph->width * size_x - 1);
            int16_t gy2 = (int16_t)(gy1 + glyph->height * size_y - 1);

            if (gx1 < min_x) {
                min_x = gx1;
            }
            if (gy1 < min_y) {
                min_y = gy1;
            }
            if (gx2 > max_x) {
                max_x = gx2;
            }
            if (gy2 > max_y) {
                max_y = gy2;
            }
            cursor_x = (int16_t)(cursor_x + glyph->xAdvance * size_x);
        }
    }

    *x1 = min_x;
    *y1 = min_y;
    *w = max_x >= min_x ? (uint16_t)(max_x - min_x + 1) : 0;
    *h = max_y >= min_y ? (uint16_t)(max_y - min_y + 1) : 0;
}

static tgfx_color_t blend_over(tgfx_color_t dst, tgfx_color_t src, uint8_t opacity)
{
    int32_t diff = (int32_t)src - (int32_t)dst;
    return (tgfx_color_t)((int32_t)dst + (diff * opacity) / 255);
}

static int16_t title_bar_h(const tgfx_node_t *node)
{
    return node && node->show_title
        ? static_cast<int16_t>(titleFont()->yAdvance + 1)
        : 0;
}

static int16_t border_w(const tgfx_node_t *node)
{
    return node && node->border == TGFX_WINDOW_BORDER_RECT ? 1 : 0;
}

static int16_t display_content_w(const tgfx_node_t *node)
{
    return node ? node->display_rect.w : 0;
}

static int16_t display_content_h(const tgfx_node_t *node)
{
    return node ? node->display_rect.h : 0;
}

static int16_t outer_w(const tgfx_node_t *node)
{
    return (int16_t)(display_content_w(node) + border_w(node) * 2);
}

static int16_t outer_h(const tgfx_node_t *node)
{
    int16_t bw = border_w(node);
    return (int16_t)(display_content_h(node) + title_bar_h(node) +
                     (title_bar_h(node) ? bw : (int16_t)(bw * 2)));
}

static int16_t content_dx(const tgfx_node_t *node)
{
    return border_w(node);
}

static int16_t content_dy(const tgfx_node_t *node)
{
    int16_t th = title_bar_h(node);
    return th ? th : border_w(node);
}

static void draw_title_text_clipped(tgfx_canvas_t *canvas, int16_t x,
                                    int16_t y, int16_t max_w,
                                    const char *text, tgfx_color_t color)
{
    const tgfx_font_t *font = titleFont();
    while (canvas && font && text && *text && max_w >= font->glyph[0].width) {
        uint8_t ch = static_cast<uint8_t>(*text++);
        if (ch >= font->first && ch <= font->last) {
            const tgfx_glyph_t *glyph = &font->glyph[ch - font->first];
            if (glyph->xAdvance > max_w) {
                break;
            }
            tgfx_draw_char_font(canvas, font, x, y, ch, color, color, 1, 1);
            x = static_cast<int16_t>(x + glyph->xAdvance);
            max_w = static_cast<int16_t>(max_w - glyph->xAdvance);
        }
    }
}

static void draw_node_decoration(tgfx_canvas_t *target, const tgfx_node_t *node,
                                 int16_t x, int16_t y)
{
    int16_t bw = border_w(node);
    int16_t th = title_bar_h(node);
    int16_t ow = outer_w(node);
    int16_t oh = outer_h(node);

    if (!target || !node || (bw == 0 && th == 0)) {
        return;
    }
    if (th) {
        tgfx_fill_rect(target, x, y, ow, th, 255);
        draw_title_text_clipped(target, (int16_t)(x + 1), (int16_t)(y + 1),
                                (int16_t)(ow - 2), node->title, 0);
    }
    if (bw) {
        tgfx_draw_rect(target, x, y, ow, oh, 255);
    }
}

static void fill_rect_over(tgfx_canvas_t *dst, int16_t x, int16_t y,
                           int16_t w, int16_t h, tgfx_color_t color,
                           uint8_t opacity)
{
    tgfx_rect_t r;
    int16_t iy;
    if (!dst || !dst->pixels || w <= 0 || h <= 0 || opacity == 0) {
        return;
    }
    if (opacity == 255) {
        tgfx_fill_rect(dst, x, y, w, h, color);
        return;
    }
    r = rect_intersection(rect_make(x, y, w, h), canvas_clip_rect(dst));
    if (rect_empty(r)) {
        return;
    }
    for (iy = 0; iy < r.h; ++iy) {
        int16_t ix;
        int16_t py = (int16_t)(r.y + iy);
        for (ix = 0; ix < r.w; ++ix) {
            int16_t px = (int16_t)(r.x + ix);
            tgfx_color_t d = tgfx_get_pixel(dst, px, py);
            tgfx_draw_pixel(dst, px, py, blend_over(d, color, opacity));
        }
    }
}

static void composite_canvas(tgfx_canvas_t *dst, const tgfx_canvas_t *src,
                             int16_t dx, int16_t dy, uint8_t opacity)
{
    tgfx_rect_t dst_clip;
    tgfx_rect_t src_area;
    int16_t y;
    if (!dst || !src || !dst->pixels || !src->pixels || opacity == 0) {
        return;
    }
    dst_clip = canvas_clip_rect(dst);
    src_area = rect_intersection(canvas_full_rect(src),
                                 rect_make((int16_t)(dst_clip.x - dx),
                                           (int16_t)(dst_clip.y - dy),
                                           dst_clip.w, dst_clip.h));
    if (rect_empty(src_area)) {
        return;
    }
    for (y = 0; y < src_area.h; ++y) {
        int16_t x;
        int16_t sy = (int16_t)(src_area.y + y);
        int16_t dy_abs = (int16_t)(dy + sy);
        for (x = 0; x < src_area.w; ++x) {
            int16_t sx = (int16_t)(src_area.x + x);
            int16_t dx_abs = (int16_t)(dx + sx);
            tgfx_color_t s = tgfx_get_pixel(src, sx, sy);
            if (opacity == 255) {
                tgfx_draw_pixel(dst, dx_abs, dy_abs, s);
            } else {
                tgfx_color_t d = tgfx_get_pixel(dst, dx_abs, dy_abs);
                tgfx_draw_pixel(dst, dx_abs, dy_abs, blend_over(d, s, opacity));
            }
        }
    }
}

static void render_node(tgfx_context_t *ctx, tgfx_node_t *node,
                        tgfx_canvas_t *target, int16_t ox, int16_t oy);

static void render_children(tgfx_context_t *ctx, tgfx_node_t *node,
                            tgfx_canvas_t *target, int16_t ox, int16_t oy)
{
    tgfx_handle_t child;
    if (!ctx || !node || !target) {
        return;
    }
    child = node->first_child;
    while (child != TGFX_INVALID_HANDLE) {
        tgfx_node_t *child_node = find_node(ctx, child);
        if (!child_node) {
            break;
        }
        render_node(ctx, child_node, target, ox, oy);
        child = child_node->next_sibling;
    }
}

static void draw_dirty_node_callback(tgfx_context_t *ctx, tgfx_node_t *node)
{
    tgfx_rect_t dirty;
    tgfx_rect_t saved_clip;
    if (!ctx || !node || !node->owns_canvas || !node->back_canvas || !node->draw ||
        !node->dirty) {
        return;
    }
    dirty = rect_empty(node->dirty_rect) ? node_full_dirty_rect(node) : node->dirty_rect;
    tgfx_canvas_get_clip(node->back_canvas, &saved_clip);
    tgfx_canvas_reset_clip(node->back_canvas);
    tgfx_canvas_set_clip(node->back_canvas, dirty);
    tgfx_clear(node->back_canvas, 0);
    node->draw(ctx, node->handle, node->back_canvas, &dirty, node->user);
    tgfx_canvas_set_clip(node->back_canvas, saved_clip);
    (void)commit_node_rect_locked(ctx, node, dirty);
    node->dirty = 0;
    node->dirty_rect = rect_make(0, 0, 0, 0);
}

static void render_node(tgfx_context_t *ctx, tgfx_node_t *node,
                        tgfx_canvas_t *target, int16_t ox, int16_t oy)
{
    tgfx_rect_t dirty;
    tgfx_rect_t saved_target_clip;
    tgfx_rect_t node_clip;
    tgfx_rect_t content_clip;
    int16_t abs_x;
    int16_t abs_y;
    int16_t cdx;
    int16_t cdy;

    if (!node || !node->visible || node->opacity == 0 || !target) {
        return;
    }

    abs_x = (int16_t)(ox + node->display_rect.x);
    abs_y = (int16_t)(oy + node->display_rect.y);
    cdx = content_dx(node);
    cdy = content_dy(node);

    tgfx_canvas_get_clip(target, &saved_target_clip);

    /* The animated display rectangle describes the visible content size.
     * Window decorations are drawn around that animated size, while the
     * backing canvas may already have the final logical size.  Keep content
     * compositing clipped to the animated content rectangle so it cannot
     * overwrite the advancing right/bottom border during resize animation. */
    node_clip = rect_make(abs_x, abs_y, outer_w(node), outer_h(node));
    content_clip = rect_make((int16_t)(abs_x + cdx),
                             (int16_t)(abs_y + cdy),
                             display_content_w(node),
                             display_content_h(node));
    tgfx_canvas_intersect_clip(target, node_clip);
    tgfx_canvas_get_clip(target, &node_clip);

    if (node->owns_canvas) {
        draw_dirty_node_callback(ctx, node);

        tgfx_canvas_intersect_clip(target, content_clip);
        if (node->front_canvas) {
            int16_t visible_w = display_content_w(node);
            int16_t visible_h = display_content_h(node);
            int16_t src_w = node->front_canvas ? (int16_t)node->front_canvas->width : 0;
            int16_t src_h = node->front_canvas ? (int16_t)node->front_canvas->height : 0;
            int16_t content_x = (int16_t)(abs_x + cdx);
            int16_t content_y = (int16_t)(abs_y + cdy);

            /* During shrink animation the backing canvas has already been
             * resized to the target size, while display_rect may still be
             * larger for a few frames.  The area outside the new backing
             * canvas is still visually inside the window and must be filled
             * with the window content background; otherwise the lower layer
             * shows through until the animated display rect catches up. */
            if (visible_w > src_w) {
                fill_rect_over(target, (int16_t)(content_x + src_w), content_y,
                               (int16_t)(visible_w - src_w), visible_h,
                               0, node->opacity);
            }
            if (visible_h > src_h) {
                int16_t bottom_w = src_w < visible_w ? src_w : visible_w;
                fill_rect_over(target, content_x, (int16_t)(content_y + src_h),
                               bottom_w, (int16_t)(visible_h - src_h),
                               0, node->opacity);
            }

            composite_canvas(target, node->front_canvas,
                             content_x, content_y, node->opacity);
        }
        render_children(ctx, node, target,
                        (int16_t)(abs_x + cdx),
                        (int16_t)(abs_y + cdy));

        /* Draw decorations last.  This keeps the border/title visually tied to
         * the current animated display rect instead of being hidden under the
         * already-resized content canvas. */
        tgfx_canvas_set_clip(target, node_clip);
        draw_node_decoration(target, node, abs_x, abs_y);

        node->committed_dirty = 0;
        node->committed_rect = rect_make(0, 0, 0, 0);
    } else {
        tgfx_canvas_intersect_clip(target, content_clip);
        dirty = node->dirty ? node->dirty_rect : node_full_dirty_rect(node);
        if (rect_empty(dirty)) {
            dirty = node_full_dirty_rect(node);
        }
        if (node->draw && node->dirty) {
            node->draw(ctx, node->handle, target, &dirty, node->user);
        }
        render_children(ctx, node, target,
                        (int16_t)(abs_x + cdx),
                        (int16_t)(abs_y + cdy));
        tgfx_canvas_set_clip(target, node_clip);
        draw_node_decoration(target, node, abs_x, abs_y);
        node->dirty = 0;
        node->dirty_rect = rect_make(0, 0, 0, 0);
    }

    tgfx_canvas_set_clip(target, saved_target_clip);
}

tgfx_result_t tgfx_render(tgfx_context_t *ctx, tgfx_canvas_t *target)
{
    tgfx_handle_t child;
    tgfx_rect_t saved_clip;
    if (!ctx || !target || !target->pixels || ctx->node_count == 0) {
        return TGFX_ERR_INVALID_ARG;
    }
    ctx_lock(ctx);
    tgfx_canvas_get_clip(target, &saved_clip);
    tgfx_canvas_reset_clip(target);
    tgfx_clear(target, 0);
    child = ctx->nodes[0].first_child;
    while (child != TGFX_INVALID_HANDLE) {
        tgfx_node_t *node = find_node(ctx, child);
        if (!node) {
            break;
        }
        render_node(ctx, node, target, 0, 0);
        child = node->next_sibling;
    }
    tgfx_canvas_set_clip(target, saved_clip);
    ctx->scene_dirty = 0;
    ctx_unlock(ctx);
    return TGFX_OK;
}

tgfx_result_t tgfx_context_attach_port(tgfx_context_t *ctx, tgfx_port_t *port)
{
    if (!ctx || !port || !port->target.pixels) {
        return TGFX_ERR_INVALID_ARG;
    }
    ctx->port = port;
    return TGFX_OK;
}

tgfx_port_t *tgfx_context_port(tgfx_context_t *ctx)
{
    return ctx ? ctx->port : NULL;
}

void tgfx_context_lock(tgfx_context_t *ctx)
{
    ctx_lock(ctx);
}

void tgfx_context_unlock(tgfx_context_t *ctx)
{
    ctx_unlock(ctx);
}

tgfx_result_t tgfx_daemon_step(tgfx_context_t *ctx)
{
    tgfx_result_t res;
    tgfx_rect_t dirty;
    if (!ctx || !ctx->port || !ctx->port->target.pixels) {
        return TGFX_ERR_INVALID_ARG;
    }
    dirty = rect_make(0, 0, (int16_t)ctx->width, (int16_t)ctx->height);
    res = tgfx_render(ctx, &ctx->port->target);
    if (res != TGFX_OK) {
        return res;
    }
    return ctx->port->present(ctx->port->target, &dirty);
}

void tgfx_daemon_run(tgfx_context_t *ctx)
{
    uint32_t last_ms;
    if (!ctx || !ctx->port) {
        return;
    }
    ctx->daemon_running = 1;
    last_ms = monotonic_ms();
    while (ctx->daemon_running) {
        uint32_t now_ms = monotonic_ms();
        uint32_t delta_ms = now_ms - last_ms;
        last_ms = now_ms;

        if (tgfx_animating(ctx) && delta_ms > 0) {
            tgfx_tick(ctx, delta_ms);
        }

        /* Drain pending render work before going to sleep.  This makes the
         * daemon robust against notifications that arrive before the thread
         * has reached waitCommit(), and lets animations keep advancing without
         * requiring new layer commits. */
        if (tgfx_needs_render(ctx)) {
            (void)tgfx_daemon_step(ctx);
            continue;
        }

        if (tgfx_animating(ctx)) {
            if (!ctx->port->waitCommitTimeout(16)) {
                break;
            }
        } else {
            if (!ctx->port->waitCommit()) {
                break;
            }
            /* A new commit may start a transition after the daemon has been
             * sleeping for an arbitrary amount of time.  Do not charge that
             * idle time to the newly-created animation, otherwise the first
             * tick can jump straight to the end position. */
            last_ms = monotonic_ms();
        }
    }
}

tgfx_result_t tgfx_daemon_start(tgfx_context_t *ctx)
{
    if (!ctx || !ctx->port) {
        return TGFX_ERR_INVALID_ARG;
    }
    ctx->daemon_running = 1;
    tgfx_result_t result = ctx->port->startDaemon((void (*)(void *))tgfx_daemon_run, ctx);
    if (result != TGFX_OK) {
        ctx->daemon_running = 0;
    }
    return result;
}

void tgfx_daemon_stop(tgfx_context_t *ctx)
{
    if (!ctx || !ctx->port) {
        return;
    }
    ctx->daemon_running = 0;
    ctx->port->stopDaemon();
    ctx->port->notifyCommit();
}

void tgfx_daemon_notify(tgfx_context_t *ctx)
{
    ctx_notify(ctx);
}

#if THINGFX_ENABLE_POINTER_EVENTS
static uint8_t point_in_rect(int16_t x, int16_t y, int16_t rx, int16_t ry,
                             int16_t rw, int16_t rh)
{
    return rw > 0 && rh > 0 && x >= rx && y >= ry &&
           x < (int16_t)(rx + rw) && y < (int16_t)(ry + rh);
}

static tgfx_handle_t hit_test_node(tgfx_context_t *ctx, tgfx_node_t *node,
                                   int16_t x, int16_t y, int16_t ox,
                                   int16_t oy, int16_t *out_x,
                                   int16_t *out_y, uint8_t need_handler)
{
    tgfx_handle_t child;
    tgfx_handle_t hit = TGFX_INVALID_HANDLE;
    int16_t abs_x;
    int16_t abs_y;

    if (!node || !node->visible || node->opacity == 0) {
        return TGFX_INVALID_HANDLE;
    }

    abs_x = (int16_t)(ox + node->display_rect.x);
    abs_y = (int16_t)(oy + node->display_rect.y);
    if (!point_in_rect(x, y, abs_x, abs_y, outer_w(node), outer_h(node))) {
        return TGFX_INVALID_HANDLE;
    }

    child = node->first_child;
    while (child != TGFX_INVALID_HANDLE) {
        tgfx_node_t *child_node = find_node(ctx, child);
        tgfx_handle_t child_hit;
        if (!child_node) {
            break;
        }
        child_hit = hit_test_node(ctx, child_node, x, y,
                                  (int16_t)(abs_x + content_dx(node)),
                                  (int16_t)(abs_y + content_dy(node)),
                                  out_x, out_y, need_handler);
        if (child_hit != TGFX_INVALID_HANDLE) {
            hit = child_hit;
        }
        child = child_node->next_sibling;
    }

    if (hit != TGFX_INVALID_HANDLE) {
        return hit;
    }
    if (!need_handler || node->event) {
        if (out_x) {
            *out_x = (int16_t)(x - abs_x - content_dx(node));
        }
        if (out_y) {
            *out_y = (int16_t)(y - abs_y - content_dy(node));
        }
        return node->handle;
    }
    return TGFX_INVALID_HANDLE;
}

tgfx_handle_t tgfx_hit_test(tgfx_context_t *ctx, int16_t x, int16_t y)
{
    int16_t local_x;
    int16_t local_y;
    if (!ctx || ctx->node_count == 0) {
        return TGFX_INVALID_HANDLE;
    }
    return hit_test_node(ctx, &ctx->nodes[0], x, y, 0, 0, &local_x, &local_y, 0);
}
#endif

static uint8_t node_can_receive_event(const tgfx_node_t *node)
{
    return node && node->visible && node->opacity != 0 && node->event;
}

static tgfx_handle_t top_event_node(tgfx_context_t *ctx, tgfx_node_t *node)
{
    tgfx_handle_t child;
    tgfx_handle_t hit = TGFX_INVALID_HANDLE;
    if (!node || !node->visible || node->opacity == 0) {
        return TGFX_INVALID_HANDLE;
    }
    child = node->first_child;
    while (child != TGFX_INVALID_HANDLE) {
        tgfx_node_t *child_node = find_node(ctx, child);
        tgfx_handle_t child_hit;
        if (!child_node) {
            break;
        }
        child_hit = top_event_node(ctx, child_node);
        if (child_hit != TGFX_INVALID_HANDLE) {
            hit = child_hit;
        }
        child = child_node->next_sibling;
    }
    if (hit != TGFX_INVALID_HANDLE) {
        return hit;
    }
    return node->event ? node->handle : TGFX_INVALID_HANDLE;
}

static tgfx_node_t *focused_event_node(tgfx_context_t *ctx)
{
    tgfx_node_t *node = find_node(ctx, ctx ? ctx->focus : TGFX_INVALID_HANDLE);
    return node_can_receive_event(node) ? node : NULL;
}

tgfx_result_t tgfx_focus_set(tgfx_context_t *ctx, tgfx_handle_t handle)
{
    if (!ctx) {
        return TGFX_ERR_INVALID_ARG;
    }
    if (handle != TGFX_INVALID_HANDLE && !find_node(ctx, handle)) {
        return TGFX_ERR_NOT_FOUND;
    }
    ctx->focus = handle;
    return TGFX_OK;
}

tgfx_handle_t tgfx_focus_get(tgfx_context_t *ctx)
{
    return (ctx && find_node(ctx, ctx->focus)) ? ctx->focus : TGFX_INVALID_HANDLE;
}

static tgfx_handle_t focus_step(tgfx_context_t *ctx, int direction)
{
    size_t i;
    size_t start = 0;
    if (!ctx || ctx->node_count == 0) {
        return TGFX_INVALID_HANDLE;
    }
    for (i = 0; i < ctx->node_count; ++i) {
        if (ctx->nodes[i].handle == ctx->focus) {
            start = i;
            break;
        }
    }
    for (i = 1; i <= ctx->node_count; ++i) {
        size_t idx;
        if (direction >= 0) {
            idx = (start + i) % ctx->node_count;
        } else {
            idx = (start + ctx->node_count - (i % ctx->node_count)) % ctx->node_count;
        }
        if (node_can_receive_event(&ctx->nodes[idx])) {
            ctx->focus = ctx->nodes[idx].handle;
            return ctx->focus;
        }
    }
    return TGFX_INVALID_HANDLE;
}

tgfx_handle_t tgfx_focus_next(tgfx_context_t *ctx)
{
    return focus_step(ctx, 1);
}

tgfx_handle_t tgfx_focus_prev(tgfx_context_t *ctx)
{
    return focus_step(ctx, -1);
}

uint8_t tgfx_dispatch_to(tgfx_context_t *ctx, tgfx_handle_t handle,
                         const tgfx_event_t *event)
{
    tgfx_node_t *node = find_node(ctx, handle);
    if (!ctx || !event || !node || !node_can_receive_event(node)) {
        return 0;
    }
    return node->event(ctx, handle, event, node->user);
}

#if THINGFX_ENABLE_POINTER_EVENTS
uint8_t tgfx_dispatch_pointer(tgfx_context_t *ctx, tgfx_event_type_t type,
                              int16_t x, int16_t y, uint8_t button,
                              tgfx_handle_t *out_target)
{
    tgfx_event_t event;
    tgfx_handle_t handle;
    tgfx_node_t *node;
    if (out_target) {
        *out_target = TGFX_INVALID_HANDLE;
    }
    if (!ctx || ctx->node_count == 0 ||
        (type != TGFX_EVENT_POINTER_DOWN && type != TGFX_EVENT_POINTER_UP &&
         type != TGFX_EVENT_POINTER_MOVE)) {
        return 0;
    }
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.x = x;
    event.y = y;
    event.button = button;
    handle = hit_test_node(ctx, &ctx->nodes[0], x, y, 0, 0,
                           &event.local_x, &event.local_y, 1);
    node = find_node(ctx, handle);
    if (!node || !node->event) {
        return 0;
    }
    if (out_target) {
        *out_target = handle;
    }
    return node->event(ctx, handle, &event, node->user);
}
#endif

uint8_t tgfx_dispatch_event(tgfx_context_t *ctx, const tgfx_event_t *event,
                            tgfx_handle_t *out_target)
{
    tgfx_handle_t handle = TGFX_INVALID_HANDLE;
    tgfx_node_t *node;

    if (out_target) {
        *out_target = TGFX_INVALID_HANDLE;
    }
    if (!ctx || ctx->node_count == 0 || !event) {
        return 0;
    }

    if (event->type == TGFX_EVENT_KEY_DOWN || event->type == TGFX_EVENT_KEY_UP ||
        event->type == TGFX_EVENT_CANCEL) {
        node = focused_event_node(ctx);
        if (node) {
            handle = node->handle;
        } else {
            handle = top_event_node(ctx, &ctx->nodes[0]);
        }
        node = find_node(ctx, handle);
        if (!node || !node->event) {
            return 0;
        }
        if (out_target) {
            *out_target = handle;
        }
        return node->event(ctx, handle, event, node->user);
    }
#if THINGFX_ENABLE_POINTER_EVENTS
    if (event->type == TGFX_EVENT_POINTER_DOWN ||
        event->type == TGFX_EVENT_POINTER_UP ||
        event->type == TGFX_EVENT_POINTER_MOVE) {
        return tgfx_dispatch_pointer(ctx, event->type, event->x, event->y,
                                     event->button, out_target);
    }
#endif
    return 0;
}

uint8_t tgfx_dispatch_key(tgfx_context_t *ctx, uint32_t key, uint8_t pressed,
                          uint8_t repeat, uint32_t modifiers,
                          tgfx_handle_t *out_target)
{
    tgfx_event_t event;
    memset(&event, 0, sizeof(event));
    event.type = pressed ? TGFX_EVENT_KEY_DOWN : TGFX_EVENT_KEY_UP;
    event.key = key;
    event.repeat = repeat ? 1u : 0u;
    event.modifiers = modifiers;
    return tgfx_dispatch_event(ctx, &event, out_target);
}

static tgfx_result_t reserve_anims(tgfx_context_t *ctx, size_t count)
{
    tgfx_anim_t *anims;
    size_t capacity;
    if (ctx->anim_capacity >= count) {
        return TGFX_OK;
    }
    capacity = ctx->anim_capacity ? ctx->anim_capacity * 2u : 4u;
    while (capacity < count) {
        capacity *= 2u;
    }
    anims = (tgfx_anim_t *)realloc(ctx->anims, capacity * sizeof(*anims));
    if (!anims) {
        return TGFX_ERR_NO_MEMORY;
    }
    ctx->anims = anims;
    ctx->anim_capacity = capacity;
    return TGFX_OK;
}

tgfx_result_t tgfx_anim_add(tgfx_context_t *ctx, const tgfx_anim_desc_t *desc)
{
    tgfx_result_t res;
    tgfx_anim_t *anim;
    if (!ctx || !desc || !find_node(ctx, desc->handle) || desc->duration_ms == 0) {
        return TGFX_ERR_INVALID_ARG;
    }
    res = reserve_anims(ctx, ctx->anim_count + 1u);
    if (res != TGFX_OK) {
        return res;
    }
    anim = &ctx->anims[ctx->anim_count++];
    anim->handle = desc->handle;
    anim->property = desc->property;
    anim->from = desc->from;
    anim->to = desc->to;
    anim->duration_ms = desc->duration_ms;
    anim->elapsed_ms = 0;
    anim->curve = desc->curve;
    anim->active = 1;
    return TGFX_OK;
}

static uint32_t easing_factor(tgfx_easing_t easing, uint32_t elapsed_ms,
                              uint32_t duration_ms)
{
    uint32_t t;
    if (duration_ms == 0 || elapsed_ms >= duration_ms) {
        return 1024u;
    }
    t = (elapsed_ms * 1024u) / duration_ms;
    switch (sanitize_easing(easing)) {
    case TGFX_EASING_NONE:
    case TGFX_EASING_LINEAR:
        return t;
    case TGFX_EASING_EASE_IN:
        return (t * t) / 1024u;
    case TGFX_EASING_EASE_OUT: {
        uint32_t u = 1024u - t;
        return 1024u - ((u * u) / 1024u);
    }
    case TGFX_EASING_EASE_IN_OUT:
        if (t < 512u) {
            return (2u * t * t) / 1024u;
        } else {
            uint32_t u = 1024u - t;
            return 1024u - ((2u * u * u) / 1024u);
        }
    case TGFX_EASING_SMOOTH_STEP:
        return (t * t * (3072u - 2u * t)) / (1024u * 1024u);
    default:
        return t;
    }
}

static int32_t ease_value(const tgfx_anim_t *anim)
{
    uint32_t k;
    int32_t delta;
    tgfx_easing_t easing = anim->curve == TGFX_ANIM_EASE_OUT ?
                               TGFX_EASING_EASE_OUT : TGFX_EASING_LINEAR;
    k = easing_factor(easing, anim->elapsed_ms, anim->duration_ms);
    delta = anim->to - anim->from;
    return anim->from + (int32_t)((delta * (int32_t)k) / 1024);
}

void tgfx_tick(tgfx_context_t *ctx, uint32_t delta_ms)
{
    size_t i;
    if (!ctx || delta_ms == 0) {
        return;
    }
    ctx_lock(ctx);
    for (i = 0; i < ctx->node_count; ++i) {
        tgfx_node_t *node = &ctx->nodes[i];
        uint64_t scaled;
        uint32_t step_ms;
        uint32_t k;
        if (!node->rect_anim_active) {
            continue;
        }
        scaled = (uint64_t)delta_ms * (uint64_t)sanitize_speed(node->rect_anim_speed_per_mille);
        step_ms = (uint32_t)(scaled / 1000u);
        if (step_ms == 0) {
            step_ms = 1;
        }
        if (node->rect_anim_elapsed_ms + step_ms >= node->rect_anim_duration_ms) {
            node->rect_anim_elapsed_ms = node->rect_anim_duration_ms;
            node->display_rect = node->anim_target_rect;
            node->rect_anim_active = 0;
            if (node->rect_anim_destroy_on_finish) {
                node->destroy_pending = 1;
            }
        } else {
            node->rect_anim_elapsed_ms += step_ms;
            k = easing_factor(node->rect_anim_easing,
                              node->rect_anim_elapsed_ms,
                              node->rect_anim_duration_ms);
            node->display_rect.x = (int16_t)lerp_i32(node->anim_start_rect.x,
                                                     node->anim_target_rect.x, k);
            node->display_rect.y = (int16_t)lerp_i32(node->anim_start_rect.y,
                                                     node->anim_target_rect.y, k);
            node->display_rect.w = (int16_t)lerp_i32(node->anim_start_rect.w,
                                                     node->anim_target_rect.w, k);
            node->display_rect.h = (int16_t)lerp_i32(node->anim_start_rect.h,
                                                     node->anim_target_rect.h, k);
        }
        ctx->scene_dirty = 1;
    }

    for (i = 0; i < ctx->anim_count; ++i) {
        tgfx_anim_t *anim = &ctx->anims[i];
        tgfx_node_t *node = find_node(ctx, anim->handle);
        int32_t value;
        if (!anim->active || !node) {
            continue;
        }
        anim->elapsed_ms += delta_ms;
        if (anim->elapsed_ms >= anim->duration_ms) {
            anim->elapsed_ms = anim->duration_ms;
            anim->active = 0;
        }
        value = ease_value(anim);
        if (anim->property == TGFX_ANIM_X) {
            node->rect.x = (int16_t)value;
            node->display_rect.x = (int16_t)value;
        } else if (anim->property == TGFX_ANIM_Y) {
            node->rect.y = (int16_t)value;
            node->display_rect.y = (int16_t)value;
        } else if (anim->property == TGFX_ANIM_OPACITY) {
            node->opacity = value < 0 ? 0 : (value > 255 ? 255 : (uint8_t)value);
        }
        ctx->scene_dirty = 1;
    }

    for (i = 0; i < ctx->node_count; ) {
        tgfx_node_t *node = &ctx->nodes[i];
        if (node->destroy_pending && !node->rect_anim_active &&
            node->parent != TGFX_INVALID_HANDLE) {
            tgfx_handle_t handle = node->handle;
            (void)tgfx_node_destroy_immediate(ctx, handle);
            ctx->scene_dirty = 1;
            i = 0;
            continue;
        }
        ++i;
    }
    ctx_unlock(ctx);
}

uint8_t tgfx_animating(const tgfx_context_t *ctx)
{
    size_t i;
    if (!ctx) {
        return 0;
    }
    for (i = 0; i < ctx->node_count; ++i) {
        if (ctx->nodes[i].rect_anim_active) {
            return 1;
        }
    }
    for (i = 0; i < ctx->anim_count; ++i) {
        if (ctx->anims[i].active) {
            return 1;
        }
    }
    return 0;
}

} // namespace detail
} // namespace thingfx
