# Controls and toasts

`raydom::extras` has form controls (checkbox, switch, radio group, slider) and toasts. They are ordinary nodes built
from core primitives, with a role, keyboard support and a default look, so they mix freely with your own nodes.

```cpp
#include <raydom/extras/controls.hh>  // C: controls.h and toast.h
```

## Events

A control fires `RD_EVENT_CHANGE` when the user changes it, and a slider also fires `RD_EVENT_INPUT` while it moves.
Listen with `rd_node_on` or `node.on`; read the value with the getter. Setters (`rdx_set_checked`,
`rdx_radio_select`, `rdx_slider_set_value`) fire nothing, so code that syncs a control from your model doesn't loop.

To disable a control, set `RD_STATE_DISABLED` on it: it stops taking input and focus, and the theme dims it.

## Checkbox and switch

```cpp
{{#include ../../examples/controls.cc:toggles}}
```

Each is a row holding the box or track and an optional label. Clicking anywhere on the row, or Space or Enter while
it has focus, toggles it. The row has `RD_STATE_CHECKED` while it's on.

## Radio group

```cpp
{{#include ../../examples/controls.cc:radio}}
```

At most one option is selected; `rdx_radio_selected` returns its index, or -1. `CHANGE` fires on the group. Tab
reaches the group once, at the selected option, and the arrow keys move the selection, wrapping and skipping disabled
options, as in browsers.

## Slider

```cpp
{{#include ../../examples/controls.cc:slider}}
```

Dragging or pressing anywhere on the track moves it; the drag continues outside the slider. Arrow keys move one step
(1% of the range when `step` is 0), Page Up and Page Down a tenth of the range, Home and End to the ends. A zero
`rdx_slider_desc` is 0 to 100, continuous. The default width is 200px; set `width` on the slider to change it.

## Toasts

```cpp
{{#include ../../examples/controls.cc:toast}}
```

| Field      | Meaning                                                                    |
| ---------- | -------------------------------------------------------------------------- |
| `title`    | Optional first line                                                        |
| `text`     | The message                                                                |
| `position` | `BOTTOM_RIGHT` (zero), `BOTTOM_LEFT`, `TOP_RIGHT`, `TOP_LEFT`, `TOP_CENTER`, `BOTTOM_CENTER` |
| `kind`     | `INFO` (zero), `SUCCESS`, `WARNING`, `ERROR`: the color of the bar on the left |
| `seconds`  | How long it stays; 0 means 4, negative until closed                        |

Toasts live in the [overlay](../guide/overlay.md), one stack per position, 16px from the window's edges, newest
nearest the edge. A stack keeps the newest five. Hovering a toast pauses its countdown, which restarts when the
pointer leaves; the × button closes it, as does `rdx_toast_close`. `rdx_toast_show` returns the toast's node, so
you can append your own content, such as an Undo button.

For menus, selects, modals and tooltips, see [the next chapter](popups.md).

Toast timers need the clock to advance; with raylib's event waiting, use
[`rd_set_event_waiting`](../getting-started/frame-loop.md#event-waiting).

## Restyling

Every part of every control and toast has a class in `rdx_theme(ctx)`. They start with a dark look; restyle them
for the whole context with `rd_class_set` and `rd_class_set_state`, or add your own classes to individual nodes:

```cpp
{{#include ../../examples/controls.cc:theme}}
```

| Classes                                              | Parts                                    |
| ---------------------------------------------------- | ---------------------------------------- |
| `check_row`, `check_box`, `check_mark`               | Checkbox row, box, checkmark (an SVG)    |
| `switch_row`, `switch_track`, `switch_thumb`         | Switch row, track, thumb                 |
| `radio_group`, `radio_row`, `radio_ring`, `radio_dot` | Group, option row, ring, dot            |
| `slider`, `slider_track`, `slider_fill`, `slider_thumb` | Slider, rail, filled part, thumb      |
| `label`                                              | Text beside a checkbox, switch or option |
| `toast_stack`, `toast`, `toast_bar`, `toast_title`, `toast_body`, `toast_close` | Toast parts |
| `toast_info`, `toast_success`, `toast_warning`, `toast_error` | The bar's color per kind        |

Parts share their control's states (see [shared states](../guide/styles.md#shared-states)), so a variant on a part
reacts to the whole control: `check_box` with `RD_STATE_HOVER` applies while the pointer is anywhere on the row, and
`RD_STATE_CHECKED` while the checkbox is on.

Controls keep their state outside the node, so `rd_node_set_userdata` on a control is still yours.
