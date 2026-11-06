#include <math.h>
#include <raydom/extras/popups.h>
#include <raydom/extras/theme.h>
#include <string.h>

#include "check.h"

static void press_key(rd_ctx* ctx, rd_key k) {
  rd_ctx_key(ctx, k, true, false, 0);
  rd_ctx_key(ctx, k, false, false, 0);
}

static void click_at(rd_ctx* ctx, float x, float y, uint8_t button) {
  rd_ctx_pointer(ctx, x, y, button, 0, 0);
  rd_ctx_pointer(ctx, x, y, 0, 0, 0);
}

static void click(rd_ctx* ctx, rd_node n) {
  rd_ctx_layout(ctx, 800, 600);
  rd_rect b = rd_node_bounds(ctx, n);
  click_at(ctx, b.x + b.width / 2, b.y + b.height / 2, RD_BUTTON_PRIMARY);
}

static int chosen = -2;
static void on_choose(rd_ctx* ctx, int index, void* data) {
  (void)ctx;
  (void)data;
  chosen = index;
}

static rd_node nth_item(rd_ctx* ctx, rd_node menu, int k) {
  int j = 0;
  for (rd_node n = rd_node_first_child(ctx, menu); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n))
    if (rd_node_role(ctx, n) == RD_ROLE_MENU_ITEM && j++ == k) return n;
  return (rd_node){0};
}

static const rdx_menu_item ITEMS[] = {{.label = "Cut", .shortcut = "Ctrl+X"}, {.label = "Copy", .disabled = true}, {0}, {.label = "Paste"}};

static void menus(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node button = rd_node_create_button(ctx, "Edit", RD_TEXT_NUL);
  rd_node_append(ctx, rd_ctx_root(ctx), button);
  rd_ctx_layout(ctx, 800, 600);
  rd_node_focus(ctx, button);

  rd_node menu = rdx_menu_open(ctx, &(rdx_menu_desc){ITEMS, 4, {.node = button}, on_choose, NULL, NULL});
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), menu) && rd_node_role(ctx, menu) == RD_ROLE_MENU);
  CHECK(!rd_node_userdata(ctx, nth_item(ctx, menu, 0)));  // the application's, not the menu's
  CHECK(rd_node_bounds(ctx, menu).y >= rd_node_bounds(ctx, button).y + rd_node_bounds(ctx, button).height);

  // Arrows skip the disabled item and the separator, and wrap; Enter chooses by index into items.
  press_key(ctx, RD_KEY_DOWN);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, menu, 0)));
  press_key(ctx, RD_KEY_DOWN);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, menu, 2)));
  press_key(ctx, RD_KEY_DOWN);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, menu, 0)));
  press_key(ctx, RD_KEY_END);
  press_key(ctx, RD_KEY_ENTER);
  CHECK(chosen == 3 && !rd_node_alive(ctx, menu) && rd_node_eq(rd_ctx_focused(ctx), button));

  // Dismissal: outside press, Escape, Tab; none call fn.
  chosen = -2;
  menu   = rdx_menu_open(ctx, &(rdx_menu_desc){ITEMS, 4, {.node = button}, on_choose, NULL, NULL});
  rd_ctx_layout(ctx, 800, 600);
  click_at(ctx, 700, 500, RD_BUTTON_PRIMARY);
  CHECK(!rd_node_alive(ctx, menu) && chosen == -2);
  menu = rdx_menu_open(ctx, &(rdx_menu_desc){ITEMS, 4, {.node = button}, on_choose, NULL, NULL});
  press_key(ctx, RD_KEY_ESCAPE);
  CHECK(!rd_node_alive(ctx, menu));
  menu = rdx_menu_open(ctx, &(rdx_menu_desc){ITEMS, 4, {.node = button}, on_choose, NULL, NULL});
  press_key(ctx, RD_KEY_TAB);
  CHECK(!rd_node_alive(ctx, menu) && chosen == -2);

  // Mouse: hovering focuses an item; clicking chooses; a disabled item does nothing.
  menu = rdx_menu_open(ctx, &(rdx_menu_desc){ITEMS, 4, {.node = button}, on_choose, NULL, NULL});
  rd_ctx_layout(ctx, 800, 600);
  rd_rect r = rd_node_bounds(ctx, nth_item(ctx, menu, 0));
  rd_ctx_pointer(ctx, r.x + 5, r.y + 5, 0, 0, 0);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, menu, 0)));
  click(ctx, nth_item(ctx, menu, 1));
  CHECK(rd_node_alive(ctx, menu) && chosen == -2);
  click(ctx, nth_item(ctx, menu, 0));
  CHECK(chosen == 0 && !rd_node_alive(ctx, menu));

  // Context menu: secondary press opens at the pointer.
  rd_node area = rd_node_create(ctx);
  rd_node_set_style(ctx, area, &(rd_style){.height = rd_px(200)});
  rd_node_append(ctx, rd_ctx_root(ctx), area);
  CHECK(rdx_context_menu_attach(ctx, area, ITEMS, 4, on_choose, NULL, NULL));
  rd_ctx_layout(ctx, 800, 600);
  rd_rect a = rd_node_bounds(ctx, area);
  click_at(ctx, a.x + 300, a.y + 50, RD_BUTTON_PRIMARY);
  CHECK(rd_node_child_count(ctx, rd_ctx_overlay(ctx)) == 0);
  click_at(ctx, a.x + 300, a.y + 50, RD_BUTTON_SECONDARY);
  rd_node ctxmenu = rd_node_first_child(ctx, rd_ctx_overlay(ctx));
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_bounds(ctx, ctxmenu).x == a.x + 300 && rd_node_bounds(ctx, ctxmenu).y == a.y + 50);
  CHECK(rdx_popup_close(ctx, nth_item(ctx, ctxmenu, 0)) && !rd_node_alive(ctx, ctxmenu));
  CHECK(!rdx_popup_close(ctx, area));
  rd_ctx_destroy(ctx);
}

static void selects(void) {
  rd_ctx*           ctx  = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  const char* const opts[] = {"Red", "Green", "Blue"};
  rd_node           sel    = rdx_select_create(ctx, opts, 3, "Pick a color");
  int               changes = 0;
  rd_node_append(ctx, rd_ctx_root(ctx), sel);
  rd_ctx_layout(ctx, 800, 600);
  rd_node value = rd_node_first_child(ctx, sel);
  CHECK(strcmp(rd_node_text(ctx, value, NULL), "Pick a color") == 0 && rdx_select_selected(ctx, sel) == -1);
  CHECK(rd_node_role(ctx, sel) == RD_ROLE_COMBO_BOX);

  click(ctx, sel);
  rd_node list = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_role(ctx, list) == RD_ROLE_MENU && rd_node_bounds(ctx, list).width == rd_node_bounds(ctx, sel).width);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, list, 0)));
  click(ctx, nth_item(ctx, list, 2));
  CHECK(rdx_select_selected(ctx, sel) == 2 && !rd_node_alive(ctx, list));
  CHECK(strcmp(rd_node_text(ctx, value, NULL), "Blue") == 0 && rd_node_eq(rd_ctx_focused(ctx), sel));

  // Reopening focuses the chosen option, which is checked; clicking the select again closes the list.
  click(ctx, sel);
  list = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, list, 2)));
  CHECK(rd_node_states(ctx, nth_item(ctx, list, 2)) & RD_STATE_CHECKED);
  click(ctx, sel);
  CHECK(!rd_node_alive(ctx, list));

  // Arrows on the closed select change the choice and fire CHANGE; setters don't.
  extern void count_change(rd_ctx*, rd_event*, void*);
  rd_node_on(ctx, sel, RD_EVENT_CHANGE, count_change, &changes, NULL, 0);
  rd_node_focus(ctx, sel);
  press_key(ctx, RD_KEY_UP);
  CHECK(rdx_select_selected(ctx, sel) == 1 && changes == 1);
  press_key(ctx, RD_KEY_UP);
  press_key(ctx, RD_KEY_UP);  // stops at the first
  CHECK(rdx_select_selected(ctx, sel) == 0 && changes == 2);
  CHECK(rdx_select_set(ctx, sel, -1) && changes == 2 && strcmp(rd_node_text(ctx, value, NULL), "Pick a color") == 0);
  CHECK(!rdx_select_set(ctx, sel, 3) && rdx_select_selected(ctx, rd_ctx_root(ctx)) == -1);
  rd_ctx_destroy(ctx);
}

void count_change(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)ev;
  ++*(int*)data;
}

static int closed = 0;
static void on_close(rd_ctx* ctx, void* data) {
  (void)ctx;
  (void)data;
  closed++;
}

static void modals(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_ctx_set_time(ctx, 0);
  rd_node page = rd_node_create_button(ctx, "Delete...", RD_TEXT_NUL);
  rd_node_append(ctx, rd_ctx_root(ctx), page);
  rd_ctx_layout(ctx, 800, 600);
  rd_node_focus(ctx, page);

  rd_node body   = rdx_modal_open(ctx, &(rdx_modal_desc){.title = "Delete file?", .on_close = on_close});
  rd_node cancel = rd_node_create_button(ctx, "Cancel", RD_TEXT_NUL);
  rd_node ok     = rd_node_create_button(ctx, "Delete", RD_TEXT_NUL);
  rd_node_append(ctx, body, cancel);
  rd_node_append(ctx, body, ok);
  rd_ctx_layout(ctx, 800, 600);
  rd_ctx_set_time(ctx, 0.1);  // focus moves in once the body is filled
  CHECK(rd_node_eq(rd_ctx_focused(ctx), cancel));

  // Tab cycles inside: ×, Cancel, Delete.
  press_key(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), ok));
  press_key(ctx, RD_KEY_TAB);
  press_key(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), cancel));

  // Presses inside the card don't close it; on the backdrop they do. Focus returns to the page.
  click(ctx, body);
  CHECK(rd_node_alive(ctx, body) && closed == 0);
  click_at(ctx, 10, 10, RD_BUTTON_PRIMARY);
  CHECK(!rd_node_alive(ctx, body) && closed == 1 && rd_node_eq(rd_ctx_focused(ctx), page));
  CHECK(rd_node_is_null(rd_ctx_focus_scope(ctx)));

  // Escape closes; a persistent modal ignores Escape and the backdrop.
  body = rdx_modal_open(ctx, &(rdx_modal_desc){.on_close = on_close});
  rd_ctx_set_time(ctx, 0.2);
  press_key(ctx, RD_KEY_ESCAPE);
  CHECK(!rd_node_alive(ctx, body) && closed == 2);
  body = rdx_modal_open(ctx, &(rdx_modal_desc){.persistent = true});
  rd_ctx_layout(ctx, 800, 600);
  rd_ctx_set_time(ctx, 0.3);
  press_key(ctx, RD_KEY_ESCAPE);
  click_at(ctx, 10, 10, RD_BUTTON_PRIMARY);
  CHECK(rd_node_alive(ctx, body));
  CHECK(rdx_popup_close(ctx, body) && !rd_node_alive(ctx, body));
  rd_ctx_destroy(ctx);
}

static void tooltips(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_ctx_set_time(ctx, 0);
  rd_node save = rd_node_create_button(ctx, "Save", RD_TEXT_NUL);
  rd_node_set_style(ctx, save, &(rd_style){.margin = rd_edges_all(rd_px(100))});
  rd_node_append(ctx, rd_ctx_root(ctx), save);
  CHECK(rdx_tooltip_attach(ctx, save, "Save the file"));
  rd_ctx_layout(ctx, 800, 600);
  rd_rect  b       = rd_node_bounds(ctx, save);
  rd_node  overlay = rd_ctx_overlay(ctx);

  rd_ctx_pointer(ctx, b.x + 5, b.y + 5, 0, 0, 0);
  rd_ctx_set_time(ctx, 0.3);
  CHECK(rd_node_child_count(ctx, overlay) == 0);
  rd_ctx_set_time(ctx, 0.6);
  rd_node tip = rd_node_first_child(ctx, overlay);
  CHECK(rd_node_role(ctx, tip) == RD_ROLE_TOOLTIP && strcmp(rd_node_text(ctx, tip, NULL), "Save the file") == 0);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_bounds(ctx, tip).y + rd_node_bounds(ctx, tip).height <= b.y);  // above
  rd_ctx_pointer(ctx, 700, 500, 0, 0, 0);
  CHECK(!rd_node_alive(ctx, tip));

  // Keyboard focus shows it at once; replacing the text; the owner going away closes it.
  CHECK(rdx_tooltip_attach(ctx, save, "Save (Ctrl+S)"));
  rd_ctx_focus_next(ctx, false);
  tip = rd_node_first_child(ctx, overlay);
  CHECK(rd_node_alive(ctx, tip) && strcmp(rd_node_text(ctx, tip, NULL), "Save (Ctrl+S)") == 0);
  rd_node_destroy(ctx, save);
  rd_ctx_pointer(ctx, 701, 500, 0, 0, 0);
  CHECK(!rd_node_alive(ctx, tip));
  rd_ctx_set_time(ctx, 5);
  rd_ctx_destroy(ctx);
}

static int  freed = 0;
static void count_free(void* data) {
  (void)data;
  freed++;
}

// A call that fails keeps nothing: the data is still the caller's.
static void failures(void) {
  rd_ctx*             ctx    = rd_ctx_create();
  rd_node             gone   = rd_node_create(ctx);
  const rdx_menu_item item[] = {{.label = "Copy"}};
  rd_node_destroy(ctx, gone);
  CHECK(!rdx_context_menu_attach(ctx, gone, item, 1, on_choose, NULL, count_free) && freed == 0);
  rd_node target = rd_node_create(ctx);
  CHECK(rdx_context_menu_attach(ctx, target, item, 1, on_choose, NULL, count_free));
  rd_node_destroy(ctx, target);
  CHECK(freed == 1);
  rd_ctx_destroy(ctx);
}

// A select list or context menu whose owner is destroyed while it's open does nothing when chosen from,
// and the orphaned list closes at the next input.
static void outlived(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  const char* const opts[] = {"One", "Two"};
  rd_node           sel    = rdx_select_create(ctx, opts, 2, "pick");
  rd_node_append(ctx, rd_ctx_root(ctx), sel);
  click(ctx, sel);
  rd_node list = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  rd_ctx_layout(ctx, 800, 600);
  rd_rect item = rd_node_bounds(ctx, nth_item(ctx, list, 1));
  rd_node_destroy(ctx, sel);
  CHECK(rd_node_alive(ctx, list));
  click_at(ctx, item.x + 4, item.y + item.height / 2, RD_BUTTON_PRIMARY);
  CHECK(!rd_node_alive(ctx, list));

  sel = rdx_select_create(ctx, opts, 2, "pick");
  rd_node_append(ctx, rd_ctx_root(ctx), sel);
  click(ctx, sel);
  list = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  rd_node_destroy(ctx, sel);
  rd_ctx_pointer(ctx, 700, 500, 0, 0, 0);
  CHECK(!rd_node_alive(ctx, list));

  sel = rdx_select_create(ctx, opts, 2, "pick");
  rd_node_append(ctx, rd_ctx_root(ctx), sel);
  click(ctx, sel);
  list = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  click(ctx, nth_item(ctx, list, 1));
  CHECK(rdx_select_selected(ctx, sel) == 1);

  rd_node row = rd_node_create(ctx);
  rd_node_set_style(ctx, row, &(rd_style){.width = rd_px(200), .height = rd_px(40)});
  rd_node_append(ctx, rd_ctx_root(ctx), row);
  const rdx_menu_item items[] = {{.label = "Delete"}};
  freed                       = 0;
  CHECK(rdx_context_menu_attach(ctx, row, items, 1, on_choose, NULL, count_free));
  rd_ctx_layout(ctx, 800, 600);
  rd_rect r = rd_node_bounds(ctx, row);
  click_at(ctx, r.x + 10, r.y + 10, RD_BUTTON_SECONDARY);
  rd_node menu = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  chosen       = -2;
  click(ctx, nth_item(ctx, menu, 0));
  CHECK(chosen == 0 && !rd_node_alive(ctx, menu));
  click_at(ctx, r.x + 10, r.y + 10, RD_BUTTON_SECONDARY);
  menu = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  rd_node_destroy(ctx, row);
  CHECK(freed == 1 && rd_node_alive(ctx, menu));
  chosen = -2;
  click(ctx, nth_item(ctx, menu, 0));
  CHECK(chosen == -2 && !rd_node_alive(ctx, menu));
  rd_ctx_destroy(ctx);
}


static void headings(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  const rdx_menu_item items[] = {{.label = "Edit", .heading = true}, {.label = "Cut"}, {0}, {.label = "View", .heading = true, .shortcut = "ignored"}, {.label = "Zoom"}};
  chosen                      = -2;
  rd_node menu = rdx_menu_open(ctx, &(rdx_menu_desc){items, 5, {.x = 10, .y = 10}, on_choose, NULL, NULL});
  rd_ctx_layout(ctx, 800, 600);

  // One child per entry, the heading a text of its own, not an item.
  CHECK(rd_node_child_count(ctx, menu) == 5);
  rd_node heading = rd_node_first_child(ctx, menu);
  size_t  len;
  CHECK(!strcmp(rd_node_text(ctx, heading, &len), "Edit"));
  CHECK(rd_node_role(ctx, heading) == RD_ROLE_HEADING);
  CHECK(rd_node_has_class(ctx, heading, rdx_theme(ctx)->menu_heading));

  // The keyboard goes around it: Down from the top lands on the first real item, and wraps past the second heading.
  press_key(ctx, RD_KEY_DOWN);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, menu, 0)));
  press_key(ctx, RD_KEY_DOWN);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, menu, 1)));
  press_key(ctx, RD_KEY_DOWN);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, menu, 0)));

  // Pressing or hovering it chooses nothing and leaves the menu open.
  rd_rect b = rd_node_bounds(ctx, heading);
  rd_ctx_pointer(ctx, b.x + 2, b.y + 2, 0, 0, 0);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), nth_item(ctx, menu, 0)));
  click(ctx, heading);
  CHECK(rd_node_alive(ctx, menu) && chosen == -2);

  // Indices count headings and separators: "Zoom" is the fifth entry.
  click(ctx, nth_item(ctx, menu, 1));
  CHECK(chosen == 4 && !rd_node_alive(ctx, menu));

  // Context menus copy the flag.
  rd_node row = rd_node_create(ctx);
  rd_node_set_style(ctx, row, &(rd_style){.width = rd_px(200), .height = rd_px(40)});
  rd_node_append(ctx, rd_ctx_root(ctx), row);
  CHECK(rdx_context_menu_attach(ctx, row, items, 5, on_choose, NULL, NULL));
  rd_ctx_layout(ctx, 800, 600);
  rd_rect rb = rd_node_bounds(ctx, row);
  click_at(ctx, rb.x + 5, rb.y + 5, RD_BUTTON_SECONDARY);
  rd_ctx_layout(ctx, 800, 600);
  rd_node open = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  CHECK(rd_node_role(ctx, rd_node_first_child(ctx, open)) == RD_ROLE_HEADING);
  rd_ctx_destroy(ctx);
}

static void select_list_width(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  const char* const shorts[] = {"a", "b"};
  const char* const longs[]  = {"a", "an option much longer than the select it opens from"};
  rd_node           narrow = rdx_select_create(ctx, shorts, 2, NULL), wide = rdx_select_create(ctx, longs, 2, NULL);
  rd_node_append(ctx, rd_ctx_root(ctx), narrow);
  rd_node_append(ctx, rd_ctx_root(ctx), wide);
  rd_ctx_layout(ctx, 800, 600);
  rd_node_set_style(ctx, wide, &(rd_style){.width = rd_px(160)});

  // Short options: exactly the select's width.
  click(ctx, narrow);
  rd_ctx_layout(ctx, 800, 600);
  rd_node list = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  CHECK(fabsf(rd_node_bounds(ctx, list).width - rd_node_bounds(ctx, narrow).width) < 0.01f);
  press_key(ctx, RD_KEY_ESCAPE);

  // A long option: the list grows so that it shows whole.
  click(ctx, wide);
  rd_ctx_layout(ctx, 800, 600);
  list = rd_node_last_child(ctx, rd_ctx_overlay(ctx));
  rd_rect lb = rd_node_bounds(ctx, list);
  CHECK(lb.width > rd_node_bounds(ctx, wide).width);
  rd_node item  = nth_item(ctx, list, 1);
  rd_node label = rd_node_last_child(ctx, item);
  CHECK(rd_node_bounds(ctx, label).width >= rd_font_measure(ctx, (rd_font){0, true}, rd_node_computed_style(ctx, label)->font_size.value, longs[1], RD_TEXT_NUL) - 0.01f);
  CHECK(rd_node_bounds(ctx, label).x + rd_node_bounds(ctx, label).width <= lb.x + lb.width);
  rd_ctx_destroy(ctx);
}

int main(void) {
  failures();
  outlived();
  menus();
  selects();
  modals();
  tooltips();
  headings();
  select_list_width();
  return TEST_RESULT();
}
