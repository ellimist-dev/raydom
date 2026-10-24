#include <stdlib.h>
#include <string.h>

#include "input_internal.h"
#include "layout_internal.h"

enum { UNDO_LIMIT = 200 };

typedef struct snapshot {
  char*  text;
  size_t len, anchor, caret;
} snapshot;

typedef struct snapshots {
  snapshot* items;
  uint32_t  len, cap;
} snapshots;

struct rd__input {
  uint32_t  flags;
  size_t    anchor, caret;  // byte offsets
  float     scroll_x;
  float     goal_x;  // x kept across vertical moves
  bool      has_goal;
  char*     placeholder;
  size_t    placeholder_len;
  snapshots undo, redo;
  bool      dirty;    // edited since the last change event
  bool      typing;   // the last edit was typed text, so the next one joins its undo step
  bool      editing;  // inside an edit's own rd_node_set_text
  uint32_t  high;     // a UTF-16 high surrogate waiting for its low half
};

#ifdef __APPLE__
static const uint8_t SHORTCUT_MOD = RD_MOD_SUPER, WORD_MOD = RD_MOD_ALT;
#else
static const uint8_t SHORTCUT_MOD = RD_MOD_CTRL, WORD_MOD = RD_MOD_CTRL;
#endif

bool rd__input_multiline(const rd__input* in) { return in->flags & RD_INPUT_MULTILINE; }

static void snapshots_clear(snapshots* s) {
  for (uint32_t j = 0; j < s->len; j++) free(s->items[j].text);
  s->len = 0;
}

void rd__input_free(rd__input* in) {
  if (!in) return;
  snapshots_clear(&in->undo);
  snapshots_clear(&in->redo);
  free(in->undo.items);
  free(in->redo.items);
  free(in->placeholder);
  free(in);
}

static size_t snap_byte(const rd_text* t, size_t b) {
  if (b >= t->len) return t->len;
  return t->offsets[rd__text_cp_of_byte(t, b)];
}

void rd__input_clamp(rd_text* t) {
  t->input->anchor = snap_byte(t, t->input->anchor);
  t->input->caret  = snap_byte(t, t->input->caret);
}

static rd__input* input_of(const rd_ctx* ctx, uint32_t i) {
  rd_text* t = rd__text_get(ctx, i);
  return t ? t->input : NULL;
}

bool rd_node_make_input(rd_ctx* ctx, rd_node node, uint32_t flags) {
  if (!rd__slot(ctx, node)) return false;
  rd_text* t = rd__text_get(ctx, node.index);
  if (!t) {
    if (!rd_node_set_text(ctx, node, "", 0)) return false;
    t = rd__text_get(ctx, node.index);
  }
  if (!t->input) {
    t->input = calloc(1, sizeof *t->input);
    if (!t->input) return false;
  }
  t->input->flags = flags;
  t->tok_valid    = false;
  rd__input_clamp(t);
  rd__mark_layout_dirty(ctx, node.index);
  return true;
}

bool rd_node_is_input(const rd_ctx* ctx, rd_node node) {
  return rd__slot(ctx, node) && input_of(ctx, node.index);
}

bool rd_node_set_placeholder(rd_ctx* ctx, rd_node node, const char* utf8, size_t len) {
  rd__input* in = rd__slot(ctx, node) ? input_of(ctx, node.index) : NULL;
  if (!in) return false;
  if (!utf8) utf8 = "";
  if (len == RD_TEXT_NUL) len = strlen(utf8);
  char* p = malloc(len + 1);
  if (!p) return false;
  memcpy(p, utf8, len);
  p[len] = 0;
  free(in->placeholder);
  in->placeholder     = p;
  in->placeholder_len = len;
  ctx->paint_dirty    = true;
  return true;
}

bool rd_input_select(rd_ctx* ctx, rd_node node, size_t anchor, size_t caret) {
  rd_text* t = rd__slot(ctx, node) ? rd__text_get(ctx, node.index) : NULL;
  if (!t || !t->input) return false;
  t->input->anchor   = snap_byte(t, anchor);
  t->input->caret    = snap_byte(t, caret);
  t->input->has_goal = false;
  t->input->typing   = false;
  ctx->paint_dirty   = true;
  return true;
}

bool rd_input_selection(const rd_ctx* ctx, rd_node node, size_t* anchor, size_t* caret) {
  rd__input* in = rd__slot(ctx, node) ? input_of(ctx, node.index) : NULL;
  if (!in) return false;
  if (anchor) *anchor = in->anchor;
  if (caret) *caret = in->caret;
  return true;
}

bool rd__input_view_get(const rd_ctx* ctx, uint32_t i, rd__input_view* out) {
  rd__input* in = input_of(ctx, i);
  if (!in) return false;
  *out = (rd__input_view){in->anchor, in->caret, in->scroll_x, in->placeholder, in->placeholder_len,
                          rd__input_multiline(in)};
  return true;
}

bool rd__input_take_dirty(rd_ctx* ctx, uint32_t i) {
  rd__input* in = input_of(ctx, i);
  if (!in || !in->dirty) return false;
  in->dirty = false;
  return true;
}

// Editing.

static bool snapshot_push(snapshots* s, const rd_text* t, const rd__input* in) {
  if (s->len == UNDO_LIMIT) {
    free(s->items[0].text);
    memmove(s->items, s->items + 1, (s->len - 1) * sizeof *s->items);
    s->len--;
  }
  if (s->len == s->cap) {
    uint32_t  cap = s->cap ? s->cap * 2 : 16;
    snapshot* p   = realloc(s->items, cap * sizeof *p);
    if (!p) return false;
    s->items = p;
    s->cap   = cap;
  }
  char* copy = malloc(t->len + 1);
  if (!copy) return false;
  memcpy(copy, t->utf8, t->len + 1);
  s->items[s->len++] = (snapshot){copy, t->len, in->anchor, in->caret};
  return true;
}

void rd__input_end_typing(rd_ctx* ctx, uint32_t i) {
  rd__input* in = input_of(ctx, i);
  if (in) in->typing = false;
}

void rd__input_before_set(rd_text* t, const char* utf8, size_t len) {
  rd__input* in = t->input;
  if (in->editing) return;
  in->typing = false;
  in->high   = 0;
  if (len == t->len && !memcmp(utf8, t->utf8, len)) return;
  snapshot_push(&in->undo, t, in);
  snapshots_clear(&in->redo);
}

static rd_node node_of(const rd_text* t) { return rd__handle(t->ctx, t->node); }

static void changed(rd_text* t) {
  t->input->dirty    = true;
  t->input->has_goal = false;
  rd__fire(t->ctx, RD_EVENT_INPUT, node_of(t));
}

static bool replace(rd_text* t, size_t from, size_t to, const char* ins, size_t ins_len, bool typing) {
  rd__input* in = t->input;
  if (from == to && !ins_len) return false;
  if (!(typing && in->typing)) snapshot_push(&in->undo, t, in);
  snapshots_clear(&in->redo);
  in->typing = typing;

  size_t len = t->len - (to - from) + ins_len;
  char*  buf = malloc(len + 1);
  if (!buf) return false;
  memcpy(buf, t->utf8, from);
  memcpy(buf + from, ins, ins_len);
  memcpy(buf + from + ins_len, t->utf8 + to, t->len - to);
  buf[len]    = 0;
  in->editing = true;
  bool ok     = rd_node_set_text(t->ctx, node_of(t), buf, len);
  in->editing = false;
  free(buf);
  if (!ok) return false;
  in->anchor = in->caret = from + ins_len;
  changed(t);
  return true;
}

static void restore(rd_text* t, snapshots* from, snapshots* to) {
  rd__input* in = t->input;
  if (!from->len) return;
  snapshot_push(to, t, in);
  snapshot s  = from->items[--from->len];
  in->editing = true;
  rd_node_set_text(t->ctx, node_of(t), s.text, s.len);
  in->editing = false;
  free(s.text);
  in->anchor = snap_byte(t, s.anchor);
  in->caret  = snap_byte(t, s.caret);
  in->typing = false;
  changed(t);
}

static bool is_word(uint32_t c) {
  return c == '_' || c >= 0x80 || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static size_t prev_cp(const rd_text* t, size_t b) {
  uint32_t cp = rd__text_cp_of_byte(t, b);
  return cp ? t->offsets[cp - 1] : 0;
}

static size_t next_cp(const rd_text* t, size_t b) {
  uint32_t cp = rd__text_cp_of_byte(t, b);
  return cp < t->cp_count ? t->offsets[cp + 1] : t->len;
}

static size_t word_left(const rd_text* t, size_t b) {
  uint32_t cp = rd__text_cp_of_byte(t, b);
  while (cp && !is_word(t->cps[cp - 1])) cp--;
  while (cp && is_word(t->cps[cp - 1])) cp--;
  return t->offsets[cp];
}

static size_t word_right(const rd_text* t, size_t b) {
  uint32_t cp = rd__text_cp_of_byte(t, b);
  while (cp < t->cp_count && !is_word(t->cps[cp])) cp++;
  while (cp < t->cp_count && is_word(t->cps[cp])) cp++;
  return t->offsets[cp];
}

static float content_width(const rd_ctx* ctx, uint32_t i) {
  const rd_node_layout* L = &ctx->layouts[i];
  return fmaxf(0, L->rect.width - L->border[EDGE_L] - L->border[EDGE_R] - L->padding[EDGE_L] - L->padding[EDGE_R]);
}

static bool text_layout(rd_text* t, rd__text_layout* tl) {
  return rd__text_layout_get(t->ctx, t->node, content_width(t->ctx, t->node), tl);
}

static size_t line_edge(rd_text* t, size_t b, bool end) {
  rd__text_layout tl;
  if (!rd__input_multiline(t->input) || !text_layout(t, &tl)) return end ? t->len : 0;
  const rd__line* ln = &tl.lines[rd__text_line_of(&tl, rd__text_cp_of_byte(t, b))];
  return t->offsets[end ? ln->cp_end : ln->cp_start];
}

static size_t vertical(rd_text* t, size_t b, int dir) {
  rd__input*      in = t->input;
  rd__text_layout tl;
  if (!rd__input_multiline(in) || !text_layout(t, &tl)) return dir < 0 ? 0 : t->len;
  uint32_t cp   = rd__text_cp_of_byte(t, b);
  uint32_t line = rd__text_line_of(&tl, cp);
  if (!in->has_goal) {
    in->goal_x   = rd__text_cp_x(&tl, line, cp);
    in->has_goal = true;
  }
  if (dir < 0 && line == 0) return 0;
  if (dir > 0 && line + 1 >= tl.line_count) return t->len;
  return t->offsets[rd__text_cp_at(&tl, (uint32_t)((int)line + dir), in->goal_x)];
}

static void move(rd__input* in, size_t to, bool extend, bool keep_goal) {
  in->caret = to;
  if (!extend) in->anchor = to;
  in->typing = false;
  if (!keep_goal) in->has_goal = false;
}

static void copy_selection(rd_text* t, size_t lo, size_t hi) {
  char* s = malloc(hi - lo + 1);
  if (!s) return;
  memcpy(s, t->utf8 + lo, hi - lo);
  s[hi - lo] = 0;
  rd__clipboard_set(t->ctx, s);
  free(s);
}

static size_t encode(uint32_t cp, char* buf) {
  if (cp < 0x80) {
    buf[0] = (char)cp;
    return 1;
  }
  if (cp < 0x800) {
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  }
  if (cp < 0x10000) {
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  }
  buf[0] = (char)(0xF0 | (cp >> 18));
  buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
  buf[3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

// Pasted text keeps tabs and, in a multi-line input, newlines; other controls go, and invalid UTF-8
// becomes U+FFFD (up to three bytes for each bad byte).
static void paste(rd_text* t, size_t lo, size_t hi) {
  const char* clip = rd__clipboard_get(t->ctx);
  if (!clip || !*clip) return;
  size_t n   = strlen(clip);
  char*  buf = n < SIZE_MAX / 3 ? malloc(3 * n + 1) : NULL;
  if (!buf) return;
  size_t m = 0;
  for (size_t j = 0; j < n;) {
    uint32_t c = rd__utf8_decode(clip, n, &j);
    if (c == '\n' && !rd__input_multiline(t->input)) c = ' ';
    if ((c < 0x20 && c != '\n' && c != '\t') || c == 0x7F) continue;
    m += encode(c, buf + m);
  }
  replace(t, lo, hi, buf, m, false);
  free(buf);
}

bool rd__input_key(rd_ctx* ctx, uint32_t i, const rd_event* ev) {
  rd_text* t = rd__text_get(ctx, i);
  if (!t || !t->input) return false;
  rd__input* in       = t->input;
  bool       shift    = ev->mods & RD_MOD_SHIFT;
  bool       shortcut = ev->mods & SHORTCUT_MOD, word = ev->mods & WORD_MOD;
  size_t     lo    = in->anchor < in->caret ? in->anchor : in->caret;
  size_t     hi    = in->anchor < in->caret ? in->caret : in->anchor;
  ctx->paint_dirty = true;

  switch (ev->key) {
    case RD_KEY_LEFT:
    case RD_KEY_RIGHT: {
      bool   right = ev->key == RD_KEY_RIGHT;
      size_t to;
#ifdef __APPLE__
      if (shortcut)
        to = line_edge(t, in->caret, right);
      else
#endif
          if (word)
        to = right ? word_right(t, in->caret) : word_left(t, in->caret);
      else if (!shift && lo != hi)
        to = right ? hi : lo;
      else
        to = right ? next_cp(t, in->caret) : prev_cp(t, in->caret);
      move(in, to, shift, false);
      return true;
    }
    case RD_KEY_UP:
    case RD_KEY_DOWN:
      move(in, vertical(t, in->caret, ev->key == RD_KEY_UP ? -1 : 1), shift, true);
      return true;
    case RD_KEY_HOME:
    case RD_KEY_END: {
      bool end = ev->key == RD_KEY_END;
      move(in, shortcut ? (end ? t->len : 0) : line_edge(t, in->caret, end), shift, false);
      return true;
    }
    case RD_KEY_BACKSPACE:
    case RD_KEY_DELETE: {
      bool fwd = ev->key == RD_KEY_DELETE;
      if (lo != hi) {
        replace(t, lo, hi, "", 0, false);
        return true;
      }
      size_t to;
#ifdef __APPLE__
      if (shortcut)
        to = line_edge(t, in->caret, fwd);
      else
#endif
          if (word)
        to = fwd ? word_right(t, in->caret) : word_left(t, in->caret);
      else
        to = fwd ? next_cp(t, in->caret) : prev_cp(t, in->caret);
      replace(t, fwd ? in->caret : to, fwd ? to : in->caret, "", 0, false);
      return true;
    }
    case RD_KEY_ENTER:
      if (rd__input_multiline(in))
        replace(t, lo, hi, "\n", 1, false);
      else if (rd__input_take_dirty(ctx, i))
        rd__fire(ctx, RD_EVENT_CHANGE, node_of(t));
      return true;
    case RD_KEY_A:
      if (!shortcut) return false;
      in->anchor = 0;
      in->caret  = t->len;
      return true;
    case RD_KEY_C:
    case RD_KEY_X:
      if (!shortcut) return false;
      if (lo != hi) {
        copy_selection(t, lo, hi);
        if (ev->key == RD_KEY_X) replace(t, lo, hi, "", 0, false);
      }
      return true;
    case RD_KEY_V:
      if (!shortcut) return false;
      paste(t, lo, hi);
      return true;
    case RD_KEY_Z:
      if (!shortcut) return false;
      if (shift)
        restore(t, &in->redo, &in->undo);
      else
        restore(t, &in->undo, &in->redo);
      return true;
    case RD_KEY_Y:
      if (!shortcut) return false;
      restore(t, &in->redo, &in->undo);
      return true;
    default:
      return false;
  }
}

// Hosts that deliver UTF-16 send a pair as two code points: join them, and drop lone halves.
void rd__input_text(rd_ctx* ctx, uint32_t i, uint32_t cp) {
  rd_text* t = rd__text_get(ctx, i);
  if (!t || !t->input) return;
  rd__input* in   = t->input;
  uint32_t   high = in->high;
  in->high        = 0;
  if (cp >= 0xD800 && cp <= 0xDBFF) {
    in->high = cp;
    return;
  }
  if (cp >= 0xDC00 && cp <= 0xDFFF) {
    if (!high) return;
    cp = 0x10000 + ((high - 0xD800) << 10) + (cp - 0xDC00);
  }
  if (cp < 0x20 || cp == 0x7F || cp > 0x10FFFF) return;
  char   buf[4];
  size_t n  = encode(cp, buf);
  size_t lo = in->anchor < in->caret ? in->anchor : in->caret;
  size_t hi = in->anchor < in->caret ? in->caret : in->anchor;
  replace(t, lo, hi, buf, n, cp != ' ');
}

// Pointer: (x, y) in root coordinates.
void rd__input_pointer(rd_ctx* ctx, uint32_t i, float x, float y, bool extend) {
  rd_text* t = rd__text_get(ctx, i);
  if (!t || !t->input) return;
  float dx = rd__input_multiline(t->input) ? 0 : t->input->scroll_x;
  move(t->input, rd__text_byte_at(ctx, i, x, y, dx), extend, false);
  ctx->paint_dirty = true;
}

float rd__input_sync_scroll(rd_ctx* ctx, uint32_t i, const rd__text_layout* tl, float width) {
  rd__input* in = input_of(ctx, i);
  if (!in || rd__input_multiline(in) || !tl->line_count) return 0;
  rd_text* t     = rd__text_get(ctx, i);
  float    caret = rd__text_cp_x(tl, 0, rd__text_cp_of_byte(t, in->caret));
  float    full  = fmaxf(tl->lines[0].width, rd__text_cp_x(tl, 0, t->cp_count)) + 1;
  float    sx    = in->scroll_x;
  if (caret - sx > width - 1) sx = caret - width + 1;
  if (caret < sx) sx = caret;
  in->scroll_x = clampf(sx, 0, fmaxf(0, full - width));
  return in->scroll_x;
}
