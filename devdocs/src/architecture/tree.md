# The context and the tree

## Slots and parallel arrays

`rd_ctx` (`src/internal.h`) stores nodes in **parallel arrays indexed by slot**:

| Array         | Element            | Contents                                                    |
| ------------- | ------------------ | ----------------------------------------------------------- |
| `slots`       | `rd_slot`          | Generation, flags, tree links, child count, userdata        |
| `styles`      | `rd_node_style`    | Computed style, inline style, classes, states, animation    |
| `layouts`     | `rd_node_layout`   | Rect, insets, scroll, caches, measure/paint callbacks, anchor |
| `events`      | `rd_node_events`   | Listeners                                                   |

A node is a slot index plus everything at that index in each array. Splitting by subsystem keeps each pass's data
dense: layout walks `slots` and `layouts` and rarely touches `events`. Slot 0 is never used, so index 0 means
"none" in tree links.

Tree links are indices (`parent`, `first_child`, `last_child`, `next_sibling`, `prev_sibling`), a doubly linked
child list per node, so append, insert and detach are O(1) and nothing is ever reallocated per node.

## Handles

`rd_node` is `{index, gen}`. Destroying a node clears its slot's `ALIVE` flag and puts the slot on a free list;
reusing the slot bumps its generation. Either way a stale handle stops matching. `rd__slot(ctx, n)` returns the slot only when the handle is
live; every public function starts with it, which is why stale handles are safe everywhere.

The free list threads through `next_sibling` of dead slots, so it costs no extra memory.

Arrays grow by doubling. Code must not hold a pointer into `ctx->slots` (or any per-node array) across anything that
can create a node; re-index after.

## Mutation

`rd_node_insert_before` refuses cycles (walks up from the parent) and the root; moving a node unlinks it first.
Every mutation marks dirty flags (`rd__mark_layout_dirty`, `rd__mark_style_dirty`) rather than recomputing.

The overlay is kept the root's last child: inserting at the end of the root inserts before it. See
[Layout](layout.md#the-overlay).

## Destroying during dispatch

Listeners may destroy any node, including the one whose listener is running. While `ctx->dispatch_depth` is
non-zero, `rd_node_destroy` detaches the node at once (so it stops being laid out, drawn and hit-tested) and pushes
it on `pending_destroy`. When the outermost dispatch ends, `flush_deferred` destroys those that are still detached;
one a later listener re-attached is spared. Listeners removed during dispatch are flagged and compacted at the same
point. This is what makes `ev->current` valid for the whole listener call.

## Extensions

`rd_ctx_set_extension(ctx, key, data, destroy)` stores one pointer per key, where the key is an address the caller
owns. Extras keep their theme, registries and popup manager here; it's a short linear list, looked up rarely.

## Destruction

`rd_ctx_destroy` frees in an order chosen so callbacks see a consistent world:

1. For every live slot: style, layout and event data (running measure, paint and listener destroy callbacks), then
   the userdata destroy callback. Slots keep their `ALIVE` flag during this loop, so a destroy callback that clears
   a timer or looks up a handle still works.
2. Style storage and fonts.
3. Layout hook destroy callbacks.
4. Extension destroy callbacks, after the nodes, because extensions often hold state keyed by nodes.
5. Images, then timers (running their destroy callbacks).
6. GPU resources (only if the raylib window still exists) and event storage.

Destroy callbacks must not mutate the tree; they may free memory, clear timers, and read handles.
