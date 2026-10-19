# Adding an extras component

Follow the patterns in [Extras](../architecture/extras.md#patterns-every-component-follows). A checklist:

1. **API** in a header under `extras/include/raydom/extras/`: `rdx_<thing>_create` returning the root node, getters
   and setters, and a desc struct if there are options. Setters fire nothing.
2. **State** in a struct owned by one listener's data, freed by that listener's destroy callback. Register it with
   `rdx__track` so getters can find it; never use userdata.
3. **Classes** in `rdx_classes` (`theme.h`) with defaults in `theme.c`. Parts use `rdx__part` to share states.
   Interactive parts get `quick()` for color transitions.
4. **Role** on the root (add one to `role.h` if AccessKit has it and raydom doesn't), and focusability through the
   role or `focusable`.
5. **Keyboard** behavior matching the native control.
6. **Events**: `RD_EVENT_CHANGE` on the root via `rd_node_dispatch` when the user changes it; `INPUT` for continuous
   changes.
7. **Popups** register with the manager in `popups.c` so dismissal, Escape and focus return work.
8. **C++** wrapper in the matching `.hh`, owning callables with `new (std::nothrow)`.
9. **Tests** in `tests/`, driving it through `rd_ctx_pointer`, `rd_ctx_key` and `rd_ctx_set_time`. Include a check
   that its internal nodes leave userdata null.
10. **Docs**: a section in the extras chapters with a `docs/examples/` program, the theme classes in its restyling
    table, and the header in the installation page's table.
11. **Showcase**: a demo in the tour.

If it needs something the core can't do through public API, add the public core feature first, documented and
tested on its own.
