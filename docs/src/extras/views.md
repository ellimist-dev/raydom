# Tabs, splitters, lists, tables and trees

```cpp
#include <raydom/extras/views.hh>  // C: views.h
```

Each of these fires `CHANGE` on its node when the user changes it; the setters fire nothing.

## Tabs

```cpp
{{#include ../../examples/views.cc:tabs}}
```

`rdx_tabs_add` returns the tab's panel. Only the selected panel is shown. Tab reaches the bar once, at the selected
tab; Left, Right, Home and End move between tabs.

## Splitter

```cpp
{{#include ../../examples/views.cc:split}}
```

Two panes with a handle between them, side by side or (`vertical`) stacked. Dragging the handle or, with it
focused, the arrow keys resize the panes, within each pane's minimum. The first pane's size is a fraction of the
splitter, so it follows window resizes; `rdx_split_fraction` reads it and `CHANGE` fires when a drag ends.

## Virtualized list

```cpp
{{#include ../../examples/views.cc:list}}
```

The list holds a spacer as tall as every row and positions only the rows in view, plus three on each side, over it.
A [layout hook](../guide/layout.md#layout-hooks) rebinds rows after each layout, so a scroll shows the right rows
in the same frame. Rows are reused: `fill` gets an empty row each time it's asked for an index, and fills it.

- **Fixed height.** Every row has the same height, so the list can place any row without measuring the others.
- **Selection.** Clicking a row, or Up, Down, Page Up, Page Down, Home and End while the list has focus, selects
  one row, scrolls it into view, and fires `CHANGE`. The selected row has `RD_STATE_CHECKED`.
- **Data changes.** `rdx_list_set_count` changes the number of rows; `rdx_list_refresh` fills the rows in view
  again.

## Table

```cpp
{{#include ../../examples/views.cc:table}}
```

A header over a virtualized list whose rows use the header's grid columns, so cells line up with their headers.
Widths are px or fr. `cell` fills each cell; `header` runs when a column header is clicked, which is where sorting
goes: reorder your data, then `rdx_list_refresh(rdx_table_list(table))`. Selection and `CHANGE` are on the body,
`rdx_table_list`.

### One listener for every row

Rows are reused as the list scrolls, and only their contents are rebuilt, so per-row listeners would follow the
row node, not the data. Put one listener on the list and ask which row an event came from:

```cpp
{{#include ../../examples/views.cc:rows}}
```

`rdx_list_index_of(ctx, list, node)` returns the row index for the row or anything inside it, or -1. For a table,
pass `rdx_table_list(table)`; for a tree, `rdx_tree_item_of` returns the item id instead of the visible row.
`ev->clicks` counts rapid clicks on the same node, so `clicks == 2` is a double click.

## Tree view

```cpp
{{#include ../../examples/views.cc:tree}}
```

Items are added under a parent (-1 for the top level) and get an id. The tree shows the expanded items as an
indented, virtualized list with disclosure chevrons. Right expands an item, or moves to its first child; Left
collapses it, or moves to its parent; Enter toggles. Selecting a hidden item with `rdx_tree_select` expands its
ancestors, and collapsing an ancestor of the selection moves the selection to the ancestor.

## Restyling

| Classes in `rdx_theme(ctx)`                                | Parts                                     |
| ---------------------------------------------------------- | ----------------------------------------- |
| `tabs`, `tab_bar`, `tab`, `tab_panel`                      | Tabs; the selected tab is `CHECKED`       |
| `split`, `split_handle`                                    | Splitter                                  |
| `list`, `list_row`                                         | Lists, and the rows of tables and trees   |
| `table`, `table_header`, `table_header_cell`, `table_cell` | Tables                                    |
| `tree_toggle`, `tree_label`                                | Tree rows                                 |

Rows, cells and items leave node userdata to the application.
