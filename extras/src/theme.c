#include <stdlib.h>
#include <string.h>

#include "extras_internal.h"

static const char key = 0;

static const char CHECK_SVG[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' width='16' height='16' fill='none' "
    "stroke='currentColor' stroke-width='2.2' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M3.5 8.5 L6.5 11.5 L12.5 4.5'/></svg>";

static const char CHEVRON_SVG[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' width='16' height='16' fill='none' "
    "stroke='currentColor' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M4 6 L8 10 L12 6'/></svg>";

static const char CHEVRON_UP_SVG[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' width='16' height='16' fill='none' "
    "stroke='currentColor' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M4 10 L8 6 L12 10'/></svg>";

static const char CHEVRON_RIGHT_SVG[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' width='16' height='16' fill='none' "
    "stroke='currentColor' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M6 4 L10 8 L6 12'/></svg>";

static const rdx_palette DEFAULT_PALETTE = {
    .accent       = {0x5b, 0x8c, 0xff, 255, true},
    .accent_hover = {0x7a, 0xa2, 0xff, 255, true},
    .on_accent    = {0xff, 0xff, 0xff, 255, true},
    .surface      = {0x1f, 0x25, 0x33, 255, true},
    .surface_line = {0x2f, 0x36, 0x47, 255, true},
    .raised       = {0x2c, 0x34, 0x46, 255, true},
    .line         = {0x5a, 0x63, 0x78, 255, true},
    .line_hover   = {0x8a, 0x93, 0xa6, 255, true},
    .track        = {0x3a, 0x42, 0x56, 255, true},
    .track_hover  = {0x46, 0x50, 0x6a, 255, true},
    .row_hover    = {0x1d, 0x22, 0x30, 255, true},
    .row_selected = {0x26, 0x35, 0x5a, 255, true},
    .text         = {0xe4, 0xe7, 0xee, 255, true},
    .muted        = {0xaa, 0xb2, 0xc5, 255, true},
    .backdrop     = {5, 7, 12, 150, true},
    .success      = {0x4c, 0xc3, 0x8a, 255, true},
    .warning      = {0xf2, 0xb8, 0x4b, 255, true},
    .error        = {0xf2, 0x55, 0x5a, 255, true},
};

// Creates the class, or on a redefinition replaces its base and drops its state variants.
static void cls(rd_ctx* ctx, rd_class* c, rd_style s) {
  if (!c->id) {
    *c = rd_class_create(ctx, &s);
    return;
  }
  rd_class_set(ctx, *c, &s);
  for (uint32_t k = 0; k < 8; k++) rd_class_set_state(ctx, *c, (rd_state)(1u << k), NULL);
}

static rd_color alpha(rd_color c, uint8_t a) {
  c.a = a;
  return c;
}

// Colors and shadows ease between states; the switch thumb also slides.
static rd_style quick(rd_style s) {
  s.transition_duration = rd_ms(120);
  s.transition_easing   = RD_EASING_EASE_OUT;
  return s;
}

static void state(rd_ctx* ctx, rd_class c, rd_state st, rd_style s) { rd_class_set_state(ctx, c, st, &s); }

// define() is split in parts that aren't inlined into each other: every rd_style literal takes its own
// stack slot, and all of them in one frame (75 KB) overflow Emscripten's default 64 KB stack.
#if defined(_MSC_VER)
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__((noinline))
#endif

// Checkboxes, switches, radio buttons and sliders.
static NOINLINE void define_controls(rd_ctx* ctx, rdx_classes* c, const rdx_palette* p, rd_shadow ring) {
  cls(ctx, &c->label, (rd_style){.user_select = RD_USER_SELECT_NONE});

  // A row: the control and its label. Focus shows a ring around the row; disabled dims it.
  rd_style row = {.flex_direction = RD_FLEX_ROW,
                  .align_items    = RD_ALIGN_CENTER,
                  .column_gap     = rd_px(8),
                  .border_radius  = rd_corners_all(rd_px(4)),
                  .cursor         = RD_CURSOR_POINTER,
                  .user_select    = RD_USER_SELECT_NONE};
  cls(ctx, &c->check_row, row);
  cls(ctx, &c->switch_row, row);
  cls(ctx, &c->radio_row, row);
  rd_class rows[] = {c->check_row, c->switch_row, c->radio_row};
  for (int j = 0; j < 3; j++) {
    state(ctx, rows[j], RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
    state(ctx, rows[j], RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.45f), .cursor = RD_CURSOR_NOT_ALLOWED});
  }

  cls(ctx, &c->check_box, quick((rd_style){.width           = rd_px(18),
                                     .height          = rd_px(18),
                                     .align_items     = RD_ALIGN_CENTER,
                                     .justify_content = RD_ALIGN_CENTER,
                                     .border_width    = rd_edges_all(rd_px(1.5f)),
                                     .border_color    = p->line,
                                     .border_radius   = rd_corners_all(rd_px(4))}));
  state(ctx, c->check_box, RD_STATE_HOVER, (rd_style){.border_color = p->line_hover});
  state(ctx, c->check_box, RD_STATE_CHECKED, (rd_style){.background = p->accent, .border_color = p->accent});
  cls(ctx, &c->check_mark, (rd_style){.height = rd_px(13), .display = RD_DISPLAY_NONE, .color = p->on_accent});
  state(ctx, c->check_mark, RD_STATE_CHECKED, (rd_style){.display = RD_DISPLAY_FLEX});

  cls(ctx, &c->switch_track, quick((rd_style){.width           = rd_px(36),
                                        .height          = rd_px(20),
                                        .padding         = rd_edges_all(rd_px(2)),
                                        .flex_direction  = RD_FLEX_ROW,
                                        .align_items     = RD_ALIGN_CENTER,
                                        .justify_content = RD_ALIGN_START,
                                        .border_radius   = rd_corners_all(rd_px(10)),
                                        .background      = p->track}));
  state(ctx, c->switch_track, RD_STATE_HOVER, (rd_style){.background = p->track_hover});
  state(ctx, c->switch_track, RD_STATE_CHECKED, (rd_style){.background = p->accent});
  cls(ctx, &c->switch_thumb, (rd_style){.margin = {.left = rd_px(0)},
                                        .transition_duration = rd_ms(150),
                                        .transition_easing   = RD_EASING_EASE_OUT,
                                        .width         = rd_px(16),
                                        .height        = rd_px(16),
                                        .border_radius = rd_corners_all(rd_px(8)),
                                        .background    = rd_hex(0xffffff),
                                        .box_shadow    = rd_shadow_of(0, 1, 3, rd_rgba(0, 0, 0, 90))});

  // The track is 36 wide with 2px padding; a 16px thumb travels 16px.
  state(ctx, c->switch_thumb, RD_STATE_CHECKED, (rd_style){.margin = {.left = rd_px(16)}});

  cls(ctx, &c->radio_group, (rd_style){.row_gap = rd_px(8)});
  cls(ctx, &c->radio_ring, quick((rd_style){.width           = rd_px(18),
                                       .height          = rd_px(18),
                                       .align_items     = RD_ALIGN_CENTER,
                                       .justify_content = RD_ALIGN_CENTER,
                                       .border_width    = rd_edges_all(rd_px(1.5f)),
                                       .border_color    = p->line,
                                       .border_radius   = rd_corners_all(rd_px(9))}));
  state(ctx, c->radio_ring, RD_STATE_HOVER, (rd_style){.border_color = p->line_hover});
  state(ctx, c->radio_ring, RD_STATE_CHECKED, (rd_style){.border_color = p->accent});
  cls(ctx, &c->radio_dot, (rd_style){.width         = rd_px(8),
                                     .height        = rd_px(8),
                                     .display       = RD_DISPLAY_NONE,
                                     .border_radius = rd_corners_all(rd_px(4)),
                                     .background    = p->accent});
  state(ctx, c->radio_dot, RD_STATE_CHECKED, (rd_style){.display = RD_DISPLAY_FLEX});

  // Slider: the rail sits inside 8px of padding so the thumb's center reaches both ends.
  cls(ctx, &c->slider, (rd_style){.width           = rd_px(200),
                                  .height          = rd_px(20),
                                  .padding         = rd_edges_vh(rd_px(0), rd_px(8)),
                                  .justify_content = RD_ALIGN_CENTER,
                                  .border_radius   = rd_corners_all(rd_px(10)),
                                  .cursor          = RD_CURSOR_POINTER,
                                  .user_select     = RD_USER_SELECT_NONE});
  state(ctx, c->slider, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  state(ctx, c->slider, RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.45f), .cursor = RD_CURSOR_NOT_ALLOWED});
  cls(ctx, &c->slider_track, (rd_style){.height        = rd_px(4),
                                        .border_radius = rd_corners_all(rd_px(2)),
                                        .background    = p->track});
  cls(ctx, &c->slider_fill, (rd_style){.position      = RD_POSITION_ABSOLUTE,
                                        .inset         = {.top = rd_px(0), .left = rd_px(0)},
                                        .height        = rd_pct(100),
                                        .border_radius = rd_corners_all(rd_px(2)),
                                        .background    = p->accent});
  cls(ctx, &c->slider_thumb, (rd_style){.position      = RD_POSITION_ABSOLUTE,
                                        .inset         = {.top = rd_px(-6)},
                                        .margin        = {.left = rd_px(-8)},
                                        .width         = rd_px(16),
                                        .height        = rd_px(16),
                                        .border_radius = rd_corners_all(rd_px(8)),
                                        .background    = rd_hex(0xffffff),
                                        .box_shadow    = rd_shadow_of(0, 1, 3, rd_rgba(0, 0, 0, 90))});
  rd_shadow glow = rd_shadow_of(0, 0, 0, alpha(p->accent, 90));
  glow.spread    = rd_px(5);
  state(ctx, c->slider_thumb, RD_STATE_HOVER, (rd_style){.box_shadow = glow});
  state(ctx, c->slider_thumb, RD_STATE_ACTIVE, (rd_style){.background = p->accent_hover, .box_shadow = glow});
}

// Toasts.
static NOINLINE void define_toasts(rd_ctx* ctx, rdx_classes* c, const rdx_palette* p, rd_shadow ring) {
  cls(ctx, &c->toast_stack, (rd_style){.position = RD_POSITION_ABSOLUTE, .row_gap = rd_px(8),
                                       .pointer_events = RD_POINTER_EVENTS_NONE});
  cls(ctx, &c->toast, (rd_style){.flex_direction = RD_FLEX_ROW,
                                       .column_gap     = rd_px(10),
                                       .padding        = rd_edges_vh(rd_px(10), rd_px(12)),
                                       .min_width      = rd_px(260),
                                       .max_width      = rd_px(380),
                                       .border_width   = rd_edges_all(rd_px(1)),
                                       .border_color   = p->surface_line,
                                       .border_radius  = rd_corners_all(rd_px(10)),
                                       .background     = p->surface,
                                       .box_shadow     = rd_shadow_of(0, 8, 24, rd_rgba(0, 0, 0, 150)),
                                       .color          = p->text,
                                       .overflow_y     = RD_OVERFLOW_HIDDEN,
                                       .pointer_events = RD_POINTER_EVENTS_AUTO});
  cls(ctx, &c->toast_bar, (rd_style){.width = rd_px(3), .border_radius = rd_corners_all(rd_px(2))});
  cls(ctx, &c->toast_title, (rd_style){.font_size = rd_px(14)});
  cls(ctx, &c->toast_body, (rd_style){.font_size = rd_px(13), .color = p->muted});
  cls(ctx, &c->toast_close, quick((rd_style){.padding       = rd_edges_vh(rd_px(0), rd_px(6)),
                                       .border_radius = rd_corners_all(rd_px(4)),
                                       .align_self    = RD_ALIGN_START,
                                       .color         = p->muted,
                                       .cursor        = RD_CURSOR_POINTER,
                                       .user_select   = RD_USER_SELECT_NONE}));
  state(ctx, c->toast_close, RD_STATE_HOVER, (rd_style){.background = p->surface_line, .color = p->text});
  state(ctx, c->toast_close, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  cls(ctx, &c->toast_info, (rd_style){.background = p->accent});
  cls(ctx, &c->toast_success, (rd_style){.background = p->success});
  cls(ctx, &c->toast_warning, (rd_style){.background = p->warning});
  cls(ctx, &c->toast_error, (rd_style){.background = p->error});
}

// Menus, selects, numeric inputs, modals and tooltips.
static NOINLINE void define_popups(rd_ctx* ctx, rdx_classes* c, const rdx_palette* p, rd_shadow ring) {
  // Menus, and the lists of selects.
  cls(ctx, &c->menu, (rd_style){.position      = RD_POSITION_ABSOLUTE,
                                .min_width     = rd_px(180),
                                .max_height    = rd_px(320),
                                .overflow_y    = RD_OVERFLOW_SCROLL,
                                .padding       = rd_edges_all(rd_px(4)),
                                .row_gap       = rd_px(1),
                                .border_width  = rd_edges_all(rd_px(1)),
                                .border_color  = p->surface_line,
                                .border_radius = rd_corners_all(rd_px(8)),
                                .background    = p->surface,
                                .box_shadow    = rd_shadow_of(0, 10, 28, rd_rgba(0, 0, 0, 160)),
                                .color         = p->text,
                                .focusable     = RD_FOCUSABLE_POINTER,
                                .user_select   = RD_USER_SELECT_NONE});
  cls(ctx, &c->menu_item, quick((rd_style){.flex_direction = RD_FLEX_ROW,
                                     .align_items    = RD_ALIGN_CENTER,
                                     .column_gap     = rd_px(10),
                                     .padding        = rd_edges_vh(rd_px(6), rd_px(10)),
                                     .border_radius  = rd_corners_all(rd_px(5)),
                                     .cursor         = RD_CURSOR_POINTER}));
  state(ctx, c->menu_item, RD_STATE_HOVER, (rd_style){.background = p->track});
  state(ctx, c->menu_item, RD_STATE_FOCUS, (rd_style){.background = p->track});
  state(ctx, c->menu_item, RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.4f), .cursor = RD_CURSOR_DEFAULT});
  cls(ctx, &c->menu_check, (rd_style){.width = rd_px(14), .height = rd_px(14), .color = p->accent,
                                      .opacity = rd_num(0)});
  state(ctx, c->menu_check, RD_STATE_CHECKED, (rd_style){.opacity = rd_num(1)});
  cls(ctx, &c->menu_label, (rd_style){.flex_grow = rd_num(1), .white_space = RD_WHITE_SPACE_NOWRAP});
  cls(ctx, &c->menu_shortcut, (rd_style){.font_size = rd_px(12), .color = p->muted, .white_space = RD_WHITE_SPACE_NOWRAP});
  cls(ctx, &c->menu_separator, (rd_style){.height = rd_px(1), .margin = rd_edges_vh(rd_px(4), rd_px(6)),
                                          .background = p->surface_line});

  cls(ctx, &c->select, quick((rd_style){.flex_direction = RD_FLEX_ROW,
                                  .align_items    = RD_ALIGN_CENTER,
                                  .column_gap     = rd_px(8),
                                  .min_width      = rd_px(160),
                                  .padding        = rd_edges_vh(rd_px(7), rd_px(10)),
                                  .border_width   = rd_edges_all(rd_px(1)),
                                  .border_color   = p->surface_line,
                                  .border_radius  = rd_corners_all(rd_px(8)),
                                  .background     = p->surface,
                                  .cursor         = RD_CURSOR_POINTER,
                                  .user_select    = RD_USER_SELECT_NONE}));
  state(ctx, c->select, RD_STATE_HOVER, (rd_style){.border_color = p->line});
  state(ctx, c->select, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  state(ctx, c->select, RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.45f), .cursor = RD_CURSOR_NOT_ALLOWED});
  cls(ctx, &c->select_value, (rd_style){.flex_grow = rd_num(1), .white_space = RD_WHITE_SPACE_NOWRAP,
                                          .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS});
  cls(ctx, &c->select_chevron, (rd_style){.height = rd_px(14), .color = p->muted});

  // Numeric input: a field like a select's, the steppers stacked at its right end.
  cls(ctx, &c->number, quick((rd_style){.flex_direction = RD_FLEX_ROW,
                                  .width          = rd_px(120),
                                  .border_width   = rd_edges_all(rd_px(1)),
                                  .border_color   = p->surface_line,
                                  .border_radius  = rd_corners_all(rd_px(8)),
                                  .background     = p->surface,
                                  .overflow_x     = RD_OVERFLOW_HIDDEN,
                                  .overflow_y     = RD_OVERFLOW_HIDDEN,
                                  .transition_properties = RD_TRANSITION_COLOR}));
  state(ctx, c->number, RD_STATE_HOVER, (rd_style){.border_color = p->line});
  state(ctx, c->number, RD_STATE_FOCUS_WITHIN, (rd_style){.border_color = p->accent});
  state(ctx, c->number, RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.45f), .cursor = RD_CURSOR_NOT_ALLOWED});
  cls(ctx, &c->number_input, (rd_style){.flex_grow = rd_num(1), .min_width = rd_px(0), .padding = rd_edges_vh(rd_px(6), rd_px(10)),
                                           .color = p->text, .white_space = RD_WHITE_SPACE_NOWRAP});
  cls(ctx, &c->number_steppers, (rd_style){.width = rd_px(22), .flex_shrink = rd_num(0), .border_width = {.left = rd_px(1)},
                                           .border_color = p->surface_line, .user_select = RD_USER_SELECT_NONE});
  cls(ctx, &c->number_step, quick((rd_style){.flex_grow = rd_num(1), .flex_basis = rd_px(0), .align_items = RD_ALIGN_CENTER,
                                           .justify_content = RD_ALIGN_CENTER, .color = p->muted,
                                           .cursor = RD_CURSOR_POINTER}));
  cls(ctx, &c->number_step_icon, (rd_style){.height = rd_px(12), .pointer_events = RD_POINTER_EVENTS_NONE});
  state(ctx, c->number_step, RD_STATE_HOVER, (rd_style){.background = p->track, .color = p->text});
  state(ctx, c->number_step, RD_STATE_ACTIVE, (rd_style){.background = p->track_hover, .color = p->text});

  cls(ctx, &c->modal_backdrop, (rd_style){.position        = RD_POSITION_ABSOLUTE,
                                          .inset           = {rd_px(0), rd_px(0), rd_px(0), rd_px(0)},
                                          .align_items     = RD_ALIGN_CENTER,
                                          .justify_content = RD_ALIGN_CENTER,
                                          .padding         = rd_edges_all(rd_px(24)),
                                          .background      = p->backdrop});
  cls(ctx, &c->modal, (rd_style){.width         = rd_px(440),
                                          .max_width     = rd_pct(100),
                                          .max_height    = rd_pct(100),
                                          .border_width  = rd_edges_all(rd_px(1)),
                                          .border_color  = p->surface_line,
                                          .border_radius = rd_corners_all(rd_px(12)),
                                          .background    = p->surface,
                                          .box_shadow    = rd_shadow_of(0, 20, 48, rd_rgba(0, 0, 0, 180)),
                                          .color         = p->text,
                                          .focusable     = RD_FOCUSABLE_POINTER});
  cls(ctx, &c->modal_header, (rd_style){.flex_direction = RD_FLEX_ROW,
                                          .align_items    = RD_ALIGN_CENTER,
                                          .padding        = {rd_px(14), rd_px(10), rd_px(4), rd_px(18)}});
  cls(ctx, &c->modal_title, (rd_style){.flex_grow = rd_num(1), .font_size = rd_px(17)});
  cls(ctx, &c->modal_close, quick((rd_style){.padding       = rd_edges_vh(rd_px(2), rd_px(8)),
                                          .border_radius = rd_corners_all(rd_px(6)),
                                          .color         = p->muted,
                                          .cursor        = RD_CURSOR_POINTER,
                                          .user_select   = RD_USER_SELECT_NONE}));
  state(ctx, c->modal_close, RD_STATE_HOVER, (rd_style){.background = p->surface_line, .color = p->text});
  state(ctx, c->modal_close, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  cls(ctx, &c->modal_body, (rd_style){.padding    = {rd_px(8), rd_px(18), rd_px(18), rd_px(18)},
                                      .row_gap    = rd_px(12),
                                      .flex_shrink = rd_num(1),
                                      .overflow_y = RD_OVERFLOW_SCROLL});

  cls(ctx, &c->tooltip, (rd_style){.position       = RD_POSITION_ABSOLUTE,
                                   .max_width      = rd_px(280),
                                   .padding        = rd_edges_vh(rd_px(5), rd_px(9)),
                                   .border_radius  = rd_corners_all(rd_px(6)),
                                   .background     = p->raised,
                                   .box_shadow     = rd_shadow_of(0, 4, 12, rd_rgba(0, 0, 0, 140)),
                                   .color          = p->text,
                                   .font_size      = rd_px(12),
                                   .pointer_events = RD_POINTER_EVENTS_NONE});
}

// Tabs, splitters, lists, tables, trees and breadcrumbs.
static NOINLINE void define_views(rd_ctx* ctx, rdx_classes* c, const rdx_palette* p, rd_shadow ring) {
  // Tabs: the selected tab is checked, and its underline slides in color.
  cls(ctx, &c->tabs, (rd_style){.row_gap = rd_px(0)});
  cls(ctx, &c->tab_bar, (rd_style){.flex_direction = RD_FLEX_ROW,
                                   .column_gap     = rd_px(4),
                                   .border_width   = {.bottom = rd_px(1)},
                                   .border_color   = p->surface_line});
  cls(ctx, &c->tab, quick((rd_style){.padding       = rd_edges_vh(rd_px(8), rd_px(14)),
                                         .margin        = {.bottom = rd_px(-1)},
                                         .border_width  = {.bottom = rd_px(2)},
                                         .border_color  = rd_rgba(0, 0, 0, 0),
                                         .color         = p->muted,
                                         .cursor        = RD_CURSOR_POINTER,
                                         .user_select   = RD_USER_SELECT_NONE,
                                         .border_radius = {.top_left = rd_px(6), .top_right = rd_px(6)}}));
  state(ctx, c->tab, RD_STATE_HOVER, (rd_style){.color = p->text, .background = p->row_hover});
  state(ctx, c->tab, RD_STATE_CHECKED, (rd_style){.color = p->text, .border_color = p->accent});
  state(ctx, c->tab, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  cls(ctx, &c->tab_panel, (rd_style){.padding = {.top = rd_px(14)}, .row_gap = rd_px(10)});

  cls(ctx, &c->split, (rd_style){.flex_direction = RD_FLEX_ROW, .min_height = rd_px(0)});
  // Colors only: the handle's size changes with the splitter's direction, which shouldn't animate.
  cls(ctx, &c->split_handle, quick((rd_style){.width = rd_px(6), .background = p->surface_line,
                                              .cursor = RD_CURSOR_EW_RESIZE, .focusable = RD_FOCUSABLE_TAB,
                                              .transition_properties = RD_TRANSITION_COLOR}));
  state(ctx, c->split_handle, RD_STATE_HOVER, (rd_style){.background = p->line});
  state(ctx, c->split_handle, RD_STATE_ACTIVE, (rd_style){.background = p->accent});
  state(ctx, c->split_handle, RD_STATE_FOCUS_VISIBLE, (rd_style){.background = p->accent});

  cls(ctx, &c->list, (rd_style){.overflow_y = RD_OVERFLOW_SCROLL, .min_height = rd_px(0), .user_select = RD_USER_SELECT_NONE,
                                    .focusable = RD_FOCUSABLE_TAB});
  state(ctx, c->list, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  cls(ctx, &c->list_row, (rd_style){.position       = RD_POSITION_ABSOLUTE,
                                    .inset          = {.left = rd_px(0), .right = rd_px(0)},
                                    .flex_direction = RD_FLEX_ROW,
                                    .align_items    = RD_ALIGN_CENTER,
                                    .padding        = rd_edges_vh(rd_px(0), rd_px(10)),
                                    .cursor         = RD_CURSOR_POINTER});
  state(ctx, c->list_row, RD_STATE_HOVER, (rd_style){.background = p->row_hover});
  state(ctx, c->list_row, RD_STATE_CHECKED, (rd_style){.background = p->row_selected});

  cls(ctx, &c->table, (rd_style){.min_height = rd_px(0), .border_width = rd_edges_all(rd_px(1)),
                                             .border_color = p->surface_line, .border_radius = rd_corners_all(rd_px(8)),
                                             .overflow_y = RD_OVERFLOW_HIDDEN});
  cls(ctx, &c->table_header, (rd_style){.display = RD_DISPLAY_GRID, .padding = rd_edges_vh(rd_px(0), rd_px(10)),
                                             .column_gap = rd_px(12), .background = p->surface,
                                             .border_width = {.bottom = rd_px(1)}, .border_color = p->surface_line});
  cls(ctx, &c->table_header_cell, quick((rd_style){.padding = rd_edges_vh(rd_px(8), rd_px(0)), .color = p->muted,
                                                   .font_size = rd_px(12), .white_space = RD_WHITE_SPACE_NOWRAP,
                                                   .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS, .cursor = RD_CURSOR_POINTER,
                                                   .user_select = RD_USER_SELECT_NONE}));
  state(ctx, c->table_header_cell, RD_STATE_HOVER, (rd_style){.color = p->text});
  cls(ctx, &c->table_cell, (rd_style){.white_space = RD_WHITE_SPACE_NOWRAP, .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS});

  cls(ctx, &c->tree_toggle, (rd_style){.width = rd_px(16), .height = rd_px(16), .flex_shrink = rd_num(0),
                                       .color = p->muted, .cursor = RD_CURSOR_POINTER});
  cls(ctx, &c->tree_label, (rd_style){.white_space = RD_WHITE_SPACE_NOWRAP, .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS,
                                       .flex_shrink = rd_num(1), .margin = {.left = rd_px(4)}});

  cls(ctx, &c->breadcrumb, (rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_CENTER, .column_gap = rd_px(4),
                                      .flex_wrap = RD_FLEX_WRAP});
  cls(ctx, &c->breadcrumb_item, quick((rd_style){.color = p->muted, .padding = rd_edges_vh(rd_px(2), rd_px(4)),
                                                 .border_radius = rd_corners_all(rd_px(4)), .white_space = RD_WHITE_SPACE_NOWRAP,
                                                 .user_select = RD_USER_SELECT_NONE}));
  state(ctx, c->breadcrumb_item, RD_STATE_HOVER, (rd_style){.color = p->text, .background = p->row_hover});
  state(ctx, c->breadcrumb_item, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  cls(ctx, &c->breadcrumb_current, (rd_style){.color = p->text, .padding = rd_edges_vh(rd_px(2), rd_px(4)),
                                              .white_space = RD_WHITE_SPACE_NOWRAP});
  cls(ctx, &c->breadcrumb_separator, (rd_style){.width = rd_px(12), .height = rd_px(12), .flex_shrink = rd_num(0), .color = p->muted});
}

// Every class, from the palette. Runs again when the palette changes, updating the classes in place.
static void define(rd_ctx* ctx, rdx__theme* t) {
  rdx_classes*       c    = &t->classes;
  const rdx_palette* p    = &t->palette;
  rd_shadow          ring = rd_shadow_of(0, 0, 0, alpha(p->accent, 200));
  ring.spread             = rd_px(2);
  define_controls(ctx, c, p, ring);
  define_toasts(ctx, c, p, ring);
  define_popups(ctx, c, p, ring);
  define_views(ctx, c, p, ring);

  if (t->images_loaded) return;
  t->images_loaded = true;
  t->check         = rd_image_load(ctx, CHECK_SVG, sizeof CHECK_SVG - 1);
  t->chevron_right = rd_image_load(ctx, CHEVRON_RIGHT_SVG, sizeof CHEVRON_RIGHT_SVG - 1);
  t->chevron       = rd_image_load(ctx, CHEVRON_SVG, sizeof CHEVRON_SVG - 1);
  t->chevron_up    = rd_image_load(ctx, CHEVRON_UP_SVG, sizeof CHEVRON_UP_SVG - 1);
}

rdx__theme* rdx__theme_get(rd_ctx* ctx) {
  rdx__theme* t = rd_ctx_extension(ctx, &key);
  if (t || !ctx) return t;
  t = calloc(1, sizeof *t);
  if (!t) return NULL;
  t->palette = DEFAULT_PALETTE;
  define(ctx, t);
  if (!rd_ctx_set_extension(ctx, &key, t, free)) {
    free(t);
    return NULL;
  }
  return t;
}

const rdx_classes* rdx_theme(rd_ctx* ctx) {
  rdx__theme* t = rdx__theme_get(ctx);
  return t ? &t->classes : NULL;
}

bool rdx_theme_palette(rd_ctx* ctx, rdx_palette* out) {
  rdx__theme* t = rdx__theme_get(ctx);
  if (out) *out = t ? t->palette : DEFAULT_PALETTE;
  return t != NULL;
}

bool rdx_theme_set_palette(rd_ctx* ctx, const rdx_palette* palette) {
  rdx__theme* t = rdx__theme_get(ctx);
  if (!t || !palette) return false;
  const rd_color* from = (const rd_color*)palette;
  rd_color*       to   = (rd_color*)&t->palette;
  for (size_t k = 0; k < sizeof *palette / sizeof *from; k++)
    if (from[k].set) to[k] = from[k];  // unset keeps the color
  define(ctx, t);
  return true;
}

void rdx__part(rd_ctx* ctx, rd_node part, rd_class c) {
  rd_node_add_class(ctx, part, c);
  rd_node_set_share_states(ctx, part, true);
}
