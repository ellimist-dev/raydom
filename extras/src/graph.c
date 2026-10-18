#include <math.h>
#include <raydom/extras/graph.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"
#include "rlgl.h"

// The scene renders at twice the device resolution and is filtered down when composited.
enum { SUPERSAMPLE = 2 };

static const float DEFAULT_RADIUS = 20, DEFAULT_EDGE_WIDTH = 2, DEFAULT_MESSAGE_RADIUS = 6;
static const float MIN_ZOOM = 0.1f, MAX_ZOOM = 10, CLICK_SLOP = 4, LABEL_GAP = 4;

typedef struct gnode {
  uint32_t gen;
  bool     alive;
  rd_vec2  pos;
  float    radius;
  rd_color color;
  rd_node  label;  // wrapper; its first child is the text
} gnode;

typedef struct gedge {
  uint32_t gen;
  bool     alive;
  rdx_node from, to;
  rd_color color;
  float    width;
  bool     directed;
  rd_node  label;
} gedge;

typedef enum drag_mode { MODE_NONE,
                         MODE_PAN,
                         MODE_DRAG } drag_mode;

struct rdx_graph {
  rd_ctx*  ctx;
  rd_node  host, origin;
  uint32_t flags;
  rd_color background;
  rd_class label_class, node_label_wrap, edge_label_wrap;

  gnode*   nodes;  // index 0 unused
  uint32_t node_len, node_cap;
  gedge*   edges;
  uint32_t edge_len, edge_cap;

  rdx_message* messages;
  size_t       message_count;

  rd_vec2 center;
  float   zoom;
  float   fit_padding;
  bool    fit_pending;

  rdx_node  hover, selected, press;
  drag_mode mode;
  float     press_x, press_y;
  rd_vec2   start;
  bool      moved;

  rdx_graph_fn  fn;
  void*         fn_data;
  rd_destroy_fn fn_destroy;

  bool            gpu;
  Shader          shader;
  Model           sphere;
  int             light_loc;
  RenderTexture2D target;
  int             target_w, target_h;
};

static const char* VS_330 =
    "#version 330\n"
    "in vec3 vertexPosition;\n"
    "in vec3 vertexNormal;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matNormal;\n"
    "out vec3 fragNormal;\n"
    "void main() {\n"
    "  fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));\n"
    "  gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* FS_330_HEAD =
    "#version 330\n"
    "in vec3 fragNormal;\n"
    "out vec4 finalColor;\n"
    "#define OUT_COLOR finalColor\n";

static const char* VS_100 =
    "#version 100\n"
    "attribute vec3 vertexPosition;\n"
    "attribute vec3 vertexNormal;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 matNormal;\n"
    "varying vec3 fragNormal;\n"
    "void main() {\n"
    "  fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));\n"
    "  gl_Position = mvp * vec4(vertexPosition, 1.0);\n"
    "}\n";

static const char* FS_100_HEAD =
    "#version 100\n"
    "precision mediump float;\n"
    "varying vec3 fragNormal;\n"
    "#define OUT_COLOR gl_FragColor\n";

// Diffuse from one light, an ambient floor, a soft rim and a small highlight.
static const char* FS_BODY =
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 lightDir;\n"
    "void main() {\n"
    "  vec3 n = normalize(fragNormal);\n"
    "  vec3 l = normalize(lightDir);\n"
    "  float diffuse = max(dot(n, l), 0.0);\n"
    "  float rim = pow(1.0 - max(n.z, 0.0), 3.0) * 0.25;\n"
    "  float spec = pow(max(dot(n, normalize(l + vec3(0.0, 0.0, 1.0))), 0.0), 48.0) * 0.35;\n"
    "  vec3 c = colDiffuse.rgb * (0.38 + 0.62 * diffuse) + vec3(rim + spec);\n"
    "  OUT_COLOR = vec4(min(c, vec3(1.0)), colDiffuse.a);\n"
    "}\n";

// Handles.

static gnode* node_get(const rdx_graph* g, rdx_node n) {
  if (!g || n.index == 0 || n.index >= g->node_len) return NULL;
  gnode* p = &g->nodes[n.index];
  return p->alive && p->gen == n.gen ? p : NULL;
}

static gedge* edge_get(const rdx_graph* g, rdx_edge e) {
  if (!g || e.index >= g->edge_len) return NULL;
  gedge* p = &g->edges[e.index];
  return p->alive && p->gen == e.gen ? p : NULL;
}

static bool grow(void** items, uint32_t* cap, uint32_t need, size_t size) {
  if (need <= *cap) return true;
  uint32_t n = *cap ? *cap * 2 : 16;
  while (n < need) n *= 2;
  void* p = realloc(*items, n * size);
  if (!p) return false;
  memset((char*)p + *cap * size, 0, (n - *cap) * size);
  *items = p;
  *cap   = n;
  return true;
}

static void emit(rdx_graph* g, rdx_graph_event ev, rdx_node node) {
  if (g->fn) g->fn(g, ev, node, g->fn_data);
}

// Geometry.

static rd_rect host_content(const rdx_graph* g) {
  rd_rect r = rd_node_bounds(g->ctx, g->host);
  rd_box  b = rd_node_box(g->ctx, g->host);
  return (rd_rect){r.x + b.border.left + b.padding.left, r.y + b.border.top + b.padding.top,
                   r.width - b.border.left - b.border.right - b.padding.left - b.padding.right,
                   r.height - b.border.top - b.border.bottom - b.padding.top - b.padding.bottom};
}

static rd_vec2 to_world(const rdx_graph* g, float x, float y) {
  rd_rect c = host_content(g);
  return (rd_vec2){g->center.x + (x - c.x - c.width / 2) / g->zoom, g->center.y + (y - c.y - c.height / 2) / g->zoom};
}

static Vector3 to_3d(rd_vec2 p, float z) { return (Vector3){p.x, -p.y, z}; }

// The visible part of an edge: from one sphere's surface to the other's.
static bool edge_span(const rdx_graph* g, const gedge* e, rd_vec2* a, rd_vec2* b) {
  const gnode *na = node_get(g, e->from), *nb = node_get(g, e->to);
  if (!na || !nb) return false;
  float dx = nb->pos.x - na->pos.x, dy = nb->pos.y - na->pos.y, len = sqrtf(dx * dx + dy * dy);
  if (len <= na->radius + nb->radius) return false;
  dx /= len;
  dy /= len;
  *a = (rd_vec2){na->pos.x + dx * na->radius, na->pos.y + dy * na->radius};
  *b = (rd_vec2){nb->pos.x - dx * nb->radius, nb->pos.y - dy * nb->radius};
  return true;
}

// Labels: text nodes under an origin pinned to the canvas center, offset by world distance times zoom.

static rd_node make_label(rdx_graph* g, const char* text, rd_class wrap) {
  rd_node w = rd_node_create(g->ctx), t = rd_node_create(g->ctx);
  rd_node_add_class(g->ctx, w, wrap);
  rd_node_add_class(g->ctx, t, g->label_class);
  rd_node_set_text(g->ctx, t, text, RD_TEXT_NUL);
  rd_node_append(g->ctx, w, t);
  rd_node_append(g->ctx, g->origin, w);
  return w;
}

static void set_label(rdx_graph* g, rd_node* slot, const char* text, rd_class wrap) {
  if (!text) {
    rd_node_destroy(g->ctx, *slot);
    *slot = (rd_node){0};
  } else if (rd_node_alive(g->ctx, *slot)) {
    rd_node_set_text(g->ctx, rd_node_first_child(g->ctx, *slot), text, RD_TEXT_NUL);
  } else {
    *slot = make_label(g, text, wrap);
  }
}

static void place(rdx_graph* g, rd_node label, float wx, float wy, float below) {
  if (!rd_node_alive(g->ctx, label)) return;
  rd_node_merge_style(g->ctx, label,
                      &(rd_style){.inset = {.left = rd_px((wx - g->center.x) * g->zoom),
                                            .top  = rd_px((wy - g->center.y) * g->zoom + below)}});
}

static void sync_labels(rdx_graph* g) {
  for (uint32_t i = 1; i < g->node_len; i++) {
    gnode* n = &g->nodes[i];
    if (n->alive) place(g, n->label, n->pos.x, n->pos.y, n->radius * g->zoom + LABEL_GAP);
  }
  for (uint32_t i = 0; i < g->edge_len; i++) {
    gedge*  e = &g->edges[i];
    rd_vec2 a, b;
    if (e->alive && edge_span(g, e, &a, &b)) place(g, e->label, (a.x + b.x) / 2, (a.y + b.y) / 2, 0);
  }
}

static void apply_fit(rdx_graph* g, float width, float height) {
  float x0 = INFINITY, y0 = INFINITY, x1 = -INFINITY, y1 = -INFINITY;
  for (uint32_t i = 1; i < g->node_len; i++) {
    const gnode* n = &g->nodes[i];
    if (!n->alive) continue;
    x0 = fminf(x0, n->pos.x - n->radius);
    y0 = fminf(y0, n->pos.y - n->radius);
    x1 = fmaxf(x1, n->pos.x + n->radius);
    y1 = fmaxf(y1, n->pos.y + n->radius);
  }
  g->fit_pending = false;
  if (x0 > x1) return;
  float w = fmaxf(1, width - 2 * g->fit_padding), h = fmaxf(1, height - 2 * g->fit_padding);
  g->center = (rd_vec2){(x0 + x1) / 2, (y0 + y1) / 2};
  g->zoom   = fminf(fmaxf(fminf(w / fmaxf(x1 - x0, 1), h / fmaxf(y1 - y0, 1)), MIN_ZOOM), MAX_ZOOM);
  sync_labels(g);
}

// Interaction.

rdx_node rdx_graph_node_at(const rdx_graph* g, float x, float y) {
  if (!g) return (rdx_node){0};
  rd_vec2 w = to_world(g, x, y);
  for (uint32_t i = g->node_len; i-- > 1;) {
    const gnode* n  = &g->nodes[i];
    float        dx = w.x - n->pos.x, dy = w.y - n->pos.y, r = n->radius + 2 / g->zoom;
    if (n->alive && dx * dx + dy * dy <= r * r) return (rdx_node){i, n->gen};
  }
  return (rdx_node){0};
}

static bool same(rdx_node a, rdx_node b) { return a.index == b.index && a.gen == b.gen; }

static void hover(rdx_graph* g, rdx_node n) {
  if (same(n, g->hover)) return;
  rdx_node old = g->hover;
  g->hover     = n;
  if (node_get(g, old)) emit(g, RDX_EVENT_NODE_LEAVE, old);
  if (node_get(g, n)) emit(g, RDX_EVENT_NODE_ENTER, n);
}

static void on_down(rd_ctx* ctx, rd_event* ev, void* data) {
  rdx_graph* g = data;
  if (ev->button != RD_BUTTON_PRIMARY) return;
  rdx_node n = rdx_graph_node_at(g, ev->x, ev->y);
  g->press   = n;
  g->press_x = ev->x;
  g->press_y = ev->y;
  g->moved   = false;
  if (node_get(g, n) && (g->flags & RDX_GRAPH_DRAG_NODES)) {
    g->mode  = MODE_DRAG;
    g->start = g->nodes[n.index].pos;
  } else {
    g->mode  = MODE_PAN;
    g->start = g->center;
  }
  rd_node_set_pointer_capture(ctx, g->host);
}

static void on_move(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  rdx_graph* g = data;
  if (g->mode == MODE_NONE || !(ev->buttons & RD_BUTTON_PRIMARY)) {
    hover(g, rdx_graph_node_at(g, ev->x, ev->y));
    return;
  }
  float dx = ev->x - g->press_x, dy = ev->y - g->press_y;
  if (fabsf(dx) > CLICK_SLOP || fabsf(dy) > CLICK_SLOP) g->moved = true;
  if (!g->moved) return;
  if (g->mode == MODE_PAN) {
    g->center = (rd_vec2){g->start.x - dx / g->zoom, g->start.y - dy / g->zoom};
    sync_labels(g);
    emit(g, RDX_EVENT_VIEW, (rdx_node){0});
  } else if (node_get(g, g->press)) {
    rdx_graph_set_position(g, g->press, (rd_vec2){g->start.x + dx / g->zoom, g->start.y + dy / g->zoom});
    emit(g, RDX_EVENT_NODE_DRAG, g->press);
  }
}

static void on_up(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  rdx_graph* g = data;
  if (ev->button != RD_BUTTON_PRIMARY) return;
  if (!g->moved && node_get(g, g->press) && same(rdx_graph_node_at(g, ev->x, ev->y), g->press))
    emit(g, RDX_EVENT_NODE_CLICK, g->press);
  g->mode  = MODE_NONE;
  g->press = (rdx_node){0};
}

static void on_leave(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)ev;
  hover(data, (rdx_node){0});
}

// The wheel zooms around the point under the cursor.
static void on_wheel(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  rdx_graph* g = data;
  rd_event_prevent_default(ev);
  if (ev->wheel_y == 0) return;
  rd_vec2 before = to_world(g, ev->x, ev->y);
  g->zoom        = fminf(fmaxf(g->zoom * expf(ev->wheel_y * 0.12f), MIN_ZOOM), MAX_ZOOM);
  rd_vec2 after  = to_world(g, ev->x, ev->y);
  g->center.x += before.x - after.x;
  g->center.y += before.y - after.y;
  sync_labels(g);
  emit(g, RDX_EVENT_VIEW, (rdx_node){0});
}

// Drawing.

static void gpu_init(rdx_graph* g) {
  bool        es   = rlGetVersion() == RL_OPENGL_ES_20;
  const char* head = es ? FS_100_HEAD : FS_330_HEAD;
  size_t      n    = strlen(head) + strlen(FS_BODY) + 1;
  char*       fs   = malloc(n);
  if (fs) {
    snprintf(fs, n, "%s%s", head, FS_BODY);
    g->shader = LoadShaderFromMemory(es ? VS_100 : VS_330, fs);
    free(fs);
  }
  g->light_loc                  = GetShaderLocation(g->shader, "lightDir");
  g->sphere                     = LoadModelFromMesh(GenMeshSphere(1, 20, 28));
  g->sphere.materials[0].shader = g->shader;
  g->gpu                        = true;
}

static Color color(rd_color c) { return (Color){c.r, c.g, c.b, c.a}; }

static void draw_sphere(rdx_graph* g, rd_vec2 p, float z, float r, rd_color c) {
  DrawModelEx(g->sphere, to_3d(p, z), (Vector3){0, 1, 0}, 0, (Vector3){r, r, r}, color(c));
}

static void draw_disc(rd_vec2 p, float z, float r, Color c) {
  DrawCylinderEx(to_3d(p, z - 0.1f), to_3d(p, z), r, r, 48, c);
}

static void draw_grid(const rdx_graph* g, rd_rect c) {
  float step = 50;
  while (step * g->zoom < 24) step *= 2;
  while (step * g->zoom > 96) step /= 2;
  float hw = c.width / 2 / g->zoom, hh = c.height / 2 / g->zoom;
  float x0 = floorf((g->center.x - hw) / step) * step, y0 = floorf((g->center.y - hh) / step) * step;
  Color line = {255, 255, 255, 14};
  int   nx = (int)((g->center.x + hw - x0) / step), ny = (int)((g->center.y + hh - y0) / step);
  for (int k = 0; k <= nx; k++) {
    float x = x0 + (float)k * step;
    DrawLine3D(to_3d((rd_vec2){x, g->center.y - hh}, -5), to_3d((rd_vec2){x, g->center.y + hh}, -5), line);
  }
  for (int k = 0; k <= ny; k++) {
    float y = y0 + (float)k * step;
    DrawLine3D(to_3d((rd_vec2){g->center.x - hw, y}, -5), to_3d((rd_vec2){g->center.x + hw, y}, -5), line);
  }
}

static void draw_edges(const rdx_graph* g) {
  for (uint32_t i = 0; i < g->edge_len; i++) {
    const gedge* e = &g->edges[i];
    rd_vec2      a, b;
    if (!e->alive || !edge_span(g, e, &a, &b)) continue;
    float r = e->width / 2 / g->zoom;
    if (e->directed) {
      float   head = 10 / g->zoom, dx = b.x - a.x, dy = b.y - a.y, len = sqrtf(dx * dx + dy * dy);
      rd_vec2 base = {b.x - dx / len * head, b.y - dy / len * head};
      DrawCylinderEx(to_3d(a, -2), to_3d(base, -2), r, r, 8, color(e->color));
      DrawCylinderEx(to_3d(base, -2), to_3d(b, -2), 4.5f / g->zoom, 0, 16, color(e->color));
    } else {
      DrawCylinderEx(to_3d(a, -2), to_3d(b, -2), r, r, 8, color(e->color));
    }
  }
}

static void draw_nodes(rdx_graph* g) {
  float light[3] = {-0.45f, 0.55f, 0.7f};
  SetShaderValue(g->shader, g->light_loc, light, SHADER_UNIFORM_VEC3);
  for (uint32_t i = 1; i < g->node_len; i++) {
    gnode* n = &g->nodes[i];
    if (!n->alive) continue;
    rdx_node id = {i, n->gen};
    if (same(id, g->selected))
      draw_disc(n->pos, -1, n->radius + 4 / g->zoom, (Color){122, 162, 255, 255});
    else if (same(id, g->hover))
      draw_disc(n->pos, -1, n->radius + 3 / g->zoom, (Color){255, 255, 255, 70});
    draw_sphere(g, n->pos, 0, n->radius, n->color);
  }
  for (size_t j = 0; j < g->message_count; j++) {
    const rdx_message* m  = &g->messages[j];
    const gnode *      na = node_get(g, m->from), *nb = node_get(g, m->to);
    if (!na || !nb) continue;
    gedge   span = {.from = m->from, .to = m->to};
    rd_vec2 a, b;
    if (!edge_span(g, &span, &a, &b)) continue;
    float   t = fminf(fmaxf(m->progress, 0), 1);
    rd_vec2 p = {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
    draw_sphere(g, p, 2, m->radius > 0 ? m->radius : DEFAULT_MESSAGE_RADIUS, m->color);
  }
}

static void paint(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)ctx;
  rdx_graph* g = data;
  rd_rect    c = info->content;
  if (c.width < 1 || c.height < 1) return;
  if (g->fit_pending) apply_fit(g, c.width, c.height);
  if (!g->gpu) gpu_init(g);

  int w = (int)ceilf(c.width * info->dpi * SUPERSAMPLE), h = (int)ceilf(c.height * info->dpi * SUPERSAMPLE);
  if (w != g->target_w || h != g->target_h) {
    if (g->target_w) UnloadRenderTexture(g->target);
    g->target   = LoadRenderTexture(w, h);
    g->target_w = w;
    g->target_h = h;
    SetTextureFilter(g->target.texture, TEXTURE_FILTER_BILINEAR);
  }

  BeginTextureMode(g->target);
  ClearBackground(color(g->background));
  Camera3D cam = {
      .position   = to_3d(g->center, 500),
      .target     = to_3d(g->center, 0),
      .up         = {0, 1, 0},
      .fovy       = c.height / g->zoom,
      .projection = CAMERA_ORTHOGRAPHIC,
  };
  BeginMode3D(cam);
  if (!(g->flags & RDX_GRAPH_NO_GRID)) draw_grid(g, c);
  draw_edges(g);
  draw_nodes(g);
  EndMode3D();
  EndTextureMode();

  rd_paint_scissor(info);
  DrawTexturePro(g->target.texture, (Rectangle){0, 0, (float)w, (float)-h}, (Rectangle){c.x, c.y, c.width, c.height},
                 (Vector2){0, 0}, 0, WHITE);
}

static void graph_free(void* data) {
  rdx_graph* g = data;
  if (g->gpu && IsWindowReady()) {
    UnloadModel(g->sphere);  // also unloads its material's shader
    if (g->target_w) UnloadRenderTexture(g->target);
  }
  if (g->fn_destroy) g->fn_destroy(g->fn_data);
  free(g->nodes);
  free(g->edges);
  free(g->messages);
  free(g);
}

// Public API.

rdx_graph* rdx_graph_create(rd_ctx* ctx, rd_node host) {
  if (!rd_node_alive(ctx, host)) return NULL;
  rdx_graph* g = calloc(1, sizeof *g);
  if (!g) return NULL;
  g->ctx         = ctx;
  g->host        = host;
  g->zoom        = 1;
  g->label_class = rd_class_create(ctx, &(rd_style){.font_size = rd_px(12), .white_space = RD_WHITE_SPACE_NOWRAP});
  rd_style wrap  = {
       .position       = RD_POSITION_ABSOLUTE,
       .width          = rd_px(0),
       .height         = rd_px(0),
       .align_items    = RD_ALIGN_CENTER,
       .pointer_events = RD_POINTER_EVENTS_NONE,
  };
  g->node_label_wrap   = rd_class_create(ctx, &wrap);
  wrap.justify_content = RD_ALIGN_CENTER;
  g->edge_label_wrap   = rd_class_create(ctx, &wrap);
  g->origin            = rd_node_create(ctx);
  rd_node_set_style(ctx, g->origin,
                    &(rd_style){.position       = RD_POSITION_ABSOLUTE,
                                .inset          = {.left = rd_pct(50), .top = rd_pct(50)},
                                .width          = rd_px(0),
                                .height         = rd_px(0),
                                .pointer_events = RD_POINTER_EVENTS_NONE});
  rd_node_append(ctx, host, g->origin);
  rd_node_merge_style(ctx, host, &(rd_style){.overflow_x = RD_OVERFLOW_HIDDEN, .overflow_y = RD_OVERFLOW_HIDDEN});

  if (!rd_node_set_paint(ctx, host, paint, g, graph_free)) {
    free(g);
    return NULL;
  }
  rd_node_on(ctx, host, RD_EVENT_POINTER_DOWN, on_down, g, NULL, 0);
  rd_node_on(ctx, host, RD_EVENT_POINTER_MOVE, on_move, g, NULL, 0);
  rd_node_on(ctx, host, RD_EVENT_POINTER_UP, on_up, g, NULL, 0);
  rd_node_on(ctx, host, RD_EVENT_POINTER_LEAVE, on_leave, g, NULL, 0);
  rd_node_on(ctx, host, RD_EVENT_WHEEL, on_wheel, g, NULL, 0);
  return g;
}

rd_node  rdx_graph_host(const rdx_graph* g) { return g ? g->host : (rd_node){0}; }
rd_ctx*  rdx_graph_ctx(const rdx_graph* g) { return g ? g->ctx : NULL; }
rd_class rdx_graph_label_class(const rdx_graph* g) { return g ? g->label_class : (rd_class){0}; }

void rdx_graph_set_flags(rdx_graph* g, uint32_t flags) {
  if (g) g->flags = flags;
}

void rdx_graph_set_background(rdx_graph* g, rd_color c) {
  if (g) g->background = c;
}

void rdx_graph_on(rdx_graph* g, rdx_graph_fn fn, void* data, rd_destroy_fn destroy) {
  if (!g) return;
  if (g->fn_destroy && g->fn_data != data) g->fn_destroy(g->fn_data);
  g->fn         = fn;
  g->fn_data    = data;
  g->fn_destroy = destroy;
}

rdx_node rdx_graph_add_node(rdx_graph* g, const rdx_node_desc* d) {
  if (!g || !d) return (rdx_node){0};
  uint32_t i = 1;
  while (i < g->node_len && g->nodes[i].alive) i++;
  if (i >= g->node_len) {
    if (!grow((void**)&g->nodes, &g->node_cap, i + 1, sizeof *g->nodes)) return (rdx_node){0};
    g->node_len = i + 1;
  }
  gnode* n  = &g->nodes[i];
  *n        = (gnode){.gen = n->gen + 1, .alive = true, .pos = d->position, .color = d->color};
  n->radius = d->radius > 0 ? d->radius : DEFAULT_RADIUS;
  if (d->label) n->label = make_label(g, d->label, g->node_label_wrap);
  sync_labels(g);
  return (rdx_node){i, n->gen};
}

bool rdx_graph_node_alive(const rdx_graph* g, rdx_node node) { return node_get(g, node) != NULL; }

bool rdx_graph_remove_node(rdx_graph* g, rdx_node node) {
  gnode* n = node_get(g, node);
  if (!n) return false;
  for (uint32_t i = 0; i < g->edge_len; i++) {
    gedge* e = &g->edges[i];
    if (e->alive && (same(e->from, node) || same(e->to, node))) rdx_graph_remove_edge(g, (rdx_edge){i, e->gen});
  }
  rd_node_destroy(g->ctx, n->label);
  n->alive = false;
  n->label = (rd_node){0};
  return true;
}

bool rdx_graph_set_position(rdx_graph* g, rdx_node node, rd_vec2 p) {
  gnode* n = node_get(g, node);
  if (!n) return false;
  n->pos = p;
  sync_labels(g);
  return true;
}

rd_vec2 rdx_graph_position(const rdx_graph* g, rdx_node node) {
  gnode* n = node_get(g, node);
  return n ? n->pos : (rd_vec2){0, 0};
}

bool rdx_graph_set_color(rdx_graph* g, rdx_node node, rd_color c) {
  gnode* n = node_get(g, node);
  if (n) n->color = c;
  return n;
}

bool rdx_graph_set_radius(rdx_graph* g, rdx_node node, float r) {
  gnode* n = node_get(g, node);
  if (!n) return false;
  n->radius = r > 0 ? r : DEFAULT_RADIUS;
  sync_labels(g);
  return true;
}

bool rdx_graph_set_label(rdx_graph* g, rdx_node node, const char* label) {
  gnode* n = node_get(g, node);
  if (!n) return false;
  set_label(g, &n->label, label, g->node_label_wrap);
  sync_labels(g);
  return true;
}

void rdx_graph_set_selected(rdx_graph* g, rdx_node node) {
  if (g) g->selected = node;
}

rdx_edge rdx_graph_connect(rdx_graph* g, rdx_node from, rdx_node to, const rdx_edge_desc* d) {
  if (!node_get(g, from) || !node_get(g, to) || same(from, to)) return (rdx_edge){0};
  uint32_t i = 0;
  while (i < g->edge_len && g->edges[i].alive) i++;
  if (i == g->edge_len) {
    if (!grow((void**)&g->edges, &g->edge_cap, i + 1, sizeof *g->edges)) return (rdx_edge){0};
    g->edge_len = i + 1;
  }
  gedge* e = &g->edges[i];
  *e       = (gedge){.gen = e->gen + 1, .alive = true, .from = from, .to = to, .color = {120, 130, 150, 255, true}};
  if (d) {
    if (d->color.set) e->color = d->color;
    e->directed = d->directed;
    e->width    = d->width;
    if (d->label) e->label = make_label(g, d->label, g->edge_label_wrap);
  }
  if (e->width <= 0) e->width = DEFAULT_EDGE_WIDTH;
  sync_labels(g);
  return (rdx_edge){i, e->gen};
}

bool rdx_graph_remove_edge(rdx_graph* g, rdx_edge edge) {
  gedge* e = edge_get(g, edge);
  if (!e) return false;
  rd_node_destroy(g->ctx, e->label);
  e->alive = false;
  e->label = (rd_node){0};
  return true;
}

bool rdx_graph_set_edge_color(rdx_graph* g, rdx_edge edge, rd_color c) {
  gedge* e = edge_get(g, edge);
  if (e) e->color = c;
  return e;
}

bool rdx_graph_set_edge_label(rdx_graph* g, rdx_edge edge, const char* label) {
  gedge* e = edge_get(g, edge);
  if (!e) return false;
  set_label(g, &e->label, label, g->edge_label_wrap);
  sync_labels(g);
  return true;
}

void rdx_graph_set_messages(rdx_graph* g, const rdx_message* messages, size_t count) {
  if (!g) return;
  rdx_message* copy = count ? malloc(count * sizeof *copy) : NULL;
  if (count && !copy) return;
  if (count) memcpy(copy, messages, count * sizeof *copy);
  free(g->messages);
  g->messages      = copy;
  g->message_count = count;
}

void rdx_graph_set_view(rdx_graph* g, rd_vec2 center, float zoom) {
  if (!g) return;
  g->center      = center;
  g->zoom        = fminf(fmaxf(zoom, MIN_ZOOM), MAX_ZOOM);
  g->fit_pending = false;
  sync_labels(g);
}

void rdx_graph_view(const rdx_graph* g, rd_vec2* center, float* zoom) {
  if (!g) return;
  if (center) *center = g->center;
  if (zoom) *zoom = g->zoom;
}

void rdx_graph_fit(rdx_graph* g, float padding) {
  if (!g) return;
  g->fit_padding = padding;
  rd_rect c      = host_content(g);
  if (c.width >= 1 && c.height >= 1)
    apply_fit(g, c.width, c.height);
  else
    g->fit_pending = true;
}

void rdx_graph_layout_circle(rdx_graph* g, const rdx_node* nodes, size_t count, rd_vec2 center, float radius) {
  for (size_t j = 0; j < count; j++) {
    float a = -1.5707963f + 6.2831853f * (float)j / (float)count;
    rdx_graph_set_position(g, nodes[j], (rd_vec2){center.x + radius * cosf(a), center.y + radius * sinf(a)});
  }
}
