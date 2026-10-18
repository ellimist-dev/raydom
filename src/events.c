#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "input_internal.h"
#include "selection_internal.h"

// Mouse wheels report whole notches; trackpads report fractional deltas of about 10px.
static const float NOTCH_STEP = 48, PRECISE_STEP = 10;
// A wheel gesture stays on one container until it pauses this long or the pointer moves.
static const double LATCH_SECONDS = 0.3;
static const float  LATCH_SLOP    = 8;

bool rd__chain_push(rd__chain* c, rd_node n) {
  if (c->len == c->cap) {
    uint32_t cap = c->cap ? c->cap * 2 : 16;
    rd_node* p   = realloc(c->nodes, cap * sizeof *p);
    if (!p) return false;
    c->nodes = p;
    c->cap   = cap;
  }
  c->nodes[c->len++] = n;
  return true;
}

static bool chain_has(const rd__chain* c, rd_node n) {
  for (uint32_t j = 0; j < c->len; j++)
    if (rd_node_eq(c->nodes[j], n)) return true;
  return false;
}

// Listeners.

rd_listener rd_node_on(rd_ctx* ctx, rd_node node, rd_event_type type, rd_event_fn fn, void* data,
                       rd_destroy_fn destroy, uint8_t flags) {
  if (!rd__slot(ctx, node) || !fn || type >= RD_EVENT_TYPE_COUNT) return (rd_listener){0};
  rd_node_events* e = &ctx->events[node.index];
  if (e->count == e->cap) {
    uint32_t      cap = e->cap ? e->cap * 2 : 2;
    rd__listener* p   = realloc(e->items, cap * sizeof *p);
    if (!p) return (rd_listener){0};
    e->items = p;
    e->cap   = cap;
  }
  if (++ctx->listener_next_id == 0) ctx->listener_next_id = 1;
  e->items[e->count++] = (rd__listener){fn, data, destroy, ctx->listener_next_id, type, flags, false};
  return (rd_listener){node, ctx->listener_next_id};
}

bool rd_node_off(rd_ctx* ctx, rd_listener listener) {
  if (!rd__slot(ctx, listener.node)) return false;
  rd_node_events* e = &ctx->events[listener.node.index];
  for (uint32_t j = 0; j < e->count; j++) {
    rd__listener* l = &e->items[j];
    if (l->id != listener.id || l->removed) continue;
    if (ctx->dispatch_depth) {
      l->removed           = true;
      ctx->listeners_dirty = true;
    } else {
      rd__listener gone = *l;
      memmove(l, l + 1, (e->count - j - 1) * sizeof *l);
      e->count--;
      if (gone.destroy) gone.destroy(gone.data);
    }
    return true;
  }
  return false;
}

static void compact(rd_node_events* e) {
  uint32_t kept = 0;
  for (uint32_t j = 0; j < e->count; j++) {
    rd__listener l = e->items[j];
    if (!l.removed)
      e->items[kept++] = l;
    else if (l.destroy)
      l.destroy(l.data);
  }
  e->count = kept;
}

void rd__events_slot_free(rd_ctx* ctx, uint32_t i) {
  rd_node_events* e = &ctx->events[i];
  for (uint32_t j = 0; j < e->count; j++)
    if (e->items[j].destroy) e->items[j].destroy(e->items[j].data);
  free(e->items);
  memset(e, 0, sizeof *e);
}

void rd__events_ctx_free(rd_ctx* ctx) {
  free(ctx->focus_chain.nodes);
  free(ctx->clip_buf);
  free(ctx->pending_destroy.nodes);
  memset(&ctx->pending_destroy, 0, sizeof ctx->pending_destroy);
}

static void flush_deferred(rd_ctx* ctx) {
  for (uint32_t j = 0; j < ctx->pending_destroy.len; j++) {
    rd_node  n = ctx->pending_destroy.nodes[j];
    rd_slot* s = rd__slot(ctx, n);
    if (s && !s->parent) rd_node_destroy(ctx, n);  // Skipped if a later listener re-attached it.
  }
  ctx->pending_destroy.len = 0;
  if (!ctx->listeners_dirty) return;
  ctx->listeners_dirty = false;
  for (uint32_t i = 1; i < ctx->len; i++)
    if (ctx->slots[i].flags & RD_SLOT_ALIVE) compact(&ctx->events[i]);
}

// Dispatch.

// At the target, capture listeners run before the others, as in the current DOM spec.
static void invoke(rd_ctx* ctx, rd_event* ev, rd_node node, rd_event_phase phase, bool capture_pass) {
  if (!rd__slot(ctx, node)) return;
  ev->current    = node;
  ev->phase      = phase;
  rd_rect b      = rd_node_bounds(ctx, node);
  ev->local_x    = ev->x - b.x;
  ev->local_y    = ev->y - b.y;
  uint32_t count = ctx->events[node.index].count;  // Listeners added now don't run in this pass.
  for (uint32_t j = 0; j < count && j < ctx->events[node.index].count; j++) {
    rd__listener l = ctx->events[node.index].items[j];
    if (l.removed || l.type != ev->type) continue;
    if (((l.flags & RD_LISTEN_CAPTURE) != 0) != capture_pass) continue;
    l.fn(ctx, ev, l.data);
    if (ev->stopped_immediate) return;
  }
}

static void dispatch(rd_ctx* ctx, rd_event* ev, bool bubbles) {
  rd__chain path = {0};
  for (uint32_t i = ev->target.index; i; i = ctx->slots[i].parent)
    if (!rd__chain_push(&path, rd__handle(ctx, i))) break;
  if (!path.len) return;

  ctx->dispatch_depth++;
  for (uint32_t k = path.len; k-- > 1 && !ev->stopped;) invoke(ctx, ev, path.nodes[k], RD_PHASE_CAPTURE, true);
  if (!ev->stopped) invoke(ctx, ev, path.nodes[0], RD_PHASE_TARGET, true);
  if (!ev->stopped) invoke(ctx, ev, path.nodes[0], RD_PHASE_TARGET, false);
  for (uint32_t k = 1; bubbles && k < path.len && !ev->stopped; k++)
    invoke(ctx, ev, path.nodes[k], RD_PHASE_BUBBLE, false);
  ctx->dispatch_depth--;

  free(path.nodes);
  if (!ctx->dispatch_depth) flush_deferred(ctx);
}

static rd_event make_event(const rd_ctx* ctx, rd_event_type type, rd_node target, uint8_t button) {
  return (rd_event){
      .type    = type,
      .target  = target,
      .x       = ctx->pointer_x,
      .y       = ctx->pointer_y,
      .button  = button,
      .buttons = ctx->pointer_buttons,
      .mods    = ctx->mods,
  };
}

// Returns whether the default action was prevented.
static bool fire(rd_ctx* ctx, rd_event_type type, rd_node target, uint8_t button, bool bubbles) {
  if (!rd__slot(ctx, target)) return false;
  rd_event ev = make_event(ctx, type, target, button);
  dispatch(ctx, &ev, bubbles);
  return ev.default_prevented;
}

void rd__fire(rd_ctx* ctx, rd_event_type type, rd_node target) { fire(ctx, type, target, 0, true); }

// Pointer.

static bool disabled(const rd_ctx* ctx, rd_node n) {
  if (!rd__slot(ctx, n)) return true;
  for (uint32_t i = n.index; i; i = ctx->slots[i].parent)
    if (ctx->styles[i].states & RD_STATE_DISABLED) return true;
  return false;
}

static rd_node common_ancestor(const rd_ctx* ctx, rd_node a, rd_node b) {
  if (!rd__slot(ctx, a) || !rd__slot(ctx, b)) return (rd_node){0};
  for (uint32_t x = a.index; x; x = ctx->slots[x].parent)
    for (uint32_t y = b.index; y; y = ctx->slots[y].parent)
      if (x == y) return rd__handle(ctx, x);
  return (rd_node){0};
}

static void build_chain(const rd_ctx* ctx, rd__chain* c, rd_node leaf) {
  c->len = 0;
  if (!rd__slot(ctx, leaf)) return;
  for (uint32_t i = leaf.index; i; i = ctx->slots[i].parent)
    if (!rd__chain_push(c, rd__handle(ctx, i))) return;
}

// Moves hover to a new leaf: leave fires deepest-first on nodes losing hover, enter fires
// outermost-first on nodes gaining it. Nodes hovered before and after get neither.
static void update_hover(rd_ctx* ctx, rd_node leaf) {
  if (rd_node_eq(leaf, ctx->hovered)) return;
  rd__chain prev = ctx->hover_chain, next = {0};
  build_chain(ctx, &next, leaf);
  ctx->hover_chain = next;
  ctx->hovered     = leaf;

  for (uint32_t j = 0; j < prev.len; j++)
    if (!chain_has(&next, prev.nodes[j])) rd_node_set_states(ctx, prev.nodes[j], RD_STATE_HOVER, false);
  for (uint32_t j = 0; j < next.len; j++) rd_node_set_states(ctx, next.nodes[j], RD_STATE_HOVER, true);

  rd__chain entering = {0};
  for (uint32_t j = next.len; j-- > 0;)
    if (!chain_has(&prev, next.nodes[j])) rd__chain_push(&entering, next.nodes[j]);
  for (uint32_t j = 0; j < prev.len; j++)
    if (!chain_has(&next, prev.nodes[j])) fire(ctx, RD_EVENT_POINTER_LEAVE, prev.nodes[j], 0, false);
  for (uint32_t j = 0; j < entering.len; j++) fire(ctx, RD_EVENT_POINTER_ENTER, entering.nodes[j], 0, false);
  free(entering.nodes);
  free(prev.nodes);
}

static void set_active(rd_ctx* ctx, rd_node leaf, bool on) {
  rd__chain* c = &ctx->active_chain;
  for (uint32_t j = 0; j < c->len; j++) rd_node_set_states(ctx, c->nodes[j], RD_STATE_ACTIVE, false);
  c->len = 0;
  if (!on) return;
  build_chain(ctx, c, leaf);
  for (uint32_t j = 0; j < c->len; j++) rd_node_set_states(ctx, c->nodes[j], RD_STATE_ACTIVE, true);
}

// Focus.

static bool displayed(const rd_ctx* ctx, uint32_t i) {
  for (; i; i = ctx->slots[i].parent) {
    if (ctx->styles[i].computed.display == RD_DISPLAY_NONE) return false;
    if (i == ctx->root) return true;
  }
  return false;
}

static bool can_focus(const rd_ctx* ctx, rd_node n, bool tab) {
  if (!rd__slot(ctx, n) || !displayed(ctx, n.index) || disabled(ctx, n)) return false;
  if (rd_node_is_input(ctx, n)) return true;
  rd_focusable f = ctx->styles[n.index].computed.focusable;
  return f == RD_FOCUSABLE_TAB || (!tab && f == RD_FOCUSABLE_POINTER);
}

static void set_focus_states(rd_ctx* ctx, bool on, bool visible) {
  for (uint32_t j = 0; j < ctx->focus_chain.len; j++)
    rd_node_set_states(ctx, ctx->focus_chain.nodes[j], RD_STATE_FOCUS_WITHIN, on);
  if (!on) {
    rd_node_set_states(ctx, ctx->focused, RD_STATE_FOCUS | RD_STATE_FOCUS_VISIBLE, false);
    return;
  }
  rd_node_set_states(ctx, ctx->focused, RD_STATE_FOCUS, true);
  rd_node_set_states(ctx, ctx->focused, RD_STATE_FOCUS_VISIBLE, visible);
}

// Drops focus without events when the focused node was destroyed, detached, hidden or disabled.
static rd_node focus_valid(rd_ctx* ctx) {
  if (rd_node_is_null(ctx->focused) || can_focus(ctx, ctx->focused, false)) return ctx->focused;
  set_focus_states(ctx, false, false);
  ctx->focused         = (rd_node){0};
  ctx->focus_chain.len = 0;
  return ctx->focused;
}

static void fire_focus(rd_ctx* ctx, rd_event_type type, rd_node target, rd_node related, bool bubbles) {
  if (!rd__slot(ctx, target)) return;
  rd_event ev = make_event(ctx, type, target, 0);
  ev.related  = related;
  dispatch(ctx, &ev, bubbles);
}

static void set_focus(rd_ctx* ctx, rd_node next, bool visible) {
  rd_node prev = focus_valid(ctx);
  if (rd_node_eq(prev, next)) {
    if (!rd_node_is_null(next)) rd_node_set_states(ctx, next, RD_STATE_FOCUS_VISIBLE, visible);
    return;
  }
  if (!rd_node_is_null(prev) && rd__input_take_dirty(ctx, prev.index)) fire(ctx, RD_EVENT_CHANGE, prev, 0, true);
  set_focus_states(ctx, false, false);
  ctx->focused = next;
  build_chain(ctx, &ctx->focus_chain, next);
  if (!rd_node_is_null(next)) set_focus_states(ctx, true, visible);

  fire_focus(ctx, RD_EVENT_BLUR, prev, next, false);
  fire_focus(ctx, RD_EVENT_FOCUS_OUT, prev, next, true);
  fire_focus(ctx, RD_EVENT_FOCUS, next, prev, false);
  fire_focus(ctx, RD_EVENT_FOCUS_IN, next, prev, true);
}

bool rd_node_focus(rd_ctx* ctx, rd_node node) {
  if (!ctx) return false;
  if (rd_node_is_null(node)) {
    set_focus(ctx, node, false);
    return true;
  }
  if (!can_focus(ctx, node, false)) return false;
  set_focus(ctx, node, ctx->keyboard_modality || rd_node_is_input(ctx, node));
  return true;
}

rd_node rd_ctx_focused(const rd_ctx* ctx) {
  return ctx && can_focus(ctx, ctx->focused, false) ? ctx->focused : (rd_node){0};
}

static void collect_tabbable(const rd_ctx* ctx, uint32_t i, rd__chain* out) {
  if (ctx->styles[i].computed.display == RD_DISPLAY_NONE || (ctx->styles[i].states & RD_STATE_DISABLED)) return;
  rd_node n = rd__handle(ctx, i);
  if (can_focus(ctx, n, true)) rd__chain_push(out, n);
  for (uint32_t c = ctx->slots[i].first_child; c; c = ctx->slots[c].next_sibling) collect_tabbable(ctx, c, out);
}

bool rd_ctx_focus_next(rd_ctx* ctx, bool backward) {
  if (!ctx) return false;
  rd__chain list = {0};
  collect_tabbable(ctx, ctx->root, &list);
  if (!list.len) {
    free(list.nodes);
    return false;
  }
  rd_node  cur = focus_valid(ctx);
  uint32_t at  = backward ? list.len - 1 : 0;
  for (uint32_t j = 0; j < list.len; j++) {
    if (!rd_node_eq(list.nodes[j], cur)) continue;
    at = backward ? (j + list.len - 1) % list.len : (j + 1) % list.len;
    break;
  }
  rd_node next = list.nodes[at];
  free(list.nodes);
  ctx->keyboard_modality = true;
  set_focus(ctx, next, true);
  rd__scroll_into_view(ctx, next.index);
  return true;
}

static void pointer_focus(rd_ctx* ctx, rd_node t, float x, float y) {
  ctx->keyboard_modality = false;
  rd_node f              = {0};
  for (uint32_t i = rd__slot(ctx, t) ? t.index : 0; i; i = ctx->slots[i].parent) {
    if (can_focus(ctx, rd__handle(ctx, i), false)) {
      f = rd__handle(ctx, i);
      break;
    }
  }
  bool input = !rd_node_is_null(f) && rd_node_is_input(ctx, f);
  set_focus(ctx, f, input);
  if (!input || !rd_node_eq(ctx->focused, f)) return;
  rd__input_pointer(ctx, f.index, x, y, ctx->mods & RD_MOD_SHIFT);
  ctx->capture = f;
}

// Keyboard.

void rd_ctx_key(rd_ctx* ctx, rd_key key, bool down, bool repeat, uint8_t mods) {
  if (!ctx) return;
  ctx->mods = mods;
  if (down) ctx->keyboard_modality = true;
  rd_node  target = focus_valid(ctx);
  rd_event ev     = make_event(ctx, down ? RD_EVENT_KEY_DOWN : RD_EVENT_KEY_UP, target, 0);
  ev.target       = rd_node_is_null(target) ? rd_ctx_root(ctx) : target;
  ev.key          = key;
  ev.repeat       = repeat;
  dispatch(ctx, &ev, true);
  if (!down || ev.default_prevented) return;

  rd_node focused  = focus_valid(ctx);
  bool    in_input = !rd_node_is_null(focused) && rd_node_is_input(ctx, focused);
  if (in_input && rd__input_key(ctx, focused.index, &ev)) return;
  if (!in_input && rd__selection_key(ctx, &ev)) return;
  if (key == RD_KEY_TAB) {
    rd_ctx_focus_next(ctx, mods & RD_MOD_SHIFT);
  } else if ((key == RD_KEY_ENTER || key == RD_KEY_SPACE) && !repeat && !rd_node_is_null(focused) &&
             !rd_node_is_input(ctx, focused)) {
    fire(ctx, RD_EVENT_CLICK, focused, RD_BUTTON_PRIMARY, true);
  }
}

void rd_ctx_text(rd_ctx* ctx, uint32_t codepoint) {
  if (!ctx) return;
  rd_node  target = focus_valid(ctx);
  rd_event ev     = make_event(ctx, RD_EVENT_TEXT, rd_node_is_null(target) ? rd_ctx_root(ctx) : target, 0);
  ev.codepoint    = codepoint;
  dispatch(ctx, &ev, true);
  rd_node focused = focus_valid(ctx);
  if (!ev.default_prevented && !rd_node_is_null(focused) && rd_node_is_input(ctx, focused))
    rd__input_text(ctx, focused.index, codepoint);
}

// Clipboard.

void rd_ctx_set_clipboard(rd_ctx* ctx, rd_clipboard_get_fn get, rd_clipboard_set_fn set, void* data) {
  if (!ctx) return;
  ctx->clip_get  = get;
  ctx->clip_set  = set;
  ctx->clip_data = data;
}

const char* rd__clipboard_get(rd_ctx* ctx) {
  if (ctx->clip_get) return ctx->clip_get(ctx->clip_data);
  return ctx->clip_buf;
}

void rd__clipboard_set(rd_ctx* ctx, const char* text) {
  if (ctx->clip_set) {
    ctx->clip_set(ctx->clip_data, text);
    return;
  }
  size_t n = strlen(text);
  char*  p = malloc(n + 1);
  if (!p) return;
  memcpy(p, text, n + 1);
  free(ctx->clip_buf);
  ctx->clip_buf = p;
}

static float wheel_step(float notches) {
  return -notches * (notches == truncf(notches) ? NOTCH_STEP : PRECISE_STEP);
}

// One container receives a whole wheel gesture: the nearest one under the pointer that can move along
// the gesture's dominant axis. Near-axis gestures lock to that axis, and the latched container absorbs
// the gesture at its edges instead of passing it to an ancestor.
static void scroll_gesture(rd_ctx* ctx, rd_node t, float x, float y, float wheel_x, float wheel_y) {
  float dx = wheel_step(wheel_x), dy = wheel_step(wheel_y);
  if ((ctx->mods & RD_MOD_SHIFT) && wheel_x == 0) {
    dx = dy;
    dy = 0;
  }
  if (fabsf(dx) > 2 * fabsf(dy))
    dy = 0;
  else if (fabsf(dy) > 2 * fabsf(dx))
    dx = 0;

  bool latched = rd__slot(ctx, ctx->wheel_latch) && ctx->now - ctx->wheel_time < LATCH_SECONDS &&
                 fabsf(x - ctx->wheel_at[0]) < LATCH_SLOP && fabsf(y - ctx->wheel_at[1]) < LATCH_SLOP;
  if (!latched) {
    int axis         = fabsf(dx) >= fabsf(dy) ? 0 : 1;
    ctx->wheel_latch = rd__handle(ctx, rd__scroll_target(ctx, t.index, axis, axis ? dy : dx));
    ctx->wheel_at[0] = x;
    ctx->wheel_at[1] = y;
  }
  ctx->wheel_time = ctx->now;
  if (rd__slot(ctx, ctx->wheel_latch)) rd__scroll_by(ctx, ctx->wheel_latch.index, dx, dy);
}

void rd_ctx_set_time(rd_ctx* ctx, double seconds) {
  if (ctx) ctx->now = seconds;
}

static rd_node pointer_target(rd_ctx* ctx, rd_node hit) {
  return rd__slot(ctx, ctx->capture) ? ctx->capture : hit;
}

void rd_ctx_pointer(rd_ctx* ctx, float x, float y, uint8_t buttons, float wheel_x, float wheel_y) {
  if (!ctx) return;
  bool moved        = !ctx->pointer_seen || x != ctx->pointer_x || y != ctx->pointer_y;
  ctx->pointer_seen = true;
  ctx->pointer_x    = x;
  ctx->pointer_y    = y;
  rd_node hit       = rd__handle(ctx, rd__hit(ctx, x, y));

  update_hover(ctx, pointer_target(ctx, hit));
  if (moved) {
    fire(ctx, RD_EVENT_POINTER_MOVE, pointer_target(ctx, hit), 0, true);
    rd_node cap = ctx->capture;
    if ((ctx->pointer_buttons & RD_BUTTON_PRIMARY) && rd__slot(ctx, cap) && rd_node_is_input(ctx, cap))
      rd__input_pointer(ctx, cap.index, x, y, true);
    else if ((ctx->pointer_buttons & RD_BUTTON_PRIMARY) && ctx->selecting)
      rd__selection_drag(ctx, hit.index, x, y);
  }

  for (uint8_t b = 1; b && b <= RD_BUTTON_MIDDLE; b = (uint8_t)(b << 1)) {
    bool was = ctx->pointer_buttons & b, is = buttons & b;
    if (was == is) continue;
    rd_node t = pointer_target(ctx, hit);
    if (is) {
      ctx->pointer_buttons |= b;
      if (disabled(ctx, t)) continue;
      if (b == RD_BUTTON_PRIMARY) {
        ctx->press_target = t;
        set_active(ctx, t, true);
      }
      bool prevented = fire(ctx, RD_EVENT_POINTER_DOWN, t, b, true);
      if (b == RD_BUTTON_PRIMARY && !prevented) {
        pointer_focus(ctx, t, x, y);
        // Text selection starts unless a listener or an input took the pointer.
        if (rd_node_is_null(ctx->capture))
          rd__selection_begin(ctx, hit.index, x, y);
        else
          rd_ctx_clear_selection(ctx);
      }
    } else {
      ctx->pointer_buttons &= (uint8_t)~b;
      if (!disabled(ctx, t)) fire(ctx, RD_EVENT_POINTER_UP, t, b, true);
      if (b == RD_BUTTON_PRIMARY) {
        ctx->selecting = false;
        rd_node c      = common_ancestor(ctx, ctx->press_target, t);
        if (!rd_node_is_null(c) && !disabled(ctx, c)) fire(ctx, RD_EVENT_CLICK, c, b, true);
        set_active(ctx, (rd_node){0}, false);
        ctx->press_target = (rd_node){0};
      }
    }
  }
  if (!ctx->pointer_buttons && !rd_node_is_null(ctx->capture)) {
    ctx->capture = (rd_node){0};
    update_hover(ctx, hit);
  }

  if (wheel_x != 0 || wheel_y != 0) {
    rd_node t = pointer_target(ctx, hit);
    if (!rd__slot(ctx, t)) return;
    rd_event ev = make_event(ctx, RD_EVENT_WHEEL, t, 0);
    ev.wheel_x  = wheel_x;
    ev.wheel_y  = wheel_y;
    dispatch(ctx, &ev, true);
    if (!ev.default_prevented && rd__slot(ctx, t)) scroll_gesture(ctx, t, x, y, wheel_x, wheel_y);
  }
}

rd_node rd_ctx_hovered(const rd_ctx* ctx) {
  return ctx && rd_node_alive(ctx, ctx->hovered) ? ctx->hovered : (rd_node){0};
}

void rd_node_set_pointer_capture(rd_ctx* ctx, rd_node node) {
  if (rd__slot(ctx, node)) ctx->capture = node;
}

void rd_node_release_pointer_capture(rd_ctx* ctx, rd_node node) {
  if (ctx && rd_node_eq(ctx->capture, node)) ctx->capture = (rd_node){0};
}

rd_node rd_ctx_pointer_capture(const rd_ctx* ctx) {
  return ctx && rd_node_alive(ctx, ctx->capture) ? ctx->capture : (rd_node){0};
}
