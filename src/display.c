#include <stdlib.h>

#include "input_internal.h"
#include "layout_internal.h"
#include "render_internal.h"
#include "selection_internal.h"

static bool overlaps(rd_rect a, rd_rect b) {
  return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

static bool contains(rd_rect r, float x, float y) {
  return x >= r.x && y >= r.y && x < r.x + r.width && y < r.y + r.height;
}

static rd_rect padding_box(rd_rect r, const rd_node_layout* L) {
  return (rd_rect){
      r.x + L->border[EDGE_L],
      r.y + L->border[EDGE_T],
      fmaxf(0, r.width - L->border[EDGE_L] - L->border[EDGE_R]),
      fmaxf(0, r.height - L->border[EDGE_T] - L->border[EDGE_B]),
  };
}

// Clip for a node's children: its padding box. As in CSS, a visible axis next to a non-visible one
// behaves as auto, so either axis clipping clips both.
static rd_rect child_clip(const rd_style* st, rd_rect pad, rd_rect clip, bool* clips) {
  bool    cx = st->overflow_x != RD_OVERFLOW_VISIBLE || st->overflow_y != RD_OVERFLOW_VISIBLE, cy = cx;
  rd_rect c = clip;
  if (cx) {
    float x0 = fmaxf(c.x, pad.x), x1 = fminf(c.x + c.width, pad.x + pad.width);
    c.x     = x0;
    c.width = fmaxf(0, x1 - x0);
  }
  if (cy) {
    float y0 = fmaxf(c.y, pad.y), y1 = fminf(c.y + c.height, pad.y + pad.height);
    c.y      = y0;
    c.height = fmaxf(0, y1 - y0);
  }
  *clips = cx || cy;
  return c;
}

static void corner_radii(const rd_style* st, float w, float h, float out[4]) {
  const rd_val* v[4] = {&st->border_radius.top_left, &st->border_radius.top_right, &st->border_radius.bottom_right,
                        &st->border_radius.bottom_left};
  float         ref  = fminf(w, h);
  for (int j = 0; j < 4; j++) out[j] = fmaxf(0, resolve0(*v[j], ref));
  float f       = 1;
  float sum[4]  = {out[0] + out[1], out[1] + out[2], out[2] + out[3], out[3] + out[0]};
  float side[4] = {w, h, w, h};
  for (int j = 0; j < 4; j++)
    if (sum[j] > side[j] && sum[j] > 0) f = fminf(f, side[j] / sum[j]);
  if (f < 1)
    for (int j = 0; j < 4; j++) out[j] *= f;
}

static rd_color fade(rd_color c, float opacity) {
  c.a = (uint8_t)lroundf((float)c.a * opacity);
  return c;
}

// Never null: on allocation failure the command lands in a sink and is dropped.
static rd__cmd* push(rd_ctx* ctx, uint8_t kind, uint32_t node) {
  static rd__cmd sink;
  if (ctx->cmd_count == ctx->cmd_cap) {
    uint32_t cap = ctx->cmd_cap ? ctx->cmd_cap * 2 : 256;
    rd__cmd* p   = realloc(ctx->cmds, cap * sizeof *p);
    if (!p) return &sink;
    ctx->cmds    = p;
    ctx->cmd_cap = cap;
  }
  rd__cmd* c = &ctx->cmds[ctx->cmd_count++];
  *c         = (rd__cmd){.kind = kind, .node = node};
  return c;
}

static void fill(rd_ctx* ctx, uint32_t i, rd_rect r, rd_color color) {
  rd__cmd* c = push(ctx, RD_CMD_FILL, i);
  c->rect    = r;
  c->fill    = color;
}

// Backgrounds for a byte range across the lines it spans, within the visible lines and the clip. A
// range that continues past a line end extends half a space beyond it.
static void fill_range(rd_ctx* ctx, uint32_t i, const rd_style* st, const rd__text_layout* tl, rd_rect content,
                       rd_rect clip, const rd_text* t, size_t lo, size_t hi, float dx, rd_color color) {
  if (lo >= hi || !color.a || !tl->line_count) return;
  uint32_t a = rd__text_cp_of_byte(t, lo), b = rd__text_cp_of_byte(t, hi);
  uint32_t la = rd__text_line_of(tl, a), lb = rd__text_line_of(tl, b);
  uint32_t shown = rd__text_visible_lines(st, tl);
  bool     cut   = st->text_overflow == RD_TEXT_OVERFLOW_ELLIPSIS;
  uint32_t first = la, last = lb;  // of those, the lines that meet the clip
  if (tl->line_height > 0) {
    float top = floorf((clip.y - content.y) / tl->line_height);
    float end = ceilf((clip.y + clip.height - content.y) / tl->line_height);
    if (end <= (float)la || top > (float)lb) return;
    if (top > (float)la) first = (uint32_t)top;
    if (end - 1 < (float)lb) last = (uint32_t)(end - 1);
  }
  for (uint32_t l = first; l <= last && l < shown; l++) {
    float x0  = l == la ? rd__text_cp_x(tl, l, a) : 0;
    float x1  = l == lb ? rd__text_cp_x(tl, l, b) : rd__text_cp_x(tl, l, tl->lines[l].cp_end) + tl->space_width / 2;
    float off = rd__text_align_offset(st, content.width, tl->lines[l].width);
    if (cut) x1 = fminf(x1, content.width - off);
    if (x1 <= x0) continue;
    fill(ctx, i, (rd_rect){content.x + off + x0 + dx, content.y + (float)l * tl->line_height, x1 - x0, tl->line_height},
         color);
  }
}

// Highlight backgrounds, clamped to the text.
static void build_highlights(rd_ctx* ctx, uint32_t i, const rd_style* st, rd_rect content, rd_rect clip, float op) {
  const rd_text*  t = rd__text_get(ctx, i);
  rd__text_layout tl;
  if (!t || !t->highlight_count || !rd__text_layout_get(ctx, i, content.width, &tl)) return;
  for (size_t j = 0; j < t->highlight_count; j++) {
    const rd_highlight* h = &t->highlights[j];
    fill_range(ctx, i, st, &tl, content, clip, t, h->start < t->len ? h->start : t->len, h->end < t->len ? h->end : t->len,
               0, fade(h->background, op));
  }
}

// Selection behind the text, then the text or placeholder, then the caret. Single-line inputs
// clip to their content box and scroll to keep the caret visible.
static void build_input(rd_ctx* ctx, uint32_t i, const rd_style* st, rd_rect content, rd_rect clip, float op,
                        const rd__input_view* v) {
  rd__text_layout tl;
  if (!rd__text_layout_get(ctx, i, content.width, &tl)) return;
  rd_text* t       = rd__text_get(ctx, i);
  bool     focused = rd_node_eq(rd_ctx_focused(ctx), rd__handle(ctx, i));
  float    sx      = v->multiline ? 0 : rd__input_sync_scroll(ctx, i, &tl, content.width);
  if (!v->multiline) {
    float x0 = fmaxf(clip.x, content.x), x1 = fminf(clip.x + clip.width, content.x + content.width);
    push(ctx, RD_CMD_CLIP, i)->rect = (rd_rect){x0, clip.y, fmaxf(0, x1 - x0), clip.height};
  }

  size_t lo = v->anchor < v->caret ? v->anchor : v->caret, hi = v->anchor < v->caret ? v->caret : v->anchor;
  if (focused && lo != hi) fill_range(ctx, i, st, &tl, content, clip, t, lo, hi, -sx, fade(st->selection_color, op));

  if (t->len || v->placeholder_len) {
    rd__cmd* c = push(ctx, t->len ? RD_CMD_TEXT : RD_CMD_PLACEHOLDER, i);
    c->rect    = content;
    c->clip    = clip;
    c->dx      = -sx;
    c->fill    = fade(st->color, t->len ? op : op * 0.45f);
  }

  if (focused) {
    uint32_t cp   = rd__text_cp_of_byte(t, v->caret);
    uint32_t line = rd__text_line_of(&tl, cp);
    float    ascent, descent, gap;
    rd__font_vmetrics(tl.font, tl.size, &ascent, &descent, &gap);
    float x = content.x + rd__text_align_offset(st, content.width, tl.lines[line].width) + rd__text_cp_x(&tl, line, cp) - sx;
    float y = content.y + (float)line * tl.line_height + (tl.line_height - ascent - descent) / 2;
    fill(ctx, i, (rd_rect){x, y, 1.5f, ascent + descent}, fade(st->caret_color.a ? st->caret_color : st->color, op));
  }
  if (!v->multiline) push(ctx, RD_CMD_CLIP, i)->rect = clip;
}

static void build_scrollbars(rd_ctx* ctx, uint32_t i, const rd_style* st, rd_rect r, float op);

// Places an image in the content box by object_fit: scaled about the center, then cropped to the box.
static void build_image(rd_ctx* ctx, uint32_t i, const rd_style* st, const rd_node_layout* L, rd_rect box,
                        const float radii[4], float op, uint32_t id) {
  const rd__image* im = rd__image_get(ctx, id);
  if (!im || im->natural_w <= 0 || im->natural_h <= 0 || box.width <= 0 || box.height <= 0) return;
  float iw = im->natural_w, ih = im->natural_h;
  float fit = fminf(box.width / iw, box.height / ih), sx = 1, sy = 1;
  switch (st->object_fit) {
    case RD_OBJECT_FIT_CONTAIN:
      sx = sy = fit;
      break;
    case RD_OBJECT_FIT_COVER:
      sx = sy = fmaxf(box.width / iw, box.height / ih);
      break;
    case RD_OBJECT_FIT_NONE:
      break;
    case RD_OBJECT_FIT_SCALE_DOWN:
      sx = sy = fminf(1, fit);
      break;
    default:
      sx = box.width / iw;
      sy = box.height / ih;
      break;
  }
  rd_rect full = {box.x + (box.width - iw * sx) / 2, box.y + (box.height - ih * sy) / 2, iw * sx, ih * sy};
  float   x0 = fmaxf(full.x, box.x), y0 = fmaxf(full.y, box.y);
  float   x1 = fminf(full.x + full.width, box.x + box.width), y1 = fminf(full.y + full.height, box.y + box.height);
  if (x1 <= x0 || y1 <= y0) return;
  rd__cmd* c = push(ctx, RD_CMD_IMAGE, i);
  c->rect    = box;
  c->image   = id;
  c->dest    = (rd_rect){x0, y0, x1 - x0, y1 - y0};
  c->src     = (rd_rect){(x0 - full.x) / sx, (y0 - full.y) / sy, (x1 - x0) / sx, (y1 - y0) / sy};
  c->fill    = fade(im->current_color ? st->color : rd_rgba(255, 255, 255, 255), op);  // currentColor SVGs are tinted
  // The content box's corners: the border radius less the border and padding on the adjacent sides.
  const int sides[4][2] = {{EDGE_T, EDGE_L}, {EDGE_T, EDGE_R}, {EDGE_B, EDGE_R}, {EDGE_B, EDGE_L}};
  for (int j = 0; j < 4; j++) {
    float in = fmaxf(L->border[sides[j][0]] + L->padding[sides[j][0]], L->border[sides[j][1]] + L->padding[sides[j][1]]);
    c->radii[j] = fmaxf(0, radii[j] - in);
  }
}

static void build(rd_ctx* ctx, uint32_t i, float ox, float oy, float opacity, rd_rect clip) {
  const rd_style*       st = &ctx->styles[i].computed;
  const rd_node_layout* L  = &ctx->layouts[i];
  if (st->display == RD_DISPLAY_NONE) return;
  float op = opacity * clampf(st->opacity.unit == RD_UNIT_NUM ? st->opacity.value : 1, 0, 1);
  if (op <= 0) return;

  rd_rect r = {ox + L->rect.x, oy + L->rect.y, L->rect.width, L->rect.height};
  float   radii[4];
  corner_radii(st, r.width, r.height, radii);

  const rd_shadow* sh = &st->box_shadow;
  if (sh->color.a) {
    float   spread = resolve0(sh->spread, NAN), blur = fmaxf(0, resolve0(sh->blur, NAN));
    rd_rect box   = {r.x + resolve0(sh->x, NAN) - spread, r.y + resolve0(sh->y, NAN) - spread, r.width + 2 * spread,
                     r.height + 2 * spread};
    rd_rect reach = {box.x - blur, box.y - blur, box.width + 2 * blur, box.height + 2 * blur};
    if (box.width > 0 && box.height > 0 && overlaps(reach, clip)) {
      rd__cmd* c = push(ctx, RD_CMD_SHADOW, i);
      c->rect    = box;
      c->fill    = fade(sh->color, op);
      c->blur    = blur;
      for (int j = 0; j < 4; j++) c->radii[j] = radii[j] > 0 ? radii[j] + spread : 0;
    }
  }

  bool visible = overlaps(r, clip);
  bool border  = (L->border[0] > 0 || L->border[1] > 0 || L->border[2] > 0 || L->border[3] > 0) && st->border_color.a;
  if (visible && (st->background.a || border)) {
    rd__cmd* c = push(ctx, RD_CMD_RECT, i);
    c->rect    = r;
    c->fill    = fade(st->background, op);
    c->stroke  = fade(st->border_color, op);
    for (int j = 0; j < 4; j++) {
      c->radii[j]  = radii[j];
      c->border[j] = border ? L->border[j] : 0;
    }
  }

  rd_rect        pad     = padding_box(r, L);
  rd_rect        content = {pad.x + L->padding[EDGE_L], pad.y + L->padding[EDGE_T],
                            fmaxf(0, pad.width - L->padding[EDGE_L] - L->padding[EDGE_R]),
                            fmaxf(0, pad.height - L->padding[EDGE_T] - L->padding[EDGE_B])};
  rd__input_view view;
  uint32_t       image = rd__node_image(ctx, i);
  if (visible && image) {
    build_image(ctx, i, st, L, content, radii, op, image);
  } else if (visible && rd__input_view_get(ctx, i, &view)) {
    build_input(ctx, i, st, content, clip, op, &view);
  } else if (visible && rd__is_text(ctx, i) && st->color.a) {
    build_highlights(ctx, i, st, content, clip, op);
    size_t lo, hi;
    if (rd__selection_range(ctx, i, &lo, &hi)) {
      rd__text_layout tl;
      if (rd__text_layout_get(ctx, i, content.width, &tl))
        fill_range(ctx, i, st, &tl, content, clip, rd__text_get(ctx, i), lo, hi, 0, fade(st->selection_color, op));
    }
    rd__cmd* c = push(ctx, RD_CMD_TEXT, i);
    c->rect    = content;
    c->clip    = clip;
    c->fill    = fade(st->color, op);
  }
  if (L->paint_fn) {
    float x0 = fmaxf(content.x, clip.x), x1 = fminf(content.x + content.width, clip.x + clip.width);
    float y0 = fmaxf(content.y, clip.y), y1 = fminf(content.y + content.height, clip.y + clip.height);
    if (x1 > x0 && y1 > y0) {
      rd__cmd* c = push(ctx, RD_CMD_PAINT, i);
      c->rect    = content;
      c->clip    = clip;
    }
  }
  if (L->measure_fn) return;

  bool    clips;
  rd_rect inner = child_clip(st, pad, clip, &clips);
  if (clips) {
    if (inner.width <= 0 || inner.height <= 0) return;
    push(ctx, RD_CMD_CLIP, i)->rect = inner;
  }
  for (uint32_t ch = ctx->slots[i].first_child; ch; ch = ctx->slots[ch].next_sibling)
    build(ctx, ch, r.x - L->scroll[0], r.y - L->scroll[1], op, inner);
  if (clips) push(ctx, RD_CMD_CLIP, i)->rect = clip;
  if (visible) build_scrollbars(ctx, i, st, r, op);
}

const rd__cmd* rd__display_list(rd_ctx* ctx, uint32_t* count) {
  ctx->cmd_count = 0;
  rd__selection_prepare(ctx);
  const rd_rect root = ctx->layouts[ctx->root].rect;
  build(ctx, ctx->root, -root.x, -root.y, 1, (rd_rect){0, 0, root.width, root.height});
  *count = ctx->cmd_count;
  return ctx->cmds;
}

static uint32_t hit(const rd_ctx* ctx, uint32_t i, float ox, float oy, rd_rect clip, float x, float y) {
  const rd_style*       st = &ctx->styles[i].computed;
  const rd_node_layout* L  = &ctx->layouts[i];
  if (st->display == RD_DISPLAY_NONE) return 0;
  rd_rect r = {ox + L->rect.x, oy + L->rect.y, L->rect.width, L->rect.height};
  if (!L->measure_fn) {
    bool    clips;
    rd_rect inner = child_clip(st, padding_box(r, L), clip, &clips);
    if (!clips || contains(inner, x, y)) {
      for (uint32_t ch = ctx->slots[i].last_child; ch; ch = ctx->slots[ch].prev_sibling) {
        uint32_t h = hit(ctx, ch, r.x - L->scroll[0], r.y - L->scroll[1], inner, x, y);
        if (h) return h;
      }
    }
  }
  bool target = st->pointer_events != RD_POINTER_EVENTS_NONE && !(i == ctx->overlay.index && rd__slot(ctx, ctx->overlay));
  return target && contains(r, x, y) && contains(clip, x, y) ? i : 0;
}

uint32_t rd__hit(const rd_ctx* ctx, float x, float y) {
  rd_rect root = ctx->layouts[ctx->root].rect;
  return hit(ctx, ctx->root, -root.x, -root.y, (rd_rect){0, 0, root.width, root.height}, x, y);
}

rd_node rd_node_at(const rd_ctx* ctx, float x, float y) {
  return ctx ? rd__handle(ctx, rd__hit(ctx, x, y)) : (rd_node){0};
}

static void scroll_range(const rd_node_layout* L, float max[2]) {
  max[0] = fmaxf(0, L->content.width - (L->rect.width - L->border[EDGE_L] - L->border[EDGE_R]));
  max[1] = fmaxf(0, L->content.height - (L->rect.height - L->border[EDGE_T] - L->border[EDGE_B]));
}

void rd__clamp_scroll(rd_node_layout* L) {
  float max[2];
  scroll_range(L, max);
  L->scroll[0] = clampf(L->scroll[0], 0, max[0]);
  L->scroll[1] = clampf(L->scroll[1], 0, max[1]);
}

void rd_node_set_scroll(rd_ctx* ctx, rd_node node, float x, float y) {
  if (!rd__slot(ctx, node)) return;
  rd_node_layout* L = &ctx->layouts[node.index];
  L->scroll[0]      = x;
  L->scroll[1]      = y;
  rd__clamp_scroll(L);
  ctx->paint_dirty = true;
}

rd_vec2 rd_node_scroll(const rd_ctx* ctx, rd_node node) {
  if (!rd__slot(ctx, node)) return (rd_vec2){0, 0};
  return (rd_vec2){ctx->layouts[node.index].scroll[0], ctx->layouts[node.index].scroll[1]};
}

// Nearest overflow: scroll container from i up that can move along an axis in delta's direction.
uint32_t rd__scroll_target(const rd_ctx* ctx, uint32_t i, int axis, float delta) {
  for (uint32_t n = i; n && delta != 0; n = ctx->slots[n].parent) {
    const rd_style* st = &ctx->styles[n].computed;
    if ((axis ? st->overflow_y : st->overflow_x) != RD_OVERFLOW_SCROLL) continue;
    const rd_node_layout* L = &ctx->layouts[n];
    float                 max[2];
    scroll_range(L, max);
    if ((delta < 0 && L->scroll[axis] > 0) || (delta > 0 && L->scroll[axis] < max[axis])) return n;
  }
  return 0;
}

bool rd__scroll_by(rd_ctx* ctx, uint32_t i, float dx, float dy) {
  const rd_style* st        = &ctx->styles[i].computed;
  rd_node_layout* L         = &ctx->layouts[i];
  float           before[2] = {L->scroll[0], L->scroll[1]};
  if (st->overflow_x == RD_OVERFLOW_SCROLL) L->scroll[0] += dx;
  if (st->overflow_y == RD_OVERFLOW_SCROLL) L->scroll[1] += dy;
  rd__clamp_scroll(L);
  bool moved = L->scroll[0] != before[0] || L->scroll[1] != before[1];
  if (moved) ctx->paint_dirty = true;
  return moved;
}

void rd__scroll_into_view(rd_ctx* ctx, uint32_t i) {
  for (uint32_t p = ctx->slots[i].parent; p; p = ctx->slots[p].parent) {
    const rd_style* st = &ctx->styles[p].computed;
    if (st->overflow_x != RD_OVERFLOW_SCROLL && st->overflow_y != RD_OVERFLOW_SCROLL) continue;
    rd_node_layout* L      = &ctx->layouts[p];
    rd_rect         n      = rd_node_bounds(ctx, rd__handle(ctx, i));
    rd_rect         box    = padding_box(rd_node_bounds(ctx, rd__handle(ctx, p)), L);
    float           pos[2] = {n.x - box.x, n.y - box.y}, size[2] = {n.width, n.height}, view[2] = {box.width, box.height};
    for (int a = 0; a < 2; a++) {
      if (pos[a] < 0)
        L->scroll[a] += pos[a];
      else if (pos[a] + size[a] > view[a])
        L->scroll[a] += fminf(pos[a] + size[a] - view[a], pos[a]);
    }
    rd__clamp_scroll(L);
    ctx->paint_dirty = true;
  }
}

bool rd_node_set_paint(rd_ctx* ctx, rd_node node, rd_paint_fn fn, void* data, rd_destroy_fn destroy) {
  if (!rd__slot(ctx, node)) return false;
  rd_node_layout* L           = &ctx->layouts[node.index];
  void*           old_data    = L->paint_data;
  rd_destroy_fn   old_destroy = L->paint_destroy;
  L->paint_fn                 = fn;
  L->paint_data               = fn ? data : NULL;
  L->paint_destroy            = fn ? destroy : NULL;
  if (old_destroy && (old_data != L->paint_data || !fn)) old_destroy(old_data);
  ctx->paint_dirty = true;
  return true;
}

// Scrollbars: overlay bars inside the padding box, over the content edge, taking no layout space.

static const float SB_GAP = 2, SB_MIN_THUMB = 24;

bool rd__scrollbar(const rd_ctx* ctx, uint32_t i, rd_rect r, int axis, rd_rect* track, rd_rect* thumb) {
  const rd_style*       st = &ctx->styles[i].computed;
  const rd_node_layout* L  = &ctx->layouts[i];
  if ((axis ? st->overflow_y : st->overflow_x) != RD_OVERFLOW_SCROLL) return false;
  float t = resolve0(st->scrollbar_width, NAN);
  if (t <= 0) return false;
  float max[2];
  scroll_range(L, max);
  if (max[axis] < 0.5f) return false;

  rd_rect pad   = padding_box(r, L);
  bool    other = (axis ? st->overflow_x : st->overflow_y) == RD_OVERFLOW_SCROLL && max[!axis] >= 0.5f;
  float   view  = axis ? pad.height : pad.width;
  float   len   = view - 2 * SB_GAP - (other ? t + SB_GAP : 0);
  if (len <= 0) return false;
  *track = axis ? (rd_rect){pad.x + pad.width - t - SB_GAP, pad.y + SB_GAP, t, len}
                : (rd_rect){pad.x + SB_GAP, pad.y + pad.height - t - SB_GAP, len, t};
  float size = fminf(len, fmaxf(SB_MIN_THUMB, len * view / (view + max[axis])));
  float pos  = (len - size) * clampf(L->scroll[axis] / max[axis], 0, 1);
  *thumb     = axis ? (rd_rect){track->x, track->y + pos, t, size} : (rd_rect){track->x + pos, track->y, size, t};
  return true;
}

float rd__scroll_max(const rd_ctx* ctx, uint32_t i, int axis) {
  float max[2];
  scroll_range(&ctx->layouts[i], max);
  return max[axis];
}

static rd_color brighten(rd_color c) {
  c.a = (uint8_t)fminf(255, (float)c.a * 1.8f);
  return c;
}

static void build_scrollbars(rd_ctx* ctx, uint32_t i, const rd_style* st, rd_rect r, float op) {
  for (int axis = 0; axis < 2; axis++) {
    rd_rect track, thumb;
    if (!rd__scrollbar(ctx, i, r, axis, &track, &thumb)) continue;
    float radius = (axis ? track.width : track.height) / 2;
    if (st->scrollbar_track_color.a) {
      rd__cmd* c = push(ctx, RD_CMD_RECT, i);
      c->rect    = track;
      c->fill    = fade(st->scrollbar_track_color, op);
      for (int j = 0; j < 4; j++) c->radii[j] = radius;
    }
    bool hot = (rd_node_eq(ctx->sb_hot.node, rd__handle(ctx, i)) && ctx->sb_hot.axis == axis) ||
               (ctx->sb_drag.active && ctx->sb_drag.thumb && rd_node_eq(ctx->sb_drag.node, rd__handle(ctx, i)) &&
                ctx->sb_drag.axis == axis);
    rd__cmd* c = push(ctx, RD_CMD_RECT, i);
    c->rect    = thumb;
    c->fill    = fade(hot ? brighten(st->scrollbar_color) : st->scrollbar_color, op);
    for (int j = 0; j < 4; j++) c->radii[j] = radius;
  }
}
