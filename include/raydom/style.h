#ifndef RAYDOM_STYLE_H
#define RAYDOM_STYLE_H

#include <raydom/tree.h>

#ifdef __cplusplus
extern "C" {
#endif

// Every style type reserves zero for "unset", so a zero-initialized rd_style sets nothing
// and designated initializers set only what they name.

RD_ENUM typedef enum rd_unit : uint8_t {
  RD_UNIT_UNSET,
  RD_UNIT_AUTO,
  RD_UNIT_PX,
  RD_UNIT_PCT,
  RD_UNIT_NUM,
  RD_UNIT_FR,
  RD_UNIT_MIN_CONTENT,
  RD_UNIT_MAX_CONTENT,
} rd_unit;

typedef struct rd_val {
  float   value;
  rd_unit unit;
} rd_val;

typedef struct rd_color {
  uint8_t r, g, b, a;
  bool    set;
} rd_color;

typedef struct rd_edges {
  rd_val top, right, bottom, left;
} rd_edges;

typedef struct rd_shadow {
  rd_val   x, y, blur, spread;
  rd_color color;
} rd_shadow;

typedef struct rd_corners {
  rd_val top_left, top_right, bottom_right, bottom_left;
} rd_corners;

// A grid track's sizing function: min is px, pct, auto, min-content or max-content; max may also be fr.
typedef struct rd_track {
  rd_val min, max;
} rd_track;

// Handle to a track list interned in the context. {0, true} is `none`.
typedef struct rd_tracks {
  uint32_t id;
  bool     set;
} rd_tracks;

RD_ENUM typedef enum rd_grid_line_kind : uint8_t {
  RD_GRID_LINE_UNSET,
  RD_GRID_LINE_AUTO,
  RD_GRID_LINE_INDEX,
  RD_GRID_LINE_SPAN,
} rd_grid_line_kind;

// INDEX is 1-based; negative counts back from the end of the explicit grid.
typedef struct rd_grid_line {
  int16_t           value;
  rd_grid_line_kind kind;
} rd_grid_line;

RD_ENUM typedef enum rd_grid_flow : uint8_t {
  RD_GRID_FLOW_UNSET,
  RD_GRID_FLOW_ROW,
  RD_GRID_FLOW_COLUMN,
} rd_grid_flow;

RD_ENUM typedef enum rd_display : uint8_t {
  RD_DISPLAY_UNSET,
  RD_DISPLAY_FLEX,
  RD_DISPLAY_GRID,
  RD_DISPLAY_NONE,
} rd_display;

RD_ENUM typedef enum rd_position : uint8_t {
  RD_POSITION_UNSET,
  RD_POSITION_RELATIVE,
  RD_POSITION_ABSOLUTE,
} rd_position;

RD_ENUM typedef enum rd_overflow : uint8_t {
  RD_OVERFLOW_UNSET,
  RD_OVERFLOW_VISIBLE,
  RD_OVERFLOW_HIDDEN,
  RD_OVERFLOW_SCROLL,
} rd_overflow;

RD_ENUM typedef enum rd_flex_direction : uint8_t {
  RD_FLEX_DIRECTION_UNSET,
  RD_FLEX_ROW,
  RD_FLEX_COLUMN,
  RD_FLEX_ROW_REVERSE,
  RD_FLEX_COLUMN_REVERSE,
} rd_flex_direction;

RD_ENUM typedef enum rd_flex_wrap : uint8_t {
  RD_FLEX_WRAP_UNSET,
  RD_FLEX_NOWRAP,
  RD_FLEX_WRAP,
  RD_FLEX_WRAP_REVERSE,
} rd_flex_wrap;

// Shared by the justify_* and align_* properties.
// Values a property doesn't accept are treated as its default during layout.
// NORMAL is start for flex justify_content, stretch everywhere else.
RD_ENUM typedef enum rd_align : uint8_t {
  RD_ALIGN_UNSET,
  RD_ALIGN_AUTO,
  RD_ALIGN_NORMAL,
  RD_ALIGN_START,
  RD_ALIGN_END,
  RD_ALIGN_CENTER,
  RD_ALIGN_STRETCH,
  RD_ALIGN_BASELINE,
  RD_ALIGN_SPACE_BETWEEN,
  RD_ALIGN_SPACE_AROUND,
  RD_ALIGN_SPACE_EVENLY,
} rd_align;

// {0, true} means the context's default font.
typedef struct rd_font {
  uint32_t id;
  bool     set;
} rd_font;

RD_ENUM typedef enum rd_white_space : uint8_t {
  RD_WHITE_SPACE_UNSET,
  RD_WHITE_SPACE_NORMAL,    // Collapse spaces and newlines, wrap.
  RD_WHITE_SPACE_NOWRAP,    // Collapse, don't wrap.
  RD_WHITE_SPACE_PRE,       // Preserve spaces and newlines, don't wrap.
  RD_WHITE_SPACE_PRE_WRAP,  // Preserve, wrap.
} rd_white_space;

RD_ENUM typedef enum rd_pointer_events : uint8_t {
  RD_POINTER_EVENTS_UNSET,
  RD_POINTER_EVENTS_AUTO,
  RD_POINTER_EVENTS_NONE,  // Not hit-tested; children inherit it unless they set auto.
} rd_pointer_events;

RD_ENUM typedef enum rd_focusable : uint8_t {
  RD_FOCUSABLE_UNSET,
  RD_FOCUSABLE_NONE,
  RD_FOCUSABLE_TAB,      // Focusable by pointer, code and Tab.
  RD_FOCUSABLE_POINTER,  // Focusable by pointer and code, skipped by Tab.
} rd_focusable;

// Mouse cursor over the node. AUTO picks text over selectable text and inputs, pointer over links,
// and default elsewhere. To show a different cursor while dragging, set it in a class's active variant.
RD_ENUM typedef enum rd_cursor : uint8_t {
  RD_CURSOR_UNSET,
  RD_CURSOR_AUTO,
  RD_CURSOR_DEFAULT,
  RD_CURSOR_POINTER,
  RD_CURSOR_TEXT,
  RD_CURSOR_CROSSHAIR,
  RD_CURSOR_MOVE,
  RD_CURSOR_NOT_ALLOWED,
  RD_CURSOR_EW_RESIZE,
  RD_CURSOR_NS_RESIZE,
  RD_CURSOR_NWSE_RESIZE,
  RD_CURSOR_NESW_RESIZE,
  RD_CURSOR_NONE,  // Hidden.
} rd_cursor;

// How an image fills its content box. The image is centered, and cropped where it overflows.
RD_ENUM typedef enum rd_object_fit : uint8_t {
  RD_OBJECT_FIT_UNSET,
  RD_OBJECT_FIT_FILL,        // Stretched to the box.
  RD_OBJECT_FIT_CONTAIN,     // Scaled to fit inside, keeping the aspect ratio.
  RD_OBJECT_FIT_COVER,       // Scaled to cover the box, keeping the aspect ratio.
  RD_OBJECT_FIT_NONE,        // Natural size.
  RD_OBJECT_FIT_SCALE_DOWN,  // NONE or CONTAIN, whichever is smaller.
} rd_object_fit;

// Timing curves for transitions and animations; the CSS cubic-bezier curves of the same names.
RD_ENUM typedef enum rd_easing : uint8_t {
  RD_EASING_UNSET,
  RD_EASING_LINEAR,
  RD_EASING_EASE,
  RD_EASING_EASE_IN,
  RD_EASING_EASE_OUT,
  RD_EASING_EASE_IN_OUT,
} rd_easing;

// Which properties transition_duration animates, as a mask. Unset means all.
RD_ENUM typedef enum rd_transition : uint8_t {
  RD_TRANSITION_UNSET   = 0,
  RD_TRANSITION_COLOR   = 1u << 0,  // background, border, text, caret, selection and scrollbar colors
  RD_TRANSITION_OPACITY = 1u << 1,
  RD_TRANSITION_SHADOW  = 1u << 2,
  RD_TRANSITION_RADIUS  = 1u << 3,
  RD_TRANSITION_SIZE    = 1u << 4,  // width, height, their min and max, flex grow, shrink and basis
  RD_TRANSITION_SPACING = 1u << 5,  // margin, padding, border width, gaps
  RD_TRANSITION_INSET   = 1u << 6,
  RD_TRANSITION_TEXT    = 1u << 7,  // font size, line height
  RD_TRANSITION_ALL     = 0xff,
} rd_transition;

RD_ENUM typedef enum rd_user_select : uint8_t {
  RD_USER_SELECT_UNSET,
  RD_USER_SELECT_AUTO,  // Text can be selected by dragging.
  RD_USER_SELECT_NONE,
} rd_user_select;

RD_ENUM typedef enum rd_text_overflow : uint8_t {
  RD_TEXT_OVERFLOW_UNSET,
  RD_TEXT_OVERFLOW_CLIP,
  RD_TEXT_OVERFLOW_ELLIPSIS,  // Cut lines that don't fit, and the last line shown under line_clamp, with "…".
} rd_text_overflow;

RD_ENUM typedef enum rd_text_align : uint8_t {
  RD_TEXT_ALIGN_UNSET,
  RD_TEXT_ALIGN_START,
  RD_TEXT_ALIGN_CENTER,
  RD_TEXT_ALIGN_END,
} rd_text_align;

// X(name, type, kind, inherited, affects_layout)
#define RD_STYLE_PROPS(X)                             \
  X(display, rd_display, ENUM, 0, 1)                  \
  X(position, rd_position, ENUM, 0, 1)                \
  X(overflow_x, rd_overflow, ENUM, 0, 1)              \
  X(overflow_y, rd_overflow, ENUM, 0, 1)              \
  X(flex_direction, rd_flex_direction, ENUM, 0, 1)    \
  X(flex_wrap, rd_flex_wrap, ENUM, 0, 1)              \
  X(justify_content, rd_align, ENUM, 0, 1)            \
  X(align_items, rd_align, ENUM, 0, 1)                \
  X(align_self, rd_align, ENUM, 0, 1)                 \
  X(align_content, rd_align, ENUM, 0, 1)              \
  X(justify_items, rd_align, ENUM, 0, 1)              \
  X(justify_self, rd_align, ENUM, 0, 1)               \
  X(grid_template_columns, rd_tracks, TRACKS, 0, 1)   \
  X(grid_template_rows, rd_tracks, TRACKS, 0, 1)      \
  X(grid_auto_columns, rd_track, TRACK, 0, 1)         \
  X(grid_auto_rows, rd_track, TRACK, 0, 1)            \
  X(grid_auto_flow, rd_grid_flow, ENUM, 0, 1)         \
  X(grid_column_start, rd_grid_line, GRID_LINE, 0, 1) \
  X(grid_column_end, rd_grid_line, GRID_LINE, 0, 1)   \
  X(grid_row_start, rd_grid_line, GRID_LINE, 0, 1)    \
  X(grid_row_end, rd_grid_line, GRID_LINE, 0, 1)      \
  X(flex_grow, rd_val, VAL, 0, 1)                     \
  X(flex_shrink, rd_val, VAL, 0, 1)                   \
  X(flex_basis, rd_val, VAL, 0, 1)                    \
  X(width, rd_val, VAL, 0, 1)                         \
  X(height, rd_val, VAL, 0, 1)                        \
  X(min_width, rd_val, VAL, 0, 1)                     \
  X(min_height, rd_val, VAL, 0, 1)                    \
  X(max_width, rd_val, VAL, 0, 1)                     \
  X(max_height, rd_val, VAL, 0, 1)                    \
  X(margin, rd_edges, EDGES, 0, 1)                    \
  X(padding, rd_edges, EDGES, 0, 1)                   \
  X(border_width, rd_edges, EDGES, 0, 1)              \
  X(inset, rd_edges, EDGES, 0, 1)                     \
  X(row_gap, rd_val, VAL, 0, 1)                       \
  X(column_gap, rd_val, VAL, 0, 1)                    \
  X(background, rd_color, COLOR, 0, 0)                \
  X(border_color, rd_color, COLOR, 0, 0)              \
  X(border_radius, rd_corners, CORNERS, 0, 0)         \
  X(opacity, rd_val, VAL, 0, 0)                       \
  X(box_shadow, rd_shadow, SHADOW, 0, 0)              \
  X(object_fit, rd_object_fit, ENUM, 0, 0)            \
  X(color, rd_color, COLOR, 1, 0)                     \
  X(font, rd_font, FONT, 1, 1)                        \
  X(font_size, rd_val, VAL, 1, 1)                     \
  X(line_height, rd_val, VAL, 1, 1)                   \
  X(white_space, rd_white_space, ENUM, 1, 1)          \
  X(text_overflow, rd_text_overflow, ENUM, 0, 0)      \
  X(user_select, rd_user_select, ENUM, 1, 0)          \
  X(line_clamp, rd_val, VAL, 0, 1)                    \
  X(pointer_events, rd_pointer_events, ENUM, 1, 0)    \
  X(focusable, rd_focusable, ENUM, 0, 0)              \
  X(cursor, rd_cursor, ENUM, 1, 0)                    \
  X(scrollbar_width, rd_val, VAL, 0, 0)               \
  X(scrollbar_color, rd_color, COLOR, 1, 0)           \
  X(scrollbar_track_color, rd_color, COLOR, 1, 0)     \
  X(transition_duration, rd_val, VAL, 0, 0)           \
  X(transition_delay, rd_val, VAL, 0, 0)              \
  X(transition_easing, rd_easing, ENUM, 0, 0)         \
  X(transition_properties, rd_transition, ENUM, 0, 0) \
  X(caret_color, rd_color, COLOR, 1, 0)               \
  X(selection_color, rd_color, COLOR, 1, 0)           \
  X(text_align, rd_text_align, ENUM, 1, 0)

typedef struct rd_style {
#define RD_STYLE_FIELD_(name, type, kind, inherited, layout) type name;
  RD_STYLE_PROPS(RD_STYLE_FIELD_)
#undef RD_STYLE_FIELD_
} rd_style;

// Bit order is precedence: later states' class variants win.
RD_ENUM typedef enum rd_state : uint8_t {
  RD_STATE_HOVER         = 1u << 0,
  RD_STATE_CHECKED       = 1u << 1,  // Set by the application or a control: a ticked checkbox, an on switch.
  RD_STATE_FOCUS_WITHIN  = 1u << 2,  // The node or a descendant has focus.
  RD_STATE_FOCUS         = 1u << 3,
  RD_STATE_FOCUS_VISIBLE = 1u << 4,  // Focus that should show a ring: from the keyboard, or on an input.
  RD_STATE_ACTIVE        = 1u << 5,
  RD_STATE_DISABLED      = 1u << 6,
} rd_state;

typedef struct rd_class {
  uint32_t id;
} rd_class;

static inline rd_val rd_px(float v) {
  rd_val r = {v, RD_UNIT_PX};
  return r;
}
static inline rd_val rd_pct(float v) {
  rd_val r = {v, RD_UNIT_PCT};
  return r;
}
static inline rd_val rd_num(float v) {
  rd_val r = {v, RD_UNIT_NUM};
  return r;
}
// Durations, as seconds: transition_duration = rd_ms(150).
static inline rd_val rd_s(float seconds) { return rd_num(seconds); }
static inline rd_val rd_ms(float ms) { return rd_num(ms / 1000.0f); }
static inline rd_val rd_auto(void) {
  rd_val r = {0, RD_UNIT_AUTO};
  return r;
}

static inline rd_val rd_fr(float v) {
  rd_val r = {v, RD_UNIT_FR};
  return r;
}
static inline rd_val rd_min_content(void) {
  rd_val r = {0, RD_UNIT_MIN_CONTENT};
  return r;
}
static inline rd_val rd_max_content(void) {
  rd_val r = {0, RD_UNIT_MAX_CONTENT};
  return r;
}

static inline rd_track rd_track_minmax(rd_val min, rd_val max) {
  rd_track t = {min, max};
  return t;
}
// fr becomes minmax(auto, fr), as in CSS; every other value is used for both bounds.
static inline rd_track  rd_track_of(rd_val v) { return rd_track_minmax(v.unit == RD_UNIT_FR ? rd_auto() : v, v); }
static inline rd_tracks rd_tracks_none(void) {
  rd_tracks t = {0, true};
  return t;
}

static inline rd_grid_line rd_line(int16_t n) {
  rd_grid_line l = {n, RD_GRID_LINE_INDEX};
  return l;
}
static inline rd_grid_line rd_span(int16_t n) {
  rd_grid_line l = {n, RD_GRID_LINE_SPAN};
  return l;
}
static inline rd_grid_line rd_line_auto(void) {
  rd_grid_line l = {0, RD_GRID_LINE_AUTO};
  return l;
}

static inline rd_color rd_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  rd_color c = {r, g, b, a, true};
  return c;
}
static inline rd_color rd_rgb(uint8_t r, uint8_t g, uint8_t b) { return rd_rgba(r, g, b, 255); }
// 0xRRGGBB, opaque.
static inline rd_color rd_hex(uint32_t rgb) {
  return rd_rgba((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb, 255);
}

static inline rd_shadow rd_shadow_of(float x, float y, float blur, rd_color color) {
  rd_shadow s = {rd_px(x), rd_px(y), rd_px(blur), rd_px(0), color};
  return s;
}

static inline rd_edges rd_edges_all(rd_val v) {
  rd_edges e = {v, v, v, v};
  return e;
}
static inline rd_edges rd_edges_vh(rd_val vertical, rd_val horizontal) {
  rd_edges e = {vertical, horizontal, vertical, horizontal};
  return e;
}
static inline rd_corners rd_corners_all(rd_val v) {
  rd_corners c = {v, v, v, v};
  return c;
}

// Copies the tracks. The list lives until the context is destroyed; creating an identical list again
// returns the same handle, so rebuilding lists doesn't grow memory.
rd_tracks rd_tracks_create(rd_ctx* ctx, const rd_track* tracks, uint32_t count);

// CSS repeat(auto-fill, ...) and repeat(auto-fit, ...): the tracks repeat as many times as fit the
// grid's content box (its definite size, else its max size, else enough to reach its min size, else
// once), each track counted at its max size if that's definite, else its min size; a track with
// neither repeats once. AUTO_FIT then removes the repetitions no item occupies, and their gaps, so the
// remaining tracks share the space. The repeated tracks are the whole list.
RD_ENUM typedef enum rd_repeat : uint8_t {
  RD_REPEAT_NONE,
  RD_REPEAT_AUTO_FILL,
  RD_REPEAT_AUTO_FIT,
} rd_repeat;
rd_tracks rd_tracks_create_auto(rd_ctx* ctx, rd_repeat repeat, const rd_track* tracks, uint32_t count);
// Null and 0 for `none` or an unknown handle.
const rd_track* rd_tracks_get(const rd_ctx* ctx, rd_tracks tracks, uint32_t* count);

// Copies every property set in src over dst.
void rd_style_merge(rd_style* dst, const rd_style* src);

// Classes live until the context is destroyed.
rd_class rd_class_create(rd_ctx* ctx, const rd_style* base);
bool     rd_class_set(rd_ctx* ctx, rd_class cls, const rd_style* base);
// Applied while the node has `state`. Null style removes the variant.
bool rd_class_set_state(rd_ctx* ctx, rd_class cls, rd_state state, const rd_style* style);
// Copy the properties set in style over the class's base, or its variant for a state (creating it), as
// rd_node_merge_style does for a node: to recolor a class without restating its layout.
bool rd_class_merge(rd_ctx* ctx, rd_class cls, const rd_style* style);
bool rd_class_merge_state(rd_ctx* ctx, rd_class cls, rd_state state, const rd_style* style);
// Copy the class's base, or its variant for a state, into out. False for an unknown class, or a state
// with no variant (out is then all unset).
bool rd_class_get(const rd_ctx* ctx, rd_class cls, rd_style* out);
bool rd_class_get_state(const rd_ctx* ctx, rd_class cls, rd_state state, rd_style* out);

// Inline style wins over classes. Null clears it.
bool rd_node_set_style(rd_ctx* ctx, rd_node node, const rd_style* style);
bool rd_node_merge_style(rd_ctx* ctx, rd_node node, const rd_style* style);

// Later classes win. Adding a class the node already has is a no-op.
bool rd_node_add_class(rd_ctx* ctx, rd_node node, rd_class cls);
bool rd_node_remove_class(rd_ctx* ctx, rd_node node, rd_class cls);
void rd_node_clear_classes(rd_ctx* ctx, rd_node node);
bool rd_node_has_class(const rd_ctx* ctx, rd_node node, rd_class cls);

void    rd_node_set_states(rd_ctx* ctx, rd_node node, uint8_t states, bool on);
// The node's class variants also apply for its parent's states, as if it had them: the box of a
// checkbox looks hovered while the pointer is anywhere on the row. Chains through sharing parents.
void    rd_node_set_share_states(rd_ctx* ctx, rd_node node, bool share);
uint8_t rd_node_states(const rd_ctx* ctx, rd_node node);

void rd_ctx_compute_styles(rd_ctx* ctx);
// Fully resolved: every property is set. Valid until the next node creation.
const rd_style* rd_node_computed_style(rd_ctx* ctx, rd_node node);

#ifdef __cplusplus
}
#endif

#endif
