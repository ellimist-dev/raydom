#include <raydom/animate.h>
#include <raydom/role.h>
#include <raydom/router/router.h>
#include <raydom/style.h>
#include <raydom/text.h>
#include <raydom/timer.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

enum { MAX_OPTIONAL = 16, MAX_CHAIN = 16, DEFAULT_LIMIT = 100 };

// Patterns.

enum { SEG_LITERAL, SEG_PARAM, SEG_WILDCARD };

typedef struct seg {
  char*   text;  // the literal, or the name (null for an unnamed wildcard)
  uint8_t kind;
  bool    optional;
} seg;

typedef struct route {
  char*         pattern;
  char*         title;
  seg*          segs;
  int           count;
  int           optionals;
  rdr_build_fn  build;
  rdr_title_fn  title_fn;
  void*         data;
  rd_destroy_fn destroy;
  uint32_t      flags;
  rd_node       kept;  // a kept-alive page's wrapper
} route;

typedef struct param {
  const char* name;  // in the route's segs
  char*       value;
} param;

// A path split into segments: one buffer, with the slashes replaced by terminators.
typedef struct path {
  char*  buf;
  char** segs;
  int    count;
} path;

typedef enum kind { PUSH, REPLACE, TRAVEL } kind;

typedef struct router {
  rd_node self, parent;
  route*  routes;
  int     route_count;
  rd_node* children;  // nested routers created with this one as parent; pruned when dead
  int      child_count, child_cap;

  // The current match.
  int     current;  // route index, -1 for none
  param*  params;
  int     param_count;
  char*   base;      // where the router is mounted; "" for a root
  char*   consumed;  // base plus the segments the route consumed
  char*   sub;       // the segments this router matched against, "/a/b" or ""
  int     consumed_count;
  rd_node slot;      // the page's wrapper
  bool    navigated;

  rdr_transition transition;
  double         duration;
  rd_node        leaving;
  bool           leaving_kept;
  uint32_t       seq;
  rd_timer       timer;

  // A root's history and trail.
  char*      path;
  char**     history;
  size_t     history_len, history_index, history_limit;
  bool       navigating;
  char*      pending;
  kind       pending_kind;
  size_t     pending_index;
  rdr_crumb* trail;
  size_t     trail_len;
} router;

static const char router_key = 0;

static router* find(const rd_ctx* ctx, rd_node n) { return rd_node_is_null(n) ? NULL : rd_node_attached(ctx, n, &router_key); }

static const char* base_of(const router* r) { return r->base ? r->base : ""; }

static router* root_of(const rd_ctx* ctx, router* r) {
  for (int k = 0; r && k < 1024; k++) {
    router* p = find(ctx, r->parent);
    if (!p) return r;
    r = p;
  }
  return r;
}

static char* dup(const char* s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char*  d = malloc(n);
  if (d) memcpy(d, s, n);
  return d;
}

static void params_free(param* p, int count) {
  for (int k = 0; k < count; k++) free(p[k].value);
  free(p);
}

static void trail_free(router* r) {
  for (size_t k = 0; k < r->trail_len; k++) {
    free((char*)r->trail[k].title);
    free((char*)r->trail[k].path);
  }
  free(r->trail);
  r->trail     = NULL;
  r->trail_len = 0;
}

static void route_free(route* rt) {
  if (rt->destroy) rt->destroy(rt->data);
  for (int k = 0; k < rt->count; k++) free(rt->segs[k].text);
  free(rt->segs);
  free(rt->pattern);
  free(rt->title);
}

static void router_free(void* p) {
  router* r = p;
  for (int k = 0; k < r->route_count; k++) route_free(&r->routes[k]);
  free(r->routes);
  free(r->children);
  params_free(r->params, r->param_count);
  free(r->base);
  free(r->consumed);
  free(r->sub);
  free(r->path);
  for (size_t k = 0; k < r->history_len; k++) free(r->history[k]);
  free(r->history);
  free(r->pending);
  trail_free(r);
  free(r);
}

// Splits s at slashes, skipping empty segments. False if out of memory.
static bool split(const char* s, path* out) {
  *out = (path){0};
  if (!(out->buf = dup(s))) return false;
  int n = 0;
  for (char* c = out->buf; *c; c++) n += *c != '/' && (c == out->buf || c[-1] == '/');
  out->segs = malloc(sizeof *out->segs * (size_t)(n ? n : 1));
  if (!out->segs) {
    free(out->buf);
    return false;
  }
  for (char* c = out->buf; *c; c++) {
    if (*c == '/') {
      *c = 0;
      continue;
    }
    if (c == out->buf || c[-1] == 0) out->segs[out->count++] = c;
  }
  return true;
}

static void path_free(path* p) {
  free(p->buf);
  free(p->segs);
}

static bool parse_pattern(const char* pattern, route* rt) {
  path p;
  if (!split(pattern, &p)) return false;
  rt->segs = calloc((size_t)(p.count ? p.count : 1), sizeof *rt->segs);
  bool ok  = rt->segs;
  for (int k = 0; ok && k < p.count; k++) {
    char*  s   = p.segs[k];
    size_t len = strlen(s);
    seg*   g   = &rt->segs[k];
    if (s[0] == '*') {
      g->kind = SEG_WILDCARD;
      ok      = k == p.count - 1 && !strchr(s + 1, '?') && (!s[1] || (g->text = dup(s + 1)));
    } else {
      g->optional = s[len - 1] == '?';
      if (g->optional) s[--len] = 0;
      g->kind = s[0] == ':' ? SEG_PARAM : SEG_LITERAL;
      if (g->kind == SEG_PARAM) s++;
      ok = *s && !strchr(s, '*') && (g->text = dup(s));
      rt->optionals += g->optional;
    }
    rt->count++;
  }
  path_free(&p);
  return ok && rt->optionals <= MAX_OPTIONAL;
}

// Matching. A binding gives, for each pattern segment, the path segment it took (-1 if skipped);
// a wildcard takes from its index to the end. Its arrays are sized for the longest pattern and the
// path, so neither has a length limit.

typedef struct binding {
  int*     at;    // per pattern segment
  uint8_t* rank;  // per path segment: 3 literal, 2 parameter, 1 wildcard
  bool     wildcard;
  bool     found;
} binding;

static bool binding_init(binding* b, int segs, int path_segs) {
  *b      = (binding){0};
  b->at   = malloc(sizeof *b->at * (size_t)(segs ? segs : 1));
  b->rank = malloc((size_t)(path_segs ? path_segs : 1));
  if (b->at && b->rank) return true;
  free(b->at);
  free(b->rank);
  *b = (binding){0};
  return false;
}

static void binding_free(binding* b) {
  free(b->at);
  free(b->rank);
  *b = (binding){0};
}

static void binding_copy(binding* to, const binding* from, int segs, int path_segs) {
  memcpy(to->at, from->at, sizeof *to->at * (size_t)segs);
  memcpy(to->rank, from->rank, (size_t)path_segs);
  to->wildcard = from->wildcard;
  to->found    = from->found;
}

static int max_segs(const router* r) {
  int n = 0;
  for (int k = 0; k < r->route_count; k++) n = r->routes[k].count > n ? r->routes[k].count : n;
  return n;
}

static bool better(const binding* a, const binding* b, int n) {
  if (!b->found) return true;
  for (int k = 0; k < n; k++)
    if (a->rank[k] != b->rank[k]) return a->rank[k] > b->rank[k];
  return !a->wildcard && b->wildcard;
}

// The route's best binding to the path, trying each choice of optional segments, earlier ones
// included first. out and the scratch binding b are sized for the route and the path.
static bool bind(const route* rt, const path* p, binding* out, binding* b) {
  out->found = false;
  for (int mask = (1 << rt->optionals) - 1; mask >= 0; mask--) {
    b->found    = true;
    b->wildcard = false;
    int  at = 0, opt = 0;
    bool ok = true;
    for (int k = 0; ok && k < rt->count; k++) {
      const seg* g = &rt->segs[k];
      b->at[k]     = -1;
      if (g->optional && !(mask >> (rt->optionals - 1 - opt++) & 1)) continue;
      if (g->kind == SEG_WILDCARD) {
        b->at[k]    = at;
        b->wildcard = true;
        for (; at < p->count; at++) b->rank[at] = 1;
        break;
      }
      ok = at < p->count && (g->kind == SEG_PARAM || !strcmp(g->text, p->segs[at]));
      if (ok) {
        b->rank[at] = g->kind == SEG_PARAM ? 2 : 3;
        b->at[k]    = at++;
      }
    }
    if (ok && at == p->count && better(b, out, p->count)) binding_copy(out, b, rt->count, p->count);
  }
  return out->found;
}

// The best route for the path, with its binding in out, which the caller frees. -1 if none matches or
// out of memory.
static int best_route(const router* r, const path* p, binding* out) {
  int     best = -1, segs = max_segs(r);
  binding b, scratch;
  bool    ok = binding_init(out, segs, p->count);
  ok         = binding_init(&b, segs, p->count) && ok;
  ok         = binding_init(&scratch, segs, p->count) && ok;
  for (int k = 0; ok && k < r->route_count; k++) {
    if (bind(&r->routes[k], p, &b, &scratch) && better(&b, out, p->count)) {
      binding_copy(out, &b, r->routes[k].count, p->count);
      best = k;
    }
  }
  binding_free(&b);
  binding_free(&scratch);
  return best;
}

// prefix, then /segment for each of [from, to) of p.
static char* join(const char* prefix, const path* p, int from, int to) {
  size_t n = strlen(prefix) + 1;
  for (int k = from; k < to; k++) n += strlen(p->segs[k]) + 1;
  char* s = malloc(n);
  if (!s) return NULL;
  size_t at = strlen(prefix);
  memcpy(s, prefix, at);
  for (int k = from; k < to; k++) {
    s[at++] = '/';
    size_t len = strlen(p->segs[k]);
    memcpy(s + at, p->segs[k], len);
    at += len;
  }
  s[at] = 0;
  return s;
}

static char* slash_join(const path* p, int from, int to) {
  char* s = join("", p, from, to);
  if (s && s[0] == '/') memmove(s, s + 1, strlen(s));
  return s;
}

// The parameters a binding gives, and how many path segments it consumed before a wildcard.
static bool bind_params(const route* rt, const binding* b, const path* p, param** out, int* count, int* consumed) {
  *out      = calloc((size_t)rt->count + 1, sizeof **out);
  *count    = 0;
  *consumed = p->count;
  if (!*out) return false;
  for (int k = 0; k < rt->count; k++) {
    const seg* g = &rt->segs[k];
    if (g->kind == SEG_WILDCARD && b->at[k] >= 0) *consumed = b->at[k];
    if (!g->text || g->kind == SEG_LITERAL || b->at[k] < 0) continue;
    char* v = g->kind == SEG_WILDCARD ? slash_join(p, b->at[k], p->count) : dup(p->segs[b->at[k]]);
    if (!v) {
      params_free(*out, *count);
      *out = NULL;
      return false;
    }
    (*out)[(*count)++] = (param){g->text, v};
  }
  return true;
}

static bool params_eq(const param* a, int na, const param* b, int nb) {
  if (na != nb) return false;
  for (int k = 0; k < na; k++)
    if (strcmp(a[k].name, b[k].name) || strcmp(a[k].value, b[k].value)) return false;
  return true;
}

// Normalizes base joined with rel (or rel alone, if absolute), resolving . and ..
static char* resolve_path(const char* base, const char* rel) {
  if (!rel) return NULL;
  size_t bl = rel[0] == '/' ? 0 : strlen(base);
  char*  s  = malloc(bl + strlen(rel) + 2);
  if (!s) return NULL;
  memcpy(s, base, bl);
  s[bl] = '/';
  strcpy(s + bl + 1, rel);
  path p;
  bool ok = split(s, &p);
  free(s);
  if (!ok) return NULL;
  int n = 0;
  for (int k = 0; k < p.count; k++) {
    if (!strcmp(p.segs[k], ".")) continue;
    if (!strcmp(p.segs[k], "..")) {
      n -= n > 0;
      continue;
    }
    p.segs[n++] = p.segs[k];
  }
  char* out = join("", &p, 0, n);
  path_free(&p);
  if (out && !out[0]) {
    free(out);
    out = dup("/");
  }
  return out;
}

// The ctx-wide registry: the router used last, and the links. Attached to the root node, whose
// listeners take back and forward.

typedef struct registry {
  rd_node  last;
  rd_node* links;
  size_t   link_count, link_cap;
} registry;

static const char registry_key = 0;

typedef struct link {
  char*    path;
  rd_node  router;
  uint32_t flags;
} link;

static const char link_key = 0;

static void registry_free(void* p) {
  registry* g = p;
  free(g->links);
  free(g);
}

static void link_free(void* p) {
  link* l = p;
  free(l->path);
  free(l);
}

static registry* registry_find(const rd_ctx* ctx) { return rd_node_attached(ctx, rd_ctx_root(ctx), &registry_key); }

// The router nearest node, itself included.
static router* router_above(const rd_ctx* ctx, rd_node n) {
  for (int k = 0; !rd_node_is_null(n) && k < 100000; k++, n = rd_node_parent(ctx, n)) {
    router* r = find(ctx, n);
    if (r) return r;
  }
  return NULL;
}

// The router an input event on target goes to: the one around it, else the one used last.
static router* router_for(const rd_ctx* ctx, rd_node target) {
  router* r = router_above(ctx, target);
  if (r) return root_of(ctx, r);
  registry* g = registry_find(ctx);
  return g ? find(ctx, g->last) : NULL;
}

static bool travel(rd_ctx* ctx, router* root, int delta);

// Alt+Left and Alt+Right navigate, except on Apple platforms, where Option+Left moves by word. A
// browser build can't tell at compile time, so it asks the browser once.
static bool alt_arrows(void) {
#if defined(__EMSCRIPTEN__)
  static int apple = -1;
  if (apple < 0) apple = EM_ASM_INT({
      const n = typeof navigator === 'undefined' ? {} : navigator;  // none outside a page in older runtimes
      const p = (n.userAgentData && n.userAgentData.platform) || n.platform || "";
      return /Mac|iPhone|iPad|iPod/.test(p) ? 1 : 0;
    });
  return !apple;
#elif defined(__APPLE__)
  return false;
#else
  return true;
#endif
}

static void nav_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  if (ev->default_prevented || ev->repeat) return;
  int dir = 0;
  if (ev->mods == RD_MOD_SUPER && (ev->key == RD_KEY_LEFT_BRACKET || ev->key == RD_KEY_RIGHT_BRACKET))
    dir = ev->key == RD_KEY_LEFT_BRACKET ? -1 : 1;
  if (ev->mods == RD_MOD_ALT && (ev->key == RD_KEY_LEFT || ev->key == RD_KEY_RIGHT) && alt_arrows())
    dir = ev->key == RD_KEY_LEFT ? -1 : 1;
  router* r = dir ? router_for(ctx, ev->target) : NULL;
  if (r && travel(ctx, r, dir)) rd_event_prevent_default(ev);
}

static void nav_buttons(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  int     dir = ev->button == RD_BUTTON_BACK ? -1 : ev->button == RD_BUTTON_FORWARD ? 1 : 0;
  router* r   = dir && !ev->default_prevented ? router_for(ctx, ev->target) : NULL;
  if (r && travel(ctx, r, dir)) rd_event_prevent_default(ev);
}

// In a browser, the mouse's side buttons and the back and forward shortcuts navigate the page itself,
// leaving the app, unless the page cancels them as they happen; raydom only sees input a frame later.
// With a router taking them, cancel them up front: the side buttons over the canvas, and the shortcuts
// while nothing else on the page has focus. Once per page.
static void claim_browser_navigation(void) {
#ifdef __EMSCRIPTEN__
  EM_ASM({
    if (globalThis.__raydomRouterInput || typeof window === 'undefined') return;  // not in a page: nothing to claim
    globalThis.__raydomRouterInput = true;
    const onCanvas = (e) => e.target === Module['canvas'];
    // Not pointerdown: cancelling it would also suppress the mousedown that raylib reads.
    for (const type of ['mousedown', 'mouseup', 'pointerup', 'auxclick'])
      window.addEventListener(type, (e) => { if ((e.button === 3 || e.button === 4) && onCanvas(e)) e.preventDefault(); }, true);
    window.addEventListener('keydown', (e) => {
      const free = !document.activeElement || document.activeElement === document.body || document.activeElement === Module['canvas'];
      const alt = $0 && e.altKey && !e.ctrlKey && !e.metaKey && !e.shiftKey && (e.code === 'ArrowLeft' || e.code === 'ArrowRight');
      const cmd = e.metaKey && !e.ctrlKey && !e.altKey && !e.shiftKey && (e.code === 'BracketLeft' || e.code === 'BracketRight');
      if (free && (alt || cmd)) e.preventDefault();
    }, true);
  }, alt_arrows());
#endif
}

static registry* registry_get(rd_ctx* ctx) {
  registry* g = registry_find(ctx);
  if (g) return g;
  claim_browser_navigation();
  rd_node root = rd_ctx_root(ctx);
  if (!(g = calloc(1, sizeof *g))) return NULL;
  if (!rd_node_attach(ctx, root, &registry_key, g, registry_free)) {
    free(g);
    return NULL;
  }
  rd_node_on(ctx, root, RD_EVENT_KEY_DOWN, nav_keys, NULL, NULL, 0);
  rd_node_on(ctx, root, RD_EVENT_POINTER_DOWN, nav_buttons, NULL, NULL, 0);
  return g;
}

// Pages and transitions.

static const rd_style SLOT = {
    .display        = RD_DISPLAY_FLEX,
    .flex_direction = RD_FLEX_COLUMN,
    .position       = RD_POSITION_RELATIVE,
    .flex_grow      = {1, RD_UNIT_NUM},
    .flex_shrink    = {1, RD_UNIT_NUM},
    .min_width      = {0, RD_UNIT_PX},
    .min_height     = {0, RD_UNIT_PX},
};

static bool inside(const rd_ctx* ctx, rd_node n, rd_node ancestor) {
  if (rd_node_is_null(ancestor)) return false;
  for (int k = 0; !rd_node_is_null(n) && k < 100000; k++, n = rd_node_parent(ctx, n))
    if (rd_node_eq(n, ancestor)) return true;
  return false;
}

// A page that was left: hidden if kept, else destroyed.
static void drop(rd_ctx* ctx, rd_node slot, bool kept) {
  if (!rd_node_alive(ctx, slot)) return;
  if (!kept) {
    rd_node_destroy(ctx, slot);
    return;
  }
  rd_style s = SLOT;
  s.display  = RD_DISPLAY_NONE;
  rd_node_set_style(ctx, slot, &s);
  rd_node_set_states(ctx, slot, RD_STATE_DISABLED, false);
}

// Ends a transition at once: the old page goes and the new one settles.
static void finish(rd_ctx* ctx, router* r) {
  bool active = r->timer.index || !rd_node_is_null(r->leaving);
  if (r->timer.index) rd_ctx_clear_timer(ctx, r->timer);
  r->timer    = (rd_timer){0};
  rd_node old = r->leaving;
  r->leaving  = (rd_node){0};
  if (!active) return;
  drop(ctx, old, r->leaving_kept);
  if (rd_node_alive(ctx, r->slot)) rd_node_set_style(ctx, r->slot, &SLOT);
}

typedef struct token {
  rd_node  router;
  uint32_t seq;
} token;

static void transition_done(rd_ctx* ctx, void* data) {
  token*  t = data;
  router* r = find(ctx, t->router);
  if (!r || r->seq != t->seq) return;
  finish(ctx, r);  // clearing the timer that's firing defers its cleanup until it returns
}

// Shows slot in place of old. dir is 1 for forward, -1 for back.
static void change_pages(rd_ctx* ctx, router* r, rd_node old, bool old_kept, rd_node slot, int dir) {
  if (rd_node_alive(ctx, slot)) rd_node_set_style(ctx, slot, &SLOT);
  bool animate = r->navigated && r->transition != RDR_TRANSITION_NONE && r->duration > 0 &&
                 (rd_node_alive(ctx, old) || rd_node_alive(ctx, slot));
  if (!animate) {
    drop(ctx, old, old_kept);
    return;
  }
  float  d    = (float)dir * 100;
  bool   fade = r->transition == RDR_TRANSITION_FADE;
  double sec  = r->duration;
  if (rd_node_alive(ctx, old)) {
    rd_style s       = SLOT;
    s.position       = RD_POSITION_ABSOLUTE;
    s.inset          = (rd_edges){.top = rd_px(0), .right = rd_pct(0), .bottom = rd_px(0), .left = rd_pct(0)};
    s.pointer_events = RD_POINTER_EVENTS_NONE;
    rd_node_set_style(ctx, old, &s);
    rd_node_set_states(ctx, old, RD_STATE_DISABLED, true);
    if (inside(ctx, rd_ctx_focused(ctx), old)) rd_node_focus(ctx, (rd_node){0});
    // From where it is: its computed inset was unset, which doesn't interpolate.
    rd_style from = {.inset = s.inset, .opacity = rd_num(1)};
    rd_style to   = fade ? (rd_style){.opacity = rd_num(0)}
                         : (rd_style){.inset = {.top = rd_px(0), .right = rd_pct(d), .bottom = rd_px(0), .left = rd_pct(-d)}};
    rd_node_animate(ctx, old, &from, &to, sec, RD_EASING_EASE_IN_OUT);
  }
  if (rd_node_alive(ctx, slot)) {
    rd_style from = fade ? (rd_style){.display = RD_DISPLAY_FLEX, .opacity = rd_num(0)}
                         : (rd_style){.display = RD_DISPLAY_FLEX, .inset = {.left = rd_pct(d)}};
    rd_style to   = fade ? (rd_style){.opacity = rd_num(1)} : (rd_style){.inset = {.left = rd_pct(0)}};
    rd_node_animate(ctx, slot, &from, &to, sec, RD_EASING_EASE_IN_OUT);
  }
  r->leaving      = rd_node_alive(ctx, old) ? old : (rd_node){0};
  r->leaving_kept = old_kept;
  token* t        = malloc(sizeof *t);
  if (t) {
    *t       = (token){r->self, ++r->seq};
    r->timer = rd_ctx_set_timer(ctx, sec, 0, transition_done, t, free);
  }
  if (!r->timer.index) finish(ctx, r);  // no timer to end it: end it now
}

// Resolution: matches a router to the segments [from, end) of p, below base, and its nested
// routers to what its route's wildcard left.
static void resolve(rd_ctx* ctx, rd_node node, const char* base, const path* p, int from, int dir, int depth) {
  router* r = find(ctx, node);
  if (!r || depth > MAX_CHAIN * 4) return;
  path sub = {p->buf, p->segs + from, p->count - from};
  binding b;
  int     index    = best_route(r, &sub, &b);
  param*  params   = NULL;
  int     count    = 0, consumed = sub.count;
  if (index >= 0 && !bind_params(&r->routes[index], &b, &sub, &params, &count, &consumed)) index = -1;
  binding_free(&b);

  char* nb = dup(base), *nc = join(base, &sub, 0, consumed), *ns = join("", &sub, 0, sub.count);
  if (nb && nc && ns) {
    free(r->base);
    free(r->consumed);
    free(r->sub);
    r->base     = nb;
    r->consumed = nc;
    r->sub      = ns;
  } else {
    free(nb);
    free(nc);
    free(ns);
  }
  r->consumed_count = index >= 0 ? consumed : 0;

  bool same = index == r->current && params_eq(params, count, r->params, r->param_count);
  // A kept page showing already stays when its parameters change, and reads the new ones.
  if (!same && index >= 0 && index == r->current && (r->routes[index].flags & RDR_ROUTE_KEEP_ALIVE))
    same = rd_node_eq(r->routes[index].kept, r->slot) && rd_node_alive(ctx, r->slot);
  params_free(r->params, r->param_count);
  r->params      = params;
  r->param_count = count;
  if (!same) {
    finish(ctx, r);
    rd_node old      = r->slot;
    bool    old_kept = r->current >= 0 && (r->routes[r->current].flags & RDR_ROUTE_KEEP_ALIVE);
    r->current       = index;
    r->slot          = (rd_node){0};
    rd_node slot     = {0};
    if (index >= 0) {
      route* rt = &r->routes[index];
      bool   keep = rt->flags & RDR_ROUTE_KEEP_ALIVE;
      if (keep && rd_node_alive(ctx, rt->kept)) {
        slot = rt->kept;
      } else if (!rd_node_is_null(slot = rd_node_create(ctx))) {
        rd_node_set_style(ctx, slot, &SLOT);
        rd_node_append(ctx, node, slot);
        if (keep) rt->kept = slot;
        r->slot = slot;
        rd_node page = rt->build ? rt->build(ctx, node, rt->data) : (rd_node){0};
        if (!(r = find(ctx, node))) return;  // the build destroyed the router
        if (rd_node_alive(ctx, slot) && rd_node_alive(ctx, page) && !rd_node_append(ctx, slot, page))
          rd_node_destroy(ctx, page);
      }
    }
    r->slot = slot;
    change_pages(ctx, r, old, old_kept, slot, dir);
  }
  r->navigated = true;

  // Nested routers in the page, given what the route didn't consume.
  int n = 0;
  for (int k = 0; k < r->child_count; k++)
    if (rd_node_alive(ctx, r->children[k])) r->children[n++] = r->children[k];
  r->child_count = n;
  if (!n || rd_node_is_null(r->slot)) return;
  rd_node* kids = malloc(sizeof *kids * (size_t)n);
  if (!kids) return;
  memcpy(kids, r->children, sizeof *kids * (size_t)n);
  char* child_base = dup(r->consumed);
  rd_node slot     = r->slot;
  for (int k = 0; child_base && k < n; k++)
    if (inside(ctx, kids[k], slot)) resolve(ctx, kids[k], child_base, p, from + consumed, dir, depth + 1);
  free(child_base);
  free(kids);
}

// What a title function sees: the parameters of the route's match on one prefix of the path, and
// through the router, those of the routers above it.
struct rdr_match {
  const rd_ctx* ctx;
  const router* router;
  const param*  params;
  int           count;
  const char*   path;
};

const char* rdr_match_param(const rdr_match* m, const char* name) {
  if (!m || !name) return NULL;
  for (int k = 0; k < m->count; k++)
    if (!strcmp(m->params[k].name, name)) return m->params[k].value;
  return rdr_param(m->ctx, m->router->parent, name);
}

const char* rdr_match_path(const rdr_match* m) { return m ? m->path : NULL; }

// Title with :name replaced by the parameter's value.
static char* substitute(const rdr_match* m, const char* title) {
  size_t cap = strlen(title) + 1, len = 0;
  char*  s   = malloc(cap);
  for (const char* c = title; s && *c;) {
    const char* v = NULL;
    size_t      skip = 1;
    if (*c == ':') {
      size_t n = 1;
      while (c[n] == '_' || (c[n] >= 'a' && c[n] <= 'z') || (c[n] >= 'A' && c[n] <= 'Z') || (c[n] >= '0' && c[n] <= '9')) n++;
      if (n > 1) {
        char name[64] = {0};
        memcpy(name, c + 1, n - 1 < sizeof name - 1 ? n - 1 : sizeof name - 1);
        v    = rdr_match_param(m, name);
        skip = v ? n : 1;
      }
    }
    size_t add = v ? strlen(v) : 1;
    if (len + add + 1 > cap) {
      char* t = realloc(s, cap = (len + add + 1) * 2);
      if (!t) {
        free(s);
        return NULL;
      }
      s = t;
    }
    memcpy(s + len, v ? v : c, add);
    len += add;
    c += skip;
  }
  if (s) s[len] = 0;
  return s;
}

// Appends a crumb for route rt matched as m, unless it has no title or its path repeats the last
// crumb's. False when out of memory.
static bool add_crumb(rd_ctx* ctx, router* root, route* rt, const rdr_match* m) {
  if (!rt->title && !rt->title_fn) return true;
  if (root->trail_len && !strcmp(root->trail[root->trail_len - 1].path, m->path)) return true;
  const char* custom = rt->title_fn ? rt->title_fn(ctx, m, rt->data) : NULL;
  char*       title  = custom ? dup(custom) : rt->title ? substitute(m, rt->title) : NULL;
  if (!title) return !custom && !rt->title;  // a title function without a static fallback returned null
  char*      path = dup(m->path);
  rdr_crumb* t    = path ? realloc(root->trail, sizeof *t * (root->trail_len + 1)) : NULL;
  if (!t) {
    free(title);
    free(path);
    return false;
  }
  root->trail                    = t;
  root->trail[root->trail_len++] = (rdr_crumb){title, path};
  return true;
}

// The crumbs of one router: for each prefix of what it matched, the route that matches that prefix
// exactly, if it has a title; then its own route. With a nested router taking over, the prefixes stop
// where the route's wildcard begins, and the nested router names the rest. A prefix matched only
// through a wildcard capturing part of it gets no crumb, nor does a pattern starting with a wildcard,
// so a catch-all doesn't name every level.
static bool router_crumbs(rd_ctx* ctx, router* root, router* r, bool nested) {
  path q;
  if (!r->sub || !split(r->sub, &q)) return false;
  int     segs = max_segs(r);
  binding best, b, scratch;
  bool    ok = binding_init(&best, segs, q.count);
  ok         = binding_init(&b, segs, q.count) && ok;
  ok         = binding_init(&scratch, segs, q.count) && ok;
  int     end = nested ? r->consumed_count : q.count;
  for (int len = 0; ok && len < end && len <= q.count; len++) {
    path prefix = {q.buf, q.segs, len};
    int  index  = -1;
    best.found  = false;
    for (int k = 0; k < r->route_count; k++) {
      const route* rt = &r->routes[k];
      if (k == r->current || (rt->count && rt->segs[0].kind == SEG_WILDCARD) || !bind(rt, &prefix, &b, &scratch)) continue;
      bool captures = false;
      for (int j = 0; j < len; j++) captures |= b.rank[j] == 1;
      if (!captures && better(&b, &best, len)) {
        binding_copy(&best, &b, rt->count, len);
        index = k;
      }
    }
    if (index < 0) continue;
    param* params = NULL;
    int    count = 0, consumed = 0;
    char*  p = join(base_of(r), &prefix, 0, len);
    if (!p || !bind_params(&r->routes[index], &best, &prefix, &params, &count, &consumed)) {
      free(p);
      ok = false;
      break;
    }
    rdr_match m = {ctx, r, params, count, p[0] ? p : "/"};
    ok          = add_crumb(ctx, root, &r->routes[index], &m);
    params_free(params, count);
    free(p);
  }
  char* full = nested ? NULL : join(base_of(r), &q, 0, q.count);
  if (ok && r->current >= 0 && (nested || full)) {
    const char* p = nested ? r->consumed : full;
    rdr_match   m = {ctx, r, r->params, r->param_count, p && p[0] ? p : "/"};
    ok            = add_crumb(ctx, root, &r->routes[r->current], &m);
  }
  free(full);
  path_free(&q);
  binding_free(&best);
  binding_free(&b);
  binding_free(&scratch);
  return ok && (nested || full);
}

static void build_trail(rd_ctx* ctx, router* root) {
  trail_free(root);
  router* r = root;
  for (int depth = 0; r && depth < MAX_CHAIN * 4; depth++) {
    if (r->current < 0) break;
    router* next = NULL;
    for (int k = 0; !next && k < r->child_count; k++) {
      router* c = find(ctx, r->children[k]);
      if (c && inside(ctx, c->self, r->slot)) next = c;
    }
    if (!router_crumbs(ctx, root, r, next != NULL)) break;
    r = next;
  }
}

// The router a link navigates: its own, else the one around it, else the one used last.
static router* link_router(const rd_ctx* ctx, const link* l, rd_node node) {
  router* r = find(ctx, l->router);
  if (!r) r = router_above(ctx, node);
  if (!r) {
    registry* g = registry_find(ctx);
    r           = g ? find(ctx, g->last) : NULL;
  }
  return r;
}

// Sets a link's CHECKED state, if it navigates root's tree.
static void mark_link(rd_ctx* ctx, rd_node node, const link* l, const router* root) {
  router* r = link_router(ctx, l, node);
  if (!r || root_of(ctx, r) != root || !root->path) return;
  char* target = resolve_path(base_of(r), l->path);
  if (!target) return;
  size_t len = strlen(target);
  bool   on  = !strcmp(target, root->path) ||
            (!(l->flags & RDR_LINK_EXACT) && len > 1 && !strncmp(target, root->path, len) && root->path[len] == '/');
  rd_node_set_states(ctx, node, RD_STATE_CHECKED, on);
  free(target);
}

static void mark_links(rd_ctx* ctx, router* root) {
  registry* g = registry_find(ctx);
  if (!g) return;
  size_t n = 0;
  for (size_t k = 0; k < g->link_count; k++) {
    rd_node node = g->links[k];
    link*   l    = rd_node_attached(ctx, node, &link_key);
    if (!l) continue;
    g->links[n++] = node;
    mark_link(ctx, node, l, root);
  }
  g->link_count = n;
}

// Moves the root's tree to target, an absolute normalized path it takes. index is the history
// entry for TRAVEL.
static bool apply(rd_ctx* ctx, rd_node root_node, char* target, kind how, size_t index, int dir) {
  router* root = find(ctx, root_node);
  if (!root || !target) {
    free(target);
    return false;
  }
  if (how != TRAVEL && root->path && !strcmp(root->path, target)) {
    free(target);
    return true;
  }
  path    p;
  binding b;
  if (!split(target, &p)) {
    free(target);
    return false;
  }
  bool    matched = best_route(root, &p, &b) >= 0;
  binding_free(&b);
  if (!matched) {
    path_free(&p);
    free(target);
    return false;
  }

  // History.
  char* entry = dup(target);
  if (!entry) {
    path_free(&p);
    free(target);
    return false;
  }
  if (how == TRAVEL) {
    root->history_index = index;
    free(entry);
  } else if (how == REPLACE && root->history_len) {
    free(root->history[root->history_index]);
    root->history[root->history_index] = entry;
  } else {
    size_t keep = root->history_len ? root->history_index + 1 : 0;
    for (size_t k = keep; k < root->history_len; k++) free(root->history[k]);
    root->history_len = keep;
    if (keep >= root->history_limit && keep) {  // drop the oldest
      free(root->history[0]);
      memmove(root->history, root->history + 1, sizeof *root->history * --root->history_len);
    }
    char** h = realloc(root->history, sizeof *h * (root->history_len + 1));
    if (!h) {
      free(entry);
      path_free(&p);
      free(target);
      return false;
    }
    root->history                     = h;
    root->history[root->history_len]  = entry;
    root->history_index               = root->history_len++;
  }
  free(root->path);
  root->path = target;

  resolve(ctx, root_node, "", &p, 0, dir, 0);
  path_free(&p);
  if (!(root = find(ctx, root_node))) return true;
  build_trail(ctx, root);
  registry* g = registry_find(ctx);
  if (g) g->last = root_node;
  mark_links(ctx, root);
  rd_node_dispatch(ctx, root_node, RD_EVENT_CHANGE);
  return true;
}

// Runs a navigation, or queues it behind the one running.
static bool request(rd_ctx* ctx, router* root, char* target, kind how, size_t index) {
  if (!target) return false;
  if (root->navigating) {
    free(root->pending);
    root->pending       = target;
    root->pending_kind  = how;
    root->pending_index = index;
    return true;
  }
  rd_node node     = root->self;
  int     dir      = how == TRAVEL && index < root->history_index ? -1 : 1;
  root->navigating = true;
  bool ok          = apply(ctx, node, target, how, index, dir);
  for (int k = 0; k < MAX_CHAIN && (root = find(ctx, node)) && root->pending; k++) {
    char* next    = root->pending;
    root->pending = NULL;
    dir           = root->pending_kind == TRAVEL && root->pending_index < root->history_index ? -1 : 1;
    if (root->pending_kind == TRAVEL && root->pending_index >= root->history_len) {
      free(next);
      continue;
    }
    apply(ctx, node, next, root->pending_kind, root->pending_index, dir);
  }
  if ((root = find(ctx, node))) {
    free(root->pending);  // a redirect loop
    root->pending    = NULL;
    root->navigating = false;
  }
  return ok;
}

static bool travel(rd_ctx* ctx, router* root, int delta) {
  if (!root->history_len) return false;
  size_t to = root->history_index + (size_t)(ptrdiff_t)delta;
  if ((delta < 0 && !root->history_index) || to >= root->history_len) return false;
  return request(ctx, root, dup(root->history[to]), TRAVEL, to);
}

// The API.

rd_node rdr_router_create(rd_ctx* ctx, const rdr_router_desc* desc) {
  rdr_router_desc d = desc ? *desc : (rdr_router_desc){0};
  router*         parent = rd_node_is_null(d.parent) ? NULL : find(ctx, d.parent);
  if (!ctx || (!rd_node_is_null(d.parent) && !parent) || !registry_get(ctx)) return (rd_node){0};
  router* r = calloc(1, sizeof *r);
  if (!r) return (rd_node){0};
  rd_node node     = rd_node_create(ctx);
  r->self          = node;
  r->parent        = d.parent;
  r->current       = -1;
  r->transition    = d.transition;
  r->duration      = d.duration > 0 ? d.duration : 0.2;
  r->history_limit = d.history_limit ? d.history_limit : DEFAULT_LIMIT;
  if (parent && parent->child_count == parent->child_cap) {
    int      cap = parent->child_cap ? parent->child_cap * 2 : 4;
    rd_node* c   = realloc(parent->children, sizeof *c * (size_t)cap);
    if (c) {
      parent->children  = c;
      parent->child_cap = cap;
    }
  }
  if (rd_node_is_null(node) || (parent && parent->child_count == parent->child_cap) ||
      !rd_node_attach(ctx, node, &router_key, r, router_free)) {
    free(r);
    rd_node_destroy(ctx, node);
    return (rd_node){0};
  }
  if (parent) parent->children[parent->child_count++] = node;
  rd_node_set_style(ctx, node, &(rd_style){.display        = RD_DISPLAY_FLEX,
                                           .flex_direction = RD_FLEX_COLUMN,
                                           .position       = RD_POSITION_RELATIVE,
                                           .overflow_x     = RD_OVERFLOW_HIDDEN,
                                           .overflow_y     = RD_OVERFLOW_HIDDEN});
  return node;
}

bool rdr_router_add(rd_ctx* ctx, rd_node node, const rdr_route_desc* desc) {
  router* r = find(ctx, node);
  if (!r || !desc || !desc->pattern) return false;
  route rt = {.build = desc->build, .title_fn = desc->title_fn, .data = desc->data, .flags = desc->flags};
  bool  ok = parse_pattern(desc->pattern, &rt) && (rt.pattern = dup(desc->pattern)) && (!desc->title || (rt.title = dup(desc->title)));
  route* routes = ok ? realloc(r->routes, sizeof *routes * (size_t)(r->route_count + 1)) : NULL;
  if (!routes) {
    route_free(&rt);  // destroy is unset, so data isn't touched
    return false;
  }
  rt.destroy                  = desc->destroy;
  r->routes                   = routes;
  r->routes[r->route_count++] = rt;
  return true;
}

static bool navigate(rd_ctx* ctx, rd_node node, const char* path, kind how) {
  router* r = find(ctx, node);
  if (!r || !path) return false;
  router* root = root_of(ctx, r);
  return request(ctx, root, resolve_path(base_of(r), path), how, 0);
}

bool rdr_router_go(rd_ctx* ctx, rd_node router, const char* path) { return navigate(ctx, router, path, PUSH); }
bool rdr_router_replace(rd_ctx* ctx, rd_node router, const char* path) { return navigate(ctx, router, path, REPLACE); }

bool rdr_router_back(rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return r && travel(ctx, root_of(ctx, r), -1);
}

bool rdr_router_forward(rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return r && travel(ctx, root_of(ctx, r), 1);
}

bool rdr_router_can_back(const rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return r && (r = root_of(ctx, r))->history_len && r->history_index > 0;
}

bool rdr_router_can_forward(const rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return r && (r = root_of(ctx, r))->history_len && r->history_index + 1 < r->history_len;
}

const char* rdr_router_path(const rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  if (!r) return NULL;
  r = root_of(ctx, r);
  return r->path ? r->path : "/";
}

const char* rdr_router_base(const rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return !r ? NULL : r->base && r->base[0] ? r->base : "/";
}

const char* rdr_router_route(const rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return r && r->current >= 0 ? r->routes[r->current].pattern : NULL;
}

rd_node rdr_router_page(const rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return r && rd_node_alive(ctx, r->slot) ? r->slot : (rd_node){0};
}

rd_node rdr_router_root(const rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return r ? root_of(ctx, r)->self : (rd_node){0};
}

bool rdr_router_transitioning(const rd_ctx* ctx, rd_node node) {
  router* r = find(ctx, node);
  return r && (r->timer.index || rd_node_alive(ctx, r->leaving));
}

bool rdr_router_set_transition(rd_ctx* ctx, rd_node node, rdr_transition transition, double duration) {
  router* r = find(ctx, node);
  if (!r) return false;
  finish(ctx, r);
  r->transition = transition;
  r->duration   = duration > 0 ? duration : 0.2;
  return true;
}

const char* rdr_param(const rd_ctx* ctx, rd_node node, const char* name) {
  if (!name) return NULL;
  for (router* r = router_above(ctx, node); r; r = find(ctx, r->parent))
    for (int k = 0; k < r->param_count; k++)
      if (!strcmp(r->params[k].name, name)) return r->params[k].value;
  return NULL;
}

size_t rdr_router_trail(const rd_ctx* ctx, rd_node node, rdr_crumb* out, size_t cap) {
  router* r = find(ctx, node);
  if (!r) return 0;
  r = root_of(ctx, r);
  for (size_t k = 0; out && k < cap && k < r->trail_len; k++) out[k] = r->trail[k];
  return r->trail_len;
}

static void link_click(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  link* l = rd_node_attached(ctx, ev->current, &link_key);
  if (!l) return;
  router* r = link_router(ctx, l, ev->current);
  if (!r) return;
  char* path = dup(l->path);  // the navigation may destroy the link
  if (path) rdr_router_go(ctx, r->self, path);
  free(path);
}

bool rdr_link_attach(rd_ctx* ctx, rd_node node, rd_node router_node, const char* path, uint32_t flags) {
  registry* g = ctx && path && rd_node_alive(ctx, node) ? registry_get(ctx) : NULL;
  if (!g || rd_node_attached(ctx, node, &link_key)) return false;
  if (g->link_count == g->link_cap) {
    size_t   cap = g->link_cap ? g->link_cap * 2 : 8;
    rd_node* n   = realloc(g->links, sizeof *n * cap);
    if (!n) return false;
    g->links    = n;
    g->link_cap = cap;
  }
  link* l = calloc(1, sizeof *l);
  if (!l || !(l->path = dup(path))) {
    free(l);
    return false;
  }
  l->router = router_node;
  l->flags  = flags;
  if (!rd_node_attach(ctx, node, &link_key, l, link_free)) {
    link_free(l);
    return false;
  }
  if (!rd_node_on(ctx, node, RD_EVENT_CLICK, link_click, NULL, NULL, 0).id) {
    rd_node_attach(ctx, node, &link_key, NULL, NULL);
    return false;
  }
  g->links[g->link_count++] = node;
  // A navigation running marks every link once it has resolved.
  router* r = link_router(ctx, l, node);
  if (r && !(r = root_of(ctx, r))->navigating) mark_link(ctx, node, l, r);
  return true;
}

rd_node rdr_link_create(rd_ctx* ctx, const char* label, rd_node router_node, const char* path, uint32_t flags) {
  rd_node n = rd_node_create_text(ctx, label ? label : "", label ? strlen(label) : 0);
  if (rd_node_is_null(n)) return n;
  if (!rd_node_set_role(ctx, n, RD_ROLE_LINK) || !rdr_link_attach(ctx, n, router_node, path, flags)) {
    rd_node_destroy(ctx, n);
    return (rd_node){0};
  }
  return n;
}
