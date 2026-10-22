#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "extras_internal.h"
#include <raydom/extras/views.h>

static const float DEFAULT_ROW = 32;
enum { OVERSCAN = 3 };

typedef struct table table;
typedef struct tree  tree;

// A virtualized list: a spacer as tall as every row, and absolutely positioned rows for the ones in
// view, reused as the list scrolls. A layout hook keeps the rows in step with the scroll offset.
typedef struct vlist {
  rd_node       node, spacer;
  int           count, selected;
  float         row_h;
  rdx_row_fn    fill;
  void*         data;
  rd_destroy_fn destroy;
  rd_node*      rows;   // pooled rows; rows[k] shows index[k], or -1 when unused
  int*          index;
  int           row_count, row_cap;
  float         spacer_h;
  bool          refill;  // fill every bound row again at the next update
  rd_role       row_role;
  table*        table;  // set for a table's body, which holds a reference to it
  tree*         tree;   // set for a tree
} vlist;

static const char list_key = 0;

static void vlist_free(void* p);

// The index a row shows. Rows' userdata stays the application's, like every node's.
static int row_index(const vlist* l, rd_node row) {
  for (int k = 0; k < l->row_count; k++)
    if (rd_node_eq(l->rows[k], row)) return l->index[k];
  return -1;
}

static float view_height(const rd_ctx* ctx, const vlist* l) {
  rd_box b = rd_node_box(ctx, l->node);
  return fmaxf(0, rd_node_rect(ctx, l->node).height - b.border.top - b.border.bottom);
}

static void bind(rd_ctx* ctx, vlist* l, int k, int index);
static void row_press(rd_ctx* ctx, rd_event* ev, void* data);

// Brings the rows in line with the scroll offset and size from the layout that just ran.
static void update(rd_ctx* ctx, vlist* l) {
  float h = l->row_h, view = view_height(ctx, l), top = rd_node_scroll(ctx, l->node).y;
  if ((float)l->count * h != l->spacer_h) {
    l->spacer_h = (float)l->count * h;
    rd_node_set_style(ctx, l->spacer, &(rd_style){.height = rd_px(l->spacer_h), .flex_shrink = rd_num(0)});
  }
  int first = (int)floorf(top / h) - OVERSCAN, last = (int)ceilf((top + view) / h) + OVERSCAN;
  if (first < 0) first = 0;
  if (last > l->count - 1) last = l->count - 1;

  // Release rows that left the range; keep the rest where they are.
  for (int k = 0; k < l->row_count; k++)
    if (l->index[k] >= 0 && (l->index[k] < first || l->index[k] > last || l->index[k] >= l->count)) {
      l->index[k] = -1;
      rd_node_set_style(ctx, l->rows[k], &(rd_style){.display = RD_DISPLAY_NONE});
    }
  for (int i = first; i <= last; i++) {
    int at = -1, free_k = -1;
    for (int k = 0; k < l->row_count && at < 0; k++) {
      if (l->index[k] == i) at = k;
      if (l->index[k] < 0 && free_k < 0) free_k = k;
    }
    if (at >= 0) {
      if (l->refill) bind(ctx, l, at, i);
      continue;
    }
    if (free_k < 0) {
      if (l->row_count == l->row_cap) {
        int      cap  = l->row_cap ? l->row_cap * 2 : 32;
        rd_node* rows = realloc(l->rows, (size_t)cap * sizeof *rows);
        if (!rows) return;
        l->rows  = rows;
        int* idx = realloc(l->index, (size_t)cap * sizeof *idx);
        if (!idx) return;
        l->index   = idx;
        l->row_cap = cap;
      }
      rd_node row = rd_node_create(ctx);
      rd_node_add_class(ctx, row, rdx_theme(ctx)->list_row);
      rd_node_set_role(ctx, row, l->row_role);
      rd_node_on(ctx, row, RD_EVENT_POINTER_DOWN, row_press, l, NULL, 0);
      rd_node_append(ctx, l->node, row);
      free_k                = l->row_count++;
      l->rows[free_k]       = row;
      l->index[free_k]      = -1;
    }
    bind(ctx, l, free_k, i);
  }
  l->refill = false;
}

// Each list's layout hook: lives as long as the list's node, and skips while it's detached.
static void hook(rd_ctx* ctx, void* data) { update(ctx, data); }

static vlist* find(const rd_ctx* ctx, rd_node n) { return rd_node_attached(ctx, n, &list_key); }

static void scroll_to(rd_ctx* ctx, vlist* l, int index) {
  if (index < 0 || index >= l->count) return;
  float top = rd_node_scroll(ctx, l->node).y, view = view_height(ctx, l);
  float y = (float)index * l->row_h;
  if (y < top)
    rd_node_set_scroll(ctx, l->node, 0, y);
  else if (y + l->row_h > top + view && view > 0)
    rd_node_set_scroll(ctx, l->node, 0, y + l->row_h - view);
}

static void mark_selection(rd_ctx* ctx, vlist* l) {
  for (int k = 0; k < l->row_count; k++)
    if (l->index[k] >= 0) rd_node_set_states(ctx, l->rows[k], RD_STATE_CHECKED, l->index[k] == l->selected);
}

static bool set_selected(rd_ctx* ctx, vlist* l, int index) {
  if (index < -1 || index >= l->count) return false;
  bool changed = index != l->selected;
  l->selected  = index;
  mark_selection(ctx, l);
  scroll_to(ctx, l, index);
  return changed;
}

static void row_press(rd_ctx* ctx, rd_event* ev, void* data) {
  vlist* l = data;
  if (ev->button != RD_BUTTON_PRIMARY) return;
  int index = row_index(l, ev->current);
  if (index >= 0 && set_selected(ctx, l, index)) rd_node_dispatch(ctx, l->node, RD_EVENT_CHANGE);
}

static bool tree_keys(rd_ctx* ctx, vlist* l, rd_event* ev);

static void list_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  vlist* l = data;
  if (l->tree && tree_keys(ctx, l, ev)) return;
  int page = (int)fmaxf(1, floorf(view_height(ctx, l) / l->row_h) - 1), cur = l->selected, to;
  switch (ev->key) {
    case RD_KEY_DOWN:
      to = cur < 0 ? 0 : cur + 1;
      break;
    case RD_KEY_UP:
      to = cur < 0 ? 0 : cur - 1;
      break;
    case RD_KEY_PAGE_DOWN:
      to = cur + page;
      break;
    case RD_KEY_PAGE_UP:
      to = cur - page;
      break;
    case RD_KEY_HOME:
      to = 0;
      break;
    case RD_KEY_END:
      to = l->count - 1;
      break;
    default:
      return;
  }
  rd_event_prevent_default(ev);
  if (!l->count) return;
  to = to < 0 ? 0 : to >= l->count ? l->count - 1 : to;
  if (set_selected(ctx, l, to)) rd_node_dispatch(ctx, l->node, RD_EVENT_CHANGE);
}

// The list's state is attached to its node. On failure the caller still owns desc->data.
static vlist* list_new(rd_ctx* ctx, const rdx_list_desc* desc, rd_role row_role) {
  const rdx_classes* cl = rdx_theme(ctx);
  vlist*             l  = cl ? calloc(1, sizeof *l) : NULL;
  if (!l) return NULL;
  rdx_list_desc d = desc ? *desc : (rdx_list_desc){0};
  l->count        = d.count > 0 ? d.count : 0;
  l->row_h        = d.row_height > 0 ? d.row_height : DEFAULT_ROW;
  l->fill         = d.fill;
  l->data         = d.data;
  l->destroy      = d.destroy;
  l->selected     = -1;
  l->spacer_h     = -1;
  l->row_role     = row_role;
  rd_node node = l->node = rd_node_create(ctx), spacer = l->spacer = rd_node_create(ctx);
  bool    ok   = rd_node_attach(ctx, node, &list_key, l, vlist_free);
  if (!ok) free(l);
  if (ok && (!rd_node_on(ctx, node, RD_EVENT_KEY_DOWN, list_keys, l, NULL, 0).id || !rd_node_on_layout(ctx, node, hook, l, NULL))) {
    l->destroy = NULL;  // the data stays the caller's
    ok         = false;
  }
  if (!ok) {
    rd_node_destroy(ctx, node);  // with l, if attached
    rd_node_destroy(ctx, spacer);
    return NULL;
  }
  rd_node_add_class(ctx, l->node, cl->list);
  rd_node_set_role(ctx, l->node, RD_ROLE_LIST);
  rd_node_append(ctx, l->node, l->spacer);
  return l;
}

rd_node rdx_list_create(rd_ctx* ctx, const rdx_list_desc* desc) {
  vlist* l = list_new(ctx, desc, RD_ROLE_LIST_ITEM);
  return l ? l->node : (rd_node){0};
}

bool rdx_list_set_count(rd_ctx* ctx, rd_node list, int count) {
  vlist* l = find(ctx, list);
  if (!l || count < 0) return false;
  l->count = count;
  if (l->selected >= count) l->selected = -1;
  l->refill = true;
  update(ctx, l);
  return true;
}

void rdx_list_refresh(rd_ctx* ctx, rd_node list) {
  vlist* l = find(ctx, list);
  if (!l) return;
  l->refill = true;
  update(ctx, l);
}

int rdx_list_selected(const rd_ctx* ctx, rd_node list) {
  vlist* l = find(ctx, list);
  return l ? l->selected : -1;
}

bool rdx_list_select(rd_ctx* ctx, rd_node list, int index) {
  vlist* l = find(ctx, list);
  if (!l || index < -1 || index >= l->count) return false;
  set_selected(ctx, l, index);
  return true;
}

void rdx_list_scroll_to(rd_ctx* ctx, rd_node list, int index) {
  vlist* l = find(ctx, list);
  if (l) scroll_to(ctx, l, index);
}

// Tables.

// Shared by the root, which it's attached to, and the body list, whose rows it fills: freed when both
// are gone, so the application may destroy or keep the body on its own.
struct table {
  rd_node       root, header, body;
  int           refs;
  int           columns;
  rd_tracks     tracks;
  rdx_cell_fn   cell;
  rdx_header_fn on_header;
  void*         data;
  rd_destroy_fn destroy;
};

static const char table_key = 0;

static table* table_find(const rd_ctx* ctx, rd_node root) { return rd_node_attached(ctx, root, &table_key); }

static void header_click(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  table* t = table_find(ctx, rd_node_parent(ctx, rd_node_parent(ctx, ev->current)));  // cell, header, root
  if (!t) return;
  int k = 0;
  for (rd_node n = rd_node_first_child(ctx, t->header); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n), k++)
    if (rd_node_eq(n, ev->current) && t->on_header) t->on_header(ctx, k, t->data);
}

static void table_fill(rd_ctx* ctx, rd_node row, int index, void* data) {
  table* t = data;
  for (int c = 0; c < t->columns; c++) {
    rd_node cell = rd_node_create(ctx);
    rd_node_add_class(ctx, cell, rdx_theme(ctx)->table_cell);
    rd_node_set_role(ctx, cell, RD_ROLE_CELL);
    rd_node_append(ctx, row, cell);
    if (t->cell) t->cell(ctx, cell, index, c, t->data);
  }
}

static void table_release(void* p) {
  table* t = p;
  if (--t->refs > 0) return;
  if (t->destroy) t->destroy(t->data);
  free(t);
}

rd_node rdx_table_create(rd_ctx* ctx, const rdx_table_desc* desc) {
  const rdx_classes* cl = rdx_theme(ctx);
  if (!cl || !desc || desc->column_count <= 0) return (rd_node){0};
  table*    t      = calloc(1, sizeof *t);
  rd_track* tracks = t ? malloc((size_t)desc->column_count * sizeof *tracks) : NULL;
  if (!tracks) {
    free(t);
    return (rd_node){0};
  }
  *t = (table){.columns = desc->column_count, .cell = desc->cell, .on_header = desc->header, .data = desc->data,
               .destroy = desc->destroy};
  for (int c = 0; c < desc->column_count; c++) {
    rd_val w  = desc->columns[c].width;
    tracks[c] = rd_track_of(w.unit == RD_UNIT_UNSET ? rd_fr(1) : w);
  }
  t->tracks = rd_tracks_create(ctx, tracks, (uint32_t)desc->column_count);
  free(tracks);

  vlist* body = list_new(ctx, &(rdx_list_desc){desc->count, desc->row_height, table_fill, t, table_release}, RD_ROLE_ROW);
  if (!body) {
    free(t);
    return (rd_node){0};
  }
  t->refs     = 1;  // the body's
  t->body     = body->node;
  body->table = t;
  rd_node_set_style(ctx, t->body, &(rd_style){.flex_grow = rd_num(1), .flex_basis = rd_px(0)});

  t->root = rd_node_create(ctx);
  t->refs++;
  if (!rd_node_attach(ctx, t->root, &table_key, t, table_release)) {
    t->refs--;
    t->destroy = NULL;  // the data stays the caller's
    rd_node_destroy(ctx, t->root);
    rd_node_destroy(ctx, t->body);  // releases t
    return (rd_node){0};
  }
  t->header = rd_node_create(ctx);
  rd_node_add_class(ctx, t->root, cl->table);
  rd_node_set_role(ctx, t->root, RD_ROLE_TABLE);
  rd_node_add_class(ctx, t->header, cl->table_header);
  rd_node_set_role(ctx, t->header, RD_ROLE_ROW);
  rd_node_set_style(ctx, t->header, &(rd_style){.grid_template_columns = t->tracks});
  for (int c = 0; c < desc->column_count; c++) {
    rd_node h = rd_node_create_text(ctx, desc->columns[c].title ? desc->columns[c].title : "", RD_TEXT_NUL);
    rd_node_add_class(ctx, h, cl->table_header_cell);
    rd_node_set_role(ctx, h, RD_ROLE_COLUMN_HEADER);
    rd_node_on(ctx, h, RD_EVENT_CLICK, header_click, NULL, NULL, 0);
    rd_node_append(ctx, t->header, h);
  }
  rd_node_append(ctx, t->root, t->header);
  rd_node_append(ctx, t->root, t->body);
  return t->root;
}

rd_node rdx_table_list(const rd_ctx* ctx, rd_node root) {
  table* t = table_find(ctx, root);
  return t && rd_node_alive(ctx, t->body) ? t->body : (rd_node){0};
}

// Trees: items in an array, flattened into the visible ones for the list.

typedef struct item {
  char* label;
  int   parent, depth;
  int   first_child, last_child, next;  // -1 for none
  bool  expanded;
} item;

struct tree {
  vlist* list;
  item*  items;
  int    count, cap;
  int    first_root, last_root;
  int*   visible;  // item ids in display order
  int    visible_count, visible_cap;
  int    selected;  // item id
};

static const float INDENT = 16;

static void flatten_from(tree* t, int id) {
  for (; id >= 0; id = t->items[id].next) {
    if (t->visible_count == t->visible_cap) {
      int  cap = t->visible_cap ? t->visible_cap * 2 : 64;
      int* v   = realloc(t->visible, (size_t)cap * sizeof *v);
      if (!v) return;
      t->visible     = v;
      t->visible_cap = cap;
    }
    t->visible[t->visible_count++] = id;
    if (t->items[id].expanded) flatten_from(t, t->items[id].first_child);
  }
}

static int visible_index(const tree* t, int id) {
  for (int j = 0; j < t->visible_count; j++)
    if (t->visible[j] == id) return j;
  return -1;
}

// Rebuilds the visible items; a selected item that's now hidden passes the selection to its visible
// ancestor.
static void reflow(rd_ctx* ctx, tree* t) {
  t->visible_count = 0;
  flatten_from(t, t->first_root);
  vlist* l = t->list;
  l->count = t->visible_count;
  int sel  = t->selected;
  while (sel >= 0 && visible_index(t, sel) < 0) sel = t->items[sel].parent;
  t->selected = sel;
  l->selected = sel >= 0 ? visible_index(t, sel) : -1;
  l->refill   = true;
  update(ctx, l);
}

static void toggle_click(rd_ctx* ctx, rd_event* ev, void* data) {
  tree* t  = data;
  int   id = row_index(t->list, rd_node_parent(ctx, ev->current));
  if (id < 0 || id >= t->visible_count) return;
  id                   = t->visible[id];
  t->items[id].expanded = !t->items[id].expanded;
  reflow(ctx, t);
}

static void tree_fill(rd_ctx* ctx, rd_node row, int index, void* data) {
  tree*              t  = data;
  const rdx_classes* cl = rdx_theme(ctx);
  rdx__theme*        th = rdx__theme_get(ctx);
  const item*        it = &t->items[t->visible[index]];
  rd_node_merge_style(ctx, row, &(rd_style){.padding = {.left = rd_px(8 + INDENT * (float)it->depth)}});
  rd_node toggle = rd_node_create(ctx);
  rd_node_add_class(ctx, toggle, cl->tree_toggle);
  if (it->first_child >= 0) {
    rd_node_set_image(ctx, toggle, it->expanded ? th->chevron : th->chevron_right);
    rd_node_on(ctx, toggle, RD_EVENT_CLICK, toggle_click, t, NULL, 0);
  }
  rd_node label = rd_node_create_text(ctx, it->label, RD_TEXT_NUL);
  rd_node_add_class(ctx, label, cl->tree_label);
  rd_node_append(ctx, row, toggle);
  rd_node_append(ctx, row, label);
}

static void tree_free(void* p) {
  tree* t = p;
  for (int j = 0; j < t->count; j++) free(t->items[j].label);
  free(t->items);
  free(t->visible);
  free(t);
}

static bool tree_keys(rd_ctx* ctx, vlist* l, rd_event* ev) {
  tree* t = l->tree;
  if (t->selected < 0) return false;
  item* it = &t->items[t->selected];
  if (ev->key == RD_KEY_RIGHT && it->first_child >= 0) {
    if (!it->expanded) {
      it->expanded = true;
      reflow(ctx, t);
    } else {
      rdx_tree_select(ctx, l->node, it->first_child);
      rd_node_dispatch(ctx, l->node, RD_EVENT_CHANGE);
    }
  } else if (ev->key == RD_KEY_LEFT) {
    if (it->expanded && it->first_child >= 0) {
      it->expanded = false;
      reflow(ctx, t);
    } else if (it->parent >= 0) {
      rdx_tree_select(ctx, l->node, it->parent);
      rd_node_dispatch(ctx, l->node, RD_EVENT_CHANGE);
    }
  } else if (ev->key == RD_KEY_ENTER && it->first_child >= 0) {
    it->expanded = !it->expanded;
    reflow(ctx, t);
  } else {
    // Up, Down and the rest move the list's selection; keep the item in step.
    return false;
  }
  rd_event_prevent_default(ev);
  return true;
}

static void tree_selection(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ev;
  tree* t     = data;
  int   index = t->list->selected;
  (void)ctx;
  t->selected = index >= 0 && index < t->visible_count ? t->visible[index] : -1;
}

rd_node rdx_tree_create(rd_ctx* ctx, float row_height) {
  tree* t = calloc(1, sizeof *t);
  if (!t) return (rd_node){0};
  t->first_root = t->last_root = t->selected = -1;
  vlist* l = list_new(ctx, &(rdx_list_desc){0, row_height, tree_fill, t, tree_free}, RD_ROLE_TREE_ITEM);
  if (!l) {
    tree_free(t);
    return (rd_node){0};
  }
  t->list = l;
  l->tree = t;
  rd_node_set_role(ctx, l->node, RD_ROLE_TREE);
  // Runs before the application's CHANGE listeners, which are added later: keeps the item id current.
  rd_node_on(ctx, l->node, RD_EVENT_CHANGE, tree_selection, t, NULL, RD_LISTEN_CAPTURE);
  return l->node;
}

static tree* tree_find(const rd_ctx* ctx, rd_node n) {
  vlist* l = find(ctx, n);
  return l ? l->tree : NULL;
}

int rdx_tree_add(rd_ctx* ctx, rd_node node, int parent, const char* label) {
  tree* t = tree_find(ctx, node);
  if (!t || parent < -1 || parent >= t->count) return -1;
  if (t->count == t->cap) {
    int   cap = t->cap ? t->cap * 2 : 64;
    item* p   = realloc(t->items, (size_t)cap * sizeof *p);
    if (!p) return -1;
    t->items = p;
    t->cap   = cap;
  }
  size_t n    = label ? strlen(label) : 0;
  char*  copy = malloc(n + 1);
  if (!copy) return -1;
  memcpy(copy, label ? label : "", n + 1);
  int id       = t->count++;
  t->items[id] = (item){copy, parent, parent >= 0 ? t->items[parent].depth + 1 : 0, -1, -1, -1, false};
  int* first   = parent >= 0 ? &t->items[parent].first_child : &t->first_root;
  int* last    = parent >= 0 ? &t->items[parent].last_child : &t->last_root;
  if (*last >= 0)
    t->items[*last].next = id;
  else
    *first = id;
  *last = id;
  reflow(ctx, t);
  return id;
}

bool rdx_tree_expand(rd_ctx* ctx, rd_node node, int id, bool expanded) {
  tree* t = tree_find(ctx, node);
  if (!t || id < 0 || id >= t->count) return false;
  t->items[id].expanded = expanded;
  reflow(ctx, t);
  return true;
}

bool rdx_tree_expanded(const rd_ctx* ctx, rd_node node, int id) {
  tree* t = tree_find(ctx, node);
  return t && id >= 0 && id < t->count && t->items[id].expanded;
}

int rdx_tree_selected(const rd_ctx* ctx, rd_node node) {
  tree* t = tree_find(ctx, node);
  return t ? t->selected : -1;
}

bool rdx_tree_select(rd_ctx* ctx, rd_node node, int id) {
  tree* t = tree_find(ctx, node);
  if (!t || id < -1 || id >= t->count) return false;
  for (int p = id >= 0 ? t->items[id].parent : -1; p >= 0; p = t->items[p].parent) t->items[p].expanded = true;
  t->selected = id;
  reflow(ctx, t);
  set_selected(ctx, t->list, id >= 0 ? visible_index(t, id) : -1);
  return true;
}

const char* rdx_tree_label(const rd_ctx* ctx, rd_node node, int id) {
  tree* t = tree_find(ctx, node);
  return t && id >= 0 && id < t->count ? t->items[id].label : NULL;
}

int rdx_tree_parent(const rd_ctx* ctx, rd_node node, int id) {
  tree* t = tree_find(ctx, node);
  return t && id >= 0 && id < t->count ? t->items[id].parent : -1;
}

// The list's own state, freed with its node.
static void vlist_free(void* p) {
  vlist* l = p;
  if (l->destroy) l->destroy(l->data);
  free(l->rows);
  free(l->index);
  free(l);
}

static void bind(rd_ctx* ctx, vlist* l, int k, int index) {
  rd_node row = l->rows[k];
  while (!rd_node_is_null(rd_node_first_child(ctx, row))) rd_node_destroy(ctx, rd_node_first_child(ctx, row));
  rd_style st = {.inset = {.top = rd_px((float)index * l->row_h)}, .height = rd_px(l->row_h), .display = RD_DISPLAY_FLEX};
  if (l->table) {
    st.display               = RD_DISPLAY_GRID;
    st.grid_template_columns = l->table->tracks;
    st.column_gap            = rd_px(12);
    st.align_items           = RD_ALIGN_CENTER;
  }
  rd_node_set_style(ctx, row, &st);
  rd_node_set_states(ctx, row, RD_STATE_CHECKED, index == l->selected);
  l->index[k] = index;
  if (l->fill) l->fill(ctx, row, index, l->data);
}

int rdx_list_index_of(const rd_ctx* ctx, rd_node list, rd_node node) {
  vlist* l = find(ctx, list);
  if (!l) return -1;
  // Climb to the list's direct child: a row, or the spacer, which isn't one.
  for (rd_node n = node; !rd_node_is_null(n); n = rd_node_parent(ctx, n)) {
    rd_node p = rd_node_parent(ctx, n);
    if (rd_node_eq(p, l->node)) return row_index(l, n);
  }
  return -1;
}

int rdx_tree_item_of(const rd_ctx* ctx, rd_node node_tree, rd_node node) {
  tree* t = tree_find(ctx, node_tree);
  if (!t) return -1;
  int row = rdx_list_index_of(ctx, node_tree, node);
  return row >= 0 && row < t->visible_count ? t->visible[row] : -1;
}
