# Events

## Listeners

```cpp
{{#include ../../examples/events.cc:listeners}}
```

The C form is:

```c
rd_listener l = rd_node_on(ctx, node, RD_EVENT_CLICK, on_click, data, free_data, 0);  // or RD_LISTEN_CAPTURE
rd_node_off(ctx, l);                                                                   // runs free_data
```

`fn(ctx, ev, data)` receives the event. `destroy(data)` runs when the listener is removed, or when its node is
destroyed. C++ listeners take `rd::Event&` or nothing; the callable is owned by the listener.

## Propagation

Dispatch follows the DOM. The path from the root to the target is fixed when dispatch starts. Capture listeners run
from the root down to the target's parent, then the target's listeners run (its capture listeners
first), then bubble listeners run from the parent up to the root. `ev->phase` says which phase is running; `ev->current` is the node whose listener is running
and `ev->target` is the node the event is for.

| Call                                  | Effect                                                         |
| ------------------------------------- | -------------------------------------------------------------- |
| `rd_event_stop_propagation`           | Finish the current node's listeners, then stop                 |
| `rd_event_stop_immediate_propagation` | Stop now; later listeners on the same node don't run           |
| `rd_event_prevent_default`            | Skip raydom's default action (scroll, focus, editing, click)   |

```cpp
{{#include ../../examples/events.cc:stop}}
```

Stopping at the target doesn't undo capture listeners that already ran on ancestors.

## Event types

| Type                         | Target                     | Bubbles | Default action                     |
| ---------------------------- | -------------------------- | ------- | ---------------------------------- |
| `POINTER_DOWN`, `_UP`, `_MOVE` | Node under the pointer, or the capturing node | Yes | Down: focus, start selection |
| `POINTER_ENTER`, `_LEAVE`    | Each node entered or left  | No      |                                    |
| `CLICK`                      | Common ancestor of the press and release targets | Yes | |
| `WHEEL`                      | Node under the pointer     | Yes     | Scroll the nearest scroll container |
| `KEY_DOWN`, `KEY_UP`         | Focused node, or the root  | Yes     | Down: Tab focus, Enter/Space click, editing |
| `TEXT`                       | Focused node, or the root  | Yes     | Insert into a focused input        |
| `FOCUS`, `BLUR`              | The node gaining or losing focus | No |                                  |
| `FOCUS_IN`, `FOCUS_OUT`      | Same                       | Yes     |                                    |
| `INPUT`                      | The input                  | Yes     |                                    |
| `CHANGE`                     | The input                  | Yes     |                                    |
| `SELECTION_CHANGE`           | The root                   | No      |                                    |

Event fields: `x`, `y` in root coordinates; `local_x`, `local_y` relative to `current`'s border box; `wheel_x`,
`wheel_y` in notches; `button` (the button that changed) and `buttons` (held mask); `clicks` for `CLICK`; `key`, `mods`, `repeat`;
`codepoint` for text; `related` for focus events.

Buttons are `RD_BUTTON_PRIMARY`, `SECONDARY`, `MIDDLE`, and a mouse's side buttons `RD_BUTTON_BACK` and
`RD_BUTTON_FORWARD`; [the router](../router/router.md) takes the side buttons for history. In the browser,
Emscripten's GLFW reports every button past the middle one as the right button. So `rd_update` reads the right and
side buttons from the canvas's own events there. `CLICK` fires for the
primary button only. `ev->clicks` counts rapid clicks on the same node: 1, then 2 for a double
click and 3 for a triple, when each follows the last within half a second and four pixels. Keyboard activation
(Enter or Space) is always 1.

## Pointer capture

```cpp
{{#include ../../examples/events.cc:drag}}
```

While a node holds the capture, every pointer event goes to it, wherever the pointer is, until all buttons are
released or `rd_node_release_pointer_capture` is called. Use it for drags, sliders and splitters.

## Overriding defaults

```cpp
{{#include ../../examples/events.cc:wheel}}
```

## Firing events

`rd_node_dispatch(ctx, node, type)` fires an event through capture, target and bubble like any other, with the
current pointer position and modifiers. Components use it to announce their own `INPUT` and `CHANGE`, so
applications listen to them the same way as to built-in inputs. It returns false if a listener called
`rd_event_prevent_default`.

## Hit-testing

`rd_node_at(ctx, x, y)` returns the topmost node under a point after layout. Nodes with `pointer_events: none` are
skipped, and children inherit it unless they set `auto`. Use `none` for overlays and decorative layers that
shouldn't take the pointer.

## Cursors

The `cursor` style property sets the mouse cursor over a node. It's inherited, so setting it on a card applies to
everything inside. `rd_update` shows the cursor of the hovered node, or of the node holding the pointer capture.

```cpp
rd::Class grip = ctx.create_class(rd::Style().cursor(RD_CURSOR_EW_RESIZE));
grip.on(RD_STATE_ACTIVE, rd::Style().cursor(RD_CURSOR_MOVE));  // while pressed, for the whole drag
```

`RD_CURSOR_AUTO`, the default, shows:

- text over inputs, and over text whose `user_select` is `auto`;
- pointer over nodes with the link role;
- default over everything else, including text inside a button or other interactive role.

While the user drags a text selection, the cursor stays text even over other nodes. `RD_CURSOR_NONE` hides it.
`rd_ctx_cursor` returns the resolved cursor, for applications that drive input themselves. Don't call raylib's
`SetMouseCursor` alongside `rd_update`; set the style instead.

## Mutating during dispatch

Listeners may create, move and destroy nodes, including the target and the current node. A node destroyed during
dispatch is detached at once and freed when dispatch ends, so `ev->current` stays valid until the listener returns.
A listener added to a node while that node's listeners are running doesn't run for the current event.
