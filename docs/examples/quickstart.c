// ANCHOR: all
#include <raydom/raydom.h>
#include <raylib.h>

int main(void) {
  SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
  InitWindow(800, 600, "hello");
  rd_ctx* ctx = rd_ctx_create();
  rd_font_load_file(ctx, FONT_PATH);  // any .ttf or .otf file

  rd_node card = rd_node_create_text(ctx, "Hello from raydom", RD_TEXT_NUL);
  rd_node_set_style(ctx, card,
                    &(rd_style){
                        .padding       = rd_edges_all(rd_px(16)),
                        .border_radius = rd_corners_all(rd_px(8)),
                        .background    = rd_hex(0x1a1f2b),
                        .color         = rd_hex(0xe4e7ee),
                    });
  rd_node_append(ctx, rd_ctx_root(ctx), card);

  while (!WindowShouldClose()) {
    rd_update(ctx);  // input, then layout at the window size
    BeginDrawing();
    ClearBackground(BLACK);
    rd_draw(ctx);
    EndDrawing();
  }
  rd_ctx_destroy(ctx);  // before CloseWindow, so GPU resources are released
  CloseWindow();
}
// ANCHOR_END: all
