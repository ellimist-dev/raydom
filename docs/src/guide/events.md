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
`wheel_y` in notches; `button` (the button that changed) and `buttons` (held mask); `key`, `mods`, `repeat`;
`codepoint` for text; `related` for focus events.

`CLICK` fires for the primary button only.

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

## Hit-testing

`rd_node_at(ctx, x, y)` returns the topmost node under a point after layout. Nodes with `pointer_events: none` are
skipped, and children inherit it unless they set `auto`. Use `none` for overlays and decorative layers that
shouldn't take the pointer.

## Mutating during dispatch

Listeners may create, move and destroy nodes, including the target and the current node. A node destroyed during
dispatch is detached at once and freed when dispatch ends, so `ev->current` stays valid until the listener returns.
A listener added to a node while that node's listeners are running doesn't run for the current event.
