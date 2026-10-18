#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input_internal.h"
#include "raylib.h"
#include "render_internal.h"
#include "rlgl.h"

enum { ATLAS_SIZE = 2048 };

static const char* FS_BODY =
    "uniform vec2 size;\n"
    "uniform vec4 radii;\n"
    "uniform vec4 borders;\n"
    "uniform vec4 fill;\n"
    "uniform vec4 stroke;\n"
    "uniform float blur;\n"
    "uniform float scale;\n"
    "float pick(vec2 p, vec4 r) { return p.x < 0.0 ? (p.y < 0.0 ? r.x : r.w) : (p.y < 0.0 ? r.y : r.z); }\n"
    "float box(vec2 p, vec2 h, float r) {\n"
    "  vec2 q = abs(p) - h + r;\n"
    "  return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;\n"
    "}\n"
    "float sdf(vec2 p, vec2 lo, vec2 hi, vec4 r) {\n"
    "  vec2 h = max((hi - lo) * 0.5, 0.0);\n"
    "  vec2 d = p - (lo + hi) * 0.5;\n"
    "  return box(d, h, min(pick(d, r), min(h.x, h.y)));\n"
    "}\n"
    "void main() {\n"
    "  vec2 p = fragTexCoord;\n"
    "  float outer = sdf(p, vec2(0.0), size, radii);\n"
    "  if (blur > 0.0) {\n"
    "    OUT_COLOR = vec4(fill.rgb, fill.a * (1.0 - smoothstep(-blur, blur, outer)));\n"
    "    return;\n"
    "  }\n"
    "  vec2 lo = vec2(borders.w, borders.x);\n"
    "  vec2 hi = size - vec2(borders.y, borders.z);\n"
    "  vec4 ir = max(radii - vec4(max(borders.w, borders.x), max(borders.x, borders.y), max(borders.y, borders.z),\n"
    "                             max(borders.z, borders.w)), 0.0);\n"
    "  float cov = clamp(0.5 - outer * scale, 0.0, 1.0);\n"
    "  float inside = clamp(0.5 - sdf(p, lo, hi, ir) * scale, 0.0, 1.0);\n"
    "  vec4 c = mix(vec4(stroke.rgb * stroke.a, stroke.a), vec4(fill.rgb * fill.a, fill.a), inside) * cov;\n"
    "  OUT_COLOR = c.a > 0.0 ? vec4(c.rgb / c.a, c.a) : vec4(0.0);\n"
    "}\n";

static const char* FS_330 =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "out vec4 finalColor;\n"
    "#define OUT_COLOR finalColor\n";

static const char* FS_120 =
    "#version 120\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "#define OUT_COLOR gl_FragColor\n";

static const char* FS_100 =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec2 fragTexCoord;\n"
    "varying vec4 fragColor;\n"
    "#define OUT_COLOR gl_FragColor\n";

enum { U_SIZE,
       U_RADII,
       U_BORDERS,
       U_FILL,
       U_STROKE,
       U_BLUR,
       U_SCALE,
       U_COUNT };
static const char* UNIFORMS[U_COUNT] = {"size", "radii", "borders", "fill", "stroke", "blur", "scale"};

typedef struct glyph_slot {
  uint64_t key;  // 0 when empty
  int16_t  x, y, w, h, ox, oy;
} glyph_slot;

typedef struct gpu {
  Shader      shader;
  int         loc[U_COUNT];
  Texture2D   atlas;
  int         shelf_x, shelf_y, shelf_h;
  glyph_slot* glyphs;
  uint32_t    glyph_cap, glyph_count;
  uint8_t*    scratch;
  size_t      scratch_cap;
  bool        in_shader;
} gpu;

static gpu* gpu_get(rd_ctx* ctx) {
  if (ctx->gpu) return ctx->gpu;
  gpu* g = calloc(1, sizeof *g);
  if (!g) return NULL;
  int         v      = rlGetVersion();
  const char* prefix = v == RL_OPENGL_ES_20 ? FS_100 : v == RL_OPENGL_21 ? FS_120
                                                                         : FS_330;
  size_t      plen = strlen(prefix), blen = strlen(FS_BODY);
  char*       src = malloc(plen + blen + 1);
  if (src) {
    snprintf(src, plen + blen + 1, "%s%s", prefix, FS_BODY);
    g->shader = LoadShaderFromMemory(NULL, src);
    free(src);
  }
  for (int j = 0; j < U_COUNT; j++) g->loc[j] = GetShaderLocation(g->shader, UNIFORMS[j]);
  Image img = GenImageColor(ATLAS_SIZE, ATLAS_SIZE, BLANK);
  g->atlas  = LoadTextureFromImage(img);
  UnloadImage(img);
  SetTextureFilter(g->atlas, TEXTURE_FILTER_BILINEAR);
  ctx->gpu = g;
  return g;
}

void rd__render_ctx_free(rd_ctx* ctx) {
  gpu* g = ctx->gpu;
  if (g) {
    if (IsWindowReady()) {
      UnloadShader(g->shader);
      UnloadTexture(g->atlas);
    }
    free(g->glyphs);
    free(g->scratch);
    free(g);
    ctx->gpu = NULL;
  }
  free(ctx->cmds);
  free(ctx->hover_chain.nodes);
  free(ctx->active_chain.nodes);
}

// Glyph atlas.

static uint64_t glyph_key(const rd__font* f, int32_t glyph, float px) {
  return ((uint64_t)rd__font_id(f) << 44) | ((uint64_t)(uint32_t)lroundf(px * 4) << 24) | (uint64_t)(uint32_t)glyph;
}

static glyph_slot* glyph_find(gpu* g, uint64_t key) {
  uint32_t mask = g->glyph_cap - 1;
  for (uint32_t j = (uint32_t)(key * 0x9E3779B97F4A7C15ull >> 32) & mask;; j = (j + 1) & mask)
    if (g->glyphs[j].key == key || g->glyphs[j].key == 0) return &g->glyphs[j];
}

static bool glyph_reserve(gpu* g) {
  if (g->glyph_cap && (g->glyph_count + 1) * 10 < g->glyph_cap * 7) return true;
  uint32_t    cap = g->glyph_cap ? g->glyph_cap * 2 : 1024;
  glyph_slot* old = g->glyphs;
  uint32_t    n   = g->glyph_cap;
  g->glyphs       = calloc(cap, sizeof *g->glyphs);
  if (!g->glyphs) {
    g->glyphs = old;
    return false;
  }
  g->glyph_cap = cap;
  for (uint32_t j = 0; j < n; j++)
    if (old[j].key) *glyph_find(g, old[j].key) = old[j];
  free(old);
  return true;
}

static void atlas_reset(gpu* g) {
  rlDrawRenderBatchActive();
  memset(g->glyphs, 0, g->glyph_cap * sizeof *g->glyphs);
  g->glyph_count = 0;
  g->shelf_x = g->shelf_y = g->shelf_h = 0;
}

static bool atlas_pack(gpu* g, int w, int h, int* x, int* y) {
  if (g->shelf_x + w + 1 > ATLAS_SIZE) {
    g->shelf_y += g->shelf_h + 1;
    g->shelf_x = 0;
    g->shelf_h = 0;
  }
  if (g->shelf_y + h + 1 > ATLAS_SIZE) return false;
  *x = g->shelf_x;
  *y = g->shelf_y;
  g->shelf_x += w + 1;
  if (h > g->shelf_h) g->shelf_h = h;
  return true;
}

static const glyph_slot* glyph_get(gpu* g, rd__font* f, int32_t glyph, float px) {
  if (!glyph_reserve(g)) return NULL;
  uint64_t    key = glyph_key(f, glyph, px);
  glyph_slot* s   = glyph_find(g, key);
  if (s->key) return s;

  float scale = rd__font_scale(f, px);
  int   x0, y0, x1, y1;
  rd__font_glyph_box(f, glyph, scale, &x0, &y0, &x1, &y1);
  int w = x1 - x0, h = y1 - y0, ax = 0, ay = 0;
  if (w > 0 && h > 0) {
    if (w >= ATLAS_SIZE || h >= ATLAS_SIZE) return NULL;
    if (!atlas_pack(g, w, h, &ax, &ay)) {
      atlas_reset(g);
      s = glyph_find(g, key);
      if (!atlas_pack(g, w, h, &ax, &ay)) return NULL;
    }
    size_t need = (size_t)w * (size_t)h * 5;
    if (need > g->scratch_cap) {
      uint8_t* p = realloc(g->scratch, need);
      if (!p) return NULL;
      g->scratch     = p;
      g->scratch_cap = need;
    }
    uint8_t* cov  = g->scratch;
    uint8_t* rgba = g->scratch + (size_t)w * (size_t)h;
    rd__font_glyph_render(f, glyph, scale, cov, w, h, w);
    for (size_t j = 0; j < (size_t)w * (size_t)h; j++) {
      rgba[j * 4] = rgba[j * 4 + 1] = rgba[j * 4 + 2] = 255;
      rgba[j * 4 + 3]                                 = cov[j];
    }
    UpdateTextureRec(g->atlas, (Rectangle){(float)ax, (float)ay, (float)w, (float)h}, rgba);
  }
  *s = (glyph_slot){key, (int16_t)ax, (int16_t)ay, (int16_t)(w > 0 ? w : 0), (int16_t)(h > 0 ? h : 0),
                    (int16_t)x0, (int16_t)y0};
  g->glyph_count++;
  return s;
}

// Drawing.

static Color to_color(rd_color c) { return (Color){c.r, c.g, c.b, c.a}; }

static float snap(float v, float dpi) { return roundf(v * dpi) / dpi; }

static Rectangle snap_rect(rd_rect r, float dpi) {
  float x0 = snap(r.x, dpi), y0 = snap(r.y, dpi);
  return (Rectangle){x0, y0, snap(r.x + r.width, dpi) - x0, snap(r.y + r.height, dpi) - y0};
}

static void shader_mode(gpu* g, bool on) {
  if (on == g->in_shader) return;
  if (on)
    BeginShaderMode(g->shader);
  else
    EndShaderMode();
  g->in_shader = on;
}

static void textured_quad(unsigned int tex, float u0, float v0, float u1, float v1, float x0, float y0, float x1,
                          float y1, Color c) {
  rlCheckRenderBatchLimit(4);
  rlSetTexture(tex);
  rlBegin(RL_QUADS);
  rlColor4ub(c.r, c.g, c.b, c.a);
  rlTexCoord2f(u0, v0);
  rlVertex2f(x0, y0);
  rlTexCoord2f(u0, v1);
  rlVertex2f(x0, y1);
  rlTexCoord2f(u1, v1);
  rlVertex2f(x1, y1);
  rlTexCoord2f(u1, v0);
  rlVertex2f(x1, y0);
  rlEnd();
  rlSetTexture(0);
}

static void set_vec4(gpu* g, int u, float a, float b, float c, float d) {
  float v[4] = {a, b, c, d};
  SetShaderValue(g->shader, g->loc[u], v, SHADER_UNIFORM_VEC4);
}

static void set_color(gpu* g, int u, rd_color c) {
  set_vec4(g, u, (float)c.r / 255.0f, (float)c.g / 255.0f, (float)c.b / 255.0f, (float)c.a / 255.0f);
}

// Local coordinates run from -pad to size + pad over the quad, so the shader sees the box at
// [0, size] with room for antialiasing or blur outside it.
static void sdf_quad(gpu* g, Rectangle r, float pad, const float radii[4], const float border[4], rd_color fill,
                     rd_color stroke, float blur, float dpi) {
  shader_mode(g, true);
  rlDrawRenderBatchActive();
  float size[2] = {r.width, r.height};
  SetShaderValue(g->shader, g->loc[U_SIZE], size, SHADER_UNIFORM_VEC2);
  set_vec4(g, U_RADII, radii[0], radii[1], radii[2], radii[3]);
  set_vec4(g, U_BORDERS, border[0], border[1], border[2], border[3]);
  set_color(g, U_FILL, fill);
  set_color(g, U_STROKE, stroke);
  SetShaderValue(g->shader, g->loc[U_BLUR], &blur, SHADER_UNIFORM_FLOAT);
  SetShaderValue(g->shader, g->loc[U_SCALE], &dpi, SHADER_UNIFORM_FLOAT);
  textured_quad(rlGetTextureIdDefault(), -pad, -pad, r.width + pad, r.height + pad, r.x - pad, r.y - pad,
                r.x + r.width + pad, r.y + r.height + pad, WHITE);
}

static void draw_rect(gpu* g, const rd__cmd* c, float dpi) {
  Rectangle r       = snap_rect(c->rect, dpi);
  bool      rounded = c->radii[0] > 0 || c->radii[1] > 0 || c->radii[2] > 0 || c->radii[3] > 0;
  if (rounded) {
    float border[4];
    for (int j = 0; j < 4; j++) border[j] = c->border[j] > 0 ? fmaxf(snap(c->border[j], dpi), 1 / dpi) : 0;
    sdf_quad(g, r, 1, c->radii, border, c->fill, c->stroke, 0, dpi);
    return;
  }
  shader_mode(g, false);
  if (c->fill.a) DrawRectangleRec(r, to_color(c->fill));
  if (!c->stroke.a) return;
  Color s = to_color(c->stroke);
  float t = snap(c->border[0], dpi), rt = snap(c->border[1], dpi), b = snap(c->border[2], dpi),
        l = snap(c->border[3], dpi);
  if (t > 0) DrawRectangleRec((Rectangle){r.x, r.y, r.width, t}, s);
  if (b > 0) DrawRectangleRec((Rectangle){r.x, r.y + r.height - b, r.width, b}, s);
  if (l > 0) DrawRectangleRec((Rectangle){r.x, r.y + t, l, r.height - t - b}, s);
  if (rt > 0) DrawRectangleRec((Rectangle){r.x + r.width - rt, r.y + t, rt, r.height - t - b}, s);
}

// One unwrapped run of UTF-8, for placeholders.
static void draw_run(gpu* g, rd__font* f, float size, const char* s, size_t len, float x, float base, Color col,
                     float dpi) {
  float   px = roundf(size * dpi * 4) / 4, units = rd__font_scale(f, size), inv = 1 / (float)ATLAS_SIZE;
  int32_t prev = -1;
  for (size_t i = 0; i < len;) {
    int32_t adv, glyph = rd__font_glyph(f, rd__utf8_decode(s, len, &i), &adv);
    if (prev >= 0) x += (float)rd__font_kern(f, prev, glyph) * units;
    const glyph_slot* sl = glyph_get(g, f, glyph, px);
    if (sl && sl->w) {
      float gx = (roundf(x * dpi) + (float)sl->ox) / dpi, gy = (base * dpi + (float)sl->oy) / dpi;
      textured_quad(g->atlas.id, (float)sl->x * inv, (float)sl->y * inv, (float)(sl->x + sl->w) * inv,
                    (float)(sl->y + sl->h) * inv, gx, gy, gx + (float)sl->w / dpi, gy + (float)sl->h / dpi, col);
    }
    x += (float)adv * units;
    prev = glyph;
  }
}

static void draw_placeholder(gpu* g, rd_ctx* ctx, const rd__cmd* c, float dpi) {
  rd__text_layout tl;
  rd__input_view  v;
  if (!rd__text_layout_get(ctx, c->node, c->rect.width, &tl) || !rd__input_view_get(ctx, c->node, &v)) return;
  shader_mode(g, false);
  draw_run(g, tl.font, tl.size, v.placeholder, v.placeholder_len, c->rect.x + c->dx,
           snap(c->rect.y + tl.baseline, dpi), to_color(c->fill), dpi);
}

static void draw_glyph(gpu* g, const rd__text_layout* tl, int32_t glyph, float px, float x, float base, Color col,
                       float dpi) {
  const glyph_slot* s = glyph_get(g, tl->font, glyph, px);
  if (!s || !s->w) return;
  float inv = 1 / (float)ATLAS_SIZE;
  float gx = (roundf(x * dpi) + (float)s->ox) / dpi, gy = (base * dpi + (float)s->oy) / dpi;
  textured_quad(g->atlas.id, (float)s->x * inv, (float)s->y * inv, (float)(s->x + s->w) * inv, (float)(s->y + s->h) * inv,
                gx, gy, gx + (float)s->w / dpi, gy + (float)s->h / dpi, col);
}

// The highlight color of a glyph, if a highlight with a text color covers it.
static Color glyph_color(const rd_text* t, uint32_t byte, Color base, float op) {
  for (size_t j = t ? t->highlight_count : 0; j-- > 0;) {
    const rd_highlight* h = &t->highlights[j];
    if (h->color.a && byte >= h->start && byte < h->end)
      return (Color){h->color.r, h->color.g, h->color.b, (unsigned char)lroundf((float)h->color.a * op)};
  }
  return base;
}

static void draw_text(gpu* g, rd_ctx* ctx, const rd__cmd* c, float dpi) {
  rd__text_layout tl;
  if (!rd__text_layout_get(ctx, c->node, c->rect.width, &tl)) return;
  shader_mode(g, false);
  const rd_style* st    = &ctx->styles[c->node].computed;
  const rd_text*  t     = rd__text_get(ctx, c->node);
  float           px    = roundf(tl.size * dpi * 4) / 4;
  Color           col   = to_color(c->fill);
  float           op    = st->color.a ? (float)c->fill.a / (float)st->color.a : 1;
  uint32_t        shown = rd__text_visible_lines(st, &tl);

  // Ellipsis: U+2026, or three periods when the font lacks it.
  bool        ellipsis = st->text_overflow == RD_TEXT_OVERFLOW_ELLIPSIS;
  int32_t     dot_adv, dots = rd__font_glyph(tl.font, 0x2026, &dot_adv);
  const char* ell   = dots ? "\xe2\x80\xa6" : "...";
  float       ell_w = ellipsis ? rd_font_measure(ctx, st->font, tl.size, ell, RD_TEXT_NUL) : 0;

  for (uint32_t l = 0; l < shown; l++) {
    const rd__line* ln    = &tl.lines[l];
    float           base  = snap(c->rect.y + (float)l * tl.line_height + tl.baseline, dpi);
    bool            more  = l + 1 == shown && shown < tl.line_count;
    bool            cut   = ellipsis && (ln->width > c->rect.width + 0.01f || more);
    float           x     = c->rect.x + c->dx + (cut ? 0 : rd__text_align_offset(st, c->rect.width, ln->width));
    float           limit = c->rect.x + c->dx + c->rect.width - ell_w;
    bool            done  = false;
    for (uint32_t k = ln->tok_start; k < ln->tok_end && !done; k++) {
      const rd__token* tok = &tl.tokens[k];
      if (tok->kind != RD_TOKEN_WORD) {
        x += tok->kind == RD_TOKEN_SPACE ? tok->width : 0;
        continue;
      }
      int32_t prev = -1;
      for (uint32_t j = tok->start; j < tok->end; j++) {
        int32_t adv, glyph = rd__font_glyph(tl.font, tl.cps[j], &adv);
        if (prev >= 0) x += (float)rd__font_kern(tl.font, prev, glyph) * tl.units;
        if (cut && x + (float)adv * tl.units > limit) {
          done = true;
          break;
        }
        draw_glyph(g, &tl, glyph, px, x, base, glyph_color(t, tl.offsets[j], col, op), dpi);
        x += (float)adv * tl.units;
        prev = glyph;
      }
    }
    if (cut && (done || more)) draw_run(g, tl.font, tl.size, ell, strlen(ell), x, base, col, dpi);
  }
}

static void scissor(rd_rect r) {
  int x0 = (int)floorf(r.x), y0 = (int)floorf(r.y);
  int x1 = (int)ceilf(r.x + r.width), y1 = (int)ceilf(r.y + r.height);
  BeginScissorMode(x0, y0, x1 > x0 ? x1 - x0 : 0, y1 > y0 ? y1 - y0 : 0);
}

void rd_paint_scissor(const rd_paint_info* info) {
  rd_rect c = info->content, k = info->clip;
  float   x0 = fmaxf(c.x, k.x), y0 = fmaxf(c.y, k.y);
  float   x1 = fminf(c.x + c.width, k.x + k.width), y1 = fminf(c.y + c.height, k.y + k.height);
  scissor((rd_rect){x0, y0, fmaxf(0, x1 - x0), fmaxf(0, y1 - y0)});
}

void rd_draw(rd_ctx* ctx) {
  if (!ctx) return;
  gpu* g = gpu_get(ctx);
  if (!g) return;
  int   sw  = GetScreenWidth();
  float dpi = sw > 0 ? (float)GetRenderWidth() / (float)sw : 1;
  if (dpi <= 0) dpi = 1;

  uint32_t       n;
  const rd__cmd* cmds    = rd__display_list(ctx, &n);
  bool           clipped = false;
  rd_rect        clip    = {0};
  for (uint32_t j = 0; j < n; j++) {
    const rd__cmd* c = &cmds[j];
    switch (c->kind) {
      case RD_CMD_SHADOW:
        sdf_quad(g, snap_rect(c->rect, dpi), c->blur + 1, c->radii, (float[4]){0}, c->fill, c->fill,
                 fmaxf(c->blur, 0.5f / dpi), dpi);
        break;
      case RD_CMD_RECT:
        draw_rect(g, c, dpi);
        break;
      case RD_CMD_TEXT:
        draw_text(g, ctx, c, dpi);
        break;
      case RD_CMD_PLACEHOLDER:
        draw_placeholder(g, ctx, c, dpi);
        break;
      case RD_CMD_FILL: {
        shader_mode(g, false);
        Rectangle r = snap_rect(c->rect, dpi);
        if (r.width < 1 / dpi) r.width = 1 / dpi;
        DrawRectangleRec(r, to_color(c->fill));
        break;
      }
      case RD_CMD_CLIP:
        scissor(c->rect);
        clip    = c->rect;
        clipped = true;
        break;
      case RD_CMD_PAINT: {
        rd_node_layout* L = &ctx->layouts[c->node];
        if (!L->paint_fn) break;
        shader_mode(g, false);
        rlDrawRenderBatchActive();
        if (clipped) EndScissorMode();
        rd_paint_info info = {rd__handle(ctx, c->node), c->rect, c->clip, dpi};
        L->paint_fn(ctx, &info, L->paint_data);
        rlDrawRenderBatchActive();
        EndScissorMode();
        if (clipped) scissor(clip);
        break;
      }
      default:
        break;
    }
  }
  shader_mode(g, false);
  if (clipped) EndScissorMode();
  ctx->paint_dirty = false;
}

static const struct {
  int    raylib;
  rd_key key;
} KEYS[] = {
    {KEY_TAB, RD_KEY_TAB},
    {KEY_ENTER, RD_KEY_ENTER},
    {KEY_KP_ENTER, RD_KEY_ENTER},
    {KEY_ESCAPE, RD_KEY_ESCAPE},
    {KEY_BACKSPACE, RD_KEY_BACKSPACE},
    {KEY_DELETE, RD_KEY_DELETE},
    {KEY_INSERT, RD_KEY_INSERT},
    {KEY_LEFT, RD_KEY_LEFT},
    {KEY_RIGHT, RD_KEY_RIGHT},
    {KEY_UP, RD_KEY_UP},
    {KEY_DOWN, RD_KEY_DOWN},
    {KEY_HOME, RD_KEY_HOME},
    {KEY_END, RD_KEY_END},
    {KEY_PAGE_UP, RD_KEY_PAGE_UP},
    {KEY_PAGE_DOWN, RD_KEY_PAGE_DOWN},
    {KEY_SPACE, RD_KEY_SPACE},
};

static const char* clipboard_get(void* data) {
  (void)data;
  return GetClipboardText();
}

static void clipboard_set(void* data, const char* text) {
  (void)data;
  SetClipboardText(text);
}

static void feed_key(rd_ctx* ctx, int raylib, rd_key key, uint8_t mods) {
  if (IsKeyPressed(raylib))
    rd_ctx_key(ctx, key, true, false, mods);
  else if (IsKeyPressedRepeat(raylib))
    rd_ctx_key(ctx, key, true, true, mods);
  if (IsKeyReleased(raylib)) rd_ctx_key(ctx, key, false, false, mods);
}

static void feed_keyboard(rd_ctx* ctx) {
  if (!ctx->clip_get) rd_ctx_set_clipboard(ctx, clipboard_get, clipboard_set, NULL);
  uint8_t mods = (uint8_t)((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? RD_MOD_SHIFT : 0) |
                           (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ? RD_MOD_CTRL : 0) |
                           (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT) ? RD_MOD_ALT : 0) |
                           (IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER) ? RD_MOD_SUPER : 0));
  ctx->mods    = mods;
  for (size_t j = 0; j < sizeof KEYS / sizeof KEYS[0]; j++) feed_key(ctx, KEYS[j].raylib, KEYS[j].key, mods);
  for (int j = 0; j < 26; j++) feed_key(ctx, KEY_A + j, (rd_key)(RD_KEY_A + j), mods);
  for (int j = 0; j < 10; j++) feed_key(ctx, KEY_ZERO + j, (rd_key)(RD_KEY_0 + j), mods);
  for (int j = 0; j < 12; j++) feed_key(ctx, KEY_F1 + j, (rd_key)(RD_KEY_F1 + j), mods);
  for (int cp = GetCharPressed(); cp; cp = GetCharPressed()) rd_ctx_text(ctx, (uint32_t)cp);
}

void rd_update(rd_ctx* ctx) {
  if (!ctx) return;
  rd_ctx_set_time(ctx, GetTime());
  feed_keyboard(ctx);
  Vector2 m = GetMousePosition(), w = GetMouseWheelMoveV();
  uint8_t buttons = (IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? RD_BUTTON_PRIMARY : 0) |
                    (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? RD_BUTTON_SECONDARY : 0) |
                    (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) ? RD_BUTTON_MIDDLE : 0);
  rd_ctx_pointer(ctx, m.x, m.y, buttons, w.x, w.y);
  rd_ctx_layout(ctx, (float)GetScreenWidth(), (float)GetScreenHeight());
}
