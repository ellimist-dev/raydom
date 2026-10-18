#ifndef RAYDOM_INPUT_INTERNAL_H
#define RAYDOM_INPUT_INTERNAL_H

#include "text_internal.h"

typedef struct rd__input rd__input;

typedef struct rd__input_view {
  size_t      anchor, caret;
  float       scroll_x;
  const char* placeholder;
  size_t      placeholder_len;
  bool        multiline;
} rd__input_view;

bool rd__input_view_get(const rd_ctx* ctx, uint32_t i, rd__input_view* out);
// True once per batch of edits, for the change event.
bool rd__input_take_dirty(rd_ctx* ctx, uint32_t i);
// Default actions; rd__input_key reports whether it handled the key.
bool rd__input_key(rd_ctx* ctx, uint32_t i, const rd_event* ev);
void rd__input_text(rd_ctx* ctx, uint32_t i, uint32_t codepoint);
void rd__input_pointer(rd_ctx* ctx, uint32_t i, float x, float y, bool extend);
// Keeps a single-line input's caret in view; returns the horizontal scroll.
float rd__input_sync_scroll(rd_ctx* ctx, uint32_t i, const rd__text_layout* tl, float width);

void        rd__fire(rd_ctx* ctx, rd_event_type type, rd_node target);
const char* rd__clipboard_get(rd_ctx* ctx);
void        rd__clipboard_set(rd_ctx* ctx, const char* text);
float       rd__text_align_offset(const rd_style* st, float width, float line_width);

#endif
