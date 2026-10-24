# Controls and toasts

`raydom::extras` has form controls (checkbox, switch, radio group, slider, numeric input) and toasts. They are ordinary nodes built
from core primitives, with a role, keyboard support and a default look, so they mix freely with your own nodes.

```cpp
#include <raydom/extras/controls.hh>  // C: controls.h and toast.h
```

## Events

A control fires `RD_EVENT_CHANGE` when the user changes it, and a slider and a numeric input also fire
`RD_EVENT_INPUT` on every change of value. Listen with `rd_node_on` or `node.on`; read the value with the getter.
Setters (`rdx_set_checked`, `rdx_radio_select`, `rdx_slider_set_value`, `rdx_number_set_value`) fire nothing, so
code that syncs a control from your model doesn't loop.

To disable a control, set `RD_STATE_DISABLED` on it: it stops taking input and focus, and the theme dims it.

## Checkbox and switch

```cpp
rd::Node remember = rdx::checkbox(ctx, "Remember me");
rd::Node sync     = rdx::toggle_switch(ctx, "Sync");
rdx::set_checked(sync, true);  // setters don't fire CHANGE
bool remembered = false;
remember.on(RD_EVENT_CHANGE, [&] { remembered = rdx::checked(remember); });
```

Each is a row holding the box or track and an optional label. Clicking anywhere on the row, or Space or Enter while
it has focus, toggles it. The row has `RD_STATE_CHECKED` while it's on.

## Radio group

```cpp
rd::Node size = rdx::radio_group(ctx);
for (const char* s : {"Small", "Medium", "Large"}) rdx::add_radio(size, s);
rdx::radio_select(size, 1);
size.on(RD_EVENT_CHANGE, [&] { /* rdx::radio_selected(size) */ });
```

At most one option is selected; `rdx_radio_selected` returns its index, or -1. `CHANGE` fires on the group. Tab
reaches the group once, at the selected option, and the arrow keys move the selection, wrapping and skipping disabled
options, as in browsers.

## Slider

```cpp
rd::Node    volume = rdx::slider(ctx, {.min = 0, .max = 100, .value = 40, .step = 5});
std::string shown;
volume.on(RD_EVENT_INPUT, [&] { shown = std::to_string(static_cast<int>(rdx::slider_value(volume))); });  // while moving
volume.on(RD_EVENT_CHANGE, [&] { /* save it */ });  // drag released or key pressed
```

`rdx_slider_set_range` changes the range later, for a timeline whose end moves, without recreating the slider; the
value is clamped into the new range and no event fires.

Dragging or pressing anywhere on the track moves it; the drag continues outside the slider. Arrow keys move one step
(1% of the range when `step` is 0), Page Up and Page Down a tenth of the range, Home and End to the ends. A zero
`rdx_slider_desc` is 0 to 100, continuous. With a step, values are `min + n * step`, as for an HTML range input; when
the range isn't a whole number of steps, the largest value is the last step below `max`. The default width is 200px; set `width` on the slider to change it.

## Numeric input

```cpp
rd::Node particles = rdx::number(ctx, {.min = 1000, .max = 100000, .value = 12000, .step = 1000});
rd::Node exposure  = rdx::number(ctx, {.min = 0.2f, .max = 3, .value = 1, .step = 0.05f, .decimals = 2});
exposure.on(RD_EVENT_INPUT, [&] { /* rdx::number_value(exposure) */ });  // every change
```

A text field with stepper buttons, for values a slider can't set precisely. Typing takes digits, a decimal point
when `decimals` is above 0, and a minus sign when the range allows negatives; anything else is refused. Enter or
leaving the field commits: the number is clamped to the range and rounded to `decimals` places, and text that isn't
a number reverts. Escape drops what was typed.

Up and Down step by `step` (ten steps with Shift), Page Up and Down by ten steps, and the stepper buttons step,
repeating while held. `step` defaults to one unit of the last place shown. With `max` at or below `min` the field is
unbounded. `INPUT` fires on every change of value; `CHANGE` when it settles: on commit, on each key, and when a held
stepper is released. The field's own text edits don't reach the control's listeners.

`rdx_number_set_range` changes the range and clamps the value without an event. `rdx_number_input` returns the text
field inside, for a placeholder or a style of its own. The role is `RD_ROLE_SPIN_BUTTON`. Disabled, the field can
still take focus but refuses edits and steps.

## Toasts

```cpp
rd::Node t = rdx::toast(ctx, {.title    = "Exported",
                              .text     = "report.csv is in your Downloads folder.",
                              .position = RDX_TOAST_TOP_RIGHT,
                              .kind     = RDX_TOAST_SUCCESS});
std::string host = "db.internal";
rdx::toast(ctx, {.text = "Lost connection to " + host, .kind = RDX_TOAST_ERROR, .seconds = -1});  // until closed
```

| Field      | Meaning                                                                    |
| ---------- | -------------------------------------------------------------------------- |
| `title`    | Optional first line                                                        |
| `text`     | The message                                                                |
| `position` | `BOTTOM_RIGHT` (zero), `BOTTOM_LEFT`, `TOP_RIGHT`, `TOP_LEFT`, `TOP_CENTER`, `BOTTOM_CENTER` |
| `kind`     | `INFO` (zero), `SUCCESS`, `WARNING`, `ERROR`: the color of the bar on the left |
| `seconds`  | How long it stays; 0 means 4, negative until closed                        |

In C++, `rdx::toast` takes `rdx::ToastOptions`, the same fields with `std::string` for the title and text.

Toasts live in the [overlay](../guide/overlay.md), one stack per position, 16px from the window's edges, newest
nearest the edge. A stack keeps the newest five. Hovering a toast pauses its countdown, which restarts when the
pointer leaves; the × button closes it, as does `rdx_toast_close`. `rdx_toast_show` returns the toast's node, so
you can append your own content, such as an Undo button.

For menus, selects, modals and tooltips, see [the next chapter](popups.md).

Toast timers need the clock to advance; with raylib's event waiting, use
[`rd_set_event_waiting`](../getting-started/frame-loop.md#event-waiting).

## Restyling

Every part of every control and toast has a class in `rdx_theme(ctx)`, built from a palette of colors. They start
with a dark look. To recolor everything, set the palette with `rdx_theme_set_palette`: the classes are redefined from
it and controls already created update; colors left unset keep theirs, so changing only `accent` works. Set the
palette first, since it redefines the classes. To change one class, merge into it with `rd_class_merge` or
`rd_class_merge_state` (C++: `merge` and `merge_on`), which keeps the layout properties you don't mention; or add your
own classes to individual nodes:

```cpp
// Every control at once: the accent, and anything else the palette names.
rdx_palette palette{};
palette.accent = rd_hex(0xf92672);
rdx_theme_set_palette(ctx.get(), &palette);
// One class: merge changes only the properties given, keeping the theme's layout.
const rdx_classes* theme = rdx_theme(ctx.get());
rd::Class(ctx.get(), theme->check_box).merge_on(RD_STATE_CHECKED, rd::Style().background(rd::hex(0x4cc38a)));  // green
```

| Classes                                              | Parts                                    |
| ---------------------------------------------------- | ---------------------------------------- |
| `check_row`, `check_box`, `check_mark`               | Checkbox row, box, checkmark (an SVG)    |
| `switch_row`, `switch_track`, `switch_thumb`         | Switch row, track, thumb                 |
| `radio_group`, `radio_row`, `radio_ring`, `radio_dot` | Group, option row, ring, dot            |
| `slider`, `slider_track`, `slider_fill`, `slider_thumb` | Slider, rail, filled part, thumb      |
| `number`, `number_input`, `number_steppers`, `number_step` | Numeric field, its text, the stepper column, a stepper |
| `label`                                              | Text beside a checkbox, switch or option |
| `toast_stack`, `toast`, `toast_bar`, `toast_title`, `toast_body`, `toast_close` | Toast parts |
| `toast_info`, `toast_success`, `toast_warning`, `toast_error` | The bar's color per kind        |

Parts share their control's states (see [shared states](../guide/styles.md#shared-states)), so a variant on a part
reacts to the whole control: `check_box` with `RD_STATE_HOVER` applies while the pointer is anywhere on the row, and
`RD_STATE_CHECKED` while the checkbox is on.

Controls keep their state outside the node, so `rd_node_set_userdata` on a control is still yours.
