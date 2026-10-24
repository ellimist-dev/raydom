#include <raydom/raydom.h>
#include <raydom/router/router.h>
#include <string.h>

#include "check.h"

static bool streq(const char* a, const char* b) { return a && b && !strcmp(a, b); }

typedef struct counts {
  int builds, destroys, changes;
} counts;

static counts c;

// A page that counts its builds and labels itself with its route's pattern.
static rd_node page(rd_ctx* ctx, rd_node router, void* data) {
  (void)router;
  c.builds++;
  const char* label = data ? data : "";
  rd_node     n     = rd_node_create_text(ctx, label, strlen(label));
  rd_node_set_style(ctx, n, &(rd_style){.height = rd_px(20)});
  return n;
}

static void count_destroy(void* data) {
  (void)data;
  c.destroys++;
}

static void count_change(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx, (void)ev, (void)data;
  c.changes++;
}

static bool add(rd_ctx* ctx, rd_node r, const char* pattern, uint32_t flags) {
  return rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = pattern, .build = page, .data = (void*)pattern, .flags = flags});
}

static rd_ctx* setup(rd_node* r, const rdr_router_desc* desc) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  c  = (counts){0};
  *r = rdr_router_create(ctx, desc);
  rd_node_set_style(ctx, *r, &(rd_style){.display = RD_DISPLAY_FLEX, .width = rd_px(400), .height = rd_px(300),
                                          .position = RD_POSITION_RELATIVE, .overflow_x = RD_OVERFLOW_HIDDEN,
                                          .overflow_y = RD_OVERFLOW_HIDDEN, .flex_direction = RD_FLEX_COLUMN});
  rd_node_append(ctx, rd_ctx_root(ctx), *r);
  rd_node_on(ctx, *r, RD_EVENT_CHANGE, count_change, NULL, NULL, 0);
  return ctx;
}

// The page shown: the wrapper's child.
static rd_node shown(rd_ctx* ctx, rd_node r) { return rd_node_first_child(ctx, rdr_router_page(ctx, r)); }

static void test_matching(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, NULL);
  CHECK(add(ctx, r, "/", 0) && add(ctx, r, "users", 0) && add(ctx, r, "users/:id", 0) && add(ctx, r, "users/new", 0));
  CHECK(add(ctx, r, "users/:id/edit?", 0) && add(ctx, r, "files/*path", 0) && add(ctx, r, "*", 0));
  CHECK(add(ctx, r, "docs/:lang?/intro", 0));
  CHECK(!add(ctx, r, "a/*/b", 0) && !add(ctx, r, ":", 0) && !add(ctx, r, "a/*x?", 0) && !add(ctx, r, "?", 0));
  CHECK(rdr_router_route(ctx, r) == NULL && streq(rdr_router_path(ctx, r), "/") && rd_node_is_null(shown(ctx, r)));

  CHECK(rdr_router_go(ctx, r, "/"));
  CHECK(streq(rdr_router_route(ctx, r), "/"));
  CHECK(rdr_router_go(ctx, r, "/users/new") && streq(rdr_router_route(ctx, r), "users/new"));
  CHECK(rdr_router_go(ctx, r, "/users/42") && streq(rdr_router_route(ctx, r), "users/:id"));
  CHECK(streq(rdr_param(ctx, r, "id"), "42") && !rdr_param(ctx, r, "path"));
  // The page and its descendants see the router's parameters.
  CHECK(streq(rdr_param(ctx, shown(ctx, r), "id"), "42"));
  // An optional literal. For /users/42, users/:id/edit? ties with users/:id, which was added first.
  CHECK(rdr_router_go(ctx, r, "/users/42/edit") && streq(rdr_router_route(ctx, r), "users/:id/edit?"));
  CHECK(rdr_router_go(ctx, r, "/files/a/b/c.txt") && streq(rdr_param(ctx, r, "path"), "a/b/c.txt"));
  CHECK(rdr_router_go(ctx, r, "/files") && streq(rdr_router_route(ctx, r), "files/*path") && streq(rdr_param(ctx, r, "path"), ""));
  CHECK(rdr_router_go(ctx, r, "/nowhere/at/all") && streq(rdr_router_route(ctx, r), "*"));
  CHECK(rdr_router_go(ctx, r, "/docs/intro") && streq(rdr_router_route(ctx, r), "docs/:lang?/intro") && !rdr_param(ctx, r, "lang"));
  CHECK(rdr_router_go(ctx, r, "/docs/fr/intro") && streq(rdr_param(ctx, r, "lang"), "fr"));

  // Normalization and relative paths.
  CHECK(rdr_router_go(ctx, r, "//users/./7/../9/") && streq(rdr_router_path(ctx, r), "/users/9"));
  CHECK(rdr_router_go(ctx, r, "edit") && streq(rdr_router_path(ctx, r), "/edit"));
  CHECK(rdr_router_go(ctx, r, "../../..") && streq(rdr_router_path(ctx, r), "/"));
  rd_ctx_destroy(ctx);
}

static void test_no_match(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, NULL);
  CHECK(add(ctx, r, "a", 0));
  CHECK(rdr_router_go(ctx, r, "/a") && !rdr_router_go(ctx, r, "/b"));
  CHECK(streq(rdr_router_path(ctx, r), "/a") && !rdr_router_can_back(ctx, r) && c.changes == 1);
  CHECK(!rdr_router_go(ctx, rd_ctx_root(ctx), "/a") && !rdr_router_add(ctx, rd_ctx_root(ctx), &(rdr_route_desc){.pattern = "x"}));
  rd_ctx_destroy(ctx);
}

static void test_history(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, &(rdr_router_desc){.history_limit = 4});
  CHECK(add(ctx, r, ":page", 0));
  CHECK(!rdr_router_back(ctx, r) && !rdr_router_forward(ctx, r));
  rdr_router_go(ctx, r, "/a");
  rdr_router_go(ctx, r, "/b");
  rdr_router_go(ctx, r, "/c");
  CHECK(rdr_router_go(ctx, r, "/c") && c.changes == 3);  // the current path: nothing happens
  CHECK(rdr_router_back(ctx, r) && streq(rdr_router_path(ctx, r), "/b"));
  CHECK(rdr_router_back(ctx, r) && streq(rdr_router_path(ctx, r), "/a"));
  CHECK(!rdr_router_back(ctx, r) && !rdr_router_can_back(ctx, r) && rdr_router_can_forward(ctx, r));
  CHECK(rdr_router_forward(ctx, r) && streq(rdr_router_path(ctx, r), "/b"));
  CHECK(rdr_router_go(ctx, r, "/d") && !rdr_router_can_forward(ctx, r));  // drops /c
  CHECK(rdr_router_replace(ctx, r, "/e") && streq(rdr_router_path(ctx, r), "/e"));
  CHECK(rdr_router_back(ctx, r) && streq(rdr_router_path(ctx, r), "/b") && rdr_router_forward(ctx, r));
  CHECK(streq(rdr_router_path(ctx, r), "/e") && !rdr_router_can_forward(ctx, r));
  // The limit drops the oldest entries: a b e, then f g, keeps b e f g.
  rdr_router_go(ctx, r, "/f");
  rdr_router_go(ctx, r, "/g");
  CHECK(rdr_router_back(ctx, r) && rdr_router_back(ctx, r) && rdr_router_back(ctx, r));
  CHECK(streq(rdr_router_path(ctx, r), "/b") && !rdr_router_can_back(ctx, r));
  rd_ctx_destroy(ctx);
}

static void test_lifetime(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, NULL);
  CHECK(rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "a", .build = page, .destroy = count_destroy}));
  CHECK(add(ctx, r, "kept/:id?", RDR_ROUTE_KEEP_ALIVE) && add(ctx, r, "user/:id", 0));
  rdr_router_go(ctx, r, "/a");
  rd_node a = shown(ctx, r);
  rdr_router_go(ctx, r, "/kept");
  rd_node kept = shown(ctx, r), kept_slot = rdr_router_page(ctx, r);
  CHECK(!rd_node_alive(ctx, a) && rd_node_alive(ctx, kept) && c.builds == 2);
  rdr_router_go(ctx, r, "/a");
  CHECK(rd_node_alive(ctx, kept) && rd_node_child_count(ctx, r) == 2);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_rect(ctx, kept_slot).height == 0);  // hidden
  // Back to the kept page, with another parameter: the same page, not rebuilt.
  rdr_router_go(ctx, r, "/kept/3");
  CHECK(rd_node_eq(shown(ctx, r), kept) && c.builds == 3 && streq(rdr_param(ctx, kept, "id"), "3"));
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_rect(ctx, kept_slot).height == 300);
  // Other pages are rebuilt when their parameters change.
  rdr_router_go(ctx, r, "/user/1");
  rd_node u1 = shown(ctx, r);
  rdr_router_go(ctx, r, "/user/2");
  CHECK(!rd_node_alive(ctx, u1) && rd_node_alive(ctx, shown(ctx, r)) && c.builds == 5);
  rd_node_destroy(ctx, r);
  CHECK(c.destroys == 1 && !rd_node_alive(ctx, kept));
  rd_ctx_destroy(ctx);
}

// Nested routers: /settings/* holds a router with its own routes.
static rd_node nested_inner;
static int     settings_builds;

static rd_node settings(rd_ctx* ctx, rd_node router, void* data) {
  (void)data;
  settings_builds++;
  rd_node box  = rd_node_create(ctx);
  rd_node_set_style(ctx, box, &(rd_style){.flex_grow = rd_num(1)});
  nested_inner = rdr_router_create(ctx, &(rdr_router_desc){.parent = router});
  rdr_router_add(ctx, nested_inner, &(rdr_route_desc){.pattern = "", .title = "General", .build = page, .data = "general"});
  rdr_router_add(ctx, nested_inner, &(rdr_route_desc){.pattern = "profile", .title = "Profile", .build = page, .data = "profile"});
  rdr_router_add(ctx, nested_inner, &(rdr_route_desc){.pattern = "users/:user", .title = "User :user", .build = page});
  rd_node_append(ctx, box, nested_inner);
  return box;
}

static void test_nested(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, NULL);
  settings_builds = 0;
  CHECK(rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "/", .title = "Home", .build = page}));
  CHECK(rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "org/:org/settings/*", .title = ":org settings", .build = settings}));
  CHECK(rdr_router_go(ctx, r, "/org/acme/settings/profile"));
  rd_node inner = nested_inner;
  CHECK(streq(rdr_router_route(ctx, inner), "profile") && streq(rdr_router_base(ctx, inner), "/org/acme/settings"));
  CHECK(streq(rdr_router_base(ctx, r), "/") && rd_node_eq(rdr_router_root(ctx, inner), r));
  CHECK(streq(rdr_param(ctx, inner, "org"), "acme") && streq(rdr_router_path(ctx, inner), "/org/acme/settings/profile"));

  rdr_crumb trail[4];
  CHECK(rdr_router_trail(ctx, inner, trail, 4) == 3 && streq(trail[0].title, "Home") && streq(trail[0].path, "/"));
  CHECK(streq(trail[1].title, "acme settings") && streq(trail[1].path, "/org/acme/settings"));
  CHECK(streq(trail[2].title, "Profile") && streq(trail[2].path, "/org/acme/settings/profile"));

  // Relative to the nested router; the outer page stays.
  CHECK(rdr_router_go(ctx, inner, "users/ann") && streq(rdr_router_path(ctx, r), "/org/acme/settings/users/ann"));
  CHECK(settings_builds == 1 && streq(rdr_param(ctx, inner, "user"), "ann") && c.changes == 2);
  CHECK(rdr_router_trail(ctx, r, trail, 4) == 3 && streq(trail[2].title, "User ann"));
  CHECK(rdr_router_go(ctx, inner, ".") && streq(rdr_router_route(ctx, inner), "") && settings_builds == 1);
  // The nested "" route's crumb would repeat its parent's path, so it's dropped.
  CHECK(rdr_router_trail(ctx, r, trail, 4) == 2 && streq(trail[1].title, "acme settings") && streq(trail[1].path, "/org/acme/settings"));
  CHECK(rdr_router_back(ctx, inner) && streq(rdr_router_route(ctx, inner), "users/:user"));

  // Another org rebuilds the outer page, and with it the nested router.
  CHECK(rdr_router_go(ctx, r, "/org/zeta/settings") && settings_builds == 2 && !rd_node_alive(ctx, inner));
  CHECK(streq(rdr_router_route(ctx, nested_inner), "") && streq(rdr_param(ctx, nested_inner, "org"), "zeta"));
  CHECK(rdr_router_go(ctx, nested_inner, "/") && rdr_router_trail(ctx, r, NULL, 0) == 1);
  CHECK(rdr_router_back(ctx, r) && settings_builds == 3 && streq(rdr_router_path(ctx, r), "/org/zeta/settings"));
  rd_ctx_destroy(ctx);
}

// Crumbs for each prefix of the path that a route matches exactly.
static const char* project_name(rd_ctx* ctx, const rdr_match* m, void* data) {
  (void)ctx, (void)data;
  const char* id = rdr_match_param(m, "id");
  return id && !strcmp(id, "loom") ? "Loom" : NULL;  // null: the static title
}

static void test_trail(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, NULL);
  rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "/", .title = "Home", .build = page});
  rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "projects", .title = "Projects", .build = page});
  rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "projects/new", .build = page});  // untitled
  rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "projects/:id", .title = "Project :id", .title_fn = project_name, .build = page});
  rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "projects/:id/files/:file", .title = ":file", .build = page});
  rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "*", .title = "Not found", .build = page});

  rdr_crumb t[8];
  CHECK(rdr_router_go(ctx, r, "/projects/loom/files/main.c") && rdr_router_trail(ctx, r, t, 8) == 4);
  CHECK(streq(t[0].title, "Home") && streq(t[0].path, "/") && streq(t[1].title, "Projects") && streq(t[1].path, "/projects"));
  CHECK(streq(t[2].title, "Loom") && streq(t[2].path, "/projects/loom"));  // named by the function
  CHECK(streq(t[3].title, "main.c") && streq(t[3].path, "/projects/loom/files/main.c"));
  // /projects/loom/files has no route of its own: no crumb, and the catch-all doesn't fill in.
  CHECK(rdr_router_go(ctx, r, "/projects/orbit") && rdr_router_trail(ctx, r, t, 8) == 3 && streq(t[2].title, "Project orbit"));
  // The most specific route names a prefix even without a title: /projects/new/files/x has no "Project new".
  CHECK(rdr_router_go(ctx, r, "/projects/new/files/x") && rdr_router_trail(ctx, r, t, 8) == 3 && streq(t[2].title, "x"));
  // The catch-all names only the page it shows.
  CHECK(rdr_router_go(ctx, r, "/projects/loom/nope") && rdr_router_trail(ctx, r, t, 8) == 4);
  CHECK(streq(t[2].title, "Loom") && streq(t[3].title, "Not found") && streq(t[3].path, "/projects/loom/nope"));
  CHECK(rdr_router_trail(ctx, r, t, 2) == 4 && streq(t[1].title, "Projects"));  // a short buffer still counts all
  rd_ctx_destroy(ctx);
}

static void test_transitions(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, &(rdr_router_desc){.transition = RDR_TRANSITION_SLIDE, .duration = 0.2});
  rd_ctx_set_time(ctx, 1);
  CHECK(add(ctx, r, "a", 0) && add(ctx, r, "b", 0) && add(ctx, r, "c", RDR_ROUTE_KEEP_ALIVE));
  rdr_router_go(ctx, r, "/a");
  CHECK(!rdr_router_transitioning(ctx, r));  // the first page doesn't animate
  rd_node a_slot = rdr_router_page(ctx, r);
  rd_ctx_layout(ctx, 800, 600);

  rdr_router_go(ctx, r, "/b");
  rd_node b_slot = rdr_router_page(ctx, r);
  CHECK(rdr_router_transitioning(ctx, r) && rd_node_alive(ctx, a_slot));
  CHECK(rd_node_states(ctx, a_slot) & RD_STATE_DISABLED);
  rd_ctx_set_time(ctx, 1.1);
  rd_ctx_layout(ctx, 800, 600);
  // Halfway: the new page comes from the right, the old one leaves to the left.
  rd_rect na = rd_node_rect(ctx, b_slot), old = rd_node_rect(ctx, a_slot);
  CHECK(na.x > 0 && na.x < 400 && old.x < 0 && old.x > -400 && na.width == 400 && old.width == 400);
  rd_ctx_set_time(ctx, 1.25);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(!rdr_router_transitioning(ctx, r) && !rd_node_alive(ctx, a_slot) && rd_node_rect(ctx, b_slot).x == 0);

  // Back slides the other way. Interrupting finishes the running transition first.
  rdr_router_go(ctx, r, "/c");
  rd_node c_slot = rdr_router_page(ctx, r);
  rd_ctx_set_time(ctx, 1.3);
  rdr_router_back(ctx, r);
  CHECK(!rd_node_alive(ctx, b_slot) && rd_node_alive(ctx, c_slot));
  rd_ctx_set_time(ctx, 1.35);
  rd_ctx_layout(ctx, 800, 600);
  rd_node b2 = rdr_router_page(ctx, r);
  CHECK(rd_node_rect(ctx, b2).x < 0 && rd_node_rect(ctx, c_slot).x > 0);  // from the left; c leaves right
  rd_ctx_set_time(ctx, 2);
  rd_ctx_layout(ctx, 800, 600);
  CHECK(rd_node_alive(ctx, c_slot) && rd_node_rect(ctx, c_slot).height == 0 && rd_node_rect(ctx, b2).x == 0);

  // Destroyed mid-transition: the pending end does nothing.
  rdr_router_go(ctx, r, "/a");
  rd_node_destroy(ctx, r);
  rd_ctx_set_time(ctx, 3);
  rd_ctx_layout(ctx, 800, 600);

  // Fade.
  rd_node f = rdr_router_create(ctx, &(rdr_router_desc){.transition = RDR_TRANSITION_FADE});
  rd_node_set_style(ctx, f, &(rd_style){.width = rd_px(100), .height = rd_px(100)});
  rd_node_append(ctx, rd_ctx_root(ctx), f);
  add(ctx, f, "a", 0);
  add(ctx, f, "b", 0);
  rdr_router_go(ctx, f, "/a");
  rdr_router_go(ctx, f, "/b");
  CHECK(rdr_router_transitioning(ctx, f));
  CHECK(rdr_router_set_transition(ctx, f, RDR_TRANSITION_NONE, 0) && !rdr_router_transitioning(ctx, f));
  rdr_router_go(ctx, f, "/a");
  CHECK(!rdr_router_transitioning(ctx, f) && rd_node_child_count(ctx, f) == 1);
  rd_ctx_destroy(ctx);
}

static void test_input(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, NULL);
  add(ctx, r, ":p", 0);
  rdr_router_go(ctx, r, "/a");
  rdr_router_go(ctx, r, "/b");
  rd_ctx_layout(ctx, 800, 600);
  rd_ctx_key(ctx, RD_KEY_LEFT_BRACKET, true, false, RD_MOD_SUPER);
  rd_ctx_key(ctx, RD_KEY_LEFT_BRACKET, false, false, RD_MOD_SUPER);
  CHECK(streq(rdr_router_path(ctx, r), "/a"));
  rd_ctx_key(ctx, RD_KEY_RIGHT_BRACKET, true, false, RD_MOD_SUPER);
  CHECK(streq(rdr_router_path(ctx, r), "/b"));
  rd_ctx_key(ctx, RD_KEY_LEFT_BRACKET, true, false, RD_MOD_SUPER | RD_MOD_SHIFT);  // other modifiers: no
  CHECK(streq(rdr_router_path(ctx, r), "/b"));
#ifndef __APPLE__
  rd_ctx_key(ctx, RD_KEY_LEFT, true, false, RD_MOD_ALT);
  CHECK(streq(rdr_router_path(ctx, r), "/a"));
  rd_ctx_key(ctx, RD_KEY_RIGHT, true, false, RD_MOD_ALT);
  CHECK(streq(rdr_router_path(ctx, r), "/b"));
#endif
  // The mouse's side buttons, over the router or anywhere else.
  rd_ctx_pointer(ctx, 10, 10, RD_BUTTON_BACK, 0, 0);
  rd_ctx_pointer(ctx, 10, 10, 0, 0, 0);
  CHECK(streq(rdr_router_path(ctx, r), "/a"));
  rd_ctx_pointer(ctx, 700, 500, RD_BUTTON_FORWARD, 0, 0);
  rd_ctx_pointer(ctx, 700, 500, 0, 0, 0);
  CHECK(streq(rdr_router_path(ctx, r), "/b"));
  rd_ctx_destroy(ctx);
}

static void click(rd_ctx* ctx, rd_node n) {
  rd_ctx_layout(ctx, 800, 600);
  rd_rect b = rd_node_bounds(ctx, n);
  rd_ctx_pointer(ctx, b.x + 2, b.y + 2, RD_BUTTON_PRIMARY, 0, 0);
  rd_ctx_pointer(ctx, b.x + 2, b.y + 2, 0, 0, 0);
}

static void test_links(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, RAYDOM_TEST_FONT);
  rd_node root = rd_ctx_root(ctx);
  rd_node_set_style(ctx, root, &(rd_style){.flex_direction = RD_FLEX_ROW, .align_items = RD_ALIGN_START});
  rd_node nav = rd_node_create(ctx);
  rd_node_set_style(ctx, nav, &(rd_style){.flex_direction = RD_FLEX_COLUMN, .width = rd_px(100)});
  rd_node r = rdr_router_create(ctx, NULL);
  rd_node_set_style(ctx, r, &(rd_style){.width = rd_px(300), .height = rd_px(300)});
  rd_node_append(ctx, root, nav);
  rd_node_append(ctx, root, r);
  add(ctx, r, "/", 0);
  add(ctx, r, "users/:id?", 0);
  rd_node home  = rdr_link_create(ctx, "Home", r, "/", 0);
  rd_node users = rdr_link_create(ctx, "Users", (rd_node){0}, "/users", 0);  // the router used last
  rd_node exact = rdr_link_create(ctx, "All", r, "/users", RDR_LINK_EXACT);
  CHECK(!rd_node_is_null(home) && !rd_node_is_null(users) && rd_node_role(ctx, home) == RD_ROLE_LINK);
  CHECK(!rdr_link_attach(ctx, home, r, "/x", 0));  // already a link
  rd_node_append(ctx, nav, home);
  rd_node_append(ctx, nav, users);
  rd_node_append(ctx, nav, exact);
  rdr_router_go(ctx, r, "/");
  CHECK((rd_node_states(ctx, home) & RD_STATE_CHECKED) && !(rd_node_states(ctx, users) & RD_STATE_CHECKED));

  click(ctx, users);
  CHECK(streq(rdr_router_path(ctx, r), "/users") && !(rd_node_states(ctx, home) & RD_STATE_CHECKED));
  CHECK((rd_node_states(ctx, users) & RD_STATE_CHECKED) && (rd_node_states(ctx, exact) & RD_STATE_CHECKED));
  rdr_router_go(ctx, r, "/users/5");
  CHECK((rd_node_states(ctx, users) & RD_STATE_CHECKED) && !(rd_node_states(ctx, exact) & RD_STATE_CHECKED));

  // Enter on a focused link.
  rd_node_focus(ctx, home);
  rd_ctx_key(ctx, RD_KEY_ENTER, true, false, 0);
  rd_ctx_key(ctx, RD_KEY_ENTER, false, false, 0);
  CHECK(streq(rdr_router_path(ctx, r), "/"));

  // A link inside a page uses the router around it, relative to its base.
  rd_node in_page = rdr_link_create(ctx, "Seven", (rd_node){0}, "users/7", 0);
  rd_node_append(ctx, rdr_router_page(ctx, r), in_page);
  click(ctx, in_page);
  CHECK(streq(rdr_router_path(ctx, r), "/users/7") && !rd_node_alive(ctx, in_page));
  rd_ctx_destroy(ctx);
}

// Navigating from a build function is queued until the current navigation ends.
static rd_node redirect(rd_ctx* ctx, rd_node router, void* data) {
  (void)data;
  CHECK(rdr_router_replace(ctx, router, "/new"));
  CHECK(streq(rdr_router_path(ctx, router), "/old"));  // not yet
  return (rd_node){0};
}

static rd_node destroys_router(rd_ctx* ctx, rd_node router, void* data) {
  (void)data;
  rd_node_destroy(ctx, router);
  return rd_node_create(ctx);
}

static void loop(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)data;
  rdr_router_go(ctx, ev->current, streq(rdr_router_path(ctx, ev->current), "/x") ? "/y" : "/x");
}

static void test_reentrancy(void) {
  rd_node r;
  rd_ctx* ctx = setup(&r, NULL);
  add(ctx, r, "new", 0);
  add(ctx, r, "/", 0);
  rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "old", .build = redirect});
  rdr_router_add(ctx, r, &(rdr_route_desc){.pattern = "boom", .build = destroys_router});
  rdr_router_go(ctx, r, "/");
  CHECK(rdr_router_go(ctx, r, "/old") && streq(rdr_router_path(ctx, r), "/new") && c.changes == 3);
  CHECK(rdr_router_back(ctx, r) && streq(rdr_router_path(ctx, r), "/"));  // /old was replaced
  CHECK(rdr_router_go(ctx, r, "/boom") && !rd_node_alive(ctx, r));
  rd_node_destroy(ctx, r);

  // A listener that always navigates stops after a bounded number of rounds.
  rd_node l = rdr_router_create(ctx, NULL);
  add(ctx, l, ":p", 0);
  rd_node_on(ctx, l, RD_EVENT_CHANGE, loop, NULL, NULL, 0);
  CHECK(rdr_router_go(ctx, l, "/a"));
  CHECK(rdr_router_go(ctx, l, "/b"));
  rd_ctx_destroy(ctx);
}

int main(void) {
  test_matching();
  test_no_match();
  test_history();
  test_lifetime();
  test_nested();
  test_trail();
  test_transitions();
  test_input();
  test_links();
  test_reentrancy();
  return TEST_RESULT();
}
