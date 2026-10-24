// Differential layout fuzzing: a tree laid out, changed and laid out again must match the same
// changed tree laid out fresh, so cached results never outlive what they depend on.
// Usage: test_layout_fuzz [trees] [first seed]
#include <math.h>
#include <raydom/raydom.h>
#include <stdlib.h>
#include <string.h>

#include "check.h"

enum { MAX_NODES = 256 };

static uint32_t rs;

static uint32_t rnd(uint32_t n) {
  rs = rs * 1664525u + 1013904223u;
  return (rs >> 8) % n;
}

typedef struct fake_text {
  float w, lh;
} fake_text;

// Wraps a run of text, never narrower than 20 units unless the run is.
static rd_size measure(void* data, float w, rd_measure_mode wm, float h, rd_measure_mode hm) {
  (void)h;
  (void)hm;
  const fake_text* t     = data;
  float            width = t->w;
  if (wm == RD_MEASURE_MIN_CONTENT)
    width = fminf(t->w, 20);
  else if (wm != RD_MEASURE_UNDEFINED && w < width)
    width = fmaxf(w, fminf(t->w, 20));
  float lines = width > 0 ? ceilf(t->w / width) : 1;
  return (rd_size){wm == RD_MEASURE_EXACTLY ? w : width, lines * t->lh};
}

static rd_val rval(bool allow_pct, bool allow_auto) {
  switch (rnd(6)) {
    case 0:
    case 1:
      return allow_auto ? rd_auto() : rd_px(0);
    case 2:
      return rd_px((float)rnd(80));
    case 3:
      return allow_pct ? rd_pct((float)rnd(100)) : rd_px((float)rnd(30));
    default:
      return (rd_val){0, RD_UNIT_UNSET};
  }
}

static rd_edges redges(bool allow_auto) { return (rd_edges){rval(true, allow_auto), rval(true, allow_auto), rval(true, allow_auto), rval(true, allow_auto)}; }

static rd_grid_line rline(void) {
  switch (rnd(4)) {
    case 0:
      return rd_line((int16_t)(1 + rnd(4)));
    case 1:
      return rd_span((int16_t)(1 + rnd(2)));
    case 2:
      return rd_line(-1);
    default:
      return (rd_grid_line){0};
  }
}

static const rd_align ALIGNS[] = {RD_ALIGN_START, RD_ALIGN_END, RD_ALIGN_CENTER, RD_ALIGN_STRETCH, RD_ALIGN_BASELINE};

static rd_style rstyle(rd_ctx* ctx, bool leaf) {
  rd_style s = {0};
  if (!leaf && rnd(4) == 0) {
    s.display = RD_DISPLAY_GRID;
    rd_track t[3];
    uint32_t n = 1 + rnd(3);
    for (uint32_t j = 0; j < n; j++) {
      switch (rnd(5)) {
        case 0:
          t[j] = rd_track_of(rd_fr((float)(1 + rnd(2))));
          break;
        case 1:
          t[j] = rd_track_of(rd_px((float)(10 + rnd(50))));
          break;
        case 2:
          t[j] = rd_track_of(rd_auto());
          break;
        case 3:
          t[j] = rd_track_of(rd_min_content());
          break;
        default:
          t[j] = rd_track_of(rd_pct((float)(10 + rnd(40))));
      }
    }
    s.grid_template_columns = rd_tracks_create(ctx, t, n);
    if (rnd(4) == 0) s.grid_auto_flow = RD_GRID_FLOW_COLUMN;
    if (rnd(2)) s.justify_items = ALIGNS[rnd(4)];
  } else {
    s.flex_direction = (rd_flex_direction)(1 + rnd(4));
    if (rnd(3) == 0) s.flex_wrap = rnd(2) ? RD_FLEX_WRAP : RD_FLEX_WRAP_REVERSE;
    s.justify_content = (rd_align)(2 + rnd(9));
  }
  if (rnd(2)) s.align_items = ALIGNS[rnd(5)];
  if (rnd(3) == 0) s.align_self = ALIGNS[rnd(5)];
  if (rnd(3) == 0) s.align_content = (rd_align)(2 + rnd(9));
  if (rnd(2)) s.flex_grow = rd_num((float)rnd(3));
  if (rnd(2)) s.flex_shrink = rd_num((float)rnd(3));
  if (rnd(3) == 0) s.flex_basis = rval(true, true);
  s.width  = rval(true, true);
  s.height = rval(true, true);
  if (rnd(3) == 0) s.min_width = rval(true, true);
  if (rnd(3) == 0) s.max_width = rval(true, true);
  if (rnd(3) == 0) s.min_height = rval(true, true);
  if (rnd(3) == 0) s.max_height = rval(true, true);
  if (rnd(3) == 0) s.padding = rd_edges_all(rval(true, false));
  if (rnd(3) == 0) s.margin = redges(true);
  if (rnd(3) == 0) s.border_width = rd_edges_all(rd_px((float)rnd(4)));
  if (rnd(3) == 0) s.row_gap = rval(true, false);
  if (rnd(3) == 0) s.column_gap = rval(true, false);
  if (rnd(4) == 0) {
    s.grid_row_start    = rline();
    s.grid_column_start = rline();
    if (rnd(2)) s.grid_column_end = rline();
  }
  if (rnd(8) == 0) {
    s.position = RD_POSITION_ABSOLUTE;
    s.inset    = redges(true);
  } else if (rnd(8) == 0) {
    s.position = RD_POSITION_RELATIVE;
    s.inset    = redges(true);
  }
  if (rnd(12) == 0) s.display = RD_DISPLAY_NONE;
  return s;
}

typedef struct tree {
  rd_ctx*   ctx;
  rd_node   n[MAX_NODES];
  int       parent[MAX_NODES];  // -1 for the root's children
  int       kind[MAX_NODES];    // 0 container, 1 measured leaf, 2 text leaf
  rd_style  style[MAX_NODES];
  fake_text text[MAX_NODES];
  int       count;
  rd_style  root_style;
} tree;

static const char* WORDS[] = {"a", "word", "longer words", "x y z", "wrap at the spaces here"};

static void build(tree* t, rd_node parent, int parent_id, int depth) {
  uint32_t kids = depth >= 4 ? 0 : rnd(5);
  for (uint32_t j = 0; j < kids && t->count < MAX_NODES; j++) {
    bool    leaf  = depth >= 3 || rnd(3) == 0;
    int     id    = t->count++;
    rd_node n     = rd_node_create(t->ctx);
    t->n[id]      = n;
    t->parent[id] = parent_id;
    t->style[id]  = rstyle(t->ctx, leaf);
    rd_node_set_style(t->ctx, n, &t->style[id]);
    rd_node_append(t->ctx, parent, n);
    if (!leaf) {
      t->kind[id] = 0;
      build(t, n, id, depth + 1);
    } else if (rnd(4) == 0) {
      t->kind[id] = 2;
      rd_node_set_text(t->ctx, n, WORDS[rnd(5)], RD_TEXT_NUL);
    } else {
      t->kind[id] = 1;
      t->text[id] = (fake_text){(float)(5 + rnd(200)), (float)(8 + rnd(10))};
      rd_node_set_measure(t->ctx, n, measure, &t->text[id], NULL);
    }
  }
}

static void make(tree* t, uint32_t seed, const void* font, size_t font_size) {
  t->ctx = rd_ctx_create();
  if (font) rd_font_load(t->ctx, font, font_size, false);
  rs                     = seed;
  t->root_style          = rstyle(t->ctx, false);
  t->root_style.position = RD_POSITION_UNSET;
  if (t->root_style.display == RD_DISPLAY_NONE) t->root_style.display = RD_DISPLAY_UNSET;
  rd_node_set_style(t->ctx, rd_ctx_root(t->ctx), &t->root_style);
  build(t, rd_ctx_root(t->ctx), -1, 0);
}

// The change applied before the second layout; the same draws give both trees the same change.
static void mutate(tree* t, int id, uint32_t seed) {
  rs         = seed;
  rd_style s = rstyle(t->ctx, false);
  rd_style m = {.width = s.width, .flex_grow = s.flex_grow, .align_items = s.align_items, .padding = s.padding};
  rd_node_merge_style(t->ctx, t->n[id], &m);
  if (m.width.unit) t->style[id].width = m.width;
  if (m.flex_grow.unit) t->style[id].flex_grow = m.flex_grow;
  if (m.align_items) t->style[id].align_items = m.align_items;
  if (m.padding.top.unit) t->style[id].padding = m.padding;
  if (t->kind[id] == 1) {
    t->text[id].w += (float)rnd(60);
    rd_node_mark_layout_dirty(t->ctx, t->n[id]);
  }
}

static void print_val(const char* name, rd_val v) {
  static const char* units[] = {"", "auto", "px", "%", "", "fr", "min-content", "max-content"};
  if (v.unit == RD_UNIT_UNSET) return;
  if (v.unit == RD_UNIT_AUTO || v.unit >= RD_UNIT_MIN_CONTENT)
    fprintf(stderr, " %s=%s", name, units[v.unit]);
  else
    fprintf(stderr, " %s=%g%s", name, v.value, units[v.unit]);
}

static void print_edges(const char* name, rd_edges e) {
  if (!e.top.unit && !e.right.unit && !e.bottom.unit && !e.left.unit) return;
  fprintf(stderr, " %s={", name);
  print_val("t", e.top);
  print_val("r", e.right);
  print_val("b", e.bottom);
  print_val("l", e.left);
  fprintf(stderr, " }");
}

static void print_line(const char* name, rd_grid_line l) {
  if (l.kind == RD_GRID_LINE_INDEX) fprintf(stderr, " %s=line(%d)", name, l.value);
  if (l.kind == RD_GRID_LINE_SPAN) fprintf(stderr, " %s=span(%d)", name, l.value);
}

static void print_style(rd_ctx* ctx, const rd_style* s) {
  if (s->display) fprintf(stderr, " display=%d", s->display);
  if (s->position) fprintf(stderr, " position=%d", s->position);
  if (s->flex_direction) fprintf(stderr, " dir=%d", s->flex_direction);
  if (s->flex_wrap) fprintf(stderr, " wrap=%d", s->flex_wrap);
  if (s->justify_content) fprintf(stderr, " justify=%d", s->justify_content);
  if (s->align_items) fprintf(stderr, " align_items=%d", s->align_items);
  if (s->align_self) fprintf(stderr, " align_self=%d", s->align_self);
  if (s->align_content) fprintf(stderr, " align_content=%d", s->align_content);
  if (s->justify_items) fprintf(stderr, " justify_items=%d", s->justify_items);
  if (s->grid_auto_flow) fprintf(stderr, " flow=%d", s->grid_auto_flow);
  if (s->grid_template_columns.id) {
    uint32_t        n;
    const rd_track* t = rd_tracks_get(ctx, s->grid_template_columns, &n);
    fprintf(stderr, " columns={");
    for (uint32_t j = 0; j < n; j++) {
      print_val("min", t[j].min);
      print_val("max", t[j].max);
      fprintf(stderr, ";");
    }
    fprintf(stderr, " }");
  }
  print_line("row_start", s->grid_row_start);
  print_line("col_start", s->grid_column_start);
  print_line("col_end", s->grid_column_end);
  print_val("grow", s->flex_grow);
  print_val("shrink", s->flex_shrink);
  print_val("basis", s->flex_basis);
  print_val("w", s->width);
  print_val("h", s->height);
  print_val("min_w", s->min_width);
  print_val("max_w", s->max_width);
  print_val("min_h", s->min_height);
  print_val("max_h", s->max_height);
  print_val("row_gap", s->row_gap);
  print_val("col_gap", s->column_gap);
  print_edges("padding", s->padding);
  print_edges("margin", s->margin);
  print_edges("border", s->border_width);
  print_edges("inset", s->inset);
}

static void print_tree(const tree* t) {
  fprintf(stderr, "  root:");
  print_style(t->ctx, &t->root_style);
  fprintf(stderr, "\n");
  for (int j = 0; j < t->count; j++) {
    fprintf(stderr, "  node %d parent %d%s:", j, t->parent[j],
            t->kind[j] == 1 ? " measured" : t->kind[j] == 2 ? " text"
                                                            : "");
    if (t->kind[j] == 1) fprintf(stderr, " text={%g, %g}", t->text[j].w, t->text[j].lh);
    print_style(t->ctx, &t->style[j]);
    fprintf(stderr, "\n");
  }
}

static bool same(rd_rect a, rd_rect b) {
  return fabsf(a.x - b.x) <= 0.01f && fabsf(a.y - b.y) <= 0.01f && fabsf(a.width - b.width) <= 0.01f &&
         fabsf(a.height - b.height) <= 0.01f;
}

static bool finite_rect(rd_rect r) { return isfinite(r.x) && isfinite(r.y) && isfinite(r.width) && isfinite(r.height); }

static void* read_font(size_t* size) {
  FILE* f = fopen(RAYDOM_TEST_FONT, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long  n   = ftell(f);
  void* buf = n > 0 ? malloc((size_t)n) : NULL;
  fseek(f, 0, SEEK_SET);
  if (buf && fread(buf, 1, (size_t)n, f) != (size_t)n) {
    free(buf);
    buf = NULL;
  }
  fclose(f);
  *size = buf ? (size_t)n : 0;
  return buf;
}

int main(int argc, char** argv) {
  uint32_t trees = argc > 1 ? (uint32_t)strtoul(argv[1], NULL, 10) : 1500;
  uint32_t first = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : 0;
  size_t   font_size;
  void*    font = read_font(&font_size);
  CHECK(font != NULL);

  uint32_t failed = 0;
  tree*    inc    = malloc(sizeof *inc);
  tree*    fresh  = malloc(sizeof *fresh);
  for (uint32_t it = first; it < first + trees && inc && fresh; it++) {
    uint32_t seed = it * 2654435761u;
    float    w1 = (float)(50 + (it * 37) % 700), h1 = (float)(50 + (it * 53) % 500);
    float    w2 = (float)(50 + (it * 91) % 700), h2 = (float)(50 + (it * 29) % 500);
    memset(inc, 0, sizeof *inc);
    memset(fresh, 0, sizeof *fresh);
    make(inc, seed, font, font_size);
    uint32_t after = rs;
    make(fresh, seed, font, font_size);

    rd_ctx_layout(inc->ctx, w1, h1);
    rs           = after;
    int      mut = inc->count ? (int)rnd((uint32_t)inc->count) : -1;
    uint32_t ms  = rs;
    if (mut >= 0) {
      mutate(inc, mut, ms);
      mutate(fresh, mut, ms);
    }
    rd_ctx_layout(inc->ctx, w2, h2);
    rd_ctx_layout(fresh->ctx, w2, h2);

    int bad = -1;
    for (int j = 0; j < inc->count && bad < 0; j++) {
      rd_rect a = rd_node_rect(inc->ctx, inc->n[j]), b = rd_node_rect(fresh->ctx, fresh->n[j]);
      if (!same(a, b) || !finite_rect(a)) {
        bad = j;
        fprintf(stderr, "seed %u: node %d incremental {%g, %g, %g, %g}, fresh {%g, %g, %g, %g}\n", it, j, a.x, a.y,
                a.width, a.height, b.x, b.y, b.width, b.height);
      }
    }
    if (bad >= 0) {
      fprintf(stderr, "  laid out at %gx%g, node %d changed, laid out at %gx%g\n", w1, h1, mut, w2, h2);
      if (failed < 3) print_tree(fresh);
      failed++;
    }
    rd_ctx_destroy(inc->ctx);
    rd_ctx_destroy(fresh->ctx);
  }
  CHECK(inc && fresh);
  CHECK(failed == 0);
  if (failed) fprintf(stderr, "%u of %u trees differ\n", failed, trees);
  free(inc);
  free(fresh);
  free(font);
  return TEST_RESULT();
}
