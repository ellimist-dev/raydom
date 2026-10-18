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

enum { RD_STATE_COUNT        = 6,
       RD_MEASURE_CACHE_SIZE = 4 };

typedef struct rd_slot {
  uint32_t gen;
  uint32_t flags;
  // next_sibling doubles as the free-list link while the slot is dead.
  uint32_t      parent, first_child, last_child, next_sibling, prev_sibling;
  uint32_t      child_count;
  void*         userdata;
  rd_destroy_fn destroy;
} rd_slot;

typedef struct rd_node_style {
  rd_style  computed;
  rd_style* inline_style;
  uint32_t* classes;
  uint32_t  class_count;
  uint32_t  class_cap;
  uint8_t   states;
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
uint32_t rd__hit(const rd_ctx* ctx, float x, float y);
uint32_t rd__scroll_target(const rd_ctx* ctx, uint32_t i, int axis, float delta);
bool     rd__scroll_by(rd_ctx* ctx, uint32_t i, float dx, float dy);
void     rd__scroll_into_view(rd_ctx* ctx, uint32_t i);
void     rd__events_slot_free(rd_ctx* ctx, uint32_t i);
void     rd__events_ctx_free(rd_ctx* ctx);
bool     rd__chain_push(rd__chain* c, rd_node n);

#endif
