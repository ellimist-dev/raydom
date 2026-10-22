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

static void test_attach(void) {
  rd_ctx*           ctx = rd_ctx_create();
  static const char a = 0, b = 0;
  int               x = 1, y = 2;
  rd_node           n = rd_node_create(ctx);
  destroyed           = 0;
  CHECK(rd_node_attach(ctx, n, &a, &x, count_destroy) && rd_node_attach(ctx, n, &b, &y, count_destroy));
  CHECK(rd_node_attached(ctx, n, &a) == &x && rd_node_attached(ctx, n, &b) == &y);  // keys don't collide
  CHECK(rd_node_userdata(ctx, n) == NULL);                                           // userdata stays the app's
  CHECK(rd_node_attach(ctx, n, &a, &y, count_destroy) && destroyed == 1);            // replacing destroys the old
  CHECK(rd_node_attach(ctx, n, &a, &y, count_destroy) && destroyed == 1);            // the same data isn't
  CHECK(rd_node_attach(ctx, n, &b, NULL, NULL) && destroyed == 2 && !rd_node_attached(ctx, n, &b));  // removal
  rd_node_destroy(ctx, n);
  CHECK(destroyed == 3 && !rd_node_attached(ctx, n, &a));  // freed with the node
  CHECK(!rd_node_attach(ctx, n, &a, &x, count_destroy) && destroyed == 3);  // a dead node keeps nothing
  rd_node m = rd_node_create(ctx);  // reuses n's slot: nothing carried over
  CHECK(!rd_node_attached(ctx, m, &a));
  rd_node_attach(ctx, m, &a, &x, count_destroy);
  rd_ctx_destroy(ctx);
  CHECK(destroyed == 4);
}

// Destroying a subtree and computing styles walk without recursion, so depth can't exhaust the stack.
// The chain is built from the bottom up, so each append marks only an unattached parent.
static void test_deep_tree(void) {
  enum { DEPTH = 20000 };  // deep enough that the old recursive style walk overflowed an 8 MB stack
  rd_ctx* ctx  = rd_ctx_create();
  rd_node leaf = rd_node_create(ctx), top = leaf;
  destroyed    = 0;
  rd_node_set_userdata(ctx, leaf, NULL, count_destroy);
  for (int d = 1; d < DEPTH; d++) {
    rd_node n = rd_node_create(ctx);
    rd_node_set_userdata(ctx, n, NULL, count_destroy);
    rd_node_append(ctx, n, top);
    top = n;
  }
  rd_node_set_style(ctx, top, &(rd_style){.color = rd_hex(0x123456)});
  rd_node_append(ctx, rd_ctx_root(ctx), top);
  rd_ctx_compute_styles(ctx);
  CHECK(rd_node_computed_style(ctx, leaf)->color.g == 0x34);  // inherited DEPTH levels down
  rd_node_destroy(ctx, top);
  CHECK(destroyed == DEPTH && !rd_node_alive(ctx, leaf) && rd_ctx_node_count(ctx) == 1);
  rd_ctx_destroy(ctx);
}

static void test_extensions(void) {
  rd_ctx*           ctx = rd_ctx_create();
  static const char key = 0, other = 0;
  int               x = 1, y = 2;
  destroyed           = 0;
  CHECK(rd_ctx_extension(ctx, &key) == NULL);
  CHECK(rd_ctx_set_extension(ctx, &key, &x, count_destroy) && rd_ctx_extension(ctx, &key) == &x);
  CHECK(rd_ctx_set_extension(ctx, &key, &y, count_destroy) && destroyed == 1 && rd_ctx_extension(ctx, &key) == &y);
  CHECK(rd_ctx_set_extension(ctx, &other, &x, NULL) && rd_ctx_extension(ctx, &other) == &x);
  CHECK(!rd_ctx_set_extension(ctx, NULL, &x, NULL));
  rd_ctx_destroy(ctx);
  CHECK(destroyed == 2);
}

int main(void) {
  test_extensions();
  test_attach();
  test_deep_tree();
  test_append_order();
  test_move_and_cycles();
  test_destroy_subtree();
  test_userdata_replace_and_ctx_destroy();
  test_growth();
  return TEST_RESULT();
}
