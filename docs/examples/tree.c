// Runs headless as a test: every claim in the "Nodes and the tree" chapter is checked here.
#include <assert.h>
#include <raydom/raydom.h>
#include <stdlib.h>

// ANCHOR: userdata
typedef struct item {
  int id;
} item;

static void free_item(void* data) { free(data); }
// ANCHOR_END: userdata

int main(void) {
  // ANCHOR: build
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);

  rd_node list = rd_node_create(ctx);
  rd_node a = rd_node_create(ctx), b = rd_node_create(ctx), c = rd_node_create(ctx);
  rd_node_append(ctx, root, list);
  rd_node_append(ctx, list, a);
  rd_node_append(ctx, list, c);
  rd_node_insert_before(ctx, list, b, c);  // list: a, b, c
  // ANCHOR_END: build
  assert(rd_node_child_count(ctx, list) == 3);

  // ANCHOR: walk
  for (rd_node n = rd_node_first_child(ctx, list); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n)) {
    // visit n
  }
  // ANCHOR_END: walk

  // ANCHOR: move
  rd_node_append(ctx, root, b);  // moving: b leaves list and becomes root's last child
  rd_node_detach(ctx, c);        // c stays alive, owned by the context, outside the tree
  rd_node_append(ctx, list, c);  // and can be attached again later
  // ANCHOR_END: move
  assert(rd_node_eq(rd_node_parent(ctx, b), root) && rd_node_child_count(ctx, list) == 2);

  // ANCHOR: attach-data
  item* data = malloc(sizeof *data);
  data->id   = 7;
  rd_node_set_userdata(ctx, a, data, free_item);
  item* back = rd_node_userdata(ctx, a);
  // ANCHOR_END: attach-data
  assert(back->id == 7);

  // ANCHOR: destroy
  rd_node_destroy(ctx, list);         // destroys list, a and c; a's userdata is freed
  bool alive = rd_node_alive(ctx, a);  // false: the handle is stale, and every call on it is a no-op
  // ANCHOR_END: destroy
  assert(!alive && rd_node_alive(ctx, b));

  rd_ctx_destroy(ctx);
  return 0;
}
