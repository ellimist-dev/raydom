#include <stdlib.h>

#include "extras_internal.h"

enum { POSITIONS = 6, MAX_SHOWN = 5 };
static const float  MARGIN          = 16;
static const double DEFAULT_SECONDS = 4;

typedef struct toast toast;

// One stack per position, created in the overlay on first use.
typedef struct stacks {
  rd_node of[POSITIONS];
} stacks;

struct toast {
  rd_ctx*  ctx;
  rd_node  node;
  rd_timer timer;
  double   seconds;  // negative: no timer
  bool     closing;  // fading out
};

static void stacks_free(void* p) { free(p); }

static const double ENTER = 0.22, EXIT = 0.18;

static const char key = 0, toast_key = 0;

static rd_node stack_for(rd_ctx* ctx, const rdx_classes* cl, rdx_toast_position pos) {
  stacks* s = rd_ctx_extension(ctx, &key);
  if (!s) {
    s = calloc(1, sizeof *s);
    if (!s || !rd_ctx_set_extension(ctx, &key, s, stacks_free)) {
      free(s);
      return (rd_node){0};
    }
  }
  if (rd_node_alive(ctx, s->of[pos]) && rd_node_eq(rd_node_parent(ctx, s->of[pos]), rd_ctx_overlay(ctx)))
    return s->of[pos];
  rd_node  n   = rd_node_create(ctx);
  rd_val   m   = rd_px(MARGIN), zero = rd_px(0);
  rd_style st  = {0};
  bool     top = pos == RDX_TOAST_TOP_RIGHT || pos == RDX_TOAST_TOP_LEFT || pos == RDX_TOAST_TOP_CENTER;
  if (top)
    st.inset.top = m;
  else
    st.inset.bottom = m;
  switch (pos) {
    case RDX_TOAST_TOP_RIGHT:
    case RDX_TOAST_BOTTOM_RIGHT:
      st.inset.right = m;
      st.align_items = RD_ALIGN_END;
      break;
    case RDX_TOAST_TOP_LEFT:
    case RDX_TOAST_BOTTOM_LEFT:
      st.inset.left  = m;
      st.align_items = RD_ALIGN_START;
      break;
    default:  // centered: the stack spans the width and centers its toasts
      st.inset.left = st.inset.right = zero;
      st.align_items                 = RD_ALIGN_CENTER;
      break;
  }
  rd_node_add_class(ctx, n, cl->toast_stack);
  rd_node_set_style(ctx, n, &st);
  rd_node_append(ctx, rd_ctx_overlay(ctx), n);
  s->of[pos] = n;
  return n;
}

static void destroy_now(rd_ctx* ctx, void* data) {
  toast* t = data;
  t->timer = (rd_timer){0};
  rd_node_destroy(ctx, t->node);  // frees t
}

// Fades and collapses, so the rest of the stack closes up smoothly, then goes.
static void dismiss(rd_ctx* ctx, toast* t) {
  if (t->closing) return;
  t->closing = true;
  rd_ctx_clear_timer(ctx, t->timer);
  rd_node_animate(ctx, t->node, NULL,
                  &(rd_style){.opacity = rd_num(0), .height = rd_px(0), .padding = rd_edges_all(rd_px(0))}, EXIT,
                  RD_EASING_EASE_IN, NULL, NULL);
  t->timer = rd_ctx_set_timer(ctx, EXIT, 0, destroy_now, t, NULL);
}

static void expire(rd_ctx* ctx, void* data) {
  toast* t = data;
  t->timer = (rd_timer){0};
  dismiss(ctx, t);
}

static void start(toast* t) {
  if (t->seconds >= 0) t->timer = rd_ctx_set_timer(t->ctx, t->seconds, 0, expire, t, NULL);
}

static void toast_free(void* data) {
  toast* t = data;
  rd_ctx_clear_timer(t->ctx, t->timer);
  free(t);
}

// Hovering pauses: the countdown restarts in full when the pointer leaves.
static void hover(rd_ctx* ctx, rd_event* ev, void* data) {
  toast* t = data;
  if (t->closing) return;
  rd_ctx_clear_timer(ctx, t->timer);
  t->timer = (rd_timer){0};
  if (ev->type == RD_EVENT_POINTER_LEAVE) start(t);
}

static void close_click(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ev;
  dismiss(ctx, data);
}

static rd_node text_part(rd_ctx* ctx, rd_node parent, const char* s, rd_class cls) {
  rd_node n = rd_node_create_text(ctx, s, RD_TEXT_NUL);
  rd_node_add_class(ctx, n, cls);
  rd_node_append(ctx, parent, n);
  return n;
}

rd_node rdx_toast_show(rd_ctx* ctx, const rdx_toast_desc* desc) {
  const rdx_classes* cl = rdx_theme(ctx);
  rdx_toast_desc     d  = desc ? *desc : (rdx_toast_desc){0};
  if (!cl || (int)d.position >= POSITIONS) return (rd_node){0};
  rd_node stack = stack_for(ctx, cl, d.position);
  toast*  t     = rd_node_is_null(stack) ? NULL : calloc(1, sizeof *t);
  if (!t) return (rd_node){0};

  rd_node card = rd_node_create(ctx);
  *t           = (toast){ctx, card, {0}, d.seconds == 0 ? DEFAULT_SECONDS : d.seconds, false};
  // Owned by the enter listener; attached only to be found by rdx_toast_close.
  if (!rd_node_attach(ctx, card, &toast_key, t, NULL) || !rd_node_on(ctx, card, RD_EVENT_POINTER_ENTER, hover, t, toast_free, 0).id) {
    free(t);
    rd_node_destroy(ctx, card);
    return (rd_node){0};
  }
  rd_node_on(ctx, card, RD_EVENT_POINTER_LEAVE, hover, t, NULL, 0);
  rd_node_add_class(ctx, card, cl->toast);

  rd_class kinds[] = {cl->toast_info, cl->toast_success, cl->toast_warning, cl->toast_error};
  rd_node  bar     = rd_node_create(ctx);
  rd_node_add_class(ctx, bar, cl->toast_bar);
  rd_node_add_class(ctx, bar, kinds[d.kind <= RDX_TOAST_ERROR ? d.kind : 0]);
  rd_node body = rd_node_create(ctx);
  rd_node_set_style(ctx, body, &(rd_style){.flex_grow = rd_num(1), .flex_shrink = rd_num(1), .row_gap = rd_px(2)});
  if (d.title) text_part(ctx, body, d.title, cl->toast_title);
  if (d.text) text_part(ctx, body, d.text, cl->toast_body);
  rd_node close = rd_node_create_button(ctx, "\xc3\x97", RD_TEXT_NUL);  // ×
  rd_node_add_class(ctx, close, cl->toast_close);
  rd_node_on(ctx, close, RD_EVENT_CLICK, close_click, t, NULL, 0);
  rd_node_append(ctx, card, bar);
  rd_node_append(ctx, card, body);
  rd_node_append(ctx, card, close);

  // Newest nearest the edge: last at the bottom, first at the top. The oldest beyond MAX_SHOWN go.
  bool top = d.position == RDX_TOAST_TOP_RIGHT || d.position == RDX_TOAST_TOP_LEFT || d.position == RDX_TOAST_TOP_CENTER;
  rd_node_insert_before(ctx, stack, card, top ? rd_node_first_child(ctx, stack) : (rd_node){0});
  while (rd_node_child_count(ctx, stack) > MAX_SHOWN)
    rd_node_destroy(ctx, top ? rd_node_last_child(ctx, stack) : rd_node_first_child(ctx, stack));
  // Slides in from its edge of the window.
  rd_style from = {.opacity = rd_num(0)}, to = {.opacity = rd_num(1)};
  switch (d.position) {
    case RDX_TOAST_BOTTOM_RIGHT:
    case RDX_TOAST_TOP_RIGHT:
      from.margin.right = rd_px(-32);
      to.margin.right   = rd_px(0);
      break;
    case RDX_TOAST_BOTTOM_LEFT:
    case RDX_TOAST_TOP_LEFT:
      from.margin.left = rd_px(-32);
      to.margin.left   = rd_px(0);
      break;
    case RDX_TOAST_TOP_CENTER:
      from.margin.top = rd_px(-16);
      to.margin.top   = rd_px(0);
      break;
    default:
      from.margin.bottom = rd_px(-16);
      to.margin.bottom   = rd_px(0);
      break;
  }
  rd_node_animate(ctx, card, &from, &to, ENTER, RD_EASING_EASE_OUT, NULL, NULL);
  start(t);
  return card;
}

void rdx_toast_close(rd_ctx* ctx, rd_node node) {
  toast* t = rd_node_attached(ctx, node, &toast_key);
  if (t) dismiss(ctx, t);
}
