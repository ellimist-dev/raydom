# Tabs, splitters, lists, tables and trees

```cpp
#include <raydom/extras/views.hh>  // C: views.h
```

Each of these fires `CHANGE` on its node when the user changes it; the setters fire nothing.

## Tabs

```cpp
rd::Node tabs    = rdx::tabs(ctx);
rd::Node general = rdx::add_tab(tabs, "General");  // panels to fill
rd::Node network = rdx::add_tab(tabs, "Network");
general.append(ctx.create_text("General settings"));
network.append(ctx.create_text("Network settings"));
tabs.on(RD_EVENT_CHANGE, [&] { /* rdx::selected_tab(tabs) */ });
```

`rdx_tabs_add` returns the tab's panel. Only the selected panel is shown. Tab reaches the bar once, at the selected
tab; Left, Right, Home and End move between tabs.

## Splitter

```cpp
rd::Node split = rdx::split(ctx, {.fraction = 0.3f, .min_first = 120, .min_second = 200});
split.style(rd::Style().height(400_px));
rd::Node sidebar = rdx::pane(split, 0), content = rdx::pane(split, 1);
```

Two panes with a handle between them, side by side or (`vertical`) stacked. Dragging the handle or, with it
focused, the arrow keys resize the panes, within each pane's minimum. The first pane's size is a fraction of the
splitter, so it follows window resizes; `rdx_split_fraction` reads it and `CHANGE` fires when a drag ends.

For a sidebar, `size` keeps the first pane at a width in pixels instead, and `reverse` puts it at the right (or,
stacked, the bottom). A `collapsible` first pane snaps shut when dragged below half its minimum and keeps its size
while shut; dragging the handle back out, double-clicking it, or Enter with it focused opens it again.

```cpp
// A 280px panel that keeps its width as the window resizes and snaps shut when dragged narrow.
rd::Node panel = rdx::split(ctx, {.size = 280, .min_first = 200, .min_second = 300, .collapsible = true});
panel.style(rd::Style().height(400_px));
rd::Node reopen = ctx.create_button("Show panel");
reopen.on_click([panel] { rdx::set_split_collapsed(panel, false); });
panel.on(RD_EVENT_CHANGE, [panel, reopen] {
  reopen.merge_style(rd::Style().display(rdx::split_collapsed(panel) ? RD_DISPLAY_FLEX : RD_DISPLAY_NONE));
});
// On a narrow window: stacked, with the panel at the bottom.
rdx::set_split_direction(panel, true, true);
```

| Function                                   | Does                                                                |
| ------------------------------------------ | ------------------------------------------------------------------- |
| `rdx_split_size`, `rdx_split_set_size`     | The first pane's size in pixels when shown, within the minimums     |
| `rdx_split_collapsed`, `rdx_split_set_collapsed` | Whether the first pane is shut; the setter fires nothing      |
| `rdx_split_set_direction`                  | Side by side or stacked, and which end the first pane is at        |

C++: `rdx::split_size`, `rdx::set_split_size`, `rdx::split_collapsed`, `rdx::set_split_collapsed`,
`rdx::set_split_direction`, `rdx::split_fraction` and `rdx::set_split_fraction`.

## Virtualized list

```cpp
std::vector<std::string> names;
for (int i = 0; i < 50000; i++) names.push_back("Contact " + std::to_string(i));
rd::Node contacts = rdx::list(ctx, static_cast<int>(names.size()), 28, [&](rd::Node row, int i) {
  row.append(ctx.create_text(names[static_cast<std::size_t>(i)]));  // the row comes empty
});
contacts.style(rd::Style().flex(1));
contacts.on(RD_EVENT_CHANGE, [&] { /* names[rdx::list_selected(contacts)] */ });
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
rd::Node table = rdx::table(ctx, {{"Name", 1_fr}, {"Size", 80_px}}, 10000, 28,
                            {.cell = [](rd::Node cell, int row, int col) {
                               char buf[32];
                               std::snprintf(buf, sizeof buf, col == 0 ? "file_%05d" : "%d KB", row);
                               cell.text(buf);
                             },
                             .header = [](int column) { (void)column; /* sort by it, then rdx::list_refresh */ }});
```

A header over a virtualized list whose rows use the header's grid columns, so cells line up with their headers.
Widths are px or fr. `cell` fills each cell; `header` runs when a column header is clicked, which is where sorting
goes: reorder your data, then `rdx_list_refresh(rdx_table_list(table))`. Selection and `CHANGE` are on the body,
`rdx_table_list`.

### One listener for every row

Rows are reused as the list scrolls, and only their contents are rebuilt, so per-row listeners would follow the
row node, not the data. Put one listener on the list and ask which row an event came from:

```cpp
// One listener for every row: rows are reused as the table scrolls, so ask which row a node is in.
rd::Node rows   = rdx::table_list(table);
int      opened = -1;
rows.on(RD_EVENT_CLICK, [&](rd::Event& e) {
  if (e.clicks() == 2) opened = rdx::list_index_of(rows, e.target());  // double click: open the file
});
rows.on(RD_EVENT_POINTER_DOWN, [&](rd::Event& e) {
  int row = rdx::list_index_of(rows, e.target());
  if (e.button() != RD_BUTTON_SECONDARY || row < 0) return;
  rdx::menu(ctx, {{"Open"}, {"Rename"}, {}, {"Delete"}}, {.x = e.x(), .y = e.y()},
            [row](int item) { (void)row, (void)item; /* act on row */ });
});
```

`rdx_list_index_of(ctx, list, node)` returns the row index for the row or anything inside it, or -1. For a table,
pass `rdx_table_list(table)`; for a tree, `rdx_tree_item_of` returns the item id instead of the visible row.
`ev->clicks` counts rapid clicks on the same node, so `clicks == 2` is a double click.

## Tree view

```cpp
rd::Node tree = rdx::tree(ctx);
int      src  = rdx::tree_add(tree, -1, "src");
rdx::tree_add(tree, src, "main.c");
rdx::tree_expand(tree, src);
tree.on(RD_EVENT_CHANGE, [&] { /* rdx::tree_label(tree, rdx::tree_selected(tree)) */ });
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
