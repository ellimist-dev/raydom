#include <math.h>
#include <raydom/extras/controls.h>
#include <raydom/extras/toast.h>

#include "check.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.01f; }

static void count(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)ev;
  ++*(int*)data;
}

static void click(rd_ctx* ctx, rd_node n) {
  rd_rect b = rd_node_bounds(ctx, n);
  rd_ctx_pointer(ctx, b.x + 4, b.y + b.height / 2, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, b.x + 4, b.y + b.height / 2, 0, 0, 0);
}

static void press(rd_ctx* ctx, rd_key k) {
  rd_ctx_key(ctx, k, true, false, 0);
  rd_ctx_key(ctx, k, false, false, 0);
}

static void toggles(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node root = rd_ctx_root(ctx);
  rd_node box  = rdx_checkbox_create(ctx, "Remember me");
  rd_node sw   = rdx_switch_create(ctx, NULL);
  int     changes = 0;
  rd_node_on(ctx, box, RD_EVENT_CHANGE, count, &changes, NULL, 0);
  rd_node_append(ctx, root, box);
  rd_node_append(ctx, root, sw);
  rd_ctx_layout(ctx, 800, 600);

  CHECK(rd_node_role(ctx, box) == RD_ROLE_CHECKBOX && rd_node_role(ctx, sw) == RD_ROLE_SWITCH);
  click(ctx, box);
  CHECK(rdx_checked(ctx, box) && changes == 1 && (rd_node_states(ctx, box) & RD_STATE_CHECKED));
  // The mark (box > mark) shows through shared states.
  rd_node mark = rd_node_first_child(ctx, rd_node_first_child(ctx, box));
  CHECK(rd_node_computed_style(ctx, mark)->display == RD_DISPLAY_FLEX);
  press(ctx, RD_KEY_SPACE);  // focused by the click
  CHECK(!rdx_checked(ctx, box) && changes == 2);
  CHECK(rd_node_computed_style(ctx, mark)->display == RD_DISPLAY_NONE);

  // Setters fire nothing; disabled controls ignore input.
  CHECK(rdx_set_checked(ctx, sw, true) && rdx_checked(ctx, sw));
  rd_node_set_states(ctx, box, RD_STATE_DISABLED, true);
  rd_ctx_layout(ctx, 800, 600);
  click(ctx, box);
  CHECK(!rdx_checked(ctx, box) && changes == 2);

  // Not a toggle.
  CHECK(!rdx_checked(ctx, root) && !rdx_set_checked(ctx, root, true));
  rd_ctx_destroy(ctx);
}

static void radios(void) {
  rd_ctx* ctx   = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node group = rdx_radio_group_create(ctx);
  rd_node a = rdx_radio_add(ctx, group, "Small"), b = rdx_radio_add(ctx, group, "Medium");
  rd_node c       = rdx_radio_add(ctx, group, "Large");
  rd_node before  = rd_node_create_button(ctx, "before", RD_TEXT_NUL);
  rd_node after   = rd_node_create_button(ctx, "after", RD_TEXT_NUL);
  int     changes = 0;
  rd_node_on(ctx, group, RD_EVENT_CHANGE, count, &changes, NULL, 0);
  rd_node_append(ctx, rd_ctx_root(ctx), before);
  rd_node_append(ctx, rd_ctx_root(ctx), group);
  rd_node_append(ctx, rd_ctx_root(ctx), after);
  rd_ctx_layout(ctx, 800, 600);

  CHECK(rdx_radio_selected(ctx, group) == -1);
  click(ctx, b);
  CHECK(rdx_radio_selected(ctx, group) == 1 && changes == 1);
  click(ctx, b);  // already selected
  CHECK(changes == 1);

  // Tab enters the group at the selection and leaves in one step.
  rd_node_focus(ctx, before);
  press(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), b));
  press(ctx, RD_KEY_TAB);
  CHECK(rd_node_eq(rd_ctx_focused(ctx), after));

  // Arrows move the selection and focus, wrapping, skipping disabled options.
  rd_node_focus(ctx, b);
  press(ctx, RD_KEY_DOWN);
  CHECK(rdx_radio_selected(ctx, group) == 2 && rd_node_eq(rd_ctx_focused(ctx), c) && changes == 2);
  rd_node_set_states(ctx, a, RD_STATE_DISABLED, true);
  press(ctx, RD_KEY_DOWN);  // wraps past the disabled first option
  CHECK(rdx_radio_selected(ctx, group) == 1 && changes == 3);
  press(ctx, RD_KEY_UP);
  press(ctx, RD_KEY_UP);
  CHECK(rdx_radio_selected(ctx, group) == 1);  // c, then wraps past a to b

  CHECK(rdx_radio_select(ctx, group, 0) && rdx_radio_selected(ctx, group) == 0 && changes == 5);
  CHECK(rdx_radio_select(ctx, group, -1) && rdx_radio_selected(ctx, group) == -1);
  CHECK(!rdx_radio_select(ctx, group, 3) && !rdx_radio_add(ctx, a, "not a group").index);
  rd_ctx_destroy(ctx);
}

static void sliders(void) {
  rd_ctx* ctx    = rd_ctx_create();
  rd_node slider = rdx_slider_create(ctx, &(rdx_slider_desc){.min = 0, .max = 10, .value = 3, .step = 1});
  rd_node_merge_style(ctx, slider, &(rd_style){.width = rd_px(216)});  // a 200px rail inside 8px padding
  int inputs = 0, changes = 0;
  rd_node_on(ctx, slider, RD_EVENT_INPUT, count, &inputs, NULL, 0);
  rd_node_on(ctx, slider, RD_EVENT_CHANGE, count, &changes, NULL, 0);
  rd_node_append(ctx, rd_ctx_root(ctx), slider);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rdx_slider_value(ctx, slider), 3));

  // Drag from 50% to beyond the end: values snap to steps and clamp; CHANGE once at release.
  rd_rect b = rd_node_bounds(ctx, slider);
  float   y = b.y + 10;
  rd_ctx_pointer(ctx, b.x + 8 + 100, y, RD_BUTTON_PRIMARY, 0, 0);
  CHECK(near(rdx_slider_value(ctx, slider), 5) && inputs == 1 && changes == 0);
  rd_ctx_pointer(ctx, b.x + 8 + 139, y, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, b.x + 600, y, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, b.x + 600, y, 0, 0, 0);
  CHECK(near(rdx_slider_value(ctx, slider), 10) && inputs == 3 && changes == 1);

  // Keys: focused by the press.
  press(ctx, RD_KEY_LEFT);
  CHECK(near(rdx_slider_value(ctx, slider), 9) && changes == 2);
  press(ctx, RD_KEY_HOME);
  CHECK(near(rdx_slider_value(ctx, slider), 0));
  press(ctx, RD_KEY_PAGE_UP);
  CHECK(near(rdx_slider_value(ctx, slider), 1));
  press(ctx, RD_KEY_HOME);
  press(ctx, RD_KEY_HOME);  // no change, no event
  CHECK(changes == 5);

  // The fill and thumb follow the value.
  rdx_slider_set_value(ctx, slider, 2.6f);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rdx_slider_value(ctx, slider), 3) && changes == 5);
  rd_node rail = rd_node_first_child(ctx, slider), fill = rd_node_first_child(ctx, rail);
  CHECK(near(rd_node_rect(ctx, fill).width, 60) && near(rd_node_rect(ctx, rd_node_last_child(ctx, rail)).x, 60 - 8));

  // A new range clamps and re-snaps the value, moves the thumb, and fires nothing.
  int before = changes + inputs;
  CHECK(rdx_slider_set_range(ctx, slider, 0, 2) && near(rdx_slider_value(ctx, slider), 2));  // 3 clamped to 2
  rd_ctx_layout(ctx, 800, 600);
  CHECK(near(rd_node_rect(ctx, fill).width, 200) && changes + inputs == before);
  CHECK(rdx_slider_set_range(ctx, slider, 0.5f, 20) && near(rdx_slider_value(ctx, slider), 2.5f));  // steps from 0.5
  CHECK(!rdx_slider_set_range(ctx, slider, 5, 5) && !rdx_slider_set_range(ctx, slider, 5, 1));
  CHECK(near(rdx_slider_value(ctx, slider), 2.5f) && !rdx_slider_set_range(ctx, rd_ctx_root(ctx), 0, 1));
  press(ctx, RD_KEY_END);
  CHECK(near(rdx_slider_value(ctx, slider), 19.5f));  // the last step within the range

  rd_node plain = rdx_slider_create(ctx, NULL);  // 0 to 100, continuous
  CHECK(near(rdx_slider_value(ctx, plain), 0) && rdx_slider_set_value(ctx, plain, 33.3f) &&
        near(rdx_slider_value(ctx, plain), 33.3f));
  rd_ctx_destroy(ctx);
}

static void at(rd_ctx* ctx, double t) {
  rd_ctx_set_time(ctx, t);
  rd_ctx_layout(ctx, 800, 600);
}

static void toasts(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  at(ctx, 0);
  rd_node first = rdx_toast_show(ctx, &(rdx_toast_desc){.title = "Saved", .text = "All changes saved."});
  rd_node stack = rd_node_parent(ctx, first);
  CHECK(rd_node_eq(rd_node_parent(ctx, stack), rd_ctx_overlay(ctx)));
  at(ctx, 0);
  rd_rect r = rd_node_bounds(ctx, first);
  CHECK(r.x + r.width > 800 - 16 + 20 && rd_node_computed_style(ctx, first)->opacity.value < 0.1f);  // sliding in
  at(ctx, 0.3);
  r = rd_node_bounds(ctx, first);
  CHECK(near(r.x + r.width, 800 - 16) && near(r.y + r.height, 600 - 16));  // in place, bottom right

  // Newer toasts stack above at the bottom; the stack keeps five.
  for (int j = 0; j < 5; j++) rdx_toast_show(ctx, &(rdx_toast_desc){.text = "more"});
  CHECK(rd_node_child_count(ctx, stack) == 5 && !rd_node_alive(ctx, first));

  // Top left, newest first; expiry after the duration plus the exit; sticky stays.
  rd_node t1 = rdx_toast_show(ctx, &(rdx_toast_desc){.text = "a", .position = RDX_TOAST_TOP_LEFT, .seconds = 1});
  rd_node t2 = rdx_toast_show(ctx, &(rdx_toast_desc){.text = "b", .position = RDX_TOAST_TOP_LEFT, .seconds = -1});
  rd_node tl = rd_node_parent(ctx, t1);
  CHECK(rd_node_eq(rd_node_first_child(ctx, tl), t2));
  at(ctx, 0.6);
  CHECK(near(rd_node_bounds(ctx, t2).x, 16) && near(rd_node_bounds(ctx, t2).y, 16));
  at(ctx, 1.3);   // expires: the fade starts
  at(ctx, 1.4);
  CHECK(rd_node_alive(ctx, t1) && rd_node_computed_style(ctx, t1)->opacity.value < 1);
  at(ctx, 1.6);
  CHECK(!rd_node_alive(ctx, t1) && rd_node_alive(ctx, t2));

  // Hover pauses; leaving restarts the full countdown.
  rd_node t3 = rdx_toast_show(ctx, &(rdx_toast_desc){.text = "c", .position = RDX_TOAST_TOP_RIGHT, .seconds = 1});
  at(ctx, 2);
  rd_rect h = rd_node_bounds(ctx, t3);
  rd_ctx_pointer(ctx, h.x + 20, h.y + 10, 0, 0, 0);
  at(ctx, 5);
  CHECK(rd_node_alive(ctx, t3));
  rd_ctx_pointer(ctx, 400, 300, 0, 0, 0);
  at(ctx, 5.5);
  CHECK(rd_node_alive(ctx, t3));
  at(ctx, 6.1);
  at(ctx, 6.4);
  CHECK(!rd_node_alive(ctx, t3));

  // The close button, and closing by handle.
  click(ctx, rd_node_last_child(ctx, t2));
  at(ctx, 7);
  CHECK(!rd_node_alive(ctx, t2));
  rd_node t4 = rdx_toast_show(ctx, &(rdx_toast_desc){.text = "d", .kind = RDX_TOAST_ERROR});
  rdx_toast_close(ctx, t4);
  at(ctx, 7.1);
  CHECK(rd_node_alive(ctx, t4));
  at(ctx, 7.3);
  CHECK(!rd_node_alive(ctx, t4));
  rdx_toast_close(ctx, rd_ctx_root(ctx));  // not a toast: ignored
  CHECK(rd_node_alive(ctx, rd_ctx_root(ctx)));
  rdx_toast_show(ctx, &(rdx_toast_desc){.text = "pending at destroy", .seconds = 10});
  rd_ctx_destroy(ctx);
}

int main(void) {
  toggles();
  radios();
  sliders();
  toasts();
  return TEST_RESULT();
}
