#include <math.h>
#include <stdlib.h>

#include "extras_internal.h"
#include <raydom/extras/views.h>

// Tabs. The bar's children are the tabs; the root's children after the bar are the panels, in the
// same order.

typedef struct tabs {
  rd_node root, bar;
  int     selected;
} tabs;

static const char tabs_key = 0;

static rd_node nth(const rd_ctx* ctx, rd_node parent, int index) {
  rd_node n = rd_node_first_child(ctx, parent);
  for (int k = 0; k < index && !rd_node_is_null(n); k++) n = rd_node_next_sibling(ctx, n);
  return n;
}

static int index_in(const rd_ctx* ctx, rd_node parent, rd_node child) {
  int k = 0;
  for (rd_node n = rd_node_first_child(ctx, parent); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n), k++)
    if (rd_node_eq(n, child)) return k;
  return -1;
}

static void show_tab(rd_ctx* ctx, tabs* t) {
  int k = 0;
  for (rd_node n = rd_node_first_child(ctx, t->bar); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n), k++) {
    bool on = k == t->selected;
    rd_node_set_states(ctx, n, RD_STATE_CHECKED, on);
    rd_node_merge_style(ctx, n, &(rd_style){.focusable = on ? RD_FOCUSABLE_TAB : RD_FOCUSABLE_POINTER});
    rd_node_merge_style(ctx, nth(ctx, t->root, k + 1), &(rd_style){.display = on ? RD_DISPLAY_FLEX : RD_DISPLAY_NONE});
  }
}

static void pick(rd_ctx* ctx, tabs* t, int index) {
  if (index == t->selected) return;
  t->selected = index;
  show_tab(ctx, t);
  rd_node_dispatch(ctx, t->root, RD_EVENT_CHANGE);
}

static void tab_click(rd_ctx* ctx, rd_event* ev, void* data) {
  tabs* t = data;
  pick(ctx, t, index_in(ctx, t->bar, ev->current));
}

static void tab_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  tabs* t   = data;
  int   n   = (int)rd_node_child_count(ctx, t->bar);
  int   dir = ev->key == RD_KEY_RIGHT ? 1 : ev->key == RD_KEY_LEFT ? -1 : 0;
  int   to  = ev->key == RD_KEY_HOME ? 0 : ev->key == RD_KEY_END ? n - 1 : dir ? (t->selected + dir + n) % n : -1;
  if (to < 0 || !n) return;
  rd_event_prevent_default(ev);
  pick(ctx, t, to);
  rd_node_focus(ctx, nth(ctx, t->bar, to));
}

rd_node rdx_tabs_create(rd_ctx* ctx) {
  const rdx_classes* cl = rdx_theme(ctx);
  tabs*              t  = cl ? calloc(1, sizeof *t) : NULL;
  if (!t) return (rd_node){0};
  t->root = rd_node_create(ctx);
  t->bar  = rd_node_create(ctx);
  if (!rd_node_on(ctx, t->bar, RD_EVENT_KEY_DOWN, tab_keys, t, free, 0).id || !rdx__track(ctx, &tabs_key, t->root, t)) {
    rd_node_destroy(ctx, t->root);
    rd_node_destroy(ctx, t->bar);
    return (rd_node){0};
  }
  rd_node_add_class(ctx, t->root, cl->tabs);
  rd_node_add_class(ctx, t->bar, cl->tab_bar);
  rd_node_set_role(ctx, t->bar, RD_ROLE_TAB_LIST);
  rd_node_append(ctx, t->root, t->bar);
  return t->root;
}

rd_node rdx_tabs_add(rd_ctx* ctx, rd_node root, const char* label) {
  tabs*              t  = rdx__lookup(ctx, &tabs_key, root);
  const rdx_classes* cl = rdx_theme(ctx);
  if (!t || !cl) return (rd_node){0};
  rd_node tab = rd_node_create_text(ctx, label ? label : "", RD_TEXT_NUL);
  rd_node_set_role(ctx, tab, RD_ROLE_TAB);
  rd_node_add_class(ctx, tab, cl->tab);
  rd_node_on(ctx, tab, RD_EVENT_CLICK, tab_click, t, NULL, 0);
  rd_node_append(ctx, t->bar, tab);
  rd_node panel = rd_node_create(ctx);
  rd_node_set_role(ctx, panel, RD_ROLE_TAB_PANEL);
  rd_node_add_class(ctx, panel, cl->tab_panel);
  rd_node_append(ctx, t->root, panel);
  if (t->selected < 0 || rd_node_child_count(ctx, t->bar) == 1) t->selected = 0;
  show_tab(ctx, t);
  return panel;
}

int rdx_tabs_selected(const rd_ctx* ctx, rd_node root) {
  tabs* t = rdx__lookup(ctx, &tabs_key, root);
  return t && rd_node_child_count(ctx, t->bar) ? t->selected : -1;
}

bool rdx_tabs_select(rd_ctx* ctx, rd_node root, int index) {
  tabs* t = rdx__lookup(ctx, &tabs_key, root);
  if (!t || index < 0 || index >= (int)rd_node_child_count(ctx, t->bar)) return false;
  t->selected = index;
  show_tab(ctx, t);
  return true;
}

// Splitter.

typedef struct split {
  rd_node root, first, handle, second;
  bool    vertical, reverse, collapsible, collapsed;
  bool    fixed;     // the first pane keeps `size` pixels instead of a share
  float   fraction;  // the first pane's share, when not fixed
  float   size;      // the first pane's size in pixels, when fixed
  float   min_first, min_second;
  // As a drag began: whether it changed anything, and the size to keep if it collapses the pane.
  float drag_fraction, drag_size;
  bool  drag_collapsed;
} split;

static const char split_key = 0;

static float total(const rd_ctx* ctx, const split* s) {
  rd_rect r = rd_node_rect(ctx, s->root);
  rd_box  b = rd_node_box(ctx, s->root);
  return s->vertical ? r.height - b.border.top - b.border.bottom - b.padding.top - b.padding.bottom
                     : r.width - b.border.left - b.border.right - b.padding.left - b.padding.right;
}

static float handle_size(const rd_ctx* ctx, const split* s) {
  rd_rect h = rd_node_rect(ctx, s->handle);
  return s->vertical ? h.height : h.width;
}

// The first pane's size in pixels when shown.
static float first_size(const rd_ctx* ctx, const split* s) { return s->fixed ? s->size : s->fraction * total(ctx, s); }

// Sets the first pane's size in pixels, within both panes' minimums once the splitter has a size.
static void set_first(const rd_ctx* ctx, split* s, float px) {
  float size = total(ctx, s);
  if (size > 0) px = fminf(fmaxf(s->min_first, size - s->min_second - handle_size(ctx, s)), fmaxf(s->min_first, px));
  if (s->fixed)
    s->size = fmaxf(0, px);
  else if (size > 0)
    s->fraction = fminf(1, fmaxf(0, px / size));
}

static void apply(rd_ctx* ctx, split* s) {
  rd_val   share = s->fixed ? rd_px(s->size) : rd_pct(s->fraction * 100);
  rd_style st    = {.display = s->collapsed ? RD_DISPLAY_NONE : RD_DISPLAY_FLEX, .flex_shrink = rd_num(s->fixed ? 1 : 0)};
  if (s->vertical)
    st.height = share, st.width = rd_auto();
  else
    st.width = share, st.height = rd_auto();
  rd_node_merge_style(ctx, s->first, &st);
}

// Direction, the handle's shape, and the minimums along the axis.
static void orient(rd_ctx* ctx, split* s) {
  rd_flex_direction dir = s->vertical ? (s->reverse ? RD_FLEX_COLUMN_REVERSE : RD_FLEX_COLUMN)
                                      : (s->reverse ? RD_FLEX_ROW_REVERSE : RD_FLEX_ROW);
  rd_node_merge_style(ctx, s->root, &(rd_style){.flex_direction = dir});
  rd_node_set_style(ctx, s->handle, s->vertical ? &(rd_style){.width = rd_auto(), .height = rd_px(6), .cursor = RD_CURSOR_NS_RESIZE}
                                                : &(rd_style){0});
  rd_val none = rd_px(0);
  rd_node_merge_style(ctx, s->first, &(rd_style){.min_width  = s->vertical ? none : rd_px(s->min_first),
                                                 .min_height = s->vertical ? rd_px(s->min_first) : none});
  rd_node_merge_style(ctx, s->second, &(rd_style){.min_width  = s->vertical ? none : rd_px(s->min_second),
                                                  .min_height = s->vertical ? rd_px(s->min_second) : none});
  apply(ctx, s);
}

static void toggle(rd_ctx* ctx, split* s) {
  s->collapsed = !s->collapsed;
  apply(ctx, s);
  rd_node_dispatch(ctx, s->root, RD_EVENT_CHANGE);
}

static void handle_pointer(rd_ctx* ctx, rd_event* ev, void* data) {
  split* s = data;
  if (ev->type == RD_EVENT_CLICK) {
    if (ev->clicks == 2 && s->collapsible) toggle(ctx, s);
    return;
  }
  if (ev->type == RD_EVENT_POINTER_DOWN) {
    if (ev->button != RD_BUTTON_PRIMARY) return;
    rd_node_set_pointer_capture(ctx, s->handle);
    s->drag_fraction  = s->fraction;
    s->drag_size      = s->size;
    s->drag_collapsed = s->collapsed;
    return;
  }
  if (ev->type == RD_EVENT_POINTER_UP) {
    if (!rd_node_eq(rd_ctx_pointer_capture(ctx), s->handle)) return;
    if (s->fraction != s->drag_fraction || s->size != s->drag_size || s->collapsed != s->drag_collapsed)
      rd_node_dispatch(ctx, s->root, RD_EVENT_CHANGE);
    return;
  }
  if (!(ev->buttons & RD_BUTTON_PRIMARY) || !rd_node_eq(rd_ctx_pointer_capture(ctx), s->handle)) return;
  rd_rect r    = rd_node_bounds(ctx, s->root);
  rd_box  b    = rd_node_box(ctx, s->root);
  float   size = total(ctx, s);
  if (size <= 0) return;
  float at   = s->vertical ? ev->y - r.y - b.border.top - b.padding.top : ev->x - r.x - b.border.left - b.padding.left;
  float want = (s->reverse ? size - at : at) - handle_size(ctx, s) / 2;
  // Below half the minimum, a collapsible pane snaps shut, keeping the size it had before the drag.
  if (s->collapsible && want < (s->min_first > 0 ? s->min_first / 2 : 16)) {
    s->collapsed = true;
    s->fraction  = s->drag_fraction;
    s->size      = s->drag_size;
  } else {
    s->collapsed = false;
    set_first(ctx, s, want);
  }
  apply(ctx, s);
}

static void handle_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  split* s = data;
  if (ev->key == RD_KEY_ENTER && s->collapsible) {
    rd_event_prevent_default(ev);
    toggle(ctx, s);
    return;
  }
  // +1 grows the first pane.
  int dir = s->vertical ? (ev->key == RD_KEY_DOWN) - (ev->key == RD_KEY_UP) : (ev->key == RD_KEY_RIGHT) - (ev->key == RD_KEY_LEFT);
  if (s->reverse) dir = -dir;
  if (!dir || total(ctx, s) <= 0) return;
  rd_event_prevent_default(ev);
  if (s->collapsed) {
    if (dir < 0) return;
    s->collapsed = false;  // back at its earlier size
  } else {
    float before = first_size(ctx, s);
    set_first(ctx, s, before + (float)dir * 16);
    if (first_size(ctx, s) == before) {
      if (dir > 0 || !s->collapsible) return;
      s->collapsed = true;  // shrinking past the minimum
    }
  }
  apply(ctx, s);
  rd_node_dispatch(ctx, s->root, RD_EVENT_CHANGE);
}

rd_node rdx_split_create(rd_ctx* ctx, const rdx_split_desc* desc) {
  const rdx_classes* cl = rdx_theme(ctx);
  split*             s  = cl ? calloc(1, sizeof *s) : NULL;
  if (!s) return (rd_node){0};
  rdx_split_desc d = desc ? *desc : (rdx_split_desc){0};
  s->vertical      = d.vertical;
  s->reverse       = d.reverse;
  s->collapsible   = d.collapsible;
  s->collapsed     = d.collapsible && d.collapsed;
  s->fixed         = d.size > 0;
  s->size          = fmaxf(0, d.size);
  s->fraction      = d.fraction > 0 && d.fraction < 1 ? d.fraction : 0.5f;
  s->min_first     = fmaxf(0, d.min_first);
  s->min_second    = fmaxf(0, d.min_second);
  s->root          = rd_node_create(ctx);
  s->first         = rd_node_create(ctx);
  s->handle        = rd_node_create(ctx);
  s->second        = rd_node_create(ctx);
  if (!rd_node_on(ctx, s->handle, RD_EVENT_KEY_DOWN, handle_keys, s, free, 0).id || !rdx__track(ctx, &split_key, s->root, s)) {
    rd_node nodes[] = {s->root, s->first, s->handle, s->second};
    for (int j = 0; j < 4; j++) rd_node_destroy(ctx, nodes[j]);
    return (rd_node){0};
  }
  rd_event_type types[] = {RD_EVENT_POINTER_DOWN, RD_EVENT_POINTER_MOVE, RD_EVENT_POINTER_UP, RD_EVENT_CLICK};
  for (int j = 0; j < 4; j++) rd_node_on(ctx, s->handle, types[j], handle_pointer, s, NULL, 0);
  rd_node_add_class(ctx, s->root, cl->split);
  rd_node_add_class(ctx, s->handle, cl->split_handle);
  rd_node_set_role(ctx, s->handle, RD_ROLE_SPLITTER);
  rd_style pane = {.overflow_x = RD_OVERFLOW_HIDDEN, .overflow_y = RD_OVERFLOW_HIDDEN};
  rd_node_set_style(ctx, s->first, &pane);
  pane.flex_grow   = rd_num(1);
  pane.flex_shrink = rd_num(1);
  pane.flex_basis  = rd_px(0);
  rd_node_set_style(ctx, s->second, &pane);
  orient(ctx, s);
  rd_node_append(ctx, s->root, s->first);
  rd_node_append(ctx, s->root, s->handle);
  rd_node_append(ctx, s->root, s->second);
  return s->root;
}

rd_node rdx_split_pane(const rd_ctx* ctx, rd_node root, int index) {
  split* s = rdx__lookup(ctx, &split_key, root);
  return !s ? (rd_node){0} : index == 0 ? s->first : index == 1 ? s->second : (rd_node){0};
}

float rdx_split_fraction(const rd_ctx* ctx, rd_node root) {
  split* s = rdx__lookup(ctx, &split_key, root);
  if (!s) return 0;
  float size = total(ctx, s);
  return !s->fixed ? s->fraction : size > 0 ? s->size / size : 0;
}

bool rdx_split_set_fraction(rd_ctx* ctx, rd_node root, float fraction) {
  split* s = rdx__lookup(ctx, &split_key, root);
  if (!s) return false;
  float size = total(ctx, s);
  if (s->fixed && size <= 0) return false;
  if (size > 0)
    set_first(ctx, s, fraction * size);
  else
    s->fraction = fminf(1, fmaxf(0, fraction));
  apply(ctx, s);
  return true;
}

float rdx_split_size(const rd_ctx* ctx, rd_node root) {
  split* s = rdx__lookup(ctx, &split_key, root);
  return s ? first_size(ctx, s) : 0;
}

bool rdx_split_set_size(rd_ctx* ctx, rd_node root, float size) {
  split* s = rdx__lookup(ctx, &split_key, root);
  if (!s || size < 0 || (!s->fixed && total(ctx, s) <= 0)) return false;
  set_first(ctx, s, size);
  apply(ctx, s);
  return true;
}

bool rdx_split_collapsed(const rd_ctx* ctx, rd_node root) {
  split* s = rdx__lookup(ctx, &split_key, root);
  return s && s->collapsed;
}

bool rdx_split_set_collapsed(rd_ctx* ctx, rd_node root, bool collapsed) {
  split* s = rdx__lookup(ctx, &split_key, root);
  if (!s || (collapsed && !s->collapsible)) return false;
  s->collapsed = collapsed;
  apply(ctx, s);
  return true;
}

bool rdx_split_set_direction(rd_ctx* ctx, rd_node root, bool vertical, bool reverse) {
  split* s = rdx__lookup(ctx, &split_key, root);
  if (!s) return false;
  s->vertical = vertical;
  s->reverse  = reverse;
  orient(ctx, s);
  return true;
}
