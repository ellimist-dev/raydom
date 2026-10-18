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

static const char CHEVRON_RIGHT_SVG[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 16 16' width='16' height='16' fill='none' "
    "stroke='currentColor' stroke-width='1.8' stroke-linecap='round' stroke-linejoin='round'>"
    "<path d='M6 4 L10 8 L6 12'/></svg>";

static const uint32_t ACCENT = 0x5b8cff, ACCENT_HI = 0x7aa2ff, LINE = 0x5a6378, LINE_HI = 0x8a93a6, TRACK = 0x3a4256,
                      TRACK_HI = 0x46506a, CARD = 0x1f2533, CARD_LINE = 0x2f3647, TEXT = 0xe4e7ee, MUTED = 0xaab2c5;

static rd_class cls(rd_ctx* ctx, rd_style s) { return rd_class_create(ctx, &s); }

// Colors and shadows ease between states; the switch thumb also slides.
static rd_style quick(rd_style s) {
  s.transition_duration = rd_ms(120);
  s.transition_easing   = RD_EASING_EASE_OUT;
  return s;
}

static void state(rd_ctx* ctx, rd_class c, rd_state st, rd_style s) { rd_class_set_state(ctx, c, st, &s); }

static void define(rd_ctx* ctx, rdx__theme* t) {
  rdx_classes* c    = &t->classes;
  rd_shadow    ring = rd_shadow_of(0, 0, 0, rd_rgba(91, 140, 255, 200));
  ring.spread       = rd_px(2);

  c->label = cls(ctx, (rd_style){.user_select = RD_USER_SELECT_NONE});

  // A row: the control and its label. Focus shows a ring around the row; disabled dims it.
  rd_style row = {.flex_direction = RD_FLEX_ROW,
                  .align_items    = RD_ALIGN_CENTER,
                  .column_gap     = rd_px(8),
                  .border_radius  = rd_corners_all(rd_px(4)),
                  .cursor         = RD_CURSOR_POINTER,
                  .user_select    = RD_USER_SELECT_NONE};
  c->check_row  = cls(ctx, row);
  c->switch_row = cls(ctx, row);
  c->radio_row  = cls(ctx, row);
  rd_class rows[] = {c->check_row, c->switch_row, c->radio_row};
  for (int j = 0; j < 3; j++) {
    state(ctx, rows[j], RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
    state(ctx, rows[j], RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.45f), .cursor = RD_CURSOR_NOT_ALLOWED});
  }

  c->check_box = cls(ctx, quick((rd_style){.width           = rd_px(18),
                                     .height          = rd_px(18),
                                     .align_items     = RD_ALIGN_CENTER,
                                     .justify_content = RD_ALIGN_CENTER,
                                     .border_width    = rd_edges_all(rd_px(1.5f)),
                                     .border_color    = rd_hex(LINE),
                                     .border_radius   = rd_corners_all(rd_px(4))}));
  state(ctx, c->check_box, RD_STATE_HOVER, (rd_style){.border_color = rd_hex(LINE_HI)});
  state(ctx, c->check_box, RD_STATE_CHECKED, (rd_style){.background = rd_hex(ACCENT), .border_color = rd_hex(ACCENT)});
  c->check_mark = cls(ctx, (rd_style){.height = rd_px(13), .display = RD_DISPLAY_NONE, .color = rd_hex(0xffffff)});
  state(ctx, c->check_mark, RD_STATE_CHECKED, (rd_style){.display = RD_DISPLAY_FLEX});

  c->switch_track = cls(ctx, quick((rd_style){.width           = rd_px(36),
                                        .height          = rd_px(20),
                                        .padding         = rd_edges_all(rd_px(2)),
                                        .flex_direction  = RD_FLEX_ROW,
                                        .align_items     = RD_ALIGN_CENTER,
                                        .justify_content = RD_ALIGN_START,
                                        .border_radius   = rd_corners_all(rd_px(10)),
                                        .background      = rd_hex(TRACK)}));
  state(ctx, c->switch_track, RD_STATE_HOVER, (rd_style){.background = rd_hex(TRACK_HI)});
  state(ctx, c->switch_track, RD_STATE_CHECKED, (rd_style){.background = rd_hex(ACCENT)});
  c->switch_thumb = cls(ctx, (rd_style){.margin = {.left = rd_px(0)},
                                        .transition_duration = rd_ms(150),
                                        .transition_easing   = RD_EASING_EASE_OUT,
                                        .width         = rd_px(16),
                                        .height        = rd_px(16),
                                        .border_radius = rd_corners_all(rd_px(8)),
                                        .background    = rd_hex(0xffffff),
                                        .box_shadow    = rd_shadow_of(0, 1, 3, rd_rgba(0, 0, 0, 90))});

  // The track is 36 wide with 2px padding; a 16px thumb travels 16px.
  state(ctx, c->switch_thumb, RD_STATE_CHECKED, (rd_style){.margin = {.left = rd_px(16)}});

  c->radio_group = cls(ctx, (rd_style){.row_gap = rd_px(8)});
  c->radio_ring  = cls(ctx, quick((rd_style){.width           = rd_px(18),
                                       .height          = rd_px(18),
                                       .align_items     = RD_ALIGN_CENTER,
                                       .justify_content = RD_ALIGN_CENTER,
                                       .border_width    = rd_edges_all(rd_px(1.5f)),
                                       .border_color    = rd_hex(LINE),
                                       .border_radius   = rd_corners_all(rd_px(9))}));
  state(ctx, c->radio_ring, RD_STATE_HOVER, (rd_style){.border_color = rd_hex(LINE_HI)});
  state(ctx, c->radio_ring, RD_STATE_CHECKED, (rd_style){.border_color = rd_hex(ACCENT)});
  c->radio_dot = cls(ctx, (rd_style){.width         = rd_px(8),
                                     .height        = rd_px(8),
                                     .display       = RD_DISPLAY_NONE,
                                     .border_radius = rd_corners_all(rd_px(4)),
                                     .background    = rd_hex(ACCENT)});
  state(ctx, c->radio_dot, RD_STATE_CHECKED, (rd_style){.display = RD_DISPLAY_FLEX});

  // Slider: the rail sits inside 8px of padding so the thumb's center reaches both ends.
  c->slider = cls(ctx, (rd_style){.width           = rd_px(200),
                                  .height          = rd_px(20),
                                  .padding         = rd_edges_vh(rd_px(0), rd_px(8)),
                                  .justify_content = RD_ALIGN_CENTER,
                                  .border_radius   = rd_corners_all(rd_px(10)),
                                  .cursor          = RD_CURSOR_POINTER,
                                  .user_select     = RD_USER_SELECT_NONE});
  state(ctx, c->slider, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  state(ctx, c->slider, RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.45f), .cursor = RD_CURSOR_NOT_ALLOWED});
  c->slider_track = cls(ctx, (rd_style){.height        = rd_px(4),
                                        .border_radius = rd_corners_all(rd_px(2)),
                                        .background    = rd_hex(TRACK)});
  c->slider_fill  = cls(ctx, (rd_style){.position      = RD_POSITION_ABSOLUTE,
                                        .inset         = {.top = rd_px(0), .left = rd_px(0)},
                                        .height        = rd_pct(100),
                                        .border_radius = rd_corners_all(rd_px(2)),
                                        .background    = rd_hex(ACCENT)});
  c->slider_thumb = cls(ctx, (rd_style){.position      = RD_POSITION_ABSOLUTE,
                                        .inset         = {.top = rd_px(-6)},
                                        .margin        = {.left = rd_px(-8)},
                                        .width         = rd_px(16),
                                        .height        = rd_px(16),
                                        .border_radius = rd_corners_all(rd_px(8)),
                                        .background    = rd_hex(0xffffff),
                                        .box_shadow    = rd_shadow_of(0, 1, 3, rd_rgba(0, 0, 0, 90))});
  rd_shadow glow = rd_shadow_of(0, 0, 0, rd_rgba(91, 140, 255, 90));
  glow.spread    = rd_px(5);
  state(ctx, c->slider_thumb, RD_STATE_HOVER, (rd_style){.box_shadow = glow});
  state(ctx, c->slider_thumb, RD_STATE_ACTIVE, (rd_style){.background = rd_hex(ACCENT_HI), .box_shadow = glow});

  c->toast_stack = cls(ctx, (rd_style){.position = RD_POSITION_ABSOLUTE, .row_gap = rd_px(8),
                                       .pointer_events = RD_POINTER_EVENTS_NONE});
  c->toast       = cls(ctx, (rd_style){.flex_direction = RD_FLEX_ROW,
                                       .column_gap     = rd_px(10),
                                       .padding        = rd_edges_vh(rd_px(10), rd_px(12)),
                                       .min_width      = rd_px(260),
                                       .max_width      = rd_px(380),
                                       .border_width   = rd_edges_all(rd_px(1)),
                                       .border_color   = rd_hex(CARD_LINE),
                                       .border_radius  = rd_corners_all(rd_px(10)),
                                       .background     = rd_hex(CARD),
                                       .box_shadow     = rd_shadow_of(0, 8, 24, rd_rgba(0, 0, 0, 150)),
                                       .color          = rd_hex(TEXT),
                                       .overflow_y     = RD_OVERFLOW_HIDDEN,
                                       .pointer_events = RD_POINTER_EVENTS_AUTO});
  c->toast_bar   = cls(ctx, (rd_style){.width = rd_px(3), .border_radius = rd_corners_all(rd_px(2))});
  c->toast_title = cls(ctx, (rd_style){.font_size = rd_px(14)});
  c->toast_body  = cls(ctx, (rd_style){.font_size = rd_px(13), .color = rd_hex(MUTED)});
  c->toast_close = cls(ctx, quick((rd_style){.padding       = rd_edges_vh(rd_px(0), rd_px(6)),
                                       .border_radius = rd_corners_all(rd_px(4)),
                                       .align_self    = RD_ALIGN_START,
                                       .color         = rd_hex(MUTED),
                                       .cursor        = RD_CURSOR_POINTER,
                                       .user_select   = RD_USER_SELECT_NONE}));
  state(ctx, c->toast_close, RD_STATE_HOVER, (rd_style){.background = rd_hex(CARD_LINE), .color = rd_hex(TEXT)});
  state(ctx, c->toast_close, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  c->toast_info    = cls(ctx, (rd_style){.background = rd_hex(ACCENT)});
  c->toast_success = cls(ctx, (rd_style){.background = rd_hex(0x4cc38a)});
  c->toast_warning = cls(ctx, (rd_style){.background = rd_hex(0xf2b84b)});
  c->toast_error   = cls(ctx, (rd_style){.background = rd_hex(0xf2555a)});

  // Menus, and the lists of selects.
  c->menu = cls(ctx, (rd_style){.position      = RD_POSITION_ABSOLUTE,
                                .min_width     = rd_px(180),
                                .max_height    = rd_px(320),
                                .overflow_y    = RD_OVERFLOW_SCROLL,
                                .padding       = rd_edges_all(rd_px(4)),
                                .row_gap       = rd_px(1),
                                .border_width  = rd_edges_all(rd_px(1)),
                                .border_color  = rd_hex(CARD_LINE),
                                .border_radius = rd_corners_all(rd_px(8)),
                                .background    = rd_hex(CARD),
                                .box_shadow    = rd_shadow_of(0, 10, 28, rd_rgba(0, 0, 0, 160)),
                                .color         = rd_hex(TEXT),
                                .focusable     = RD_FOCUSABLE_POINTER,
                                .user_select   = RD_USER_SELECT_NONE});
  c->menu_item = cls(ctx, quick((rd_style){.flex_direction = RD_FLEX_ROW,
                                     .align_items    = RD_ALIGN_CENTER,
                                     .column_gap     = rd_px(10),
                                     .padding        = rd_edges_vh(rd_px(6), rd_px(10)),
                                     .border_radius  = rd_corners_all(rd_px(5)),
                                     .cursor         = RD_CURSOR_POINTER}));
  state(ctx, c->menu_item, RD_STATE_HOVER, (rd_style){.background = rd_hex(TRACK)});
  state(ctx, c->menu_item, RD_STATE_FOCUS, (rd_style){.background = rd_hex(TRACK)});
  state(ctx, c->menu_item, RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.4f), .cursor = RD_CURSOR_DEFAULT});
  c->menu_check = cls(ctx, (rd_style){.width = rd_px(14), .height = rd_px(14), .color = rd_hex(ACCENT),
                                      .opacity = rd_num(0)});
  state(ctx, c->menu_check, RD_STATE_CHECKED, (rd_style){.opacity = rd_num(1)});
  c->menu_label     = cls(ctx, (rd_style){.flex_grow = rd_num(1), .white_space = RD_WHITE_SPACE_NOWRAP});
  c->menu_shortcut  = cls(ctx, (rd_style){.font_size = rd_px(12), .color = rd_hex(MUTED), .white_space = RD_WHITE_SPACE_NOWRAP});
  c->menu_separator = cls(ctx, (rd_style){.height = rd_px(1), .margin = rd_edges_vh(rd_px(4), rd_px(6)),
                                          .background = rd_hex(CARD_LINE)});

  c->select = cls(ctx, quick((rd_style){.flex_direction = RD_FLEX_ROW,
                                  .align_items    = RD_ALIGN_CENTER,
                                  .column_gap     = rd_px(8),
                                  .min_width      = rd_px(160),
                                  .padding        = rd_edges_vh(rd_px(7), rd_px(10)),
                                  .border_width   = rd_edges_all(rd_px(1)),
                                  .border_color   = rd_hex(CARD_LINE),
                                  .border_radius  = rd_corners_all(rd_px(8)),
                                  .background     = rd_hex(CARD),
                                  .cursor         = RD_CURSOR_POINTER,
                                  .user_select    = RD_USER_SELECT_NONE}));
  state(ctx, c->select, RD_STATE_HOVER, (rd_style){.border_color = rd_hex(LINE)});
  state(ctx, c->select, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  state(ctx, c->select, RD_STATE_DISABLED, (rd_style){.opacity = rd_num(0.45f), .cursor = RD_CURSOR_NOT_ALLOWED});
  c->select_value   = cls(ctx, (rd_style){.flex_grow = rd_num(1), .white_space = RD_WHITE_SPACE_NOWRAP,
                                          .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS});
  c->select_chevron = cls(ctx, (rd_style){.height = rd_px(14), .color = rd_hex(MUTED)});

  c->modal_backdrop = cls(ctx, (rd_style){.position        = RD_POSITION_ABSOLUTE,
                                          .inset           = {rd_px(0), rd_px(0), rd_px(0), rd_px(0)},
                                          .align_items     = RD_ALIGN_CENTER,
                                          .justify_content = RD_ALIGN_CENTER,
                                          .padding         = rd_edges_all(rd_px(24)),
                                          .background      = rd_rgba(5, 7, 12, 150)});
  c->modal          = cls(ctx, (rd_style){.width         = rd_px(440),
                                          .max_width     = rd_pct(100),
                                          .max_height    = rd_pct(100),
                                          .border_width  = rd_edges_all(rd_px(1)),
                                          .border_color  = rd_hex(CARD_LINE),
                                          .border_radius = rd_corners_all(rd_px(12)),
                                          .background    = rd_hex(CARD),
                                          .box_shadow    = rd_shadow_of(0, 20, 48, rd_rgba(0, 0, 0, 180)),
                                          .color         = rd_hex(TEXT),
                                          .focusable     = RD_FOCUSABLE_POINTER});
  c->modal_header   = cls(ctx, (rd_style){.flex_direction = RD_FLEX_ROW,
                                          .align_items    = RD_ALIGN_CENTER,
                                          .padding        = {rd_px(14), rd_px(10), rd_px(4), rd_px(18)}});
  c->modal_title    = cls(ctx, (rd_style){.flex_grow = rd_num(1), .font_size = rd_px(17)});
  c->modal_close    = cls(ctx, quick((rd_style){.padding       = rd_edges_vh(rd_px(2), rd_px(8)),
                                          .border_radius = rd_corners_all(rd_px(6)),
                                          .color         = rd_hex(MUTED),
                                          .cursor        = RD_CURSOR_POINTER,
                                          .user_select   = RD_USER_SELECT_NONE}));
  state(ctx, c->modal_close, RD_STATE_HOVER, (rd_style){.background = rd_hex(CARD_LINE), .color = rd_hex(TEXT)});
  state(ctx, c->modal_close, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  c->modal_body = cls(ctx, (rd_style){.padding    = {rd_px(8), rd_px(18), rd_px(18), rd_px(18)},
                                      .row_gap    = rd_px(12),
                                      .flex_shrink = rd_num(1),
                                      .overflow_y = RD_OVERFLOW_SCROLL});

  c->tooltip = cls(ctx, (rd_style){.position       = RD_POSITION_ABSOLUTE,
                                   .max_width      = rd_px(280),
                                   .padding        = rd_edges_vh(rd_px(5), rd_px(9)),
                                   .border_radius  = rd_corners_all(rd_px(6)),
                                   .background     = rd_hex(0x2c3446),
                                   .box_shadow     = rd_shadow_of(0, 4, 12, rd_rgba(0, 0, 0, 140)),
                                   .color          = rd_hex(TEXT),
                                   .font_size      = rd_px(12),
                                   .pointer_events = RD_POINTER_EVENTS_NONE});

  // Tabs: the selected tab is checked, and its underline slides in color.
  c->tabs    = cls(ctx, (rd_style){.row_gap = rd_px(0)});
  c->tab_bar = cls(ctx, (rd_style){.flex_direction = RD_FLEX_ROW,
                                   .column_gap     = rd_px(4),
                                   .border_width   = {.bottom = rd_px(1)},
                                   .border_color   = rd_hex(CARD_LINE)});
  c->tab     = cls(ctx, quick((rd_style){.padding       = rd_edges_vh(rd_px(8), rd_px(14)),
                                         .margin        = {.bottom = rd_px(-1)},
                                         .border_width  = {.bottom = rd_px(2)},
                                         .border_color  = rd_rgba(0, 0, 0, 0),
                                         .color         = rd_hex(MUTED),
                                         .cursor        = RD_CURSOR_POINTER,
                                         .user_select   = RD_USER_SELECT_NONE,
                                         .border_radius = {.top_left = rd_px(6), .top_right = rd_px(6)}}));
  state(ctx, c->tab, RD_STATE_HOVER, (rd_style){.color = rd_hex(TEXT), .background = rd_hex(0x1d2230)});
  state(ctx, c->tab, RD_STATE_CHECKED, (rd_style){.color = rd_hex(TEXT), .border_color = rd_hex(ACCENT)});
  state(ctx, c->tab, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  c->tab_panel = cls(ctx, (rd_style){.padding = {.top = rd_px(14)}, .row_gap = rd_px(10)});

  c->split        = cls(ctx, (rd_style){.flex_direction = RD_FLEX_ROW, .min_height = rd_px(0)});
  c->split_handle = cls(ctx, quick((rd_style){.width = rd_px(6), .background = rd_hex(CARD_LINE),
                                              .cursor = RD_CURSOR_EW_RESIZE, .focusable = RD_FOCUSABLE_TAB}));
  state(ctx, c->split_handle, RD_STATE_HOVER, (rd_style){.background = rd_hex(LINE)});
  state(ctx, c->split_handle, RD_STATE_ACTIVE, (rd_style){.background = rd_hex(ACCENT)});
  state(ctx, c->split_handle, RD_STATE_FOCUS_VISIBLE, (rd_style){.background = rd_hex(ACCENT)});

  c->list     = cls(ctx, (rd_style){.overflow_y = RD_OVERFLOW_SCROLL, .min_height = rd_px(0), .user_select = RD_USER_SELECT_NONE,
                                    .focusable = RD_FOCUSABLE_TAB});
  state(ctx, c->list, RD_STATE_FOCUS_VISIBLE, (rd_style){.box_shadow = ring});
  c->list_row = cls(ctx, (rd_style){.position       = RD_POSITION_ABSOLUTE,
                                    .inset          = {.left = rd_px(0), .right = rd_px(0)},
                                    .flex_direction = RD_FLEX_ROW,
                                    .align_items    = RD_ALIGN_CENTER,
                                    .padding        = rd_edges_vh(rd_px(0), rd_px(10)),
                                    .cursor         = RD_CURSOR_POINTER});
  state(ctx, c->list_row, RD_STATE_HOVER, (rd_style){.background = rd_hex(0x1d2230)});
  state(ctx, c->list_row, RD_STATE_CHECKED, (rd_style){.background = rd_hex(0x26355a)});

  c->table             = cls(ctx, (rd_style){.min_height = rd_px(0), .border_width = rd_edges_all(rd_px(1)),
                                             .border_color = rd_hex(CARD_LINE), .border_radius = rd_corners_all(rd_px(8)),
                                             .overflow_y = RD_OVERFLOW_HIDDEN});
  c->table_header      = cls(ctx, (rd_style){.display = RD_DISPLAY_GRID, .padding = rd_edges_vh(rd_px(0), rd_px(10)),
                                             .column_gap = rd_px(12), .background = rd_hex(CARD),
                                             .border_width = {.bottom = rd_px(1)}, .border_color = rd_hex(CARD_LINE)});
  c->table_header_cell = cls(ctx, quick((rd_style){.padding = rd_edges_vh(rd_px(8), rd_px(0)), .color = rd_hex(MUTED),
                                                   .font_size = rd_px(12), .white_space = RD_WHITE_SPACE_NOWRAP,
                                                   .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS, .cursor = RD_CURSOR_POINTER,
                                                   .user_select = RD_USER_SELECT_NONE}));
  state(ctx, c->table_header_cell, RD_STATE_HOVER, (rd_style){.color = rd_hex(TEXT)});
  c->table_cell = cls(ctx, (rd_style){.white_space = RD_WHITE_SPACE_NOWRAP, .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS});

  c->tree_toggle = cls(ctx, (rd_style){.width = rd_px(16), .height = rd_px(16), .flex_shrink = rd_num(0),
                                       .color = rd_hex(MUTED), .cursor = RD_CURSOR_POINTER});
  c->tree_label  = cls(ctx, (rd_style){.white_space = RD_WHITE_SPACE_NOWRAP, .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS,
                                       .flex_shrink = rd_num(1), .margin = {.left = rd_px(4)}});

  t->check         = rd_image_load(ctx, CHECK_SVG, sizeof CHECK_SVG - 1);
  t->chevron_right = rd_image_load(ctx, CHEVRON_RIGHT_SVG, sizeof CHEVRON_RIGHT_SVG - 1);
  t->chevron = rd_image_load(ctx, CHEVRON_SVG, sizeof CHEVRON_SVG - 1);
}

rdx__theme* rdx__theme_get(rd_ctx* ctx) {
  rdx__theme* t = rd_ctx_extension(ctx, &key);
  if (t || !ctx) return t;
  t = calloc(1, sizeof *t);
  if (!t) return NULL;
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

void rdx__part(rd_ctx* ctx, rd_node part, rd_class c) {
  rd_node_add_class(ctx, part, c);
  rd_node_set_share_states(ctx, part, true);
}
