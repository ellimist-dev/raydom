// Runs headless as a test for the "Styles and classes" chapter.
#include <assert.h>
#include <raydom/raydom.h>

int main(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);
  rd_node node = rd_node_create(ctx);
  rd_node_append(ctx, root, node);

  // ANCHOR: inline
  rd_node_set_style(ctx, node,
                    &(rd_style){
                        .width         = rd_px(240),
                        .padding       = rd_edges_vh(rd_px(8), rd_px(12)),  // vertical, horizontal
                        .border_width  = rd_edges_all(rd_px(1)),
                        .border_color  = rd_hex(0x272d3a),
                        .border_radius = rd_corners_all(rd_px(6)),
                        .background    = rd_hex(0x1a1f2b),
                    });
  rd_node_merge_style(ctx, node, &(rd_style){.padding.left = rd_px(20)});  // only the left side changes
  // ANCHOR_END: inline

  // ANCHOR: classes
  rd_class button_style = rd_class_create(ctx, &(rd_style){.padding    = rd_edges_vh(rd_px(6), rd_px(14)),
                                                          .background = rd_hex(0x1d2230)});
  rd_class primary      = rd_class_create(ctx, &(rd_style){.background = rd_hex(0x5b8cff)});
  rd_class_set_state(ctx, button_style, RD_STATE_HOVER, &(rd_style){.background = rd_hex(0x252c3d)});
  rd_class_set_state(ctx, button_style, RD_STATE_DISABLED, &(rd_style){.opacity = rd_num(0.4f)});

  rd_node ok = rd_node_create(ctx);
  rd_node_append(ctx, root, ok);
  rd_node_add_class(ctx, ok, button_style);
  rd_node_add_class(ctx, ok, primary);  // later classes win: the background is primary's
  // ANCHOR_END: classes

  // ANCHOR: computed
  const rd_style* s = rd_node_computed_style(ctx, ok);
  // background from primary (the later class), padding from button_style (primary doesn't set it)
  // ANCHOR_END: computed
  assert(s->background.b == 0xff && s->padding.top.value == 6 && s->padding.left.value == 14);
  const rd_style* in = rd_node_computed_style(ctx, node);
  assert(in->padding.left.value == 20 && in->padding.top.value == 8);

  // ANCHOR: states
  rd_node_set_states(ctx, ok, RD_STATE_HOVER, true);
  // the hover variant of button_style now beats primary's base background
  // ANCHOR_END: states
  assert(rd_node_computed_style(ctx, ok)->background.r == 0x25);

  // ANCHOR: inherit
  rd_node child = rd_node_create(ctx);
  rd_node_append(ctx, node, child);
  rd_node_merge_style(ctx, node, &(rd_style){.color = rd_hex(0xe4e7ee), .font_size = rd_px(18)});
  // child's computed color and font_size come from node; its background doesn't
  // ANCHOR_END: inherit
  const rd_style* cs = rd_node_computed_style(ctx, child);
  assert(cs->font_size.value == 18 && cs->color.r == 0xe4 && cs->background.a == 0);

  rd_ctx_destroy(ctx);
  return 0;
}
