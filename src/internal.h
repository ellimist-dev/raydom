#ifndef RAYDOM_INTERNAL_H
#define RAYDOM_INTERNAL_H

#include <raydom/raydom.h>
#include <stddef.h>

enum : uint32_t {
  RD_SLOT_ALIVE               = 0x1u,
  RD_FLAG_STYLE_DIRTY         = 0x2u,
  RD_FLAG_SUBTREE_STYLE_DIRTY = 0x4u,
  RD_FLAG_LAYOUT_DIRTY        = 0x8u,
};

enum { RD_STATE_COUNT        = 7,
       RD_MEASURE_CACHE_SIZE = 4 };

typedef struct rd_slot {
  uint32_t gen;
  uint32_t flags;
  // next_sibling doubles as the free-list link while the slot is dead.
  uint32_t      parent, first_child, last_child, next_sibling, prev_sibling;
  uint32_t      child_count;
  void*         userdata;
  rd_destroy_fn destroy;
  rd_role       role;
} rd_slot;

typedef struct rd_node_style {
  rd_style  computed;
  rd_style* inline_style;
  uint32_t* classes;
  uint32_t  class_count;
  uint32_t  class_cap;
  uint8_t   states;
  bool      share_states;   // class variants also see the parent's states
  bool      computed_once;  // a first style never transitions
  struct rd__anim* anim;    // transitions in progress; null when nothing animates
} rd_node_style;

typedef struct rd_class_data {
  rd_style base;
  rd_style states[RD_STATE_COUNT];
  uint8_t  state_mask;
} rd_class_data;

typedef struct rd_layout_key {
  float           aw, ah;    // available border-box size
  float           cbw, cbh;  // containing block, for percentages
  rd_measure_mode wm, hm;
} rd_layout_key;

typedef struct rd_layout_cache {
  rd_layout_key key;
  rd_size       size;
  bool          valid;
} rd_layout_cache;

// Edge arrays are indexed top, right, bottom, left.
typedef struct rd_node_layout {
  rd_rect         rect;
  float           margin[4], border[4], padding[4];
  rd_size         content;
  float           baseline;  // from the border-box top; NaN if none
  float           scroll[2];
  uint32_t        order;  // pre-order position, refreshed while a selection exists
  rd_layout_cache layout;
  rd_layout_cache measure[RD_MEASURE_CACHE_SIZE];
  uint8_t         measure_next;
  rd_measure_fn   measure_fn;
  void*           measure_data;
  rd_destroy_fn   measure_destroy;
  struct rd__anchor* anchor;
  rd_paint_fn     paint_fn;
  void*           paint_data;
  rd_destroy_fn   paint_destroy;
} rd_node_layout;

typedef struct rd__listener {
  rd_event_fn   fn;
  void*         data;
  rd_destroy_fn destroy;
  uint32_t      id;
  uint8_t       type, flags;
  bool          removed;
} rd__listener;

typedef struct rd_node_events {
  rd__listener* items;
  uint32_t      count, cap;
} rd_node_events;

// An image's pixels until they're uploaded, and its texture after. Sizes in layout use the natural
// size; an SVG's pixels are a raster of it at raster_scale texels per unit, redone when drawn larger.
typedef struct rd__image {
  unsigned char*   pixels;  // RGBA8; freed after upload
  int              w, h;    // of pixels
  float            natural_w, natural_h;
  float            raster_scale;  // texels per natural unit: 1 for bitmaps
  struct NSVGimage* svg;          // parsed SVG, kept for re-rasterizing; null for bitmaps
  bool             current_color; // the SVG uses currentColor: drawn tinted by the node's color
  unsigned int     tex;           // GL texture id, 0 before the first draw
  int              tex_w, tex_h;
  bool             alive, dirty;  // dirty: pixels newer than the texture
} rd__image;

typedef struct rd__hook {
  rd_layout_hook fn;
  void*          data;
  rd_destroy_fn  destroy;
} rd__hook;

typedef struct rd__extension {
  const void*   key;
  void*         data;
  rd_destroy_fn destroy;
} rd__extension;

typedef struct rd__timer {
  double        due, period;  // period 0: one-shot
  rd_timer_fn   fn;
  void*         data;
  rd_destroy_fn destroy;
  uint32_t      gen, epoch, fired;  // epoch: the run it was set in; fired: the last run it fired in
  bool          active, firing, cleared;
} rd__timer;

typedef struct rd__chain {
  rd_node* nodes;
  uint32_t len, cap;
} rd__chain;

struct rd__cmd;

struct rd_ctx {
  // Parallel arrays indexed by slot index, all sized cap.
  rd_slot*        slots;
  rd_node_style*  styles;
  rd_node_layout* layouts;
  rd_node_events* events;
  uint32_t        cap;
  uint32_t        len;
  uint32_t        free_head;
  uint32_t        live;
  uint32_t        root;
  struct rd__extension* extensions;
  uint32_t              extension_count;
  rd__timer*      timers;
  uint32_t        timer_count, timer_cap, timer_epoch;
  bool            event_waiting, waiting_now;  // rd_set_event_waiting, and whether raylib is waiting now
  rd__image*      images;
  uint32_t        image_count, image_cap;
  rd_node         overlay;
  rd_node         focus_scope;  // Tab stays inside it; null for the whole tree   // created by rd_ctx_overlay; kept as root's last child
  rd__chain       anchored;
  rd__chain       animating;  // nodes with an rd__anim
  struct rd__hook* hooks;
  uint32_t         hook_count;  // nodes with an anchor, in the order they got one

  rd_class_data* classes;
  uint32_t       class_count;
  uint32_t       class_cap;

  // Track lists are spans of track_pool, append-only.
  rd_track* track_pool;
  uint32_t  track_pool_len, track_pool_cap;
  uint32_t* track_lists;  // pairs: offset, count
  uint32_t  track_list_count, track_list_cap;

  struct rd__font** fonts;
  uint32_t          font_count, font_cap;
  uint32_t          default_font;

  bool paint_dirty;
  // Scrollbar under the pointer, and the one being dragged or held after a track press.
  struct {
    rd_node node;
    int     axis;
  } sb_hot;
  struct {
    rd_node node;
    int     axis;
    float   grab;   // pointer offset into the thumb
    bool    thumb;  // false: a track press, held until release
    bool    active;
  } sb_drag;

  struct rd__cmd* cmds;
  uint32_t        cmd_count, cmd_cap;
  rd_node         hovered;
  rd__chain       hover_chain, active_chain;
  void*           gpu;

  uint32_t  listener_next_id;
  uint32_t  dispatch_depth;
  rd__chain pending_destroy;
  bool      listeners_dirty;
  bool      pointer_seen;
  uint8_t   pointer_buttons;
  float     pointer_x, pointer_y;
  rd_node   press_target, capture;

  rd_node             focused;
  rd__chain           focus_chain;
  bool                keyboard_modality;
  uint8_t             mods;
  rd_cursor           applied_cursor;  // last cursor rd_update set; UNSET before the first
  rd_clipboard_get_fn clip_get;
  rd_clipboard_set_fn clip_set;
  void*               clip_data;
  char*               clip_buf;

  double  now;
  rd_node wheel_latch;
  double  wheel_time;
  float   wheel_at[2];

  rd_node  sel_anchor, sel_focus;
  size_t   sel_anchor_off, sel_focus_off;
  bool     selecting, sel_active;
  uint32_t sel_start, sel_end;  // tree order of the selection's first and last node
  size_t   sel_start_off, sel_end_off;
};

static inline rd_slot* rd__slot(const rd_ctx* ctx, rd_node n) {
  if (!ctx || n.index == 0 || n.index >= ctx->len) return NULL;
  rd_slot* s = &ctx->slots[n.index];
  if (!(s->flags & RD_SLOT_ALIVE) || s->gen != n.gen) return NULL;
  return s;
}

static inline rd_node rd__handle(const rd_ctx* ctx, uint32_t index) {
  return index ? (rd_node){index, ctx->slots[index].gen} : (rd_node){0, 0};
}

void     rd__mark_style_dirty(rd_ctx* ctx, uint32_t i);
void     rd__mark_layout_dirty(rd_ctx* ctx, uint32_t i);
void     rd__style_slot_free(rd_ctx* ctx, uint32_t i);
void     rd__style_ctx_free(rd_ctx* ctx);
void     rd__style_check_defaults(void);
void     rd__layout_slot_free(rd_ctx* ctx, uint32_t i);
void     rd__font_ctx_free(rd_ctx* ctx);
void     rd__render_ctx_free(rd_ctx* ctx);
void     rd__clamp_scroll(rd_node_layout* L);
// Scrollbar of scroll container i along axis (0 horizontal, 1 vertical), for its border box r in root
// coordinates. False when the axis doesn't scroll, the bar has no width, or nothing overflows.
bool     rd__scrollbar(const rd_ctx* ctx, uint32_t i, rd_rect r, int axis, rd_rect* track, rd_rect* thumb);
float    rd__scroll_max(const rd_ctx* ctx, uint32_t i, int axis);
uint32_t rd__hit(const rd_ctx* ctx, float x, float y);
uint32_t rd__scroll_target(const rd_ctx* ctx, uint32_t i, int axis, float delta);
bool     rd__scroll_by(rd_ctx* ctx, uint32_t i, float dx, float dy);
void     rd__scroll_into_view(rd_ctx* ctx, uint32_t i);
void     rd__events_slot_free(rd_ctx* ctx, uint32_t i);
void     rd__events_ctx_free(rd_ctx* ctx);
bool     rd__chain_push(rd__chain* c, rd_node n);
bool     rd__role_interactive(const rd_ctx* ctx, uint32_t i);
rd__image* rd__image_get(const rd_ctx* ctx, uint32_t id);
uint32_t   rd__node_image(const rd_ctx* ctx, uint32_t i);  // 0 unless the node is an image leaf
void       rd__image_release(rd_ctx* ctx, rd__image* im);   // render.c: frees the texture
void       rd__images_free(rd_ctx* ctx);
enum { RD__MAX_RASTER = 4096 };
// Rasterizes an SVG image at scale texels per unit, capped to a RD__MAX_RASTER side. False on failure.
bool       rd__image_rasterize(rd__image* im, float scale);
void       rd__timers_run(rd_ctx* ctx);
void       rd__timers_free(rd_ctx* ctx);
void       rd__transition(rd_ctx* ctx, uint32_t i, rd_style* next);
void       rd__anim_tick(rd_ctx* ctx);
void       rd__anim_free(rd_node_style* ns);
float      rd__natural_height(rd_ctx* ctx, uint32_t i);  // border-box height with height: auto

#endif
