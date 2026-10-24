#ifndef RAYDOM_EVENTS_H
#define RAYDOM_EVENTS_H

#include <raydom/style.h>

#ifdef __cplusplus
extern "C" {
#endif

RD_ENUM typedef enum rd_event_type : uint8_t {
  RD_EVENT_POINTER_DOWN,
  RD_EVENT_POINTER_UP,
  RD_EVENT_POINTER_MOVE,
  RD_EVENT_POINTER_ENTER,  // Doesn't bubble.
  RD_EVENT_POINTER_LEAVE,  // Doesn't bubble.
  RD_EVENT_CLICK,          // Primary button; nearest common ancestor of the press and release targets.
                           // ev->clicks counts rapid clicks on the same node: 2 is a double click.
  RD_EVENT_WHEEL,          // Default action: scroll the nearest scrollable ancestor.
  RD_EVENT_KEY_DOWN,       // To the focused node or the root. Default actions: Tab focus, Enter/Space click, editing.
  RD_EVENT_KEY_UP,
  RD_EVENT_TEXT,   // One codepoint of text input. Default action: insert into a focused input.
  RD_EVENT_FOCUS,  // Doesn't bubble.
  RD_EVENT_BLUR,   // Doesn't bubble.
  RD_EVENT_FOCUS_IN,
  RD_EVENT_FOCUS_OUT,
  RD_EVENT_INPUT,             // An input's text changed through editing.
  RD_EVENT_CHANGE,            // An input's edits were committed: Enter on a single-line input, or blur.
  RD_EVENT_SELECTION_CHANGE,  // The document text selection changed. Fired on the root.
  RD_EVENT_TYPE_COUNT,
} rd_event_type;

RD_ENUM typedef enum rd_event_phase : uint8_t {
  RD_PHASE_CAPTURE,
  RD_PHASE_TARGET,
  RD_PHASE_BUBBLE,
} rd_event_phase;

// BACK and FORWARD are a mouse's side buttons. Only PRIMARY clicks; the others fire down and up.
enum { RD_BUTTON_PRIMARY   = 1u << 0,
       RD_BUTTON_SECONDARY = 1u << 1,
       RD_BUTTON_MIDDLE    = 1u << 2,
       RD_BUTTON_BACK      = 1u << 3,
       RD_BUTTON_FORWARD   = 1u << 4 };

RD_ENUM typedef enum rd_key : uint16_t {
  RD_KEY_UNKNOWN,
  RD_KEY_TAB,
  RD_KEY_ENTER,
  RD_KEY_ESCAPE,
  RD_KEY_BACKSPACE,
  RD_KEY_DELETE,
  RD_KEY_INSERT,
  RD_KEY_LEFT,
  RD_KEY_RIGHT,
  RD_KEY_UP,
  RD_KEY_DOWN,
  RD_KEY_HOME,
  RD_KEY_END,
  RD_KEY_PAGE_UP,
  RD_KEY_PAGE_DOWN,
  RD_KEY_SPACE,
  RD_KEY_0,
  RD_KEY_1,
  RD_KEY_2,
  RD_KEY_3,
  RD_KEY_4,
  RD_KEY_5,
  RD_KEY_6,
  RD_KEY_7,
  RD_KEY_8,
  RD_KEY_9,
  RD_KEY_A,
  RD_KEY_B,
  RD_KEY_C,
  RD_KEY_D,
  RD_KEY_E,
  RD_KEY_F,
  RD_KEY_G,
  RD_KEY_H,
  RD_KEY_I,
  RD_KEY_J,
  RD_KEY_K,
  RD_KEY_L,
  RD_KEY_M,
  RD_KEY_N,
  RD_KEY_O,
  RD_KEY_P,
  RD_KEY_Q,
  RD_KEY_R,
  RD_KEY_S,
  RD_KEY_T,
  RD_KEY_U,
  RD_KEY_V,
  RD_KEY_W,
  RD_KEY_X,
  RD_KEY_Y,
  RD_KEY_Z,
  RD_KEY_F1,
  RD_KEY_F2,
  RD_KEY_F3,
  RD_KEY_F4,
  RD_KEY_F5,
  RD_KEY_F6,
  RD_KEY_F7,
  RD_KEY_F8,
  RD_KEY_F9,
  RD_KEY_F10,
  RD_KEY_F11,
  RD_KEY_F12,
  RD_KEY_LEFT_BRACKET,
  RD_KEY_RIGHT_BRACKET,
} rd_key;

enum { RD_MOD_SHIFT = 1u << 0,
       RD_MOD_CTRL  = 1u << 1,
       RD_MOD_ALT   = 1u << 2,
       RD_MOD_SUPER = 1u << 3 };

typedef struct rd_event {
  rd_event_type  type;
  rd_event_phase phase;
  rd_node        target;
  rd_node        current;
  float          x, y;              // root coordinates
  float          local_x, local_y;  // relative to current's border box
  float          wheel_x, wheel_y;  // notches
  uint8_t        button;            // the button that changed, for down/up/click
  uint8_t        clicks;            // CLICK: 1, or 2 for a double click, 3 for a triple...
  uint8_t        buttons;           // held buttons
  rd_key         key;
  uint8_t        mods;
  bool           repeat;
  uint32_t       codepoint;  // RD_EVENT_TEXT
  rd_node        related;    // focus events: the node losing or gaining focus
  bool           stopped, stopped_immediate, default_prevented;
} rd_event;

typedef void (*rd_event_fn)(rd_ctx* ctx, rd_event* ev, void* data);

typedef struct rd_listener {
  rd_node  node;
  uint32_t id;
} rd_listener;

enum { RD_LISTEN_CAPTURE = 1u << 0 };

// Listeners may mutate the tree. Nodes destroyed during dispatch are detached at once and freed
// when dispatch ends; the propagation path is fixed when dispatch starts.
rd_listener rd_node_on(rd_ctx* ctx, rd_node node, rd_event_type type, rd_event_fn fn, void* data,
                       rd_destroy_fn destroy, uint8_t flags);
// Runs the listener's destroy callback, deferred if dispatch is in progress.
bool rd_node_off(rd_ctx* ctx, rd_listener listener);

// Fires an event of type at node, through capture and bubble like any other, with the pointer
// position and modifiers of the moment. For components announcing their own INPUT and CHANGE. Returns
// false if a listener called rd_event_prevent_default.
bool rd_node_dispatch(rd_ctx* ctx, rd_node node, rd_event_type type);

static inline void rd_event_stop_propagation(rd_event* ev) { ev->stopped = true; }
static inline void rd_event_stop_immediate_propagation(rd_event* ev) { ev->stopped = ev->stopped_immediate = true; }
static inline void rd_event_prevent_default(rd_event* ev) { ev->default_prevented = true; }

// Routes pointer events to the node until every button is released.
void    rd_node_set_pointer_capture(rd_ctx* ctx, rd_node node);
void    rd_node_release_pointer_capture(rd_ctx* ctx, rd_node node);
rd_node rd_ctx_pointer_capture(const rd_ctx* ctx);

// Feeds pointer state, dispatching events and updating hover and active states.
// buttons is a mask of RD_BUTTON_*; wheel deltas are in notches. Disabled nodes get no pointer
// events, except that BACK and FORWARD over one (or over nothing) go to the root, for a router there.
// A click whose pressed node was destroyed or detached goes to its nearest ancestor still in the tree.
// It also drops focus from a node that can no longer have it (see rd_node_focus).
void    rd_ctx_pointer(rd_ctx* ctx, float x, float y, uint8_t buttons, float wheel_x, float wheel_y);
rd_node rd_ctx_hovered(const rd_ctx* ctx);
// The cursor to show, as of the last layout: the hovered (or capturing) node's cursor, with AUTO
// resolved, or text while a selection is being dragged. Never AUTO. rd_update applies it.
rd_cursor rd_ctx_cursor(const rd_ctx* ctx);

// Seconds on any monotonic clock; used to end wheel gestures, and fires due timers (timer.h).
// rd_update sets it from raylib.
void rd_ctx_set_time(rd_ctx* ctx, double seconds);
// The clock as last set: what timers and animations run on. 0 before the first set.
double rd_ctx_time(const rd_ctx* ctx);

// Keyboard input goes to the focused node, or the root when nothing has focus.
void rd_ctx_key(rd_ctx* ctx, rd_key key, bool down, bool repeat, uint8_t mods);
void rd_ctx_text(rd_ctx* ctx, uint32_t codepoint);

// Focus moves fire change (for an edited input), blur, focusout, focus, focusin. A listener that
// moves focus again takes over, and the rest of the outer sequence is skipped. A null node clears
// focus. A focused node that is destroyed, detached, hidden or disabled loses focus, with those
// events, at the end of the next rd_ctx_layout; rd_ctx_focused returns null for it at once.
bool    rd_node_focus(rd_ctx* ctx, rd_node node);
rd_node rd_ctx_focused(const rd_ctx* ctx);
// Next or previous node with focusable: tab, in tree order, wrapping, within the focus scope. False
// if there is none.
bool rd_ctx_focus_next(rd_ctx* ctx, bool backward);
// Keeps Tab and Shift+Tab inside a subtree, such as a modal dialog. Null, or a destroyed node, means
// the whole tree. Pointer and programmatic focus aren't restricted.
void    rd_ctx_set_focus_scope(rd_ctx* ctx, rd_node scope);
rd_node rd_ctx_focus_scope(const rd_ctx* ctx);

// Clipboard used by inputs. Defaults to an in-memory buffer; rd_update installs raylib's.
typedef const char* (*rd_clipboard_get_fn)(void* data);
typedef void (*rd_clipboard_set_fn)(void* data, const char* text);
void rd_ctx_set_clipboard(rd_ctx* ctx, rd_clipboard_get_fn get, rd_clipboard_set_fn set, void* data);

#ifdef __cplusplus
}
#endif

#endif
