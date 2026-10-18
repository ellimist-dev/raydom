# Nodes and the tree

A context starts with one node, the root, which is sized to the layout viewport. Every other node is created
detached and becomes visible when it is attached under the root.

```c
{{#include ../../examples/tree.c:build}}
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
{{#include ../../examples/tree.c:walk}}
```

C++ offers `for (rd::Node child : node.children())`. Neither form is stable if the loop body adds or removes siblings
of the node being visited; collect the nodes first in that case.

## Moving and detaching

```c
{{#include ../../examples/tree.c:move}}
```

`rd_node_append` and `rd_node_insert_before` move a node that already has a parent. They fail, returning `false`,
when the move would create a cycle, or when either handle is stale. A detached node keeps its subtree, styles and
listeners. It isn't laid out, drawn or hit-tested.

## Userdata

Each node carries one `void*` and an optional destroy callback that runs when the node is destroyed or the userdata
is replaced.

```c
{{#include ../../examples/tree.c:userdata}}
```

```c
{{#include ../../examples/tree.c:attach-data}}
```

C++: `node.emplace_userdata<T>(args...)` allocates a `T` owned by the node, and `node.userdata<T>()` returns it.

## Destroying

```c
{{#include ../../examples/tree.c:destroy}}
```

`rd_node_destroy` destroys the subtree, depth first, and runs its destroy callbacks. The root can't be destroyed.
Destroying a node from inside one of its own listeners is allowed; see [Events](events.md#mutating-during-dispatch).
