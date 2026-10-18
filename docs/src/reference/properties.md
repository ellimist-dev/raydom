# Style properties

Every field of `rd_style`, generated from the `RD_STYLE_PROPS` X-macro in `include/raydom/style.h` and the defaults
in `src/style.c` at this version.

- **Default** is the value a node has when nothing sets the property and it isn't inherited.
- *inherited* properties take the parent's computed value when unset.
- *paint only* properties change drawing without triggering layout. All others trigger relayout when they change.

In C++, each property is a setter of the same name on `rd::Style`. Enumerated values are written in full in C, for
example `RD_DISPLAY_GRID`.

{{properties}}
