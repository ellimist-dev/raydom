// Runs headless as a test for the "Images" chapter: images decode and lay out without a window.
#include <assert.h>
#include <raydom/raydom.h>
#include <stdlib.h>

int main(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);

  // ANCHOR: load
  rd_image logo = rd_image_load_file(ctx, "logo.png");  // or rd_image_load(ctx, bytes, size) for #embed data
  if (logo.id == 0) {
    // missing or unreadable file: fall back to generated pixels
  }
  // ANCHOR_END: load

  // ANCHOR: pixels
  int            w = 64, h = 32;
  unsigned char* rgba = calloc((size_t)w * (size_t)h, 4);
  for (int i = 0; i < w * h; i++) rgba[i * 4 + 0] = rgba[i * 4 + 3] = 255;  // opaque red
  rd_image swatch = rd_image_from_pixels(ctx, rgba, w, h);                 // copied; rgba can be freed
  free(rgba);
  // ANCHOR_END: pixels

  // ANCHOR: node
  rd_node banner = rd_node_create(ctx);
  rd_node_set_image(ctx, banner, swatch);
  rd_node_set_style(ctx, banner, &(rd_style){.width = rd_px(200)});  // height follows: 100
  rd_node_append(ctx, root, banner);

  rd_node avatar = rd_node_create(ctx);
  rd_node_set_image(ctx, avatar, swatch);
  rd_node_set_style(ctx, avatar, &(rd_style){.width          = rd_px(48),
                                             .height         = rd_px(48),
                                             .object_fit     = RD_OBJECT_FIT_COVER,
                                             .border_radius  = rd_corners_all(rd_px(24))});  // a circle
  rd_node_append(ctx, root, avatar);
  // ANCHOR_END: node

  rd_ctx_layout(ctx, 800, 600);
  assert(rd_node_rect(ctx, banner).width == 200 && rd_node_rect(ctx, banner).height == 100);
  assert(rd_image_size(ctx, swatch).width == 64);

  // ANCHOR: update
  unsigned char frame[16 * 16 * 4] = {0};
  rd_image_update(ctx, swatch, frame, 16, 16);  // a new size: nodes showing it relayout
  rd_image_unload(ctx, logo);                   // safe on a null image
  // ANCHOR_END: update
  rd_ctx_layout(ctx, 800, 600);
  assert(rd_node_rect(ctx, banner).height == 200);

  rd_ctx_destroy(ctx);
  return 0;
}
