#include <raydom/extras/popups.h>
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

static const rdx_menu_item ITEMS[] = {{"Cut", "Ctrl+X", false}, {"Copy", NULL, true}, {NULL, NULL, false}, {"Paste", NULL, false}};

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

int main(void) {
  menus();
  selects();
  modals();
  tooltips();
  return TEST_RESULT();
}
