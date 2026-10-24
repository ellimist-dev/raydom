// Runs under Node in the Emscripten build (just web-smoke), with overflow checks and half of Emscripten's
// default 64 KB stack: the parts of raydom that have needed the most stack, the extras theme and the
// router, must leave the other half to the application. An overflow corrupts memory silently in a
// browser; here it aborts.
#include <raydom/extras/views.h>
#include <raydom/raydom.h>
#include <raydom/router/router.h>
#include <stdio.h>
#include <string.h>

static rd_node page(rd_ctx* ctx, rd_node router, void* data) {
  (void)router, (void)data;
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &(rd_style){.height = rd_px(20)});
  return n;
}

int main(void) {
  rd_ctx* ctx = rd_ctx_create();
  if (!ctx || !rdx_theme(ctx)) return 1;
  rd_node tabs = rdx_tabs_create(ctx), crumbs = rdx_breadcrumb_create(ctx);
  rd_node app  = rdr_router_create(ctx, &(rdr_router_desc){.transition = RDR_TRANSITION_FADE});
  rdr_router_add(ctx, app, &(rdr_route_desc){.pattern = "/", .title = "Home", .build = page});
  rdr_router_add(ctx, app, &(rdr_route_desc){.pattern = "users/:id", .title = "User :id", .build = page});
  rd_node root = rd_ctx_root(ctx);
  rd_node_append(ctx, root, tabs);
  rd_node_append(ctx, root, crumbs);
  rd_node_append(ctx, root, app);
  bool ok = rdr_router_go(ctx, app, "/") && rdr_router_go(ctx, app, "/users/7") && rdr_router_back(ctx, app);
  rd_ctx_set_time(ctx, 1);
  rd_ctx_layout(ctx, 800, 600);
  ok = ok && !strcmp(rdr_router_path(ctx, app), "/");
  rd_ctx_destroy(ctx);
  printf("web smoke: %s\n", ok ? "ok" : "failed");
  return ok ? 0 : 1;
}
