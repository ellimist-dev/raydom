#ifndef RAYDOM_EXTRAS_VIEWS_H
#define RAYDOM_EXTRAS_VIEWS_H

#include <raydom/extras/theme.h>

#ifdef __cplusplus
extern "C" {
#endif

// Tabs, splitters, virtualized lists, tables and tree views. Each fires RD_EVENT_CHANGE on its node
// when the user changes it; the setters fire nothing.

// Tabs: a bar of tabs over the selected tab's panel. Left and Right move between tabs; Tab reaches the
// bar once.
rd_node rdx_tabs_create(rd_ctx* ctx);
// Adds a tab and returns its panel, a column to fill. The first tab added is selected.
rd_node rdx_tabs_add(rd_ctx* ctx, rd_node tabs, const char* label);
int     rdx_tabs_selected(const rd_ctx* ctx, rd_node tabs);
bool    rdx_tabs_select(rd_ctx* ctx, rd_node tabs, int index);

// Splitter: two panes with a handle between them. Drag the handle, or focus it and use the arrow keys.
// The first pane's share is a fraction of the splitter, so it follows window resizes.
typedef struct rdx_split_desc {
  bool  vertical;  // panes above and below each other
  float fraction;  // the first pane's share; 0 means 0.5
  float min_first, min_second;  // pixels
} rdx_split_desc;

rd_node rdx_split_create(rd_ctx* ctx, const rdx_split_desc* desc);
rd_node rdx_split_pane(const rd_ctx* ctx, rd_node split, int index);  // 0 or 1
float   rdx_split_fraction(const rd_ctx* ctx, rd_node split);
bool    rdx_split_set_fraction(rd_ctx* ctx, rd_node split, float fraction);

// Virtualized list: rows of a fixed height, of which only those in view (and a few around them)
// exist. fill populates a row for an index; rows are reused as the list scrolls, so fill gets a
// row emptied of what it held before. The list has single selection: click a row, or use Up, Down,
// Page Up, Page Down, Home and End while the list has focus. Selected rows have RD_STATE_CHECKED.
typedef void (*rdx_row_fn)(rd_ctx* ctx, rd_node row, int index, void* data);

typedef struct rdx_list_desc {
  int           count;
  float         row_height;  // 0 means 32
  rdx_row_fn    fill;
  void*         data;
  rd_destroy_fn destroy;  // destroy(data) with the list
} rdx_list_desc;

rd_node rdx_list_create(rd_ctx* ctx, const rdx_list_desc* desc);
bool    rdx_list_set_count(rd_ctx* ctx, rd_node list, int count);
// Fills the rows in view again, after the data behind them changed.
void    rdx_list_refresh(rd_ctx* ctx, rd_node list);
int     rdx_list_selected(const rd_ctx* ctx, rd_node list);  // -1 for none
// Selects and scrolls the row into view; -1 clears.
bool    rdx_list_select(rd_ctx* ctx, rd_node list, int index);
void    rdx_list_scroll_to(rd_ctx* ctx, rd_node list, int index);
// The row index that node is in (the row itself or anything inside it), or -1. Rows are reused as the
// list scrolls, so use this in one listener on the list rather than a listener per row or cell: a
// context menu, or a double click (ev->clicks == 2) to open or edit. For tables, pass rdx_table_list.
int     rdx_list_index_of(const rd_ctx* ctx, rd_node list, rd_node node);

// Table: a header row over a virtualized list whose rows share the header's columns. Column widths are
// px or fr. cell fills one cell; header, if set, runs when a column header is clicked, for sorting.
typedef struct rdx_column {
  const char* title;
  rd_val      width;  // rd_px(...) or rd_fr(...); unset means 1fr
} rdx_column;

typedef void (*rdx_cell_fn)(rd_ctx* ctx, rd_node cell, int row, int column, void* data);
typedef void (*rdx_header_fn)(rd_ctx* ctx, int column, void* data);

typedef struct rdx_table_desc {
  const rdx_column* columns;  // copied
  int               column_count;
  int               count;
  float             row_height;  // 0 means 32
  rdx_cell_fn       cell;
  rdx_header_fn     header;
  void*             data;
  rd_destroy_fn     destroy;
} rdx_table_desc;

rd_node rdx_table_create(rd_ctx* ctx, const rdx_table_desc* desc);
// The table's body, a list: for selection, CHANGE, rdx_list_set_count and rdx_list_refresh.
rd_node rdx_table_list(const rd_ctx* ctx, rd_node table);

// Tree view: items with labels under parents, shown as an indented, virtualized list of the expanded
// items. Right expands or moves to the first child, Left collapses or moves to the parent, Enter
// toggles. The tree is a list: CHANGE fires on it when the selection changes.
rd_node     rdx_tree_create(rd_ctx* ctx, float row_height);
// Returns the new item's id; parent -1 adds a top-level item. Items start collapsed.
int         rdx_tree_add(rd_ctx* ctx, rd_node tree, int parent, const char* label);
bool        rdx_tree_expand(rd_ctx* ctx, rd_node tree, int item, bool expanded);
bool        rdx_tree_expanded(const rd_ctx* ctx, rd_node tree, int item);
int         rdx_tree_selected(const rd_ctx* ctx, rd_node tree);  // item id, or -1
bool        rdx_tree_select(rd_ctx* ctx, rd_node tree, int item);  // expands its ancestors
const char* rdx_tree_label(const rd_ctx* ctx, rd_node tree, int item);
int         rdx_tree_parent(const rd_ctx* ctx, rd_node tree, int item);
// The item that node is in, or -1: rdx_list_index_of for trees, which returns the stable item id
// rather than a visible row index that shifts as items expand and collapse.
int         rdx_tree_item_of(const rd_ctx* ctx, rd_node tree, rd_node node);

#ifdef __cplusplus
}
#endif

#endif
