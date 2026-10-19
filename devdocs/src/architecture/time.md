# Time: timers, animation, hooks

## The clock

The context has one clock, `ctx->now`, set by `rd_ctx_set_time` (from `GetTime()` in `rd_update`, by hand in
tests). Nothing reads the system time. Everything time-based in the library is therefore deterministic under test:
set the time, lay out, assert.

## Timers

`timer.c` keeps an array of timers with generational handles. `rd_ctx_set_time` runs due timers in due order. Two
rules keep callbacks safe:

- A timer set during a run gets that run's epoch and doesn't fire until the next clock update, so a callback that
  sets a zero-delay timer can't loop forever within a frame.
- A timer cleared from its own callback is marked and finished after the callback returns, so its data stays valid.

A repeating timer that falls behind fires once and skips ahead rather than bursting.

## Event waiting

raylib's `EnableEventWaiting` blocks in `glfwWaitEvents` without a timeout, so nothing can wake it at a time.
`rd_set_event_waiting` hands the switch to raydom: `rd_update` disables waiting while a timer is pending or anything
animates, and re-enables it when neither is true. Idle UIs cost nothing; a repeating timer keeps frames running.

## Transitions

Animation state is per node (`rd__anim`, allocated only while something animates), listed in `ctx->animating`.

**Starting.** `rd__transition` runs inside style computation, right after `build` produces the new target style.
For each animatable property (kinds `VAL`, `COLOR`, `EDGES`, `CORNERS`, `SHADOW`):

- already animating toward the same target: keep showing the current value;
- target differs from what's shown, the property's group is in `transition_properties`, and the duration is
  positive: record from (what's shown), to, start, duration and easing, and show the start value;
- otherwise: stop any animation and show the target.

Lengths animate only between equal units. `height` between `auto` and px is special-cased: from `auto` starts at the
laid-out height; to `auto` re-measures the natural height every frame and ends as `auto`.

**Ticking.** `rd__anim_tick` runs first in `rd_ctx_layout`. It writes interpolated values straight into the
computed styles, marks layout dirty for layout properties and restyles children for inherited ones, and frees
state when nothing is left.

Because animated values live in the computed style, layout, hit-testing, anchoring and drawing need no knowledge of
animation, which is how layout properties animate "for free" (at the cost of a relayout per frame for the moving
part of the tree).

**Explicit animations.** `rd_node_animate` records a pending request (duration, easing, which properties, optional
start values, the call time) and merges `to` into the inline style. The next computation consumes the request as
transitions with those parameters, starting at the call time. `done` is an ordinary timer.

**Easing** is CSS's cubic-bezier, solved with Newton's method and a bisection fallback.

## Layout hooks

See [Layout](layout.md#layout-hooks-and-the-second-pass). They're here in spirit: they let code react to this
frame's geometry within the frame, the way a virtualized list must.
