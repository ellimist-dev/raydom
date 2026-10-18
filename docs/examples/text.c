// Runs headless as a test for the "Text and fonts" chapter. FONT_PATH is a path to any font file.
#include <assert.h>
#include <raydom/raydom.h>
#include <string.h>

static rd_node add_text(rd_ctx* ctx, rd_node parent, rd_style style, const char* text) {
  rd_node n = rd_node_create(ctx);
  rd_node_set_style(ctx, n, &style);
  rd_node_set_text(ctx, n, text, RD_TEXT_NUL);
  rd_node_append(ctx, parent, n);
  return n;
}

int main(void) {
  rd_ctx* ctx  = rd_ctx_create();
  rd_node root = rd_ctx_root(ctx);

  // ANCHOR: fonts
  rd_font body = rd_font_load_file(ctx, FONT_PATH);  // the first font loaded is the default
  rd_font mono = rd_font_load_file(ctx, FONT_PATH);  // stands in for a second font file
  if (body.id == 0) return 1;                        // a null handle means the file couldn't be loaded

  rd_node code = rd_node_create(ctx);
  rd_node_set_style(ctx, code, &(rd_style){.font = mono, .font_size = rd_px(13)});  // inherited by the text inside
  rd_node_append(ctx, root, code);
  // ANCHOR_END: fonts

  // ANCHOR: text
  rd_node para = add_text(ctx, root, (rd_style){.color = rd_hex(0xe4e7ee), .line_height = rd_num(1.4f)},
                          "Text wraps on spaces to the width its container gives it.");
  rd_node_set_text(ctx, para, "New text; the next layout re-measures it.", RD_TEXT_NUL);
  size_t      len;
  const char* now = rd_node_text(ctx, para, &len);
  // ANCHOR_END: text
  assert(strcmp(now, "New text; the next layout re-measures it.") == 0);

  // ANCHOR: overflow
  rd_node log_line = add_text(ctx, root,
                              (rd_style){.white_space   = RD_WHITE_SPACE_NOWRAP,
                                         .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS},
                              "a single long line that is cut with an ellipsis when it doesn't fit");
  rd_node summary  = add_text(ctx, root,
                              (rd_style){.line_clamp    = rd_num(3),
                                         .text_overflow = RD_TEXT_OVERFLOW_ELLIPSIS},
                              "A paragraph that shows at most three lines and ends the third with an ellipsis.");
  // ANCHOR_END: overflow
  (void)log_line;
  (void)summary;

  // ANCHOR: highlights
  rd_highlight marks[] = {
      {.start = 0, .end = 3, .background = rd_hex(0xffd84d), .color = rd_hex(0x15171c)},  // "New"
  };
  rd_node_set_highlights(ctx, para, marks, 1);
  // ANCHOR_END: highlights

  // ANCHOR: measure
  float           w = rd_font_measure(ctx, (rd_font){0, true}, 16, "Hello", RD_TEXT_NUL);  // default font
  rd_font_metrics m = rd_font_get_metrics(ctx, body, 16);
  // ANCHOR_END: measure
  assert(w > 0 && m.ascent > 0);

  // ANCHOR: selection
  rd_ctx_select(ctx, para, 0, para, 8);
  char   buf[64];
  size_t n = rd_ctx_selection_text(ctx, buf, sizeof buf);  // "New text"
  // ANCHOR_END: selection
  assert(n == 8 && strcmp(buf, "New text") == 0);

  rd_ctx_layout(ctx, 400, 300);
  assert(rd_node_rect(ctx, para).height > 0);
  rd_ctx_destroy(ctx);
  return 0;
}
