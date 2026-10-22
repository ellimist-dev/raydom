#ifndef RAYDOM_EXTRAS_THEME_H
#define RAYDOM_EXTRAS_THEME_H

#include <raydom/raydom.h>

#ifdef __cplusplus
extern "C" {
#endif

// Events, for every component: CHANGE fires on the component when the user changes its value and the
// change settles (a drag released, a key pressed, an entry committed, a choice made). Components with a
// continuous value (slider, numeric input, splitter) also fire INPUT on every change, during a drag too.
// Setters fire nothing, so syncing a component from your model doesn't loop.

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
  rd_class number, number_input, number_steppers, number_step;  // numeric inputs: the field, its text, the buttons
  rd_class number_step_icon;  // a stepper's chevron; steppers have their own hover, not the field's
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

// The colors every class is built from. Setting the palette redefines the classes from it, updating
// controls already created; set it before restyling individual classes, whose changes it replaces.
// Colors left unset keep their current value, so a palette can change just the accent:
// rdx_theme_set_palette(ctx, &(rdx_palette){.accent = rd_hex(0xf92672)}).
typedef struct rdx_palette {
  rd_color accent RD_DEFAULT({}), accent_hover RD_DEFAULT({});    // checked, filled and focused parts; a slider thumb while dragged
  rd_color on_accent RD_DEFAULT({});               // drawn on the accent: the checkmark
  rd_color surface RD_DEFAULT({}), surface_line RD_DEFAULT({});   // cards, menus, fields and their borders; tab and table rules
  rd_color raised RD_DEFAULT({});                  // tooltips
  rd_color line RD_DEFAULT({}), line_hover RD_DEFAULT({});        // checkbox and radio outlines, hovered field borders
  rd_color track RD_DEFAULT({}), track_hover RD_DEFAULT({});      // slider rails, switch tracks, hovered menu items and steppers
  rd_color row_hover RD_DEFAULT({}), row_selected RD_DEFAULT({}); // list rows; a hovered tab
  rd_color text RD_DEFAULT({}), muted RD_DEFAULT({});
  rd_color backdrop RD_DEFAULT({});                // behind a modal
  rd_color success RD_DEFAULT({}), warning RD_DEFAULT({}), error RD_DEFAULT({}); // toast bars; info uses the accent
} rdx_palette;

bool rdx_theme_palette(rd_ctx* ctx, rdx_palette* out);
bool rdx_theme_set_palette(rd_ctx* ctx, const rdx_palette* palette);

#ifdef __cplusplus
}
#endif

#endif
