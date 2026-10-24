#include <math.h>
#include <raydom/raydom.h>
#include <string.h>

#include "check.h"
#include "render_internal.h"

#ifdef __APPLE__
static const uint8_t SC = RD_MOD_SUPER;
#else
static const uint8_t SC = RD_MOD_CTRL;
#endif

static const float LH = 16 * 1.2f;
static float       cw;

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static rd_node add_text(rd_ctx* ctx, rd_node parent, rd_style style, const char* text) {
  rd_node n = add(ctx, parent, style);
  rd_node_set_text(ctx, n, text, RD_TEXT_NUL);
  return n;
}

static rd_ctx* setup(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font f   = rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  CHECK(f.id != 0);
  cw = rd_font_measure(ctx, f, 16, "a", 1);
  return ctx;
}

static bool selected_is(rd_ctx* ctx, const char* expect) {
  char   buf[256];
  size_t n = rd_ctx_selection_text(ctx, buf, sizeof buf);
  if (n == strlen(expect) && strcmp(buf, expect) == 0) return true;
  fprintf(stderr, "selected \"%s\", expected \"%s\"\n", buf, expect);
  return false;
}

static void drag(rd_ctx* ctx, float x0, float y0, float x1, float y1) {
  rd_ctx_pointer(ctx, x0, y0, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, (x0 + x1) / 2, (y0 + y1) / 2, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, x1, y1, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, x1, y1, 0, 0, 0);
}

static int fills(rd_ctx* ctx, rd_node n) {
  uint32_t       len;
  const rd__cmd* cmds = rd__display_list(ctx, &len);
  int            c    = 0;
  for (uint32_t j = 0; j < len; j++) c += cmds[j].kind == RD_CMD_FILL && cmds[j].node == n.index;
  return c;
}

static int  changes;
static void count_change(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)ev;
  (void)data;
  changes++;
}

static char        copied[256];
static const char* clip_get(void* data) {
  (void)data;
  return copied;
}
static void clip_set(void* data, const char* text) {
  (void)data;
  strncpy(copied, text, sizeof copied - 1);
}

static void test_drag_across_nodes(void) {
  rd_ctx* ctx  = setup();
  rd_node root = rd_ctx_root(ctx);
  rd_node_merge_style(ctx, root, &(rd_style){.align_items = RD_ALIGN_START});
  rd_node a    = add_text(ctx, root, (rd_style){0}, "aaa bbb");
  rd_node skip = add_text(ctx, root, (rd_style){.user_select = RD_USER_SELECT_NONE}, "zzz");
  rd_node b    = add_text(ctx, root, (rd_style){0}, "ccc ddd");
  rd_node_on(ctx, root, RD_EVENT_SELECTION_CHANGE, count_change, NULL, NULL, 0);
  rd_ctx_set_clipboard(ctx, clip_get, clip_set, NULL);
  rd_ctx_layout(ctx, 400, 300);

  drag(ctx, 1.2f * cw, LH / 2, 2.2f * cw, 2.5f * LH);
  CHECK(selected_is(ctx, "aa bbb\ncc"));
  CHECK(changes > 0);
  CHECK(fills(ctx, a) == 1 && fills(ctx, b) == 1 && fills(ctx, skip) == 0);

  rd_node an, fo;
  size_t  ao, fo_off;
  CHECK(rd_ctx_selection(ctx, &an, &ao, &fo, &fo_off));
  CHECK(rd_node_eq(an, a) && ao == 1 && rd_node_eq(fo, b) && fo_off == 2);

  drag(ctx, 2.2f * cw, 2.5f * LH, 1.2f * cw, LH / 2);
  CHECK(selected_is(ctx, "aa bbb\ncc"));

  rd_ctx_key(ctx, RD_KEY_C, true, false, SC);
  CHECK(strcmp(copied, "aa bbb\ncc") == 0);

  drag(ctx, 5.2f * cw, LH / 2, 5.2f * cw, LH / 2);
  CHECK(!rd_ctx_selection(ctx, NULL, NULL, NULL, NULL));

  drag(ctx, 2.2f * cw, 2.5f * LH, 50, 290);
  CHECK(selected_is(ctx, "c ddd"));
  drag(ctx, 2.2f * cw, 2.5f * LH, 50, -20);
  CHECK(selected_is(ctx, "aaa bbb\ncc"));

  rd_ctx_key(ctx, RD_KEY_ESCAPE, true, false, 0);
  CHECK(!rd_ctx_selection(ctx, NULL, NULL, NULL, NULL));
  rd_ctx_key(ctx, RD_KEY_A, true, false, SC);
  CHECK(selected_is(ctx, "aaa bbb\nccc ddd"));
  rd_ctx_destroy(ctx);
}

static void grab(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  rd_node_set_pointer_capture(ctx, ev->current);
}

static void test_selection_yields(void) {
  rd_ctx* ctx    = setup();
  rd_node root   = rd_ctx_root(ctx);
  rd_node para   = add_text(ctx, root, (rd_style){.align_self = RD_ALIGN_START}, "hello world");
  rd_node input  = add(ctx, root, (rd_style){.width = rd_px(200)});
  rd_node slider = add_text(ctx, root, (rd_style){.align_self = RD_ALIGN_START}, "drag handle");
  rd_node_make_input(ctx, input, 0);
  rd_node_set_text(ctx, input, "typed", RD_TEXT_NUL);
  rd_node_on(ctx, slider, RD_EVENT_POINTER_DOWN, grab, NULL, NULL, 0);
  rd_ctx_layout(ctx, 400, 300);

  drag(ctx, 0.2f * cw, LH / 2, 4.8f * cw, LH / 2);
  CHECK(selected_is(ctx, "hello"));
  drag(ctx, 0.2f * cw, 1.5f * LH, 3.2f * cw, 1.5f * LH);
  CHECK(!rd_ctx_selection(ctx, NULL, NULL, NULL, NULL));
  size_t anchor, caret;
  CHECK(rd_input_selection(ctx, input, &anchor, &caret) && anchor == 0 && caret == 3);

  drag(ctx, 0.2f * cw, 2.5f * LH, 4.8f * cw, 2.5f * LH);
  CHECK(!rd_ctx_selection(ctx, NULL, NULL, NULL, NULL));

  CHECK(rd_ctx_select(ctx, para, 6, para, 99));
  CHECK(selected_is(ctx, "world"));
  rd_node_destroy(ctx, para);
  CHECK(!rd_ctx_selection(ctx, NULL, NULL, NULL, NULL));
  rd_ctx_destroy(ctx);
}

static void test_wheel_gestures(void) {
  rd_ctx* ctx   = setup();
  rd_node outer = add(ctx, rd_ctx_root(ctx), (rd_style){.height = rd_px(200), .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node inner = add(ctx, outer,
                      (rd_style){.width = rd_px(100), .flex_direction = RD_FLEX_ROW, .overflow_x = RD_OVERFLOW_SCROLL});
  add(ctx, inner, (rd_style){.width = rd_px(400), .height = rd_px(40)});
  add(ctx, outer, (rd_style){.height = rd_px(600)});
  rd_ctx_layout(ctx, 400, 300);

  rd_ctx_set_time(ctx, 1.0);
  rd_ctx_pointer(ctx, 20, 20, 0, -0.5f, -0.1f);
  CHECK(near(rd_node_scroll(ctx, inner).x, 5) && near(rd_node_scroll(ctx, outer).y, 0));
  rd_ctx_set_time(ctx, 1.05);
  rd_ctx_pointer(ctx, 20, 20, 0, -0.05f, -0.3f);
  CHECK(near(rd_node_scroll(ctx, outer).y, 0));
  rd_ctx_set_time(ctx, 1.6);
  rd_ctx_pointer(ctx, 20, 20, 0, -0.05f, -0.3f);
  CHECK(near(rd_node_scroll(ctx, outer).y, 3) && near(rd_node_scroll(ctx, inner).x, 5));

  rd_ctx_set_time(ctx, 1.65);
  rd_ctx_pointer(ctx, 20, 20, 0, -2, 0);
  CHECK(near(rd_node_scroll(ctx, inner).x, 5));
  rd_ctx_pointer(ctx, 40, 20, 0, -2, 0);
  CHECK(near(rd_node_scroll(ctx, inner).x, 101));

  rd_node_set_scroll(ctx, inner, 300, 0);
  rd_ctx_set_time(ctx, 3.0);
  rd_ctx_pointer(ctx, 20, 20, 0, -1, 0);
  CHECK(near(rd_node_scroll(ctx, inner).x, 300) && near(rd_node_scroll(ctx, outer).y, 3));
  rd_ctx_set_time(ctx, 4.0);
  rd_ctx_pointer(ctx, 20, 20, 0, 0, -1);
  CHECK(near(rd_node_scroll(ctx, outer).y, 51));
  rd_ctx_destroy(ctx);
}

// Offsets inside a codepoint snap to a boundary, also after the text changes under the selection.
static void test_select_snaps_to_codepoints(void) {
  rd_ctx* ctx = setup();
  rd_node t   = add_text(ctx, rd_ctx_root(ctx), (rd_style){0}, "\xc3\xa9t\xc3\xa9");  // "été"
  rd_ctx_layout(ctx, 400, 300);
  CHECK(rd_ctx_select(ctx, t, 1, t, 4));
  size_t a, f;
  CHECK(rd_ctx_selection(ctx, NULL, &a, NULL, &f) && a == 2 && f == 5);
  CHECK(selected_is(ctx, "t\xc3\xa9"));
  rd_node_set_text(ctx, t, "\xe4\xb8\x80\xe4\xb8\x80", RD_TEXT_NUL);  // offsets 2 and 5 are now mid-codepoint
  CHECK(selected_is(ctx, "\xe4\xb8\x80"));
  rd_ctx_destroy(ctx);
}

int main(void) {
  test_drag_across_nodes();
  test_selection_yields();
  test_wheel_gestures();
  test_select_snaps_to_codepoints();
  return TEST_RESULT();
}
