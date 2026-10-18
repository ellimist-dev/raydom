// Compiles with the docs; opens a window, so it doesn't run as a test.
#include <raydom/raydom.h>
#include <raylib.h>

// ANCHOR: paint
typedef struct chart {
  float values[5];
} chart;

static void paint_chart(rd_ctx* ctx, const rd_paint_info* info, void* data) {
  (void)ctx;
  const chart* c = data;
  rd_rect      r = info->content;  // screen coordinates, already scrolled
  rd_paint_scissor(info);          // clip to the visible part of the node
  float bar = r.width / 5;
  for (int i = 0; i < 5; i++) {
    float h = r.height * c->values[i];
    DrawRectangleRec((Rectangle){r.x + bar * (float)i + 4, r.y + r.height - h, bar - 8, h}, SKYBLUE);
  }
}
// ANCHOR_END: paint

int main(void) {
  InitWindow(640, 480, "canvas");
  rd_ctx* ctx = rd_ctx_create();

  // ANCHOR: attach
  static chart data = {{0.2f, 0.5f, 0.9f, 0.4f, 0.7f}};
  rd_node      plot = rd_node_create(ctx);
  rd_node_set_style(ctx, plot, &(rd_style){.height = rd_px(240), .margin = rd_edges_all(rd_px(20))});
  rd_node_set_paint(ctx, plot, paint_chart, &data, NULL);  // NULL: nothing to free when the node goes away
  rd_node_append(ctx, rd_ctx_root(ctx), plot);
  // ANCHOR_END: attach

  while (!WindowShouldClose()) {
    rd_update(ctx);
    BeginDrawing();
    ClearBackground(BLACK);
    rd_draw(ctx);
    EndDrawing();
  }
  rd_ctx_destroy(ctx);
  CloseWindow();
}
