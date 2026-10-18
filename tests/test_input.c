#include <math.h>
#include <raydom/raydom.h>
#include <string.h>

#include "check.h"
#include "render_internal.h"
#include "text_internal.h"

#ifdef __APPLE__
static const uint8_t SC = RD_MOD_SUPER, WM = RD_MOD_ALT;
#else
static const uint8_t SC = RD_MOD_CTRL, WM = RD_MOD_CTRL;
#endif

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static void press(rd_ctx* ctx, rd_key k, uint8_t mods) {
  rd_ctx_key(ctx, k, true, false, mods);
  rd_ctx_key(ctx, k, false, false, mods);
}

static void type(rd_ctx* ctx, const char* s) {
  size_t n = strlen(s);
  for (size_t i = 0; i < n;) rd_ctx_text(ctx, rd__utf8_decode(s, n, &i));
}

static void click(rd_ctx* ctx, float x, float y) {
  rd_ctx_pointer(ctx, x, y, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, x, y, 0, 0, 0);
}

static bool text_is(rd_ctx* ctx, rd_node n, const char* expect) {
  size_t      len;
  const char* s = rd_node_text(ctx, n, &len);
  return s && len == strlen(expect) && memcmp(s, expect, len) == 0;
}

static bool selection_is(rd_ctx* ctx, rd_node n, size_t anchor, size_t caret) {
  size_t a, c;
  return rd_input_selection(ctx, n, &a, &c) && a == anchor && c == caret;
}

typedef struct log {
  rd_event_type types[32];
  rd_node       targets[32], related[32];
  int           len;
} log_t;

static void record(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  log_t* l = data;
  if (l->len >= 32) return;
  l->types[l->len]     = ev->type;
  l->targets[l->len]   = ev->current;
  l->related[l->len++] = ev->related;
}

static int count(const log_t* l, rd_event_type type) {
  int c = 0;
  for (int j = 0; j < l->len; j++) c += l->types[j] == type;
  return c;
}

static void test_tab_order_and_states(void) {
  rd_ctx*  ctx  = rd_ctx_create();
  rd_node  root = rd_ctx_root(ctx);
  rd_style tab  = {.focusable = RD_FOCUSABLE_TAB, .height = rd_px(20)};
  rd_node  a    = add(ctx, root, tab);
  rd_node  box  = add(ctx, root, (rd_style){0});
  rd_node  b    = add(ctx, box, tab);
  rd_node  ptr  = add(ctx, root, (rd_style){.focusable = RD_FOCUSABLE_POINTER, .height = rd_px(20)});
  rd_node  off  = add(ctx, root, tab);
  rd_node  gone = add(ctx, root, (rd_style){.focusable = RD_FOCUSABLE_TAB, .display = RD_DISPLAY_NONE});
  rd_node  c    = add(ctx, root, tab);
  rd_node_set_states(ctx, off, RD_STATE_DISABLED, true);
  (void)gone;
  (void)ptr;
  rd_ctx_layout(ctx, 400, 300);

  log_t l = {0};
  for (rd_node n = rd_node_first_child(ctx, root); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n)) {
    rd_node_on(ctx, n, RD_EVENT_FOCUS, record, &l, NULL, 0);
    rd_node_on(ctx, n, RD_EVENT_BLUR, record, &l, NULL, 0);
  }
  rd_node_on(ctx, root, RD_EVENT_FOCUS_IN, record, &l, NULL, 0);
  rd_node_on(ctx, root, RD_EVENT_FOCUS_OUT, record, &l, NULL, 0);

  press(ctx, RD_KEY_TAB, 0);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), a));
  CHECK(l.len == 2 && l.types[0] == RD_EVENT_FOCUS && l.types[1] == RD_EVENT_FOCUS_IN);
  press(ctx, RD_KEY_TAB, 0);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), b));
  uint8_t sb = rd_node_states(ctx, b);
  CHECK((sb & RD_STATE_FOCUS) && (sb & RD_STATE_FOCUS_VISIBLE) && (sb & RD_STATE_FOCUS_WITHIN));
  CHECK((rd_node_states(ctx, box) & RD_STATE_FOCUS_WITHIN) && !(rd_node_states(ctx, box) & RD_STATE_FOCUS));
  CHECK(!(rd_node_states(ctx, a) & (RD_STATE_FOCUS | RD_STATE_FOCUS_WITHIN)));
  CHECK(l.len == 5 && l.types[2] == RD_EVENT_BLUR && rd_node_eq(l.targets[2], a) && rd_node_eq(l.related[2], b));
  CHECK(l.types[3] == RD_EVENT_FOCUS_OUT && l.types[4] == RD_EVENT_FOCUS_IN && rd_node_eq(l.related[4], a));

  press(ctx, RD_KEY_TAB, 0);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), c));
  press(ctx, RD_KEY_TAB, 0);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), a));
  press(ctx, RD_KEY_TAB, RD_MOD_SHIFT);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), c));

  rd_node_destroy(ctx, c);
  CHECK(rd_node_is_null(rd_ctx_focused(ctx)));
  press(ctx, RD_KEY_TAB, 0);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), a));
  rd_ctx_destroy(ctx);
}

static void prevent(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)data;
  rd_event_prevent_default(ev);
}

static void test_pointer_focus_and_activation(void) {
  rd_ctx* ctx    = rd_ctx_create();
  rd_node root   = rd_ctx_root(ctx);
  rd_node button = add(ctx, root, (rd_style){.focusable = RD_FOCUSABLE_POINTER, .height = rd_px(40), .padding = rd_edges_all(rd_px(5))});
  rd_node label  = add(ctx, button, (rd_style){.height = rd_px(10)});
  rd_node guard  = add(ctx, root, (rd_style){.focusable = RD_FOCUSABLE_TAB, .height = rd_px(40)});
  rd_ctx_layout(ctx, 400, 300);

  log_t l = {0};
  rd_node_on(ctx, button, RD_EVENT_CLICK, record, &l, NULL, 0);
  rd_node_on(ctx, root, RD_EVENT_KEY_DOWN, record, &l, NULL, 0);
  click(ctx, 10, 10);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), button));
  CHECK(!(rd_node_states(ctx, button) & RD_STATE_FOCUS_VISIBLE));
  CHECK(count(&l, RD_EVENT_CLICK) == 1);
  (void)label;

  l.len = 0;
  press(ctx, RD_KEY_ENTER, 0);
  press(ctx, RD_KEY_SPACE, 0);
  rd_ctx_key(ctx, RD_KEY_ENTER, true, true, 0);
  CHECK(count(&l, RD_EVENT_CLICK) == 2 && count(&l, RD_EVENT_KEY_DOWN) == 3);
  CHECK(rd_node_eq(l.targets[0], root));

  rd_listener p = rd_node_on(ctx, button, RD_EVENT_KEY_DOWN, prevent, NULL, NULL, 0);
  l.len         = 0;
  press(ctx, RD_KEY_ENTER, 0);
  CHECK(count(&l, RD_EVENT_CLICK) == 0);
  rd_node_off(ctx, p);

  click(ctx, 10, 250);
  CHECK(rd_node_is_null(rd_ctx_focused(ctx)));
  rd_node_on(ctx, guard, RD_EVENT_POINTER_DOWN, prevent, NULL, NULL, 0);
  click(ctx, 10, 60);
  CHECK(rd_node_is_null(rd_ctx_focused(ctx)));
  CHECK(rd_node_focus(ctx, guard) && rd_node_eq(rd_ctx_focused(ctx), guard));
  rd_node_set_states(ctx, guard, RD_STATE_DISABLED, true);
  CHECK(rd_node_is_null(rd_ctx_focused(ctx)));
  CHECK(!rd_node_focus(ctx, guard));
  rd_ctx_destroy(ctx);
}

static void test_tab_scrolls_into_view(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node list = add(ctx, rd_ctx_root(ctx), (rd_style){.height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node items[6];
  for (int j = 0; j < 6; j++) items[j] = add(ctx, list, (rd_style){.height = rd_px(40), .focusable = RD_FOCUSABLE_TAB});
  rd_ctx_layout(ctx, 400, 300);
  for (int j = 0; j < 4; j++) press(ctx, RD_KEY_TAB, 0);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), items[3]));
  CHECK(near(rd_node_scroll(ctx, list).y, 60));
  press(ctx, RD_KEY_TAB, RD_MOD_SHIFT);
  press(ctx, RD_KEY_TAB, RD_MOD_SHIFT);
  press(ctx, RD_KEY_TAB, RD_MOD_SHIFT);
  CHECK(near(rd_node_scroll(ctx, list).y, 0));
  rd_ctx_destroy(ctx);
}

static float cw;

static rd_ctx* font_ctx(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font f   = rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  CHECK(f.id != 0);
  cw = rd_font_measure(ctx, f, 16, "a", 1);
  return ctx;
}

static void test_typing_and_editing(void) {
  rd_ctx* ctx   = font_ctx();
  rd_node input = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(300)});
  CHECK(rd_node_make_input(ctx, input, 0) && rd_node_is_input(ctx, input));
  log_t l = {0};
  rd_node_on(ctx, input, RD_EVENT_INPUT, record, &l, NULL, 0);
  rd_node_on(ctx, input, RD_EVENT_CHANGE, record, &l, NULL, 0);
  rd_ctx_layout(ctx, 400, 300);
  CHECK(near(rd_node_rect(ctx, input).height, 16 * 1.2f));

  press(ctx, RD_KEY_TAB, 0);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), input));
  type(ctx, "hello world");
  CHECK(text_is(ctx, input, "hello world") && selection_is(ctx, input, 11, 11));
  CHECK(count(&l, RD_EVENT_INPUT) == 11);

  press(ctx, RD_KEY_BACKSPACE, 0);
  CHECK(text_is(ctx, input, "hello worl"));
  press(ctx, RD_KEY_LEFT, WM);
  CHECK(selection_is(ctx, input, 6, 6));
  press(ctx, RD_KEY_RIGHT, RD_MOD_SHIFT);
  press(ctx, RD_KEY_RIGHT, RD_MOD_SHIFT);
  CHECK(selection_is(ctx, input, 6, 8));
  press(ctx, RD_KEY_C, SC);
  press(ctx, RD_KEY_END, 0);
  press(ctx, RD_KEY_V, SC);
  CHECK(text_is(ctx, input, "hello worlwo"));
  press(ctx, RD_KEY_BACKSPACE, WM);
  CHECK(text_is(ctx, input, "hello "));
  press(ctx, RD_KEY_HOME, 0);
  press(ctx, RD_KEY_DELETE, 0);
  CHECK(text_is(ctx, input, "ello ") && selection_is(ctx, input, 0, 0));

  press(ctx, RD_KEY_A, SC);
  CHECK(selection_is(ctx, input, 0, 5));
  press(ctx, RD_KEY_X, SC);
  CHECK(text_is(ctx, input, ""));
  press(ctx, RD_KEY_V, SC);
  press(ctx, RD_KEY_V, SC);
  CHECK(text_is(ctx, input, "ello ello "));

  l.len = 0;
  press(ctx, RD_KEY_ENTER, 0);
  CHECK(text_is(ctx, input, "ello ello ") && count(&l, RD_EVENT_CHANGE) == 1);
  press(ctx, RD_KEY_ENTER, 0);
  CHECK(count(&l, RD_EVENT_CHANGE) == 1);
  rd_ctx_destroy(ctx);
}

static void test_undo_redo(void) {
  rd_ctx* ctx   = font_ctx();
  rd_node input = add(ctx, rd_ctx_root(ctx), (rd_style){0});
  rd_node_make_input(ctx, input, 0);
  rd_ctx_layout(ctx, 400, 300);
  rd_node_focus(ctx, input);
  type(ctx, "abc def");
  press(ctx, RD_KEY_Z, SC);
  CHECK(text_is(ctx, input, "abc "));
  press(ctx, RD_KEY_Z, SC);
  CHECK(text_is(ctx, input, "abc"));
  press(ctx, RD_KEY_Z, SC);
  CHECK(text_is(ctx, input, ""));
  press(ctx, RD_KEY_Z, SC);
  CHECK(text_is(ctx, input, ""));
  press(ctx, RD_KEY_Z, SC | RD_MOD_SHIFT);
  press(ctx, RD_KEY_Z, SC | RD_MOD_SHIFT);
  CHECK(text_is(ctx, input, "abc "));
  type(ctx, "x");
  press(ctx, RD_KEY_Z, SC | RD_MOD_SHIFT);
  CHECK(text_is(ctx, input, "abc x"));
  rd_ctx_destroy(ctx);
}

static void test_multiline(void) {
  rd_ctx* ctx  = font_ctx();
  rd_node area = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(6 * cw), .align_self = RD_ALIGN_START});
  rd_node_make_input(ctx, area, RD_INPUT_MULTILINE);
  log_t l = {0};
  rd_node_on(ctx, area, RD_EVENT_CHANGE, record, &l, NULL, 0);
  rd_ctx_layout(ctx, 400, 300);
  rd_node_focus(ctx, area);
  type(ctx, "abc");
  press(ctx, RD_KEY_ENTER, 0);
  type(ctx, "defgh ijk");
  CHECK(text_is(ctx, area, "abc\ndefgh ijk"));
  rd_ctx_layout(ctx, 400, 300);
  CHECK(near(rd_node_rect(ctx, area).height, 3 * 16 * 1.2f));

  press(ctx, RD_KEY_UP, 0);
  CHECK(selection_is(ctx, area, 7, 7));
  press(ctx, RD_KEY_UP, 0);
  CHECK(selection_is(ctx, area, 3, 3));
  press(ctx, RD_KEY_DOWN, 0);
  CHECK(selection_is(ctx, area, 7, 7));
  press(ctx, RD_KEY_HOME, 0);
  CHECK(selection_is(ctx, area, 4, 4));
  press(ctx, RD_KEY_END, 0);
  CHECK(selection_is(ctx, area, 10, 10));
  press(ctx, RD_KEY_DOWN, RD_MOD_SHIFT);
  CHECK(selection_is(ctx, area, 10, 13));

  CHECK(count(&l, RD_EVENT_CHANGE) == 0);
  rd_node_focus(ctx, (rd_node){0});
  CHECK(count(&l, RD_EVENT_CHANGE) == 1);
  rd_ctx_destroy(ctx);
}

static void test_pointer_caret_and_display(void) {
  rd_ctx* ctx   = font_ctx();
  rd_node input = add(ctx, rd_ctx_root(ctx),
                      (rd_style){.width = rd_px(5 * cw + 20), .padding = rd_edges_all(rd_px(10)), .align_self = RD_ALIGN_START});
  rd_node_make_input(ctx, input, 0);
  rd_node_set_placeholder(ctx, input, "Name", RD_TEXT_NUL);
  rd_ctx_layout(ctx, 400, 300);

  uint32_t       n;
  const rd__cmd* cmds        = rd__display_list(ctx, &n);
  bool           placeholder = false, caret = false;
  for (uint32_t j = 0; j < n; j++) placeholder |= cmds[j].kind == RD_CMD_PLACEHOLDER;
  CHECK(placeholder);

  rd_node_set_text(ctx, input, "abcdefghij", RD_TEXT_NUL);
  rd_ctx_layout(ctx, 400, 300);
  click(ctx, 10 + 2.6f * cw, 15);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), input));
  CHECK(rd_node_states(ctx, input) & RD_STATE_FOCUS_VISIBLE);
  CHECK(selection_is(ctx, input, 3, 3));

  rd_ctx_pointer(ctx, 10 + 1.2f * cw, 15, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 10 + 4.1f * cw, 15, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, 10 + 4.1f * cw, 15, 0, 0, 0);
  CHECK(selection_is(ctx, input, 1, 4));

  press(ctx, RD_KEY_END, 0);
  cmds     = rd__display_list(ctx, &n);
  float dx = 0;
  for (uint32_t j = 0; j < n; j++) {
    if (cmds[j].kind == RD_CMD_TEXT) dx = cmds[j].dx;
    if (cmds[j].kind == RD_CMD_FILL) caret = near(cmds[j].rect.x, 10 + 5 * cw - 1);
  }
  CHECK(near(dx, -(5 * cw + 1)));
  CHECK(caret);

  rd_node_set_text(ctx, input, "ab", RD_TEXT_NUL);
  CHECK(selection_is(ctx, input, 2, 2));
  press(ctx, RD_KEY_TAB, 0);
  CHECK(text_is(ctx, input, "ab") && rd_node_is_null(rd_ctx_focused(ctx)) == false);
  rd_ctx_destroy(ctx);
}

static void test_utf8_editing(void) {
  rd_ctx* ctx   = font_ctx();
  rd_node input = add(ctx, rd_ctx_root(ctx), (rd_style){0});
  rd_node_make_input(ctx, input, 0);
  rd_ctx_layout(ctx, 400, 300);
  rd_node_focus(ctx, input);
  type(ctx, "a\xc3\xa9\xe4\xb8\x80");
  CHECK(text_is(ctx, input, "a\xc3\xa9\xe4\xb8\x80") && selection_is(ctx, input, 6, 6));
  press(ctx, RD_KEY_LEFT, 0);
  CHECK(selection_is(ctx, input, 3, 3));
  press(ctx, RD_KEY_BACKSPACE, 0);
  CHECK(text_is(ctx, input, "a\xe4\xb8\x80") && selection_is(ctx, input, 1, 1));
  rd_ctx_text(ctx, 0x1F600);
  CHECK(text_is(ctx, input, "a\xf0\x9f\x98\x80\xe4\xb8\x80"));
  rd_ctx_text(ctx, '\t');
  CHECK(text_is(ctx, input, "a\xf0\x9f\x98\x80\xe4\xb8\x80"));
  rd_ctx_destroy(ctx);
}

int main(void) {
  test_tab_order_and_states();
  test_pointer_focus_and_activation();
  test_tab_scrolls_into_view();
  test_typing_and_editing();
  test_undo_redo();
  test_multiline();
  test_pointer_caret_and_display();
  test_utf8_editing();
  return TEST_RESULT();
}
