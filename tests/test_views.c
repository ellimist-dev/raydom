#include <math.h>
#include <raydom/extras/views.h>
#include <stdio.h>
#include <string.h>

#include "check.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.6f; }

static void count(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)ev;
  ++*(int*)data;
}

static void press(rd_ctx* ctx, rd_key k) {
  rd_ctx_key(ctx, k, true, false, 0);
  rd_ctx_key(ctx, k, false, false, 0);
}

static void click(rd_ctx* ctx, rd_node n) {
  rd_ctx_layout(ctx, 800, 600);
  rd_rect b = rd_node_bounds(ctx, n);
  rd_ctx_pointer(ctx, b.x + b.width / 2, b.y + b.height / 2, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, b.x + b.width / 2, b.y + b.height / 2, 0, 0, 0);
}

static bool shown(rd_ctx* ctx, rd_node n) { return rd_node_computed_style(ctx, n)->display != RD_DISPLAY_NONE; }

static void tabs(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node tabs = rdx_tabs_create(ctx);
  rd_node a = rdx_tabs_add(ctx, tabs, "General"), b = rdx_tabs_add(ctx, tabs, "Network");
  rd_node c       = rdx_tabs_add(ctx, tabs, "About");
  int     changes = 0;
  rd_node_on(ctx, tabs, RD_EVENT_CHANGE, count, &changes, NULL, 0);
  rd_node_append(ctx, rd_ctx_root(ctx), tabs);
  rd_node after = rd_node_create_button(ctx, "after", RD_TEXT_NUL);
  rd_node_append(ctx, rd_ctx_root(ctx), after);
  rd_ctx_layout(ctx, 800, 600);
  rd_node bar = rd_node_first_child(ctx, tabs);
  CHECK(rdx_tabs_selected(ctx, tabs) == 0 && shown(ctx, a) && !shown(ctx, b) && !shown(ctx, c));
  CHECK(rd_node_role(ctx, bar) == RD_ROLE_TAB_LIST && rd_node_role(ctx, a) == RD_ROLE_TAB_PANEL);

  click(ctx, rd_node_last_child(ctx, bar));
  CHECK(rdx_tabs_selected(ctx, tabs) == 2 && shown(ctx, c) && !shown(ctx, a) && changes == 1);
  press(ctx, RD_KEY_RIGHT);  // wraps
  CHECK(rdx_tabs_selected(ctx, tabs) == 0 && changes == 2 && rd_node_eq(rd_ctx_focused(ctx), rd_node_first_child(ctx, bar)));
  press(ctx, RD_KEY_END);
  CHECK(rdx_tabs_selected(ctx, tabs) == 2);
  // Tab leaves the bar in one step.
  press(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), after));
  CHECK(rdx_tabs_select(ctx, tabs, 1) && changes == 3 && !rdx_tabs_select(ctx, tabs, 3));
  rd_ctx_destroy(ctx);
}

static void splits(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node s   = rdx_split_create(ctx, &(rdx_split_desc){.min_first = 100, .min_second = 150});
  rd_node_merge_style(ctx, s, &(rd_style){.width = rd_px(606), .height = rd_px(300)});
  rd_node_append(ctx, rd_ctx_root(ctx), s);
  int changes = 0;
  rd_node_on(ctx, s, RD_EVENT_CHANGE, count, &changes, NULL, 0);
  rd_ctx_layout(ctx, 800, 600);
  rd_node first = rdx_split_pane(ctx, s, 0), second = rdx_split_pane(ctx, s, 1);
  CHECK(near(rd_node_rect(ctx, first).width, 303) && near(rd_node_rect(ctx, second).width, 297));

  // Dragging the handle; the second pane keeps its minimum.
  rd_node handle = rd_node_next_sibling(ctx, first);
  rd_rect h      = rd_node_bounds(ctx, handle);
  rd_ctx_pointer(ctx, h.x + 3, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 203, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_rect(ctx, first).width, 200));
  rd_ctx_pointer(ctx, 790, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 790, 100, 0, 0, 0);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_rect(ctx, second).width, 150) && changes == 1);
  press(ctx, RD_KEY_LEFT);  // focused by the press
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_rect(ctx, second).width, 166) && changes == 2);

  // Resizing keeps the fraction.
  float f = rdx_split_fraction(ctx, s);
  rd_node_merge_style(ctx, s, &(rd_style){.width = rd_px(1006)});
  rd_ctx_layout(ctx, 1200, 600);
  CHECK(near(rd_node_rect(ctx, first).width, f * 1006));
  CHECK(rdx_split_set_fraction(ctx, s, 0.01f) && near(rdx_split_fraction(ctx, s), 0.1f));  // min_first

  rd_node v = rdx_split_create(ctx, &(rdx_split_desc){.vertical = true, .fraction = 0.25f});
  rd_node_merge_style(ctx, v, &(rd_style){.height = rd_px(406)});
  rd_node_append(ctx, rd_ctx_root(ctx), v);
  rd_ctx_layout(ctx, 1200, 1200);
  CHECK(near(rd_node_rect(ctx, rdx_split_pane(ctx, v, 0)).height, 0.25f * 406));
  rd_ctx_destroy(ctx);
}

// A sidebar: a fixed-size, collapsible first pane, at either end.
static void sidebar(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node s   = rdx_split_create(ctx, &(rdx_split_desc){.size = 200, .min_first = 120, .min_second = 100, .collapsible = true});
  rd_node_merge_style(ctx, s, &(rd_style){.width = rd_px(606), .height = rd_px(300)});
  rd_node_append(ctx, rd_ctx_root(ctx), s);
  int changes = 0, inputs = 0;
  rd_node_on(ctx, s, RD_EVENT_CHANGE, count, &changes, NULL, 0);
  rd_node_on(ctx, s, RD_EVENT_INPUT, count, &inputs, NULL, 0);
  rd_ctx_layout(ctx, 800, 600);
  rd_node first = rdx_split_pane(ctx, s, 0), second = rdx_split_pane(ctx, s, 1);
  rd_node handle = rd_node_next_sibling(ctx, first);
  CHECK(near(rd_node_rect(ctx, first).width, 200) && near(rd_node_rect(ctx, second).width, 400));

  // The size is kept in pixels as the splitter grows.
  rd_node_merge_style(ctx, s, &(rd_style){.width = rd_px(806)});
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(near(rd_node_rect(ctx, first).width, 200) && near(rdx_split_size(ctx, s), 200));

  // Dragging below half the minimum collapses the pane; it keeps the size it had before the drag.
  rd_rect h = rd_node_bounds(ctx, handle);
  rd_ctx_pointer(ctx, h.x + 3, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 130, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(near(rd_node_rect(ctx, first).width, 127) && !rdx_split_collapsed(ctx, s));
  rd_ctx_pointer(ctx, 40, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 40, 100, 0, 0, 0);
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(rdx_split_collapsed(ctx, s) && near(rd_node_rect(ctx, second).width, 800) && changes == 1);
  CHECK(inputs == 2);  // each move that changed something: to 127, then shut
  CHECK(near(rdx_split_size(ctx, s), 200));

  // Enter on the focused handle, then a double-click, toggle it; Left at the minimum collapses it.
  press(ctx, RD_KEY_ENTER);
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(!rdx_split_collapsed(ctx, s) && near(rd_node_rect(ctx, first).width, 200) && changes == 2);
  h = rd_node_bounds(ctx, handle);
  for (int j = 0; j < 2; j++) {
    rd_ctx_pointer(ctx, h.x + 3, 100, RD_BUTTON_PRIMARY, 0, 0);
    rd_ctx_pointer(ctx, h.x + 3, 100, 0, 0, 0);
  }
  CHECK(rdx_split_collapsed(ctx, s) && changes == 3);
  CHECK(rdx_split_set_collapsed(ctx, s, false) && rdx_split_set_size(ctx, s, 10));
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(near(rd_node_rect(ctx, first).width, 120));  // the minimum
  press(ctx, RD_KEY_LEFT);
  CHECK(rdx_split_collapsed(ctx, s) && changes == 4);

  // At the right, stacked, and at the bottom: the handle moves the other way.
  rdx_split_set_collapsed(ctx, s, false);
  rdx_split_set_size(ctx, s, 200);
  CHECK(rdx_split_set_direction(ctx, s, false, true));
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(near(rd_node_rect(ctx, first).x, 606) && near(rd_node_rect(ctx, first).width, 200));
  h = rd_node_bounds(ctx, handle);
  rd_ctx_pointer(ctx, h.x + 3, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 503, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 503, 100, 0, 0, 0);
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(near(rd_node_rect(ctx, first).width, 300));
  press(ctx, RD_KEY_LEFT);  // grows a pane at the right
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(near(rd_node_rect(ctx, first).width, 316));
  CHECK(rdx_split_set_direction(ctx, s, true, true));
  rd_ctx_layout(ctx, 1000, 600);
  CHECK(near(rd_node_rect(ctx, first).height, 194) && near(rd_node_rect(ctx, first).y, 106));  // within the minimums
  CHECK(near(rd_node_rect(ctx, first).width, 806));
  CHECK(!rdx_split_set_collapsed(ctx, rdx_split_create(ctx, &(rdx_split_desc){0}), true));  // not collapsible
  rd_ctx_destroy(ctx);
}

static int fills = 0;
static void fill_row(rd_ctx* ctx, rd_node row, int index, void* data) {
  (void)data;
  fills++;
  char buf[32];
  snprintf(buf, sizeof buf, "row %d", index);
  rd_node t = rd_node_create_text(ctx, buf, RD_TEXT_NUL);
  rd_node_append(ctx, row, t);
}

static rd_node row_showing(rd_ctx* ctx, rd_node list, int index) {
  char want[32];
  snprintf(want, sizeof want, "row %d", index);
  for (rd_node r = rd_node_first_child(ctx, list); !rd_node_is_null(r); r = rd_node_next_sibling(ctx, r)) {
    rd_node t = rd_node_first_child(ctx, r);
    if (!rd_node_is_null(t) && rd_node_text(ctx, t, NULL) && strcmp(rd_node_text(ctx, t, NULL), want) == 0 &&
        rd_node_computed_style(ctx, r)->display != RD_DISPLAY_NONE)
      return r;
  }
  return (rd_node){0};
}

static void lists(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node list = rdx_list_create(ctx, &(rdx_list_desc){.count = 10000, .row_height = 20, .fill = fill_row});
  rd_node_merge_style(ctx, list, &(rd_style){.height = rd_px(200)});
  rd_node_append(ctx, rd_ctx_root(ctx), list);
  int changes = 0;
  rd_node_on(ctx, list, RD_EVENT_CHANGE, count, &changes, NULL, 0);
  rd_ctx_layout(ctx, 800, 600);

  // Only the rows in view, plus a few, exist; the content is as tall as every row.
  CHECK(rd_node_child_count(ctx, list) < 20 && near(rd_node_content_size(ctx, list).height, 200000));
  rd_node r0 = row_showing(ctx, list, 0);
  CHECK(!rd_node_is_null(r0) && near(rd_node_rect(ctx, r0).y, 0) && near(rd_node_rect(ctx, r0).height, 20));
  CHECK(!rd_node_userdata(ctx, r0));  // the application's, not the list's

  // Scrolling binds the rows in view in the same frame, reusing nodes.
  uint32_t nodes = rd_ctx_node_count(ctx);
  rd_node_set_scroll(ctx, list, 0, 100000);
  rd_ctx_layout(ctx, 800, 600);
  rd_node r5000 = row_showing(ctx, list, 5000);
  // Mid-list keeps overscan rows on both sides, so a few more rows than at the top; nothing else grows.
  CHECK(!rd_node_is_null(r5000) && near(rd_node_bounds(ctx, r5000).y, 0) && rd_ctx_node_count(ctx) <= nodes + 8);
  CHECK(rd_node_is_null(row_showing(ctx, list, 0)));

  // Selection: click, keys, scrolled into view.
  click(ctx, r5000);
  CHECK(rdx_list_selected(ctx, list) == 5000 && changes == 1 && (rd_node_states(ctx, r5000) & RD_STATE_CHECKED));
  press(ctx, RD_KEY_DOWN);
  press(ctx, RD_KEY_PAGE_DOWN);
  CHECK(rdx_list_selected(ctx, list) == 5010 && changes == 3);  // a page is the rows in view, less one
  press(ctx, RD_KEY_END);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rdx_list_selected(ctx, list) == 9999 && near(rd_node_scroll(ctx, list).y, 200000 - 200));
  CHECK(!rd_node_is_null(row_showing(ctx, list, 9999)));

  // Count and refresh.
  int before = fills;
  rdx_list_refresh(ctx, list);
  CHECK(fills > before);
  CHECK(rdx_list_set_count(ctx, list, 5) && rdx_list_selected(ctx, list) == -1);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_scroll(ctx, list).y, 0) && !rd_node_is_null(row_showing(ctx, list, 4)) &&
        rd_node_is_null(row_showing(ctx, list, 5)));
  CHECK(rdx_list_select(ctx, list, 2) && changes == 4);  // setters fire nothing
  rd_ctx_destroy(ctx);
}

static int header_col = -1;
static void on_header(rd_ctx* ctx, int col, void* data) {
  (void)ctx;
  (void)data;
  header_col = col;
}

static void fill_cell(rd_ctx* ctx, rd_node cell, int row, int col, void* data) {
  (void)data;
  char buf[32];
  snprintf(buf, sizeof buf, "%d:%d", row, col);
  rd_node_set_text(ctx, cell, buf, RD_TEXT_NUL);
}

static void tables(void) {
  rd_ctx*          ctx  = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  const rdx_column cols[] = {{"Name", rd_px(200)}, {"Size", rd_fr(1)}, {"Kind", rd_fr(2)}};
  rd_node          t    = rdx_table_create(ctx, &(rdx_table_desc){cols, 3, 1000, 24, fill_cell, on_header, NULL, NULL});
  rd_node_merge_style(ctx, t, &(rd_style){.width = rd_px(626), .height = rd_px(300)});
  rd_node_append(ctx, rd_ctx_root(ctx), t);
  rd_ctx_layout(ctx, 800, 600);
  rd_node header = rd_node_first_child(ctx, t), body = rdx_table_list(ctx, t);
  CHECK(rd_node_role(ctx, t) == RD_ROLE_TABLE && rd_node_role(ctx, body) == RD_ROLE_LIST);

  // Rows share the header's columns.
  rd_node row = rd_node_is_null(body) ? body : rd_node_next_sibling(ctx, rd_node_first_child(ctx, body));
  rd_node c1  = rd_node_next_sibling(ctx, rd_node_first_child(ctx, row));
  rd_node h1  = rd_node_next_sibling(ctx, rd_node_first_child(ctx, header));
  CHECK(strcmp(rd_node_text(ctx, c1, NULL), "0:1") == 0);
  CHECK(near(rd_node_bounds(ctx, c1).x, rd_node_bounds(ctx, h1).x) && near(rd_node_rect(ctx, c1).width, rd_node_rect(ctx, h1).width));
  CHECK(near(rd_node_rect(ctx, rd_node_first_child(ctx, row)).width, 200));

  click(ctx, rd_node_last_child(ctx, header));
  CHECK(header_col == 2);

  // Any node inside a row maps to its index, also after the row is reused for another index.
  rd_node cell = rd_node_last_child(ctx, row);
  CHECK(rdx_list_index_of(ctx, body, cell) == 0 && rdx_list_index_of(ctx, body, row) == 0);
  CHECK(rdx_list_index_of(ctx, body, rd_node_first_child(ctx, body)) == -1);  // the spacer isn't a row
  CHECK(rdx_list_index_of(ctx, body, header) == -1 && rdx_list_index_of(ctx, t, cell) == -1);
  rd_node_set_scroll(ctx, body, 0, 24 * 500);
  rd_ctx_layout(ctx, 800, 600);
  int shown = rdx_list_index_of(ctx, body, row);
  CHECK(shown >= 497 && shown <= 520);
  char want[16];
  snprintf(want, sizeof want, "%d:2", shown);
  CHECK(strcmp(rd_node_text(ctx, rd_node_last_child(ctx, row), NULL), want) == 0);
  rd_ctx_destroy(ctx);
}

static int  table_freed = 0;
static void count_free(void* data) {
  (void)data;
  table_freed++;
}

// The body can be destroyed or kept apart from the table; the data goes once both are gone.
static void table_ownership(void) {
  rd_ctx*          ctx    = rd_ctx_create();
  const rdx_column cols[] = {{"Name", rd_px(100)}};
  rd_node          t = rdx_table_create(ctx, &(rdx_table_desc){cols, 1, 10, 24, fill_cell, on_header, NULL, count_free});
  rd_node          body = rdx_table_list(ctx, t);
  rd_node_destroy(ctx, body);
  CHECK(rd_node_is_null(rdx_table_list(ctx, t)) && table_freed == 0);
  click(ctx, rd_node_first_child(ctx, rd_node_first_child(ctx, t)));  // a header click still works
  rd_node_destroy(ctx, t);
  CHECK(table_freed == 1);

  t    = rdx_table_create(ctx, &(rdx_table_desc){cols, 1, 10, 24, fill_cell, on_header, NULL, count_free});
  body = rdx_table_list(ctx, t);
  rd_node_detach(ctx, body);
  rd_node_destroy(ctx, t);
  CHECK(table_freed == 1);
  rd_node_merge_style(ctx, body, &(rd_style){.height = rd_px(100)});
  rd_node_append(ctx, rd_ctx_root(ctx), body);
  rdx_list_refresh(ctx, body);  // still fills its rows
  rd_ctx_layout(ctx, 800, 600);
  rd_node_destroy(ctx, body);
  CHECK(table_freed == 2);

  // A part moved out of its component finds no state: its listeners do nothing.
  rd_node tabs = rdx_tabs_create(ctx);
  rdx_tabs_add(ctx, tabs, "One");
  rd_node bar = rd_node_first_child(ctx, tabs);
  rd_node_append(ctx, rd_ctx_root(ctx), bar);
  rd_node_destroy(ctx, tabs);
  rd_node_focus(ctx, rd_node_first_child(ctx, bar));
  press(ctx, RD_KEY_RIGHT);
  rd_node split  = rdx_split_create(ctx, &(rdx_split_desc){.collapsible = true});
  rd_node handle = rd_node_next_sibling(ctx, rdx_split_pane(ctx, split, 0));
  rd_node_append(ctx, rd_ctx_root(ctx), handle);
  rd_node_destroy(ctx, split);
  rd_node_focus(ctx, handle);
  press(ctx, RD_KEY_ENTER);
  rd_ctx_destroy(ctx);
}

static void trees(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node tree = rdx_tree_create(ctx, 22);
  rd_node_merge_style(ctx, tree, &(rd_style){.height = rd_px(300)});
  rd_node_append(ctx, rd_ctx_root(ctx), tree);
  int src  = rdx_tree_add(ctx, tree, -1, "src");
  int docs = rdx_tree_add(ctx, tree, -1, "docs");
  int main_c = rdx_tree_add(ctx, tree, src, "main.c");
  int lib    = rdx_tree_add(ctx, tree, src, "lib");
  int util   = rdx_tree_add(ctx, tree, lib, "util.c");
  rdx_tree_add(ctx, tree, docs, "README.md");
  int changes = 0;
  rd_node_on(ctx, tree, RD_EVENT_CHANGE, count, &changes, NULL, 0);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_role(ctx, tree) == RD_ROLE_TREE && rdx_tree_selected(ctx, tree) == -1);
  CHECK(strcmp(rdx_tree_label(ctx, tree, util), "util.c") == 0 && rdx_tree_parent(ctx, tree, util) == lib);

  // Collapsed: two rows. Keyboard: Down selects src, Right expands, Right moves to the first child.
  rd_node_focus(ctx, tree);
  press(ctx, RD_KEY_DOWN);
  CHECK(rdx_tree_selected(ctx, tree) == src && changes == 1);
  press(ctx, RD_KEY_RIGHT);
  CHECK(rdx_tree_expanded(ctx, tree, src) && changes == 1);
  press(ctx, RD_KEY_RIGHT);
  CHECK(rdx_tree_selected(ctx, tree) == main_c && changes == 2);
  press(ctx, RD_KEY_DOWN);
  CHECK(rdx_tree_selected(ctx, tree) == lib);
  press(ctx, RD_KEY_LEFT);  // lib is collapsed: to the parent
  CHECK(rdx_tree_selected(ctx, tree) == src);
  press(ctx, RD_KEY_LEFT);  // src is expanded: collapse
  CHECK(!rdx_tree_expanded(ctx, tree, src) && rdx_tree_selected(ctx, tree) == src);

  // Selecting a hidden item expands its ancestors; collapsing an ancestor moves the selection up.
  CHECK(rdx_tree_select(ctx, tree, util) && rdx_tree_expanded(ctx, tree, lib) && rdx_tree_expanded(ctx, tree, src));
  rd_ctx_layout(ctx, 800, 600);
  rdx_tree_expand(ctx, tree, src, false);
  CHECK(rdx_tree_selected(ctx, tree) == src);

  // Rows indent by depth; the chevron toggles.
  rdx_tree_expand(ctx, tree, src, true);
  rd_ctx_layout(ctx, 800, 600);
  rd_node row_src = rd_node_next_sibling(ctx, rd_node_first_child(ctx, tree));
  rd_node toggle  = rd_node_first_child(ctx, row_src);
  CHECK(rdx_tree_item_of(ctx, tree, toggle) == src && rdx_list_index_of(ctx, tree, toggle) == 0);
  rd_node row_main = rd_node_next_sibling(ctx, row_src);
  CHECK(rdx_tree_item_of(ctx, tree, rd_node_last_child(ctx, row_main)) == main_c);  // the label
  click(ctx, toggle);
  CHECK(!rdx_tree_expanded(ctx, tree, src));
  rd_ctx_destroy(ctx);
}

int main(void) {
  tabs();
  splits();
  table_ownership();
  sidebar();
  lists();
  tables();
  trees();
  return TEST_RESULT();
}
