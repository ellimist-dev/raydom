# Roles

A role says what a node is: a button, a heading, a text input. raydom doesn't draw or behave differently by role,
with one exception below. Roles exist so a future accessibility backend can describe the tree to screen readers
without every application revisiting its controls. Set them now and that work becomes a backend change only.

## Constructors

The direct way to get a role is to create the node with one:

| C                               | C++                              | Role                                  |
| ------------------------------- | -------------------------------- | ------------------------------------- |
| `rd_node_create_text(ctx, s, n)`| `ctx.create_text(s, style)`      | text                                  |
| `rd_node_create_button(ctx, s, n)` | `ctx.create_button(s, style)` | button, with a text label            |
| `rd_node_create_button(ctx, NULL, 0)` | `ctx.create_button(style)` | button, empty, for icon or composite content |
| `rd_node_create_input(ctx, flags)` | `ctx.create_input(multiline, style)` | text input or multi-line text input |

```cpp
{{#include ../../examples/quickstart.cc:all}}
```

Each returns a detached node, or a null node on allocation failure.

## Setting a role

```c
rd_node_set_role(ctx, title, RD_ROLE_HEADING);
rd_node_set_role(ctx, menu, RD_ROLE_LIST);
rd_role r = rd_node_role(ctx, node);
```

`RD_ROLE_AUTO`, the default, derives the role from the node: multi-line or single-line text input for inputs, text
for other text leaves, canvas for nodes with a paint callback, and generic otherwise. `rd_node_role` returns the
resolved role. Setting `RD_ROLE_AUTO` returns a node to its derived role.

The roles are a subset of [AccessKit](https://accesskit.dev/)'s: generic, group, button, link, checkbox, radio
button, radio group, switch, slider, text, label, heading, text input, multi-line text input, list, list item, image,
canvas, menu, menu item, combo box, dialog, tooltip. More can be added
in a minor release.

## Focus

Interactive roles (button, link, checkbox, radio button, switch, slider, menu item, combo box) take focus without a `focusable` style, as inputs do:

- Tab reaches them, unless their `focusable` is `POINTER`, which keeps pointer focus and skips Tab.
- Enter and Space on a focused one fire `CLICK`, as for any focused node that isn't an input.
- To take one out of focus and pointer input, set `RD_STATE_DISABLED` on it.

Roles carry no other behavior. A checkbox doesn't toggle itself; the application tracks its state and styles it.
