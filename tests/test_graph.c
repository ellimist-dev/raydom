#include <math.h>
#include <raydom/extras/graph.h>
#include <raydom/raydom.h>

#include "check.h"

static bool near(float a, float b) { return fabsf(a - b) < 0.05f; }

typedef struct log {
  rdx_graph_event events[32];
  rdx_node        nodes[32];
  int             len;
} log_t;

static void record(rdx_graph* g, rdx_graph_event ev, rdx_node node, void* data) {
  (void)g;
  log_t* l = data;
  if (l->len < 32) {
    l->events[l->len]  = ev;
    l->nodes[l->len++] = node;
  }
}

static int count(const log_t* l, rdx_graph_event ev) {
  int c = 0;
  for (int j = 0; j < l->len; j++) c += l->events[j] == ev;
  return c;
}

static bool same(rdx_node a, rdx_node b) { return a.index == b.index && a.gen == b.gen; }

// A 400x300 canvas at (100, 50); with center (0, 0) and zoom 1, world (0, 0) is at screen (300, 200).
typedef struct fixture {
  rd_ctx*    ctx;
  rd_node    host;
  rdx_graph* g;
  log_t      log;
} fixture;

static void setup(fixture* f) {
  f->ctx = rd_ctx_create();
  rd_font_load_file(f->ctx, RAYDOM_TEST_FONT);
  rd_node root = rd_ctx_root(f->ctx);
  rd_node_set_style(f->ctx, root, &(rd_style){.padding = rd_edges_vh(rd_px(50), rd_px(100)), .align_items = RD_ALIGN_START});
  f->host = rd_node_create(f->ctx);
  rd_node_set_style(f->ctx, f->host, &(rd_style){.width = rd_px(400), .height = rd_px(300)});
  rd_node_append(f->ctx, root, f->host);
  f->g       = rdx_graph_create(f->ctx, f->host);
  f->log.len = 0;
  rdx_graph_on(f->g, record, &f->log, NULL);
  rd_ctx_layout(f->ctx, 800, 600);
}

static void test_nodes_edges_and_labels(void) {
  fixture f;
  setup(&f);
  rdx_node a = rdx_graph_add_node(f.g, &(rdx_node_desc){.position = {-100, 0}, .label = "alpha"});
  rdx_node b = rdx_graph_add_node(f.g, &(rdx_node_desc){.position = {100, 0}, .radius = 30});
  rdx_edge e = rdx_graph_connect(f.g, a, b, &(rdx_edge_desc){.directed = true, .label = "prepare"});
  CHECK(a.index && b.index && rdx_graph_node_alive(f.g, a));
  CHECK(rdx_graph_connect(f.g, a, a, NULL).gen == 0);
  rd_ctx_layout(f.ctx, 800, 600);

  rd_node label = rd_node_first_child(f.ctx, rd_node_first_child(f.ctx, rd_node_first_child(f.ctx, f.host)));
  rd_rect r     = rd_node_bounds(f.ctx, label);
  CHECK(near(r.x + r.width / 2, 200) && near(r.y, 200 + 20 + 4));

  CHECK(same(rdx_graph_node_at(f.g, 205, 210), a));
  CHECK(same(rdx_graph_node_at(f.g, 425, 200), b));
  CHECK(rdx_graph_node_at(f.g, 300, 200).index == 0);

  CHECK(rdx_graph_set_label(f.g, b, "beta"));
  CHECK(rdx_graph_set_label(f.g, a, NULL));
  CHECK(rdx_graph_remove_node(f.g, b));
  CHECK(!rdx_graph_node_alive(f.g, b) && !rdx_graph_set_edge_color(f.g, e, rd_rgb(1, 2, 3)));
  rdx_node c = rdx_graph_add_node(f.g, &(rdx_node_desc){0});
  CHECK(c.index == b.index && c.gen != b.gen && !rdx_graph_node_alive(f.g, b));
  rd_ctx_destroy(f.ctx);
}

static void test_pan_click_and_hover(void) {
  fixture f;
  setup(&f);
  rdx_node a = rdx_graph_add_node(f.g, &(rdx_node_desc){.position = {0, 0}, .label = "a"});
  rd_ctx_layout(f.ctx, 800, 600);

  rd_ctx_pointer(f.ctx, 300, 200, 0, 0, 0);
  CHECK(count(&f.log, RDX_EVENT_NODE_ENTER) == 1);
  rd_ctx_pointer(f.ctx, 300, 200, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 302, 201, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 302, 201, 0, 0, 0);
  CHECK(count(&f.log, RDX_EVENT_NODE_CLICK) == 1 && f.log.len > 0 && same(f.log.nodes[f.log.len - 1], a));

  rd_ctx_pointer(f.ctx, 150, 100, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 190, 130, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 700, 500, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 700, 500, 0, 0, 0);
  rd_vec2 center;
  float   zoom;
  rdx_graph_view(f.g, &center, &zoom);
  CHECK(near(center.x, -550) && near(center.y, -400) && zoom == 1);
  CHECK(count(&f.log, RDX_EVENT_VIEW) >= 2 && count(&f.log, RDX_EVENT_NODE_CLICK) == 1);
  CHECK(rdx_graph_node_at(f.g, 300, 200).index == 0 && same(rdx_graph_node_at(f.g, 850, 600), a));

  rdx_graph_set_flags(f.g, RDX_GRAPH_DRAG_NODES);
  rdx_graph_set_view(f.g, (rd_vec2){0, 0}, 1);
  rd_ctx_pointer(f.ctx, 300, 200, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 340, 210, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(f.ctx, 340, 210, 0, 0, 0);
  rd_vec2 p = rdx_graph_position(f.g, a);
  CHECK(near(p.x, 40) && near(p.y, 10) && count(&f.log, RDX_EVENT_NODE_DRAG) >= 1);
  rd_ctx_destroy(f.ctx);
}

static void test_zoom_and_fit(void) {
  fixture f;
  setup(&f);
  rdx_node nodes[4];
  for (int j = 0; j < 4; j++) nodes[j] = rdx_graph_add_node(f.g, &(rdx_node_desc){.radius = 10});
  rdx_graph_layout_circle(f.g, nodes, 4, (rd_vec2){1000, 1000}, 100);
  rd_vec2 top = rdx_graph_position(f.g, nodes[0]), right = rdx_graph_position(f.g, nodes[1]);
  CHECK(near(top.x, 1000) && near(top.y, 900) && near(right.x, 1100) && near(right.y, 1000));

  rdx_graph_fit(f.g, 40);
  rd_vec2 center;
  float   zoom;
  rdx_graph_view(f.g, &center, &zoom);
  CHECK(near(center.x, 1000) && near(center.y, 1000) && near(zoom, 220.0f / 220.0f));
  CHECK(same(rdx_graph_node_at(f.g, 300, 200 - 100), nodes[0]));

  rd_ctx_pointer(f.ctx, 400, 200, 0, 0, 1);
  rdx_graph_view(f.g, &center, &zoom);
  CHECK(zoom > 1.1f);
  CHECK(same(rdx_graph_node_at(f.g, 400, 200), nodes[1]));
  rd_ctx_destroy(f.ctx);
}

static int  callbacks_destroyed;
static void count_destroy(void* data) {
  (void)data;
  callbacks_destroyed++;
}

static void test_lifetime(void) {
  fixture f;
  setup(&f);
  rdx_graph_on(f.g, record, &f.log, count_destroy);
  rdx_node a = rdx_graph_add_node(f.g, &(rdx_node_desc){.label = "a"});
  rdx_node b = rdx_graph_add_node(f.g, &(rdx_node_desc){.position = {100, 0}});
  rdx_graph_set_messages(f.g, (rdx_message[]){{a, b, 0.5f, rd_rgb(255, 0, 0), 0}}, 1);
  rd_node_destroy(f.ctx, f.host);
  CHECK(callbacks_destroyed == 1);
  rd_ctx_destroy(f.ctx);
}

int main(void) {
  test_nodes_edges_and_labels();
  test_pan_click_and_hover();
  test_zoom_and_fit();
  test_lifetime();
  return TEST_RESULT();
}
