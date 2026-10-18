#ifndef RAYDOM_EXTRAS_THEME_H
#define RAYDOM_EXTRAS_THEME_H

#include <raydom/raydom.h>

#ifdef __cplusplus
extern "C" {
#endif

// The classes behind every extras control and toast, created with a dark default look on first use.
// Restyle them for the whole context with rd_class_set and rd_class_set_state, or add your own
// classes to individual nodes. Parts share their control's states (rd_node_set_share_states), so a
// variant on a part reacts to the whole control being hovered, focused, checked or disabled.
typedef struct rdx_classes {
  rd_class label;  // the text beside a checkbox, switch or radio button
  rd_class check_row, check_box, check_mark;
  rd_class switch_row, switch_track, switch_thumb;
  rd_class radio_group, radio_row, radio_ring, radio_dot;
  rd_class slider, slider_track, slider_fill, slider_thumb;
  rd_class toast_stack, toast, toast_bar, toast_title, toast_body, toast_close;
  rd_class toast_info, toast_success, toast_warning, toast_error;  // on the bar
  rd_class menu, menu_item, menu_check, menu_label, menu_shortcut, menu_separator;  // menus and select lists
  rd_class select, select_value, select_chevron;
  rd_class modal_backdrop, modal, modal_header, modal_title, modal_close, modal_body;
  rd_class tooltip;
  rd_class tabs, tab_bar, tab, tab_panel;
  rd_class split, split_handle;
  rd_class list, list_row;  // also the rows of tables and trees
  rd_class table, table_header, table_header_cell, table_cell;
  rd_class tree_toggle, tree_label;
} rdx_classes;

// Null if the classes can't be created.
const rdx_classes* rdx_theme(rd_ctx* ctx);

#ifdef __cplusplus
}
#endif

#endif
