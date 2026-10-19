# Events and input

## Listeners and dispatch

Listeners are stored per node (`rd_node_events`) with an id, flags (capture) and a destroy callback. `rd_node_off`
during dispatch flags the listener removed; storage is compacted when dispatch ends.

`dispatch` builds the path from the target to the root **before** calling anything, then runs capture listeners
root-to-parent, the target's capture listeners, the target's bubble listeners, and bubble listeners up to the root.
Fixing the path first means listeners can reshape the tree without changing who receives the event. Listeners added
to a node during its own invocation don't run for that event: `invoke` reads the count up front.

`fire` returns whether the default action was prevented; default actions (focus on press, scrolling, editing,
Tab, Enter-to-click) run in the caller after dispatch, which is what makes `rd_event_prevent_default` work for all of
them. `rd_node_dispatch` is `fire` made public, for components announcing `INPUT` and `CHANGE`.

## Pointer

`rd_ctx_pointer(x, y, buttons, wheel)` is the only pointer entry point; `rd_update` feeds it from raylib. In order:

1. **Scrollbars first.** `scrollbar_pointer` checks for a bar under the pointer on the nearest scroll container
   above the hit node, or an active bar drag. If it handles the pointer, the content sees nothing: no pointer events,
   no focus change, no selection.
2. **Hover.** The hovered leaf is the hit node, or the pointer-capture node. `update_hover` diffs the old and new
   ancestor chains: `POINTER_LEAVE` deepest-first on nodes losing hover, `POINTER_ENTER` outermost-first on nodes
   gaining it, and `RD_STATE_HOVER` on the chain.
3. **Move.** `POINTER_MOVE`, then input drag-selection or document drag-selection.
4. **Buttons.** Per changed button: `POINTER_DOWN` (with `RD_STATE_ACTIVE` on the chain, and unless prevented,
   pointer focus and starting a selection), `POINTER_UP`, and `CLICK` on the common ancestor of the press and
   release targets for the primary button.
5. **Wheel.** A gesture latches to the scroll container chosen at its start (`wheel_latch`) until the wheel pauses
   (`LATCH_SECONDS`) or the pointer moves, so a horizontal scroll inside a vertical list doesn't get stolen halfway.

Disabled subtrees receive no down, up or click.

## Focus

`can_focus` accepts inputs, interactive roles (button, link, checkbox, radio, switch, slider, menu item, combo box,
tab, splitter) and nodes with `focusable`, if displayed and not disabled. Tab collects focusable nodes in tree order
within the focus scope (`rd_ctx_set_focus_scope`). `keyboard_modality` records whether focus last moved by keyboard,
which decides `RD_STATE_FOCUS_VISIBLE`. Focus changes fire `BLUR`, `FOCUS_OUT`, `FOCUS`, `FOCUS_IN`, after the states
are set, so a `FOCUS` listener can read `FOCUS_VISIBLE`. A focused node that gets destroyed, detached, hidden or
disabled loses focus silently (`focus_valid`).

## Keyboard

`rd_ctx_key` dispatches `KEY_DOWN`/`KEY_UP` to the focused node or the root. Default actions, unless prevented:
editing in an input, selection keys outside inputs, Tab navigation, and Enter or Space as `CLICK` on a focused
non-input. `rd_ctx_text` dispatches `TEXT` and inserts into a focused input.

## Cursors

`rd_ctx_cursor` resolves the cursor from state at the time it's asked: a selection drag forces text; a scrollbar
under the pointer forces default; otherwise the hovered node's computed `cursor`, with `AUTO` resolved by role and
content. `rd_update` applies it through raylib only when it changes.

## Hit-testing

`rd__hit` walks the laid-out tree back to front (last child first), honoring clips, `display: none`,
`pointer_events: none`, and skipping the overlay node itself. Measured leaves don't hit-test their children.
