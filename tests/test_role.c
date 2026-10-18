#include <raydom/raydom.h>
#include <string.h>

#include "check.h"

static void press(rd_ctx* ctx, rd_key k) {
  rd_ctx_key(ctx, k, true, false, 0);
  rd_ctx_key(ctx, k, false, false, 0);
}

static void on_click(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)ev;
  ++*(int*)data;
}

static void paint(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)ctx;
  (void)info;
  (void)data;
}

static void derived(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node box = rd_node_create(ctx), text = rd_node_create_text(ctx, "hi", RD_TEXT_NUL);
  rd_node in = rd_node_create_input(ctx, 0), area = rd_node_create_input(ctx, RD_INPUT_MULTILINE);
  rd_node canvas = rd_node_create(ctx);
  rd_node_set_paint(ctx, canvas, paint, NULL, NULL);

  CHECK(rd_node_role(ctx, box) == RD_ROLE_GENERIC);
  CHECK(rd_node_role(ctx, text) == RD_ROLE_TEXT);
  CHECK(rd_node_role(ctx, in) == RD_ROLE_TEXT_INPUT);
  CHECK(rd_node_role(ctx, area) == RD_ROLE_MULTILINE_TEXT_INPUT);
  CHECK(rd_node_role(ctx, canvas) == RD_ROLE_CANVAS);

  size_t len;
  CHECK(strcmp(rd_node_text(ctx, text, &len), "hi") == 0 && len == 2);

  // An explicit role wins; AUTO goes back to the derived one.
  CHECK(rd_node_set_role(ctx, text, RD_ROLE_HEADING));
  CHECK(rd_node_role(ctx, text) == RD_ROLE_HEADING);
  rd_node_set_role(ctx, text, RD_ROLE_AUTO);
  CHECK(rd_node_role(ctx, text) == RD_ROLE_TEXT);

  // Stale handles, and a recycled slot doesn't keep the old role.
  rd_node_set_role(ctx, box, RD_ROLE_LIST);
  rd_node_destroy(ctx, box);
  CHECK(!rd_node_set_role(ctx, box, RD_ROLE_BUTTON));
  CHECK(rd_node_role(ctx, box) == RD_ROLE_AUTO);
  rd_node fresh = rd_node_create(ctx);
  CHECK(fresh.index == box.index && rd_node_role(ctx, fresh) == RD_ROLE_GENERIC);
  rd_ctx_destroy(ctx);
}

static void buttons(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node save = rd_node_create_button(ctx, "Save", RD_TEXT_NUL);
  rd_node icon = rd_node_create_button(ctx, NULL, 0);
  rd_node skip = rd_node_create_button(ctx, NULL, 0);
  rd_node off  = rd_node_create_button(ctx, NULL, 0);
  rd_node plain = rd_node_create(ctx);
  rd_node_set_style(ctx, skip, &(rd_style){.focusable = RD_FOCUSABLE_POINTER});
  rd_node_set_states(ctx, off, RD_STATE_DISABLED, true);
  rd_node_append(ctx, root, plain);
  rd_node_append(ctx, root, save);
  rd_node_append(ctx, root, skip);
  rd_node_append(ctx, root, off);
  rd_node_append(ctx, root, icon);
  rd_node_append(ctx, icon, rd_node_create(ctx));

  CHECK(rd_node_role(ctx, save) == RD_ROLE_BUTTON && rd_node_role(ctx, icon) == RD_ROLE_BUTTON);
  CHECK(rd_node_text(ctx, save, NULL) && !rd_node_text(ctx, icon, NULL));
  rd_ctx_layout(ctx, 400, 300);

  // Tab reaches buttons without a focusable style; POINTER skips Tab; disabled is skipped.
  press(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), save));
  press(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), icon));
  press(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), save));
  CHECK(rd_node_focus(ctx, skip));
  CHECK(!rd_node_focus(ctx, off) && !rd_node_focus(ctx, plain));

  // Enter activates the focused button.
  int clicks = 0;
  rd_node_on(ctx, save, RD_EVENT_CLICK, on_click, &clicks, NULL, 0);
  rd_node_focus(ctx, save);
  press(ctx, RD_KEY_ENTER);
  press(ctx, RD_KEY_SPACE);
  CHECK(clicks == 2);
  rd_ctx_destroy(ctx);
}

static void scope(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node page = rd_node_create_button(ctx, "page", RD_TEXT_NUL);
  rd_node dlg  = rd_node_create(ctx);
  rd_node a = rd_node_create_button(ctx, "a", RD_TEXT_NUL), b = rd_node_create_button(ctx, "b", RD_TEXT_NUL);
  rd_node_append(ctx, rd_ctx_root(ctx), page);
  rd_node_append(ctx, rd_ctx_root(ctx), dlg);
  rd_node_append(ctx, dlg, a);
  rd_node_append(ctx, dlg, b);
  rd_ctx_layout(ctx, 400, 300);
  rd_ctx_set_focus_scope(ctx, dlg);
  CHECK(rd_node_eq(rd_ctx_focus_scope(ctx), dlg));
  press(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), a));
  press(ctx, RD_KEY_TAB);
  press(ctx, RD_KEY_TAB);  // wraps inside the scope
  CHECK(rd_node_eq(rd_ctx_focused(ctx), a));
  rd_node_destroy(ctx, dlg);  // a dead scope is the whole tree
  CHECK(rd_node_is_null(rd_ctx_focus_scope(ctx)));
  press(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), page));
  rd_ctx_destroy(ctx);
}

int main(void) {
  derived();
  scope();
  buttons();
  return TEST_RESULT();
}
