#include <raydom/raydom.h>

#include "check.h"

static int  destroyed = 0;
static void count_destroy(void* p) {
  (void)p;
  destroyed++;
}

static void test_append_order(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node a = rd_node_create(ctx), b = rd_node_create(ctx), c = rd_node_create(ctx);
  CHECK(rd_node_append(ctx, root, a));
  CHECK(rd_node_append(ctx, root, c));
  CHECK(rd_node_insert_before(ctx, root, b, c));
  CHECK(rd_node_child_count(ctx, root) == 3);
  CHECK(rd_node_eq(rd_node_first_child(ctx, root), a));
  CHECK(rd_node_eq(rd_node_next_sibling(ctx, a), b));
  CHECK(rd_node_eq(rd_node_next_sibling(ctx, b), c));
  CHECK(rd_node_eq(rd_node_last_child(ctx, root), c));
  CHECK(rd_node_eq(rd_node_prev_sibling(ctx, c), b));
  CHECK(rd_node_eq(rd_node_parent(ctx, b), root));

  CHECK(rd_node_insert_before(ctx, root, c, a));
  CHECK(rd_node_eq(rd_node_first_child(ctx, root), c));
  CHECK(rd_node_eq(rd_node_last_child(ctx, root), b));
  CHECK(rd_node_child_count(ctx, root) == 3);
  rd_ctx_destroy(ctx);
}

static void test_move_and_cycles(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node a = rd_node_create(ctx), b = rd_node_create(ctx), c = rd_node_create(ctx);
  rd_node_append(ctx, root, a);
  rd_node_append(ctx, a, b);
  rd_node_append(ctx, b, c);

  CHECK(!rd_node_append(ctx, c, a));
  CHECK(!rd_node_append(ctx, a, a));
  CHECK(!rd_node_append(ctx, a, root));
  CHECK(!rd_node_insert_before(ctx, root, b, c));

  CHECK(rd_node_append(ctx, root, c));
  CHECK(rd_node_child_count(ctx, b) == 0);
  CHECK(rd_node_child_count(ctx, root) == 2);
  CHECK(rd_node_eq(rd_node_parent(ctx, c), root));

  rd_node_detach(ctx, c);
  CHECK(rd_node_is_null(rd_node_parent(ctx, c)));
  CHECK(rd_node_alive(ctx, c));
  CHECK(rd_node_child_count(ctx, root) == 1);
  rd_ctx_destroy(ctx);
}

static void test_destroy_subtree(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node a = rd_node_create(ctx), b = rd_node_create(ctx), c = rd_node_create(ctx);
  rd_node_append(ctx, root, a);
  rd_node_append(ctx, a, b);
  rd_node_append(ctx, a, c);
  rd_node_set_userdata(ctx, a, NULL, count_destroy);
  rd_node_set_userdata(ctx, b, NULL, count_destroy);
  rd_node_set_userdata(ctx, c, NULL, count_destroy);

  destroyed = 0;
  rd_node_destroy(ctx, a);
  CHECK(destroyed == 3);
  CHECK(!rd_node_alive(ctx, a) && !rd_node_alive(ctx, b) && !rd_node_alive(ctx, c));
  CHECK(rd_node_child_count(ctx, root) == 0);
  CHECK(rd_ctx_node_count(ctx) == 1);

  rd_node d = rd_node_create(ctx);
  CHECK(!rd_node_alive(ctx, c));
  CHECK(!rd_node_append(ctx, root, b));
  CHECK(rd_node_alive(ctx, d));

  rd_node_destroy(ctx, root);
  CHECK(rd_node_alive(ctx, root));
  rd_ctx_destroy(ctx);
}

static void test_userdata_replace_and_ctx_destroy(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node a   = rd_node_create(ctx);
  int     x = 1, y = 2;
  destroyed = 0;
  rd_node_set_userdata(ctx, a, &x, count_destroy);
  rd_node_set_userdata(ctx, a, &x, count_destroy);
  CHECK(destroyed == 0);
  rd_node_set_userdata(ctx, a, &y, count_destroy);
  CHECK(destroyed == 1);
  CHECK(rd_node_userdata(ctx, a) == &y);
  rd_ctx_destroy(ctx);
  CHECK(destroyed == 2);
}

static void test_growth(void) {
  rd_ctx* ctx   = rd_ctx_create();
  rd_node root  = rd_ctx_root(ctx);
  rd_node first = {0}, last = {0};
  for (int i = 0; i < 1000; i++) {
    rd_node n = rd_node_create(ctx);
    rd_node_append(ctx, root, n);
    if (i == 0) first = n;
    last = n;
  }
  CHECK(rd_node_child_count(ctx, root) == 1000);
  CHECK(rd_node_eq(rd_node_first_child(ctx, root), first));
  CHECK(rd_node_eq(rd_node_last_child(ctx, root), last));
  CHECK(rd_ctx_node_count(ctx) == 1001);
  rd_ctx_destroy(ctx);
}

int main(void) {
  test_append_order();
  test_move_and_cycles();
  test_destroy_subtree();
  test_userdata_replace_and_ctx_destroy();
  test_growth();
  return TEST_RESULT();
}
