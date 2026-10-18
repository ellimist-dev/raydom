# Nodes and the tree

A context starts with one node, the root, which is sized to the layout viewport. Every other node is created
detached and becomes visible when it is attached under the root.

```c
rd_ctx* ctx  = rd_ctx_create();
rd_node root = rd_ctx_root(ctx);

rd_node list = rd_node_create(ctx);
rd_node a = rd_node_create(ctx), b = rd_node_create(ctx), c = rd_node_create(ctx);
rd_node_append(ctx, root, list);
rd_node_append(ctx, list, a);
rd_node_append(ctx, list, c);
rd_node_insert_before(ctx, list, b, c);  // list: a, b, c
```

## Handles

`rd_node` is `{index, gen}`. The context stores nodes in a slot array; `index` names a slot and `gen` counts how
many times that slot has been reused. Destroying a node bumps its slot's generation, so old handles stop matching.

Consequences:

- Handles are plain values. Copy them, store them in structs, compare them with `rd_node_eq`.
- A stale handle is safe. Every function checks it and returns `false`, a null node, or does nothing.
- `rd_node_is_null` tests for the zero handle, which functions return on failure. It doesn't test liveness; use
  `rd_node_alive` for that.

## Walking

```c
for (rd_node n = rd_node_first_child(ctx, list); !rd_node_is_null(n); n = rd_node_next_sibling(ctx, n)) {
  // visit n
}
```

C++ offers `for (rd::Node child : node.children())`. Neither form is stable if the loop body adds or removes siblings
of the node being visited; collect the nodes first in that case.

## Moving and detaching

```c
rd_node_append(ctx, root, b);  // moving: b leaves list and becomes root's last child
rd_node_detach(ctx, c);        // c stays alive, owned by the context, outside the tree
rd_node_append(ctx, list, c);  // and can be attached again later
```

`rd_node_append` and `rd_node_insert_before` move a node that already has a parent. They fail, returning `false`,
when the move would create a cycle, or when either handle is stale. A detached node keeps its subtree, styles and
listeners. It isn't laid out, drawn or hit-tested.

## Userdata

Each node carries one `void*` and an optional destroy callback that runs when the node is destroyed or the userdata
is replaced.

```c
typedef struct item {
  int id;
} item;

static void free_item(void* data) { free(data); }
```

```c
item* data = malloc(sizeof *data);
data->id   = 7;
rd_node_set_userdata(ctx, a, data, free_item);
item* back = rd_node_userdata(ctx, a);
```

C++: `node.emplace_userdata<T>(args...)` allocates a `T` owned by the node, and `node.userdata<T>()` returns it.

## Extension storage

Libraries built on raydom can keep per-context data without a global map:

```c
static const char key;  // any address the library owns
rd_ctx_set_extension(ctx, &key, data, free_data);  // destroyed with the context
my_state* s = rd_ctx_extension(ctx, &key);
```

The extras controls keep their classes and state this way, which leaves every node's userdata to the application.

## Destroying

```c
rd_node_destroy(ctx, list);         // destroys list, a and c; a's userdata is freed
bool alive = rd_node_alive(ctx, a);  // false: the handle is stale, and every call on it is a no-op
```

`rd_node_destroy` destroys the subtree, depth first, and runs its destroy callbacks. The root can't be destroyed.
Destroying a node from inside one of its own listeners is allowed; see [Events](events.md#mutating-during-dispatch).
