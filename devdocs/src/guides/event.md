# Adding an event type

1. Add the type to `rd_event_type` in `include/raydom/events.h`, **before** `RD_EVENT_TYPE_COUNT`, with a comment
   saying where it's fired and whether it bubbles. Appending keeps existing values stable.
2. Add any fields it needs to `rd_event`, and accessors to `rd::Event` in `raydom.hh`.
3. Fire it with `fire(ctx, type, target, button, bubbles)` from the subsystem that owns the moment, or with
   `rd_node_dispatch` if it's a component-level event. If it has a default action, run it after dispatch and only
   when `fire` reports it wasn't prevented.
4. Document it in the event table of the user book's Events chapter.
5. Test the target, the propagation and the default action in `tests/test_events.c`.

Most component-level needs are covered by `CHANGE` and `INPUT` with `rd_node_dispatch`; prefer those over a new type.
