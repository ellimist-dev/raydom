#include <raydom/raydom.h>

#include "check.h"

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static rd_node add_text(rd_ctx* ctx, rd_node parent, rd_style style, const char* s) {
  rd_node n = add(ctx, parent, style);
  rd_node_set_text(ctx, n, s, RD_TEXT_NUL);
  return n;
}

static rd_cursor at(rd_ctx* ctx, rd_node n, uint8_t buttons) {
  rd_rect b = rd_node_bounds(ctx, n);
  rd_ctx_pointer(ctx, b.x + 2, b.y + 2, buttons, 0, 0);
  rd_ctx_layout(ctx, 800, 600);
  return rd_ctx_cursor(ctx);
}

static void on_down(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  rd_node_set_pointer_capture(ctx, ev->current);
}

int main(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);

  rd_node box      = add(ctx, root, (rd_style){.height = rd_px(20)});
  rd_node para     = add_text(ctx, root, (rd_style){0}, "selectable text");
  rd_node label    = add_text(ctx, root, (rd_style){.user_select = RD_USER_SELECT_NONE}, "label");
  rd_node input    = rd_node_create_input(ctx, 0);
  rd_node button   = rd_node_create_button(ctx, "Save", RD_TEXT_NUL);
  rd_node link     = rd_node_create_text(ctx, "docs", RD_TEXT_NUL);
  rd_node card     = add(ctx, root, (rd_style){.cursor = RD_CURSOR_POINTER});
  rd_node in_card  = add_text(ctx, card, (rd_style){0}, "inherits");
  rd_node hidden   = add(ctx, root, (rd_style){.height = rd_px(20), .cursor = RD_CURSOR_NONE});
  rd_class handle  = rd_class_create(ctx, &(rd_style){.height = rd_px(20), .cursor = RD_CURSOR_EW_RESIZE});
  rd_class_set_state(ctx, handle, RD_STATE_ACTIVE, &(rd_style){.cursor = RD_CURSOR_MOVE});
  rd_node grip = add(ctx, root, (rd_style){0});
  rd_node_add_class(ctx, grip, handle);
  rd_node_on(ctx, grip, RD_EVENT_POINTER_DOWN, on_down, NULL, NULL, 0);
  rd_node_set_role(ctx, link, RD_ROLE_LINK);
  rd_node_set_text(ctx, input, "value", RD_TEXT_NUL);
  rd_node_append(ctx, root, input);
  rd_node_append(ctx, root, button);
  rd_node_append(ctx, root, link);
  rd_ctx_layout(ctx, 800, 600);

  CHECK(rd_ctx_cursor(ctx) == RD_CURSOR_DEFAULT);  // no pointer yet
  CHECK(at(ctx, box, 0) == RD_CURSOR_DEFAULT);
  CHECK(at(ctx, para, 0) == RD_CURSOR_TEXT);
  CHECK(at(ctx, label, 0) == RD_CURSOR_DEFAULT);
  CHECK(at(ctx, input, 0) == RD_CURSOR_TEXT);
  CHECK(at(ctx, button, 0) == RD_CURSOR_DEFAULT);  // a button's text isn't an I-beam
  CHECK(at(ctx, link, 0) == RD_CURSOR_POINTER);
  CHECK(at(ctx, in_card, 0) == RD_CURSOR_POINTER);  // inherited
  CHECK(at(ctx, hidden, 0) == RD_CURSOR_NONE);

  // Dragging: the active variant applies, and stays while the captured pointer leaves the node.
  CHECK(at(ctx, grip, 0) == RD_CURSOR_EW_RESIZE);
  CHECK(at(ctx, grip, RD_BUTTON_PRIMARY) == RD_CURSOR_MOVE);
  CHECK(at(ctx, box, RD_BUTTON_PRIMARY) == RD_CURSOR_MOVE);
  CHECK(at(ctx, box, 0) == RD_CURSOR_DEFAULT);

  // Selecting text: the I-beam stays while the drag crosses non-text nodes.
  CHECK(at(ctx, para, RD_BUTTON_PRIMARY) == RD_CURSOR_TEXT);
  CHECK(at(ctx, box, RD_BUTTON_PRIMARY) == RD_CURSOR_TEXT);
  CHECK(at(ctx, box, 0) == RD_CURSOR_DEFAULT);

  rd_ctx_destroy(ctx);
  return TEST_RESULT();
}
