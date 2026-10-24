#ifndef RAYDOM_ROUTER_H
#define RAYDOM_ROUTER_H

#include <raydom/events.h>
#include <raydom/tree.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A router is a node that shows one page at a time, chosen by matching a path against its routes.
//
// Paths are absolute, like "/users/42/posts": segments separated by slashes. A pattern has one
// entry per segment:
//   users     a literal
//   :id       a parameter, matching any one segment
//   :id?      an optional parameter; edit? is an optional literal
//   *, *rest  a wildcard, last only, matching the remaining segments (zero or more). A named
//             wildcard's value is a parameter, the segments joined with slashes.
// When several routes match, the most specific wins: comparing segment by segment, a literal beats a
// parameter, which beats a wildcard; then a route without a wildcard; then the route added first.
//
// A router created with a parent is nested: it sits inside one of the parent's pages, whose route
// ends in an unnamed *, and matches what that * captured. Routes are relative to it, parameters of
// the routers above it are visible through it, and one history, in the root router, covers the tree.
//
// Pages are built when their route is entered and destroyed when it's left, unless the route has
// RDR_ROUTE_KEEP_ALIVE: then the page is hidden and shown again when the route is entered next, and
// lives as long as the router. A page is also left, and rebuilt, when its parameters change; a kept
// page stays, shown and without a transition, and reads the new values, listening for RD_EVENT_CHANGE
// on the root router.
//
// A navigation resolves the whole tree of routers and then fires RD_EVENT_CHANGE on the root router.
// Navigating from a page's build function or a CHANGE listener queues the navigation, which runs when
// the current one has finished.
//
// The root router also takes, while the pointer or focus is inside it (or, if inside no router, for
// the router used last): the mouse's back and forward buttons, Cmd+[ and Cmd+] (Super, elsewhere), and
// Alt+Left and Alt+Right outside macOS and iOS, where Option+Left moves by word. A browser build checks
// the browser's platform for that.
//
// In a browser (Emscripten), those inputs would otherwise also navigate the page away from the app:
// creating a router cancels the browser's handling of the side buttons over the canvas, and of the
// shortcuts while nothing else on the page has focus. The router's history doesn't sync with the
// browser's yet.

RD_ENUM typedef enum rdr_transition : uint8_t {
  RDR_TRANSITION_NONE,
  RDR_TRANSITION_FADE,   // a crossfade: the old page fades out as the new one fades in, together
  RDR_TRANSITION_SLIDE,  // forward from the right, back from the left
} rdr_transition;

// Every field is optional; a null desc makes a root router without transitions.
typedef struct rdr_router_desc {
  rd_node        parent RD_DEFAULT({});      // null for a root router, with its own history
  rdr_transition transition RD_DEFAULT({});  // for this router's own page changes
  double         duration RD_DEFAULT({});    // seconds; 0 means 0.2
  size_t         history_limit RD_DEFAULT({});  // root only: entries kept; 0 means 100
} rdr_router_desc;

// The router's node is a flex column that clips its pages (overflow hidden); give it a size, for
// example flex 1, and style it like any node. Each page goes in a wrapper filling it, a flex column.
// Shows nothing until the first navigation.
rd_node rdr_router_create(rd_ctx* ctx, const rdr_router_desc* desc);

// Builds a page for a route that was entered. router is the router showing it, for rdr_param and for
// creating nested routers. Null shows nothing.
typedef rd_node (*rdr_build_fn)(rd_ctx* ctx, rd_node router, void* data);

enum { RDR_ROUTE_KEEP_ALIVE = 1u << 0 };

// A route's match on some path, for title functions: the parameters it bound, and those of the
// routers above.
typedef struct rdr_match rdr_match;
const char* rdr_match_param(const rdr_match* match, const char* name);  // null if absent
const char* rdr_match_path(const rdr_match* match);                     // absolute

// Names a route for rdr_router_trail, for example from data: "Loom" for :id = "loom". The string is
// copied; it must stay valid until the function returns. Null falls back to the route's title.
typedef const char* (*rdr_title_fn)(rd_ctx* ctx, const rdr_match* match, void* data);

typedef struct rdr_route_desc {
  const char*   pattern RD_DEFAULT({});  // copied
  const char*   title RD_DEFAULT({});    // optional, for rdr_router_trail; :name is replaced by the parameter
  rdr_title_fn  title_fn RD_DEFAULT({}); // optional; takes precedence over title
  rdr_build_fn  build RD_DEFAULT({});
  void*         data RD_DEFAULT({});
  rd_destroy_fn destroy RD_DEFAULT({});  // destroy(data) runs when the router is destroyed
  uint32_t      flags RD_DEFAULT({});    // RDR_ROUTE_*
} rdr_route_desc;

// Adds a route. Takes effect at the next navigation. Patterns and paths have any number of segments.
// False for a malformed pattern (a wildcard not last, an empty name, more than 16 optional segments) or a node that isn't a router; data isn't kept.
bool rdr_router_add(rd_ctx* ctx, rd_node router, const rdr_route_desc* desc);

// Navigation. path is absolute ("/a/b") or relative to the router's base ("b", "../c"). Any router in
// a tree navigates the whole tree. go pushes a history entry, dropping the entries after the current
// one; replace overwrites the current entry. False when the root router has no route for the path, or
// for a node that isn't a router; history doesn't change then. Navigating to the current path does
// nothing and returns true.
bool rdr_router_go(rd_ctx* ctx, rd_node router, const char* path);
bool rdr_router_replace(rd_ctx* ctx, rd_node router, const char* path);
// One entry back or forward. False at either end of the history.
bool rdr_router_back(rd_ctx* ctx, rd_node router);
bool rdr_router_forward(rd_ctx* ctx, rd_node router);
bool rdr_router_can_back(const rd_ctx* ctx, rd_node router);
bool rdr_router_can_forward(const rd_ctx* ctx, rd_node router);

// The tree's current path, normalized ("/" before the first navigation). Valid until the next one.
const char* rdr_router_path(const rd_ctx* ctx, rd_node router);
// The path where the router is mounted: "/" for a root router, "/settings" for one nested under a
// route matching /settings. Relative paths resolve against it.
const char* rdr_router_base(const rd_ctx* ctx, rd_node router);
// The matched route's pattern, as added, or null if none matched.
const char* rdr_router_route(const rd_ctx* ctx, rd_node router);
// The page's wrapper, a child of the router; its child is the page. Null if none.
rd_node     rdr_router_page(const rd_ctx* ctx, rd_node router);
rd_node     rdr_router_root(const rd_ctx* ctx, rd_node router);
// True while the router's pages are changing places. The old page doesn't take input meanwhile.
bool        rdr_router_transitioning(const rd_ctx* ctx, rd_node router);
bool        rdr_router_set_transition(rd_ctx* ctx, rd_node router, rdr_transition transition, double duration);

// A parameter of the route matched by the router of node (node itself, or its nearest ancestor that is
// a router), or of the routers above it. Null if there is no such parameter. Valid until the next
// navigation.
const char* rdr_param(const rd_ctx* ctx, rd_node node, const char* name);

// Breadcrumbs: the titled routes along the current path, from the root router down. For each prefix of
// the path, the route that matches that prefix exactly gives a crumb: at /projects/loom/files/main.c,
// routes projects, projects/:id and projects/:id/files/:file give three. Then the matched route gives
// one. A prefix matched only through a wildcard gives none, nor does a pattern starting with one, and
// a crumb whose path repeats the one before is dropped, so a nested router's "" route doesn't repeat
// its parent's crumb. Nested routers are followed into the first one in each page. Strings are valid
// until the next navigation.
typedef struct rdr_crumb {
  const char* title;
  const char* path;  // absolute: where the route's segments end
} rdr_crumb;
// Copies up to cap crumbs into out and returns how many there are.
size_t rdr_router_trail(const rd_ctx* ctx, rd_node router, rdr_crumb* out, size_t cap);

// Links. A click on node, or Enter on it, navigates router to path; with a null router, the nearest
// router above the node, or the router used last. The node has RD_STATE_CHECKED while the current
// path is the link's or lies under it ("/users" under "/users/42"); for "/", or with RDR_LINK_EXACT,
// only while it is the link's.
enum { RDR_LINK_EXACT = 1u << 0 };
bool    rdr_link_attach(rd_ctx* ctx, rd_node node, rd_node router, const char* path, uint32_t flags);
// A text node with the link role, focusable, with the link attached.
rd_node rdr_link_create(rd_ctx* ctx, const char* label, rd_node router, const char* path, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
