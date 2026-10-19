#ifndef RAYDOM_EXTRAS_CONTROLS_H
#define RAYDOM_EXTRAS_CONTROLS_H

#include <raydom/extras/theme.h>

#ifdef __cplusplus
extern "C" {
#endif

// Form controls. Each is a node to append anywhere, with a role, keyboard support and a look from
// rdx_theme. A change by the user fires RD_EVENT_CHANGE on the control (a slider also fires
// RD_EVENT_INPUT while it moves); listen with rd_node_on. Changes made through the setters below
// fire nothing. Disable a control with rd_node_set_states(ctx, node, RD_STATE_DISABLED, true).
// Functions return false, or a null or zero value, for a node that isn't the control they expect.

// Checkbox and switch: a row with the box or track and an optional label (null for none). Click,
// Enter or Space toggles. The row has RD_STATE_CHECKED while on.
rd_node rdx_checkbox_create(rd_ctx* ctx, const char* label);
rd_node rdx_switch_create(rd_ctx* ctx, const char* label);
bool    rdx_checked(const rd_ctx* ctx, rd_node control);
bool    rdx_set_checked(rd_ctx* ctx, rd_node control, bool checked);

// Radio group: a column of options, at most one selected. Tab reaches the group once; the arrow keys
// move the selection. CHANGE fires on the group.
rd_node rdx_radio_group_create(rd_ctx* ctx);
// Appends an option and returns its row.
rd_node rdx_radio_add(rd_ctx* ctx, rd_node group, const char* label);
// The selected option's index, or -1 for none.
int     rdx_radio_selected(const rd_ctx* ctx, rd_node group);
// -1 clears the selection.
bool    rdx_radio_select(rd_ctx* ctx, rd_node group, int index);

// Slider. Drag or click the track; arrow keys step, Page Up and Down move a tenth of the range,
// Home and End go to the ends. INPUT fires on every change, CHANGE when a drag or key press ends.
typedef struct rdx_slider_desc {
  float min, max;  // max <= min means 0 to 100
  float value;     // clamped to the range
  float step;      // 0 for continuous
} rdx_slider_desc;

rd_node rdx_slider_create(rd_ctx* ctx, const rdx_slider_desc* desc);
float   rdx_slider_value(const rd_ctx* ctx, rd_node slider);
bool    rdx_slider_set_value(rd_ctx* ctx, rd_node slider, float value);
// Changes the range, keeping the step; the value is clamped (and snapped) into it without firing an
// event. False, and nothing changes, unless max > min.
bool    rdx_slider_set_range(rd_ctx* ctx, rd_node slider, float min, float max);

#ifdef __cplusplus
}
#endif

#endif
