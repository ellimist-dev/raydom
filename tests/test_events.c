#include <raydom/raydom.h>
#include <stdint.h>

#include "check.h"

typedef struct entry {
  int            tag;
  float          local_x, local_y;
  rd_node        target, current;
  rd_event_type  type;
  rd_event_phase phase;
  uint8_t        button;
} entry;

static entry log_[64];
static int   log_len;
static int   destroyed;

static void reset(void) { log_len = 0; }

static void record(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  if (log_len < 64)
    log_[log_len++] = (entry){.tag     = (int)(intptr_t)data,
                              .local_x = ev->local_x,
                              .local_y = ev->local_y,
                              .target  = ev->target,
                              .current = ev->current,
                              .type    = ev->type,
                              .phase   = ev->phase,
                              .button  = ev->button};
}

static void count_destroy(void* data) {
  (void)data;
  destroyed++;
}

static rd_listener listen(rd_ctx* ctx, rd_node n, rd_event_type type, int tag, uint8_t flags) {
  return rd_node_on(ctx, n, type, record, (void*)(intptr_t)tag, count_destroy, flags);
}

static int count_type(rd_event_type type) {
  int c = 0;
  for (int j = 0; j < log_len; j++) c += log_[j].type == type;
  return c;
}

static int tags_of(rd_event_type type, int* out, int max) {
  int n = 0;
  for (int j = 0; j < log_len && n < max; j++)
    if (log_[j].type == type) out[n++] = log_[j].tag;
  return n;
}

static rd_node add(rd_ctx* ctx, rd_node parent, rd_style style) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_append(ctx, parent, n);
  return n;
}

static void click(rd_ctx* ctx, float x, float y) {
  rd_ctx_pointer(ctx, x, y, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, x, y, 0, 0, 0);
}

// root 400x300 > outer (0,0 200x200, padding 10) > inner (10,10 100x50)
typedef struct fixture {
  rd_ctx* ctx;
  rd_node root, outer, inner, sibling;
} fixture;

static fixture setup(void) {
  fixture f = {.ctx = rd_ctx_create()};
  f.root    = rd_ctx_root(f.ctx);
  f.outer   = add(f.ctx, f.root, (rd_style){.width = rd_px(200), .height = rd_px(200), .padding = rd_edges_all(rd_px(10))});
  f.inner   = add(f.ctx, f.outer, (rd_style){.height = rd_px(50)});
  f.sibling = add(f.ctx, f.outer, (rd_style){.height = rd_px(50)});
  rd_ctx_layout(f.ctx, 400, 300);
  reset();
  destroyed = 0;
  return f;
}

static void test_propagation_order(void) {
  fixture f = setup();
  listen(f.ctx, f.outer, RD_EVENT_CLICK, 1, RD_LISTEN_CAPTURE);
  listen(f.ctx, f.outer, RD_EVENT_CLICK, 2, 0);
  listen(f.ctx, f.inner, RD_EVENT_CLICK, 3, 0);
  listen(f.ctx, f.inner, RD_EVENT_CLICK, 4, RD_LISTEN_CAPTURE);
  listen(f.ctx, f.root, RD_EVENT_CLICK, 5, 0);
  click(f.ctx, 20, 20);

  int tags[8] = {0};
  CHECK(tags_of(RD_EVENT_CLICK, tags, 8) == 5);
  CHECK(tags[0] == 1 && tags[1] == 4 && tags[2] == 3 && tags[3] == 2 && tags[4] == 5);
  CHECK(log_[0].phase == RD_PHASE_CAPTURE && rd_node_eq(log_[0].current, f.outer));
  for (int j = 0; j < log_len; j++) CHECK(rd_node_eq(log_[j].target, f.inner));
  CHECK(count_type(RD_EVENT_POINTER_DOWN) == 0);
  rd_ctx_destroy(f.ctx);
  CHECK(destroyed == 5);
}

static void stop_here(rd_ctx* ctx, rd_event* ev, void* data) {
  record(ctx, ev, data);
  rd_event_stop_propagation(ev);
}

static void stop_now(rd_ctx* ctx, rd_event* ev, void* data) {
  record(ctx, ev, data);
  rd_event_stop_immediate_propagation(ev);
}

static void test_stop(void) {
  fixture f = setup();
  rd_node_on(f.ctx, f.inner, RD_EVENT_CLICK, stop_here, (void*)1, NULL, 0);
  listen(f.ctx, f.inner, RD_EVENT_CLICK, 2, 0);
  listen(f.ctx, f.outer, RD_EVENT_CLICK, 3, 0);
  click(f.ctx, 20, 20);
  int tags[8] = {0};
  CHECK(tags_of(RD_EVENT_CLICK, tags, 8) == 2 && tags[0] == 1 && tags[1] == 2);

  fixture g = setup();
  rd_node_on(g.ctx, g.inner, RD_EVENT_CLICK, stop_now, (void*)1, NULL, 0);
  listen(g.ctx, g.inner, RD_EVENT_CLICK, 2, 0);
  listen(g.ctx, g.outer, RD_EVENT_CLICK, 3, 0);
  click(g.ctx, 20, 20);
  CHECK(tags_of(RD_EVENT_CLICK, tags, 8) == 1 && tags[0] == 1);
  rd_ctx_destroy(f.ctx);
  rd_ctx_destroy(g.ctx);
}

static void test_click_targets(void) {
  fixture f = setup();
  listen(f.ctx, f.root, RD_EVENT_CLICK, 1, 0);
  listen(f.ctx, f.root, RD_EVENT_POINTER_DOWN, 2, 0);
  listen(f.ctx, f.root, RD_EVENT_POINTER_UP, 3, 0);

  rd_ctx_pointer(f.ctx, 20, 20, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 20, 70, 0, 0, 0);
  CHECK(log_len == 3);
  CHECK(log_[0].type == RD_EVENT_POINTER_DOWN && rd_node_eq(log_[0].target, f.inner));
  CHECK(log_[0].local_x == 20 && log_[0].local_y == 20 && log_[0].button == RD_BUTTON_PRIMARY);
  CHECK(log_[1].type == RD_EVENT_POINTER_UP && rd_node_eq(log_[1].target, f.sibling));
  CHECK(log_[2].type == RD_EVENT_CLICK && rd_node_eq(log_[2].target, f.outer));

  reset();
  rd_ctx_pointer(f.ctx, 20, 20, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 900, 900, 0, 0, 0);
  CHECK(count_type(RD_EVENT_CLICK) == 0 && count_type(RD_EVENT_POINTER_UP) == 0);

  reset();
  rd_ctx_pointer(f.ctx, 20, 20, RD_BUTTON_SECONDARY, 0, 0);
  rd_ctx_pointer(f.ctx, 20, 20, 0, 0, 0);
  CHECK(count_type(RD_EVENT_CLICK) == 0 && count_type(RD_EVENT_POINTER_DOWN) == 1);
  CHECK(log_[0].button == RD_BUTTON_SECONDARY);
  rd_ctx_destroy(f.ctx);
}

static void test_local_coordinates_and_move(void) {
  fixture f = setup();
  listen(f.ctx, f.inner, RD_EVENT_POINTER_MOVE, 1, 0);
  listen(f.ctx, f.outer, RD_EVENT_POINTER_MOVE, 2, 0);
  rd_ctx_pointer(f.ctx, 25, 30, 0, 0, 0);
  rd_ctx_pointer(f.ctx, 25, 30, 0, 0, 0);
  CHECK(log_len == 2);
  CHECK(log_[0].local_x == 15 && log_[0].local_y == 20);
  CHECK(log_[1].local_x == 25 && log_[1].local_y == 30);
  rd_ctx_destroy(f.ctx);
}

static void test_enter_leave(void) {
  fixture f        = setup();
  rd_node nodes[4] = {f.root, f.outer, f.inner, f.sibling};
  for (int j = 0; j < 4; j++) {
    listen(f.ctx, nodes[j], RD_EVENT_POINTER_ENTER, 10 + j, 0);
    listen(f.ctx, nodes[j], RD_EVENT_POINTER_LEAVE, 20 + j, 0);
  }
  rd_ctx_pointer(f.ctx, 20, 20, 0, 0, 0);
  int tags[8] = {0};
  CHECK(tags_of(RD_EVENT_POINTER_ENTER, tags, 8) == 3 && tags[0] == 10 && tags[1] == 11 && tags[2] == 12);
  CHECK(rd_node_states(f.ctx, f.outer) & RD_STATE_HOVER);

  reset();
  rd_ctx_pointer(f.ctx, 20, 70, 0, 0, 0);
  CHECK(tags_of(RD_EVENT_POINTER_LEAVE, tags, 8) == 1 && tags[0] == 22);
  CHECK(tags_of(RD_EVENT_POINTER_ENTER, tags, 8) == 1 && tags[0] == 13);
  CHECK(!(rd_node_states(f.ctx, f.inner) & RD_STATE_HOVER));

  reset();
  rd_ctx_pointer(f.ctx, 300, 250, 0, 0, 0);
  CHECK(tags_of(RD_EVENT_POINTER_LEAVE, tags, 8) == 2 && tags[0] == 23 && tags[1] == 21);
  CHECK(count_type(RD_EVENT_POINTER_ENTER) == 0);
  rd_ctx_destroy(f.ctx);
}

static void prevent(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)data;
  rd_event_prevent_default(ev);
}

static void test_wheel(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node list = add(ctx, rd_ctx_root(ctx),
                     (rd_style){.width = rd_px(100), .height = rd_px(100), .overflow_y = RD_OVERFLOW_SCROLL});
  rd_node row  = add(ctx, list, (rd_style){.height = rd_px(500)});
  rd_ctx_layout(ctx, 400, 300);
  rd_listener l = rd_node_on(ctx, row, RD_EVENT_WHEEL, prevent, NULL, NULL, 0);
  rd_ctx_pointer(ctx, 10, 10, 0, 0, -1);
  CHECK(rd_node_scroll(ctx, list).y == 0);
  rd_node_off(ctx, l);
  rd_ctx_pointer(ctx, 10, 10, 0, 0, -1);
  CHECK(rd_node_scroll(ctx, list).y == 48);
  CHECK(rd_node_bounds(ctx, row).y == -48);
  rd_ctx_destroy(ctx);
}

typedef struct mutate {
  rd_node victim, spawned, parent;
} mutate;

static void destroy_and_spawn(rd_ctx* ctx, rd_event* ev, void* data) {
  mutate* m = data;
  rd_node_destroy(ctx, m->victim);
  CHECK(rd_node_alive(ctx, m->victim));
  CHECK(rd_node_is_null(rd_node_parent(ctx, m->victim)));
  m->spawned = rd_node_create(ctx);
  rd_node_append(ctx, m->parent, m->spawned);
  record(ctx, ev, (void*)1);
}

static void test_mutation_during_dispatch(void) {
  fixture f = setup();
  mutate  m = {.victim = f.inner, .parent = f.root};
  rd_node_on(f.ctx, f.inner, RD_EVENT_CLICK, destroy_and_spawn, &m, NULL, 0);
  listen(f.ctx, f.inner, RD_EVENT_CLICK, 2, 0);
  listen(f.ctx, f.outer, RD_EVENT_CLICK, 3, 0);
  click(f.ctx, 20, 20);

  int tags[8] = {0};
  CHECK(tags_of(RD_EVENT_CLICK, tags, 8) == 3 && tags[0] == 1 && tags[1] == 2 && tags[2] == 3);
  CHECK(!rd_node_alive(f.ctx, f.inner));
  CHECK(destroyed == 1);
  CHECK(rd_node_alive(f.ctx, m.spawned) && rd_node_eq(rd_node_parent(f.ctx, m.spawned), f.root));
  CHECK(rd_node_child_count(f.ctx, f.outer) == 1);

  rd_ctx_layout(f.ctx, 400, 300);
  CHECK(rd_node_eq(rd_node_at(f.ctx, 20, 20), f.sibling));
  rd_ctx_destroy(f.ctx);
}

typedef struct self_off {
  rd_listener l;
  int         calls;
} self_off;

static void remove_self(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ev;
  self_off* s = data;
  s->calls++;
  rd_node_off(ctx, s->l);
  CHECK(destroyed == 0);
}

static void test_off_during_dispatch(void) {
  fixture  f = setup();
  self_off s = {0};
  s.l        = rd_node_on(f.ctx, f.inner, RD_EVENT_CLICK, remove_self, &s, count_destroy, 0);
  click(f.ctx, 20, 20);
  CHECK(s.calls == 1 && destroyed == 1);
  click(f.ctx, 20, 20);
  CHECK(s.calls == 1);
  CHECK(!rd_node_off(f.ctx, s.l));
  rd_ctx_destroy(f.ctx);
}

static void grab(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  rd_node_set_pointer_capture(ctx, ev->current);
}

static void test_pointer_capture(void) {
  fixture f = setup();
  rd_node_on(f.ctx, f.inner, RD_EVENT_POINTER_DOWN, grab, NULL, NULL, 0);
  listen(f.ctx, f.inner, RD_EVENT_POINTER_MOVE, 1, 0);
  listen(f.ctx, f.inner, RD_EVENT_POINTER_UP, 2, 0);
  rd_ctx_pointer(f.ctx, 20, 20, RD_BUTTON_PRIMARY, 0, 0);
  reset();
  rd_ctx_pointer(f.ctx, 350, 250, RD_BUTTON_PRIMARY, 0, 0);
  CHECK(log_len == 1 && log_[0].type == RD_EVENT_POINTER_MOVE && log_[0].local_x == 340);
  CHECK(rd_node_states(f.ctx, f.inner) & RD_STATE_HOVER);
  rd_ctx_pointer(f.ctx, 350, 250, 0, 0, 0);
  CHECK(log_len == 2 && log_[1].type == RD_EVENT_POINTER_UP);
  CHECK(rd_node_is_null(rd_ctx_pointer_capture(f.ctx)));
  CHECK(!(rd_node_states(f.ctx, f.inner) & RD_STATE_HOVER));
  CHECK(rd_node_eq(rd_ctx_hovered(f.ctx), f.root));
  rd_ctx_destroy(f.ctx);
}

static void test_pointer_events_none_and_disabled(void) {
  fixture f       = setup();
  rd_node overlay = add(f.ctx, f.root,
                        (rd_style){.position       = RD_POSITION_ABSOLUTE,
                                   .inset          = {.left = rd_px(0), .top = rd_px(0)},
                                   .width          = rd_px(300),
                                   .height         = rd_px(300),
                                   .pointer_events = RD_POINTER_EVENTS_NONE});
  rd_node passive = add(f.ctx, overlay, (rd_style){.height = rd_px(30)});
  rd_node badge   = add(f.ctx, overlay, (rd_style){.height = rd_px(30), .pointer_events = RD_POINTER_EVENTS_AUTO});
  rd_ctx_layout(f.ctx, 400, 300);
  CHECK(rd_node_eq(rd_node_at(f.ctx, 20, 20), f.inner));
  CHECK(rd_node_eq(rd_node_at(f.ctx, 20, 40), badge));
  (void)passive;

  listen(f.ctx, f.outer, RD_EVENT_CLICK, 1, 0);
  listen(f.ctx, f.outer, RD_EVENT_POINTER_DOWN, 2, 0);
  rd_node_set_states(f.ctx, f.inner, RD_STATE_DISABLED, true);
  click(f.ctx, 20, 20);
  CHECK(log_len == 0);
  CHECK(!(rd_node_states(f.ctx, f.inner) & RD_STATE_ACTIVE));
  rd_node_set_states(f.ctx, f.inner, RD_STATE_DISABLED, false);
  click(f.ctx, 20, 20);
  CHECK(count_type(RD_EVENT_CLICK) == 1);
  rd_ctx_destroy(f.ctx);
}

typedef struct relabel {
  rd_node label;
  int     clicks;
} relabel;

static void bump(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ev;
  relabel* r = data;
  r->clicks++;
  rd_node_set_style(ctx, r->label, &(rd_style){.width = rd_px(30.0f * (float)r->clicks), .height = rd_px(10)});
}

static void test_handler_drives_layout(void) {
  rd_ctx* ctx    = rd_ctx_create();
  rd_node button = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(100), .height = rd_px(40)});
  relabel r      = {.label = add(ctx, rd_ctx_root(ctx), (rd_style){.width = rd_px(0)})};
  rd_node_on(ctx, button, RD_EVENT_CLICK, bump, &r, NULL, 0);
  rd_ctx_layout(ctx, 400, 300);
  click(ctx, 10, 10);
  click(ctx, 10, 10);
  rd_ctx_layout(ctx, 400, 300);
  CHECK(r.clicks == 2 && rd_node_rect(ctx, r.label).width == 60);
  rd_ctx_destroy(ctx);
}

static void record_clicks(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  *(uint8_t*)data = ev->clicks;
}

static void test_click_count(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node a   = add(ctx, rd_ctx_root(ctx), (rd_style){.height = rd_px(50), .focusable = RD_FOCUSABLE_TAB});
  rd_node b   = add(ctx, rd_ctx_root(ctx), (rd_style){.height = rd_px(50)});
  uint8_t ca = 0, cb = 0;
  rd_node_on(ctx, a, RD_EVENT_CLICK, record_clicks, &ca, NULL, 0);
  rd_node_on(ctx, b, RD_EVENT_CLICK, record_clicks, &cb, NULL, 0);
  rd_ctx_layout(ctx, 400, 300);

  rd_ctx_set_time(ctx, 1);
  click(ctx, 10, 10);
  CHECK(ca == 1);
  rd_ctx_set_time(ctx, 1.3);
  click(ctx, 12, 11);  // soon and close: a double click
  CHECK(ca == 2);
  rd_ctx_set_time(ctx, 1.6);
  click(ctx, 12, 11);
  CHECK(ca == 3);
  rd_ctx_set_time(ctx, 2.5);  // too late
  click(ctx, 12, 11);
  CHECK(ca == 1);
  rd_ctx_set_time(ctx, 2.6);
  click(ctx, 30, 11);  // too far
  CHECK(ca == 1);
  rd_ctx_set_time(ctx, 2.7);
  click(ctx, 30, 70);  // another node starts over
  CHECK(cb == 1);
  rd_ctx_set_time(ctx, 2.8);
  click(ctx, 30, 11);
  CHECK(ca == 1);

  // Keyboard activation is a single click, and ends the run.
  rd_node_focus(ctx, a);
  rd_ctx_key(ctx, RD_KEY_ENTER, true, false, 0);
  CHECK(ca == 1);
  rd_ctx_set_time(ctx, 2.9);
  click(ctx, 30, 11);
  CHECK(ca == 1);
  rd_ctx_destroy(ctx);
}

static void prevent_it(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)data;
  rd_event_prevent_default(ev);
}

// rd_node_dispatch runs capture, target and bubble like any event; its result says whether a listener
// prevented the default. Releasing capture hands pointer events back to hit testing.
static void test_dispatch_and_release(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node a   = rd_node_create(ctx), b = rd_node_create(ctx);
  rd_node_set_style(ctx, a, &(rd_style){.width = rd_px(100), .height = rd_px(100)});
  rd_node_set_style(ctx, b, &(rd_style){.width = rd_px(50), .height = rd_px(50)});
  rd_node_append(ctx, rd_ctx_root(ctx), a);
  rd_node_append(ctx, a, b);
  rd_ctx_layout(ctx, 800, 600);
  reset();
  listen(ctx, a, RD_EVENT_CHANGE, 1, RD_LISTEN_CAPTURE);
  listen(ctx, b, RD_EVENT_CHANGE, 2, 0);
  listen(ctx, a, RD_EVENT_CHANGE, 3, 0);
  CHECK(rd_node_dispatch(ctx, b, RD_EVENT_CHANGE));
  CHECK(log_len == 3 && log_[0].tag == 1 && log_[0].phase == RD_PHASE_CAPTURE && log_[1].tag == 2 &&
        log_[2].tag == 3 && log_[2].phase == RD_PHASE_BUBBLE && rd_node_eq(log_[2].target, b));
  rd_node_on(ctx, a, RD_EVENT_CHANGE, prevent_it, NULL, NULL, 0);
  CHECK(!rd_node_dispatch(ctx, b, RD_EVENT_CHANGE));

  reset();
  listen(ctx, a, RD_EVENT_POINTER_MOVE, 4, 0);
  rd_node_set_pointer_capture(ctx, b);
  rd_ctx_pointer(ctx, 80, 80, 0, 0, 0);  // over a only, but b has the pointer
  CHECK(log_len == 1 && rd_node_eq(log_[0].target, b));
  rd_node_release_pointer_capture(ctx, b);
  rd_ctx_pointer(ctx, 81, 81, 0, 0, 0);
  CHECK(log_len == 2 && rd_node_eq(log_[1].target, a) && rd_node_is_null(rd_ctx_pointer_capture(ctx)));
  rd_ctx_destroy(ctx);
}

int main(void) {
  test_dispatch_and_release();
  test_propagation_order();
  test_click_count();
  test_stop();
  test_click_targets();
  test_local_coordinates_and_move();
  test_enter_leave();
  test_wheel();
  test_mutation_during_dispatch();
  test_off_during_dispatch();
  test_pointer_capture();
  test_pointer_events_none_and_disabled();
  test_handler_drives_layout();
  return TEST_RESULT();
}
