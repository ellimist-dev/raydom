#include <raydom/raydom.h>
#include <raydom/router/router.h>
#include <stdio.h>

int main(void) {
  rd_ctx* ctx = rd_ctx_create();
  rd_node box = rd_node_create(ctx);
  rd_node_set_style(ctx, box, &(rd_style){.width = rd_px(120), .height = rd_px(40)});
  rd_node_append(ctx, rd_ctx_root(ctx), box);
  rd_ctx_layout(ctx, 800, 600);
  rd_node router = rdr_router_create(ctx, NULL);
  bool    routed = rdr_router_add(ctx, router, &(rdr_route_desc){.pattern = "/"}) && rdr_router_go(ctx, router, "/");
  rd_rect r      = rd_node_rect(ctx, box);
  rd_ctx_destroy(ctx);
  printf("raydom %s: %gx%g\n", RD_VERSION_STRING, r.width, r.height);
  return r.width == 120 && r.height == 40 && routed ? 0 : 1;
}
