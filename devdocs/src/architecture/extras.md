# Extras

`raydom::extras` is a second library built only on the public core API. That constraint is the point: if a
component needs something the core doesn't offer publicly, the core gains a public feature that applications can use
too. Per-context storage (`rd_ctx_set_extension`), post-layout callbacks (`rd_ctx_on_layout`), firing events
(`rd_node_dispatch`), shared states, the checked state, focus scopes and roles all arrived this way.

## Files

| File           | Contents                                                           |
| -------------- | ------------------------------------------------------------------ |
| `theme.c`      | `rdx_theme`: every component's classes, created on first use; SVG icons |
| `registry.c`   | `rdx__track` / `rdx__lookup`: objects by node                       |
| `controls.c`   | Checkbox, switch, radio group, slider                              |
| `toast.c`      | Toasts                                                             |
| `popups.c`     | The popup manager, menus, context menus, select, modal, tooltip    |
| `views.c`      | Tabs, splitter                                                     |
| `lists.c`      | Virtualized list, table, tree                                      |
| `graph.c`      | The network view                                                   |
| `dialog.c`     | File dialogs over nativefiledialog-extended                        |

## Patterns every component follows

**A component is a node.** Creating one returns its root node, which the application appends anywhere. Internal
parts are child nodes with classes from the theme.

**State lives outside the node.** A component's state struct is the data of one of its listeners, with a destroy
callback that frees it, so it's freed exactly when the node is destroyed. Getters find the state through a registry
keyed by node handle (`rdx__lookup`); the registry checks the node is alive before returning a pointer, which is the
only proof the state hasn't been freed. Controls use an index by slot number for O(1) lookup; the rest use the
generic registry. **Never use node userdata**: it belongs to the application (see
[Conventions](../conventions.md#ownership)).

**Changes are events.** The component fires `RD_EVENT_CHANGE` (and `INPUT` for continuous controls) on its root
with `rd_node_dispatch`, when the user changes it. Setters fire nothing, so an application syncing a component from
its model can't cause a feedback loop.

**Looks are classes.** `rdx_theme(ctx)` returns `rdx_classes`, one class per part. Parts call `rdx__part`, which
adds the class and `rd_node_set_share_states`, so a part's variants follow the component's states: the box of a
checkbox looks hovered while the pointer is anywhere on its row. State variants on parts are the main restyling
surface; components set inline styles only for per-instance geometry (a slider's fill width, a tree row's indent).

**Keyboard behavior matches the platform's native control:** roving focus in radio groups and tab bars, arrows,
Home/End and Page keys in lists and sliders, Escape closing popups.

## The popup manager

`popups.c` keeps one manager per context with the open popups (bottom to top), and registers four listeners on the
root, once, in the capture phase:

- `POINTER_DOWN`: close popups from the top until reaching one containing the target (or whose anchor contains it),
  or a modal.
- `WHEEL`: close menus and lists the wheel isn't inside.
- `KEY_DOWN`: Escape closes the topmost popup and stops the event.
- `POINTER_MOVE`: close tooltips whose owner was destroyed.

Closing returns focus to the popup's opener if focus was inside it, and for modals restores the previous focus
scope and calls `on_close`. Capture-phase listeners on the root run before anything else sees the input, which is
how outside presses close a menu before the press reaches what's under it.

## Virtualized lists

A list is a scroll container holding a spacer as tall as all rows and a pool of absolutely positioned row nodes. Each
list has a node-scoped layout hook (`rd_node_on_layout`), removed with the list and skipped while it's detached: compute the visible range from this layout's scroll offset and
size (plus three rows of overscan each side), release pooled rows outside it (`display: none`), and bind rows for
indices without one: empty the row, position it, call the application's fill callback. Bound rows that stay in range
are left alone, so scrolling costs only the rows entering view. Tables and trees are lists with their own fill
callbacks.

Fixed row height is what makes this O(visible): any row's position is `index * height`.

## Toasts

Each position has a stack node in the overlay. Toasts slide in with `rd_node_animate` from an offset margin, and
close by animating opacity, height and padding to zero (height from `auto`, so the stack closes up smoothly), then
destroying themselves with a timer. A per-context list of live toasts lets `rdx_toast_close` find a toast's state.
