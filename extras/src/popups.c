#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "extras_internal.h"
#include <raydom/extras/popups.h>

static const double TOOLTIP_DELAY = 0.5;

typedef enum pkind : uint8_t { P_MENU, P_LIST, P_MODAL, P_TOOLTIP } pkind;

typedef struct popup {
  rd_node node;     // the popup's root in the overlay
  rd_node anchor;   // presses on it don't dismiss the popup
  rd_node restore;  // focus returns here on close, if focus was inside
  pkind   kind;
  bool    persistent;  // P_MODAL: Escape doesn't close it
  rd_node prev_scope;  // P_MODAL: the focus scope before it opened
  void (*on_close)(rd_ctx*, void*);
  void* data;
} popup;

typedef struct select_state select_state;
typedef struct tip          tip;

// An owned object found by its node; alive(node) means the object hasn't been freed yet.
typedef struct entry {
  rd_node node;
  void*   obj;
} entry;

typedef struct list {
  entry*   items;
  uint32_t len, cap;
} list;

typedef struct manager {
  popup*   open;  // bottom to top
  uint32_t open_len, open_cap;
  list     selects, tips;
  bool     hooked;
} manager;

static const char key = 0;

static void manager_free(void* data) {
  manager* m = data;
  free(m->open);
  free(m->selects.items);
  free(m->tips.items);
  free(m);
}

static bool list_put(list* l, rd_node n, void* obj) {
  if (l->len == l->cap) {
    uint32_t cap = l->cap ? l->cap * 2 : 8;
    entry*   p   = realloc(l->items, cap * sizeof *p);
    if (!p) return false;
    l->items = p;
    l->cap   = cap;
  }
  l->items[l->len++] = (entry){n, obj};
  return true;
}

static void* list_find(const rd_ctx* ctx, list* l, rd_node n) {
  uint32_t kept = 0;
  void*    hit  = NULL;
  for (uint32_t j = 0; j < l->len; j++) {
    if (!rd_node_alive(ctx, l->items[j].node)) continue;  // its object is gone with it
    if (rd_node_eq(l->items[j].node, n)) hit = l->items[j].obj;
    l->items[kept++] = l->items[j];
  }
  l->len = kept;
  return hit;
}

static bool inside(const rd_ctx* ctx, rd_node outer, rd_node n) {
  if (rd_node_is_null(outer)) return false;
  for (; !rd_node_is_null(n); n = rd_node_parent(ctx, n))
    if (rd_node_eq(n, outer)) return true;
  return false;
}

static void prune(rd_ctx* ctx, manager* m) {
  uint32_t kept = 0;
  for (uint32_t j = 0; j < m->open_len; j++)
    if (rd_node_alive(ctx, m->open[j].node)) m->open[kept++] = m->open[j];
  m->open_len = kept;
}

static void close_at(rd_ctx* ctx, manager* m, uint32_t j) {
  popup p = m->open[j];
  memmove(&m->open[j], &m->open[j + 1], (m->open_len - j - 1) * sizeof *m->open);
  m->open_len--;
  if (p.kind == P_MODAL) {
    rd_ctx_set_focus_scope(ctx, p.prev_scope);
    if (p.on_close) p.on_close(ctx, p.data);
  }
  bool had_focus = inside(ctx, p.node, rd_ctx_focused(ctx));
  rd_node_destroy(ctx, p.node);
  if (had_focus) {
    if (!rd_node_alive(ctx, p.restore) || !rd_node_focus(ctx, p.restore)) rd_node_focus(ctx, (rd_node){0});
  }
}

static int find_open(const rd_ctx* ctx, const manager* m, rd_node n) {
  for (int j = (int)m->open_len - 1; j >= 0; j--)
    if (inside(ctx, m->open[j].node, n)) return j;
  return -1;
}

// Dismissal, from listeners on the root that run before anything else sees the input.

static void on_press(rd_ctx* ctx, rd_event* ev, void* data) {
  manager* m = data;
  prune(ctx, m);
  for (int j = (int)m->open_len - 1; j >= 0; j--) {
    popup* p = &m->open[j];
    if (p->kind == P_TOOLTIP) {
      close_at(ctx, m, (uint32_t)j);
      continue;
    }
    if (p->kind == P_MODAL || inside(ctx, p->node, ev->target) || inside(ctx, p->anchor, ev->target)) break;
    close_at(ctx, m, (uint32_t)j);
  }
}

static void on_wheel(rd_ctx* ctx, rd_event* ev, void* data) {
  manager* m = data;
  prune(ctx, m);
  for (int j = (int)m->open_len - 1; j >= 0; j--) {
    popup* p = &m->open[j];
    if (p->kind != P_TOOLTIP && (p->kind == P_MODAL || inside(ctx, p->node, ev->target))) break;
    close_at(ctx, m, (uint32_t)j);
  }
}

static void on_key(rd_ctx* ctx, rd_event* ev, void* data) {
  manager* m = data;
  if (ev->key != RD_KEY_ESCAPE) return;
  prune(ctx, m);
  if (!m->open_len) return;
  popup* top = &m->open[m->open_len - 1];
  if (top->kind == P_MODAL && top->persistent) return;
  close_at(ctx, m, m->open_len - 1);
  rd_event_stop_propagation(ev);
  rd_event_prevent_default(ev);
}

// Tooltips whose node went away close at the next pointer move.
static void on_move(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ev;
  manager* m = data;
  prune(ctx, m);
  for (int j = (int)m->open_len - 1; j >= 0; j--)
    if (m->open[j].kind == P_TOOLTIP && !rd_node_alive(ctx, m->open[j].anchor)) close_at(ctx, m, (uint32_t)j);
}

static manager* manager_get(rd_ctx* ctx) {
  manager* m = rd_ctx_extension(ctx, &key);
  if (!m && ctx) {
    m = calloc(1, sizeof *m);
    if (m && !rd_ctx_set_extension(ctx, &key, m, manager_free)) {
      free(m);
      m = NULL;
    }
  }
  if (m && !m->hooked) {
    rd_node root = rd_ctx_root(ctx);
    rd_node_on(ctx, root, RD_EVENT_POINTER_DOWN, on_press, m, NULL, RD_LISTEN_CAPTURE);
    rd_node_on(ctx, root, RD_EVENT_WHEEL, on_wheel, m, NULL, RD_LISTEN_CAPTURE);
    rd_node_on(ctx, root, RD_EVENT_KEY_DOWN, on_key, m, NULL, RD_LISTEN_CAPTURE);
    rd_node_on(ctx, root, RD_EVENT_POINTER_MOVE, on_move, m, NULL, RD_LISTEN_CAPTURE);
    m->hooked = true;
  }
  return m;
}

static bool push_popup(manager* m, popup p) {
  if (m->open_len == m->open_cap) {
    uint32_t cap = m->open_cap ? m->open_cap * 2 : 8;
    popup*   q   = realloc(m->open, cap * sizeof *q);
    if (!q) return false;
    m->open     = q;
    m->open_cap = cap;
  }
  m->open[m->open_len++] = p;
  return true;
}

bool rdx_popup_close(rd_ctx* ctx, rd_node node) {
  manager* m = rd_ctx_extension(ctx, &key);
  if (!m) return false;
  prune(ctx, m);
  int j = find_open(ctx, m, node);
  if (j < 0) return false;
  close_at(ctx, m, (uint32_t)j);
  return true;
}

// Popups appear with a short fade; anchoring places them, so only opacity animates.
static void fade_in(rd_ctx* ctx, rd_node n, double seconds) {
  rd_node_animate(ctx, n, &(rd_style){.opacity = rd_num(0)}, &(rd_style){.opacity = rd_num(1)}, seconds,
                  RD_EASING_EASE_OUT, NULL, NULL);
}

// Menus and select lists.

struct select_state {
  rd_node  node, value;
  char**   options;
  size_t   count;
  int      selected;
  char*    placeholder;
  manager* m;
};

typedef struct menu_state {
  manager*      m;
  rdx_menu_fn   fn;
  void*         data;
  rd_destroy_fn destroy;
  select_state* select;  // a select's list
} menu_state;

static void menu_state_free(void* p) {
  menu_state* st = p;
  if (st->destroy) st->destroy(st->data);
  free(st);
}

static char* dup(const char* s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char*  p = malloc(n);
  if (p) memcpy(p, s, n);
  return p;
}

// Every item, separators included, is one child of the menu in order, so position is the index.
static int item_index(const rd_ctx* ctx, rd_node item) {
  int k = 0;
  for (rd_node n = rd_node_first_child(ctx, rd_node_parent(ctx, item)); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n), k++)
    if (rd_node_eq(n, item)) return k;
  return -1;
}

static bool is_item(const rd_ctx* ctx, rd_node n) {
  return rd_node_role(ctx, n) == RD_ROLE_MENU_ITEM && !(rd_node_states(ctx, n) & RD_STATE_DISABLED);
}

static void show_selection(rd_ctx* ctx, select_state* s) {
  const char* text = s->selected >= 0 ? s->options[s->selected] : s->placeholder ? s->placeholder : "";
  rd_node_set_text(ctx, s->value, text, RD_TEXT_NUL);
}

static void choose(rd_ctx* ctx, rd_event* ev, void* data) {
  menu_state*   st    = data;
  int           index = item_index(ctx, ev->current);
  rdx_menu_fn   fn    = st->fn;
  void*         fdata = st->data;
  select_state* sel   = st->select;
  rdx_popup_close(ctx, ev->current);  // the state stays valid until this dispatch ends
  if (sel && rd_node_alive(ctx, sel->node)) {
    bool changed  = sel->selected != index;
    sel->selected = index;
    show_selection(ctx, sel);
    if (changed) rd_node_dispatch(ctx, sel->node, RD_EVENT_CHANGE);
  } else if (fn) {
    fn(ctx, index, fdata);
  }
}

static void hover_focus(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  if (is_item(ctx, ev->current)) rd_node_focus(ctx, ev->current);
}

static void menu_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  rd_node menu = ev->current;
  if (ev->key == RD_KEY_TAB) {
    rd_event_prevent_default(ev);
    rdx_popup_close(ctx, menu);
    return;
  }
  if (ev->key != RD_KEY_DOWN && ev->key != RD_KEY_UP && ev->key != RD_KEY_HOME && ev->key != RD_KEY_END) return;
  rd_event_prevent_default(ev);
  // The enabled items, and where focus is among them.
  uint32_t cap   = rd_node_child_count(ctx, menu);
  rd_node* items = cap ? malloc(cap * sizeof *items) : NULL;
  if (!items) return;
  int n = 0, at = -1;
  for (rd_node c = rd_node_first_child(ctx, menu); !rd_node_is_null(c); c = rd_node_next_sibling(ctx, c)) {
    if (!is_item(ctx, c)) continue;
    if (rd_node_eq(c, rd_ctx_focused(ctx))) at = n;
    items[n++] = c;
  }
  if (n) {
    int to = ev->key == RD_KEY_HOME ? 0
           : ev->key == RD_KEY_END  ? n - 1
           : ev->key == RD_KEY_DOWN ? (at < 0 ? 0 : (at + 1) % n)
                                    : (at < 0 ? n - 1 : (at + n - 1) % n);
    rd_node_focus(ctx, items[to]);
  }
  free(items);
}

static rd_node text_part(rd_ctx* ctx, rd_node parent, const char* s, rd_class cls) {
  rd_node n = rd_node_create_text(ctx, s ? s : "", RD_TEXT_NUL);
  rdx__part(ctx, n, cls);
  rd_node_append(ctx, parent, n);
  return n;
}

// Builds a menu of items in the overlay, anchored, and registers it. Select lists have a check column.
static rd_node build_menu(rd_ctx* ctx, manager* m, pkind kind, const rdx_menu_item* items, size_t count,
                          rd_anchor anchor, menu_state init, rd_node restore) {
  rdx__theme* t  = rdx__theme_get(ctx);
  menu_state* st = t ? malloc(sizeof *st) : NULL;
  if (!st) {
    if (init.destroy) init.destroy(init.data);
    return (rd_node){0};
  }
  *st                   = init;
  st->m                 = m;
  const rdx_classes* cl = &t->classes;
  rd_node            menu = rd_node_create(ctx);
  if (!rd_node_on(ctx, menu, RD_EVENT_KEY_DOWN, menu_keys, st, menu_state_free, 0).id) {
    menu_state_free(st);
    rd_node_destroy(ctx, menu);
    return (rd_node){0};
  }
  rd_node_set_role(ctx, menu, RD_ROLE_MENU);
  rd_node_add_class(ctx, menu, cl->menu);
  for (size_t i = 0; i < count; i++) {
    if (!items[i].label) {
      rd_node sep = rd_node_create(ctx);
      rd_node_add_class(ctx, sep, cl->menu_separator);
      rd_node_append(ctx, menu, sep);
      continue;
    }
    rd_node item = rd_node_create(ctx);
    rd_node_set_role(ctx, item, RD_ROLE_MENU_ITEM);
    rd_node_add_class(ctx, item, cl->menu_item);
    if (kind == P_LIST) {
      rd_node check = rd_node_create(ctx);
      rdx__part(ctx, check, cl->menu_check);
      rd_node_set_image(ctx, check, t->check);
      rd_node_append(ctx, item, check);
      if (st->select && (int)i == st->select->selected) rd_node_set_states(ctx, item, RD_STATE_CHECKED, true);
    }
    text_part(ctx, item, items[i].label, cl->menu_label);
    if (items[i].shortcut) text_part(ctx, item, items[i].shortcut, cl->menu_shortcut);
    if (items[i].disabled) rd_node_set_states(ctx, item, RD_STATE_DISABLED, true);
    rd_node_on(ctx, item, RD_EVENT_CLICK, choose, st, NULL, 0);
    rd_node_on(ctx, item, RD_EVENT_POINTER_ENTER, hover_focus, NULL, NULL, 0);
    rd_node_append(ctx, menu, item);
  }
  rd_node_append(ctx, rd_ctx_overlay(ctx), menu);
  rd_node_set_anchor(ctx, menu, &anchor);
  fade_in(ctx, menu, 0.12);
  if (!push_popup(m, (popup){.node = menu, .anchor = anchor.node, .restore = restore, .kind = kind})) {
    rd_node_destroy(ctx, menu);
    return (rd_node){0};
  }
  return menu;
}

rd_node rdx_menu_open(rd_ctx* ctx, const rdx_menu_desc* desc) {
  manager* m = manager_get(ctx);
  if (!m || !desc) {
    if (desc && desc->destroy) desc->destroy(desc->data);
    return (rd_node){0};
  }
  rd_node restore = rd_node_is_null(desc->anchor.node) ? rd_ctx_focused(ctx) : desc->anchor.node;
  rd_node menu    = build_menu(ctx, m, P_MENU, desc->items, desc->count, desc->anchor,
                               (menu_state){.fn = desc->fn, .data = desc->data, .destroy = desc->destroy}, restore);
  rd_node_focus(ctx, menu);
  return menu;
}

// Context menus keep a copy of their items.
typedef struct context_menu {
  rdx_menu_item* items;
  size_t         count;
  rdx_menu_fn    fn;
  void*          data;
  rd_destroy_fn  destroy;
} context_menu;

static void context_free(void* p) {
  context_menu* c = p;
  for (size_t i = 0; i < c->count; i++) {
    free((void*)c->items[i].label);
    free((void*)c->items[i].shortcut);
  }
  free(c->items);
  if (c->destroy) c->destroy(c->data);
  free(c);
}

static void context_press(rd_ctx* ctx, rd_event* ev, void* data) {
  context_menu* c = data;
  if (ev->button != RD_BUTTON_SECONDARY) return;
  rd_event_prevent_default(ev);
  rdx_menu_open(ctx, &(rdx_menu_desc){c->items, c->count, {.x = ev->x, .y = ev->y}, c->fn, c->data, NULL});
}

bool rdx_context_menu_attach(rd_ctx* ctx, rd_node target, const rdx_menu_item* items, size_t count, rdx_menu_fn fn,
                             void* data, rd_destroy_fn destroy) {
  if (!rd_node_alive(ctx, target) || !manager_get(ctx)) return false;
  context_menu* c = calloc(1, sizeof *c);
  if (!c) return false;
  c->items = calloc(count ? count : 1, sizeof *c->items);
  if (!c->items) {
    free(c);
    return false;
  }
  for (size_t i = 0; i < count; i++)
    c->items[i] = (rdx_menu_item){dup(items[i].label), dup(items[i].shortcut), items[i].disabled};
  c->count   = count;
  c->fn      = fn;
  c->data    = data;
  c->destroy = destroy;
  if (!rd_node_on(ctx, target, RD_EVENT_POINTER_DOWN, context_press, c, context_free, 0).id) {
    c->destroy = NULL;
    context_free(c);
    return false;
  }
  return true;
}

// Select.

static void select_free(void* p) {
  select_state* s = p;
  for (size_t i = 0; i < s->count; i++) free(s->options[i]);
  free((void*)s->options);
  free(s->placeholder);
  free(s);
}

static bool list_open(const rd_ctx* ctx, const select_state* s, int* at) {
  for (int j = (int)s->m->open_len - 1; j >= 0; j--) {
    if (s->m->open[j].kind == P_LIST && rd_node_eq(s->m->open[j].anchor, s->node) &&
        rd_node_alive(ctx, s->m->open[j].node)) {
      *at = j;
      return true;
    }
  }
  return false;
}

static void select_click(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ev;
  select_state* s = data;
  int           j;
  if (list_open(ctx, s, &j)) {
    close_at(ctx, s->m, (uint32_t)j);
    return;
  }
  rdx_menu_item* items = calloc(s->count ? s->count : 1, sizeof *items);
  if (!items) return;
  for (size_t i = 0; i < s->count; i++) items[i].label = s->options[i];
  rd_node list = build_menu(ctx, s->m, P_LIST, items, s->count, (rd_anchor){.node = s->node, .offset = 4, .match_width = true},
                            (menu_state){.select = s}, s->node);
  free(items);
  // Focus the chosen option, or the first.
  rd_node focus = list;
  for (rd_node n = rd_node_first_child(ctx, list); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n)) {
    if (!is_item(ctx, n)) continue;
    if (rd_node_is_null(focus) || rd_node_eq(focus, list)) focus = n;
    if (item_index(ctx, n) == s->selected) {
      focus = n;
      break;
    }
  }
  rd_node_focus(ctx, focus);
}

static void select_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  select_state* s   = data;
  int           dir = ev->key == RD_KEY_DOWN ? 1 : ev->key == RD_KEY_UP ? -1 : 0;
  if (!dir || !s->count) return;
  rd_event_prevent_default(ev);
  int next = s->selected < 0 ? (dir > 0 ? 0 : (int)s->count - 1) : s->selected + dir;
  if (next < 0 || next >= (int)s->count || next == s->selected) return;
  s->selected = next;
  show_selection(ctx, s);
  rd_node_dispatch(ctx, s->node, RD_EVENT_CHANGE);
}

rd_node rdx_select_create(rd_ctx* ctx, const char* const* options, size_t count, const char* placeholder) {
  manager*    m = manager_get(ctx);
  rdx__theme* t = m ? rdx__theme_get(ctx) : NULL;
  if (!t) return (rd_node){0};
  select_state* s = calloc(1, sizeof *s);
  if (!s) return (rd_node){0};
  s->options = (char**)calloc(count ? count : 1, sizeof *s->options);
  if (!s->options) {
    free(s);
    return (rd_node){0};
  }
  for (size_t i = 0; i < count; i++) s->options[i] = dup(options[i]);
  s->count       = count;
  s->selected    = -1;
  s->placeholder = dup(placeholder);
  s->m           = m;
  s->node        = rd_node_create(ctx);
  if (!rd_node_on(ctx, s->node, RD_EVENT_CLICK, select_click, s, select_free, 0).id) {
    rd_node_destroy(ctx, s->node);
    select_free(s);
    return (rd_node){0};
  }
  const rdx_classes* cl = &t->classes;
  rd_node_set_role(ctx, s->node, RD_ROLE_COMBO_BOX);
  rd_node_add_class(ctx, s->node, cl->select);
  s->value        = text_part(ctx, s->node, "", cl->select_value);
  rd_node chevron = rd_node_create(ctx);
  rdx__part(ctx, chevron, cl->select_chevron);
  rd_node_set_image(ctx, chevron, t->chevron);
  rd_node_append(ctx, s->node, chevron);
  rd_node_on(ctx, s->node, RD_EVENT_KEY_DOWN, select_keys, s, NULL, 0);
  list_put(&m->selects, s->node, s);
  show_selection(ctx, s);
  return s->node;
}

static select_state* select_find(const rd_ctx* ctx, rd_node n) {
  manager* m = rd_ctx_extension(ctx, &key);
  return m ? list_find(ctx, &m->selects, n) : NULL;
}

int rdx_select_selected(const rd_ctx* ctx, rd_node select) {
  select_state* s = select_find(ctx, select);
  return s ? s->selected : -1;
}

bool rdx_select_set(rd_ctx* ctx, rd_node select, int index) {
  select_state* s = select_find(ctx, select);
  if (!s || index >= (int)s->count) return false;
  s->selected = index < 0 ? -1 : index;
  show_selection(ctx, s);
  return true;
}

// Modal.

typedef struct modal_focus {
  rd_node backdrop, card, body;
} modal_focus;

// Runs at the next clock update, once the caller has filled the body: focuses the body's first
// focusable node, so the × in the header isn't where focus starts.
static void focus_first(rd_ctx* ctx, void* data) {
  modal_focus* f = data;
  if (!rd_node_alive(ctx, f->backdrop) || inside(ctx, f->backdrop, rd_ctx_focused(ctx))) return;
  rd_node scope = rd_ctx_focus_scope(ctx);
  rd_node_focus(ctx, (rd_node){0});
  rd_ctx_set_focus_scope(ctx, f->body);
  bool found = rd_ctx_focus_next(ctx, false);
  rd_ctx_set_focus_scope(ctx, scope);
  if (!found) rd_node_focus(ctx, f->card);
}

typedef struct modal_data {
  bool          persistent;
  void*         data;
  rd_destroy_fn destroy;
} modal_data;

static void modal_data_free(void* p) {
  modal_data* d = p;
  if (d->destroy) d->destroy(d->data);
  free(d);
}

static void backdrop_press(rd_ctx* ctx, rd_event* ev, void* data) {
  if (((modal_data*)data)->persistent || !rd_node_eq(ev->target, ev->current)) return;
  rd_event_prevent_default(ev);  // keep the focus that closing restores
  rdx_popup_close(ctx, ev->current);
}

static void close_click(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  rdx_popup_close(ctx, ev->current);
}

rd_node rdx_modal_open(rd_ctx* ctx, const rdx_modal_desc* desc) {
  manager*    m = manager_get(ctx);
  rdx__theme* t = m ? rdx__theme_get(ctx) : NULL;
  if (!t) return (rd_node){0};
  rdx_modal_desc     d  = desc ? *desc : (rdx_modal_desc){0};
  const rdx_classes* cl = &t->classes;

  rd_node backdrop = rd_node_create(ctx);
  rd_node_add_class(ctx, backdrop, cl->modal_backdrop);
  rd_node card = rd_node_create(ctx);
  rd_node_set_role(ctx, card, RD_ROLE_DIALOG);
  rd_node_add_class(ctx, card, cl->modal);
  if (d.width > 0) rd_node_set_style(ctx, card, &(rd_style){.width = rd_px(d.width)});
  if (d.title || !d.persistent) {
    rd_node header = rd_node_create(ctx);
    rd_node_add_class(ctx, header, cl->modal_header);
    if (d.title)
      text_part(ctx, header, d.title, cl->modal_title);
    else
      rd_node_append(ctx, header, rd_node_create(ctx));  // keeps the × at the right
    rd_node_set_style(ctx, rd_node_first_child(ctx, header), &(rd_style){.flex_grow = rd_num(1)});
    if (!d.persistent) {
      rd_node x = rd_node_create_button(ctx, "\xc3\x97", RD_TEXT_NUL);
      rd_node_add_class(ctx, x, cl->modal_close);
      rd_node_on(ctx, x, RD_EVENT_CLICK, close_click, NULL, NULL, 0);
      rd_node_append(ctx, header, x);
    }
    rd_node_append(ctx, card, header);
  }
  rd_node body = rd_node_create(ctx);
  rd_node_add_class(ctx, body, cl->modal_body);
  rd_node_append(ctx, card, body);
  rd_node_append(ctx, backdrop, card);
  modal_data* md = malloc(sizeof *md);
  if (md) *md = (modal_data){d.persistent, d.data, d.destroy};
  if (!md || !rd_node_on(ctx, backdrop, RD_EVENT_POINTER_DOWN, backdrop_press, md, modal_data_free, 0).id) {
    if (md) modal_data_free(md);
    else if (d.destroy) d.destroy(d.data);
    rd_node_destroy(ctx, backdrop);
    return (rd_node){0};
  }
  rd_node_append(ctx, rd_ctx_overlay(ctx), backdrop);
  fade_in(ctx, backdrop, 0.15);  // the card fades with it, and rises
  rd_node_animate(ctx, card, &(rd_style){.margin.top = rd_px(24)}, &(rd_style){.margin.top = rd_px(0)}, 0.2,
                  RD_EASING_EASE_OUT, NULL, NULL);

  popup p = {.node       = backdrop,
             .restore    = rd_ctx_focused(ctx),
             .kind       = P_MODAL,
             .persistent = d.persistent,
             .prev_scope = rd_ctx_focus_scope(ctx),
             .on_close   = d.on_close,
             .data       = d.data};
  if (!push_popup(m, p)) {
    rd_node_destroy(ctx, backdrop);
    return (rd_node){0};
  }
  rd_ctx_set_focus_scope(ctx, backdrop);
  modal_focus* f = malloc(sizeof *f);
  if (f) {
    *f = (modal_focus){backdrop, card, body};
    if (!rd_ctx_set_timer(ctx, 0, 0, focus_first, f, free).index) free(f);
  }
  return body;
}

// Tooltips.

struct tip {
  rd_ctx*  ctx;
  manager* m;
  rd_node  node;
  char*    text;
  rd_timer timer;
};

static void tip_hide(rd_ctx* ctx, tip* t) {
  rd_ctx_clear_timer(ctx, t->timer);
  t->timer = (rd_timer){0};
  prune(ctx, t->m);
  for (int j = (int)t->m->open_len - 1; j >= 0; j--)
    if (t->m->open[j].kind == P_TOOLTIP && rd_node_eq(t->m->open[j].anchor, t->node)) close_at(ctx, t->m, (uint32_t)j);
}

static void tip_show(rd_ctx* ctx, void* data) {
  tip* t   = data;
  t->timer = (rd_timer){0};
  rdx__theme* th = rdx__theme_get(ctx);
  if (!th || !rd_node_alive(ctx, t->node)) return;
  prune(ctx, t->m);
  for (int j = (int)t->m->open_len - 1; j >= 0; j--)  // one tooltip at a time
    if (t->m->open[j].kind == P_TOOLTIP) close_at(ctx, t->m, (uint32_t)j);
  rd_node n = rd_node_create_text(ctx, t->text, RD_TEXT_NUL);
  rd_node_set_role(ctx, n, RD_ROLE_TOOLTIP);
  rd_node_add_class(ctx, n, th->classes.tooltip);
  rd_node_append(ctx, rd_ctx_overlay(ctx), n);
  rd_node_set_anchor(ctx, n, &(rd_anchor){.node = t->node, .side = RD_SIDE_TOP, .align = RD_ALIGN_CENTER, .offset = 6});
  fade_in(ctx, n, 0.1);
  if (!push_popup(t->m, (popup){.node = n, .anchor = t->node, .kind = P_TOOLTIP})) rd_node_destroy(ctx, n);
}

static void tip_event(rd_ctx* ctx, rd_event* ev, void* data) {
  tip* t = data;
  switch (ev->type) {
    case RD_EVENT_POINTER_ENTER:
      rd_ctx_clear_timer(ctx, t->timer);
      t->timer = rd_ctx_set_timer(ctx, TOOLTIP_DELAY, 0, tip_show, t, NULL);
      break;
    case RD_EVENT_FOCUS:
      if (rd_node_states(ctx, t->node) & RD_STATE_FOCUS_VISIBLE) tip_show(ctx, t);
      break;
    default:  // leave, press, blur
      tip_hide(ctx, t);
      break;
  }
}

static void tip_free(void* data) {
  tip* t = data;
  rd_ctx_clear_timer(t->ctx, t->timer);  // a shown tooltip closes at the next pointer move
  free(t->text);
  free(t);
}

bool rdx_tooltip_attach(rd_ctx* ctx, rd_node node, const char* text) {
  manager* m = manager_get(ctx);
  if (!m || !rd_node_alive(ctx, node)) return false;
  char* copy = dup(text ? text : "");
  if (!copy) return false;
  tip* t = list_find(ctx, &m->tips, node);
  if (t) {
    free(t->text);
    t->text = copy;
    return true;
  }
  t = calloc(1, sizeof *t);
  if (!t || !list_put(&m->tips, node, t)) {
    free(t);
    free(copy);
    return false;
  }
  *t = (tip){ctx, m, node, copy, {0}};
  if (!rd_node_on(ctx, node, RD_EVENT_POINTER_ENTER, tip_event, t, tip_free, 0).id) {
    m->tips.len--;
    free(copy);
    free(t);
    return false;
  }
  rd_event_type more[] = {RD_EVENT_POINTER_LEAVE, RD_EVENT_POINTER_DOWN, RD_EVENT_FOCUS, RD_EVENT_BLUR};
  for (int j = 0; j < 4; j++) rd_node_on(ctx, node, more[j], tip_event, t, NULL, 0);
  return true;
}
