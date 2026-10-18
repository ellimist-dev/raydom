#include "internal.h"
#include "text_internal.h"

bool rd_node_set_role(rd_ctx* ctx, rd_node node, rd_role role) {
  rd_slot* s = rd__slot(ctx, node);
  if (!s) return false;
  s->role = role;
  return true;
}

rd_role rd_node_role(const rd_ctx* ctx, rd_node node) {
  const rd_slot* s = rd__slot(ctx, node);
  if (!s) return RD_ROLE_AUTO;
  if (s->role != RD_ROLE_AUTO) return s->role;
  const rd_text* t = rd__text_get(ctx, node.index);
  if (t && t->input) return rd__input_multiline(t->input) ? RD_ROLE_MULTILINE_TEXT_INPUT : RD_ROLE_TEXT_INPUT;
  if (t) return RD_ROLE_TEXT;
  if (ctx->layouts[node.index].paint_fn) return RD_ROLE_CANVAS;
  return RD_ROLE_GENERIC;
}

bool rd__role_interactive(const rd_ctx* ctx, uint32_t i) {
  switch (ctx->slots[i].role) {
    case RD_ROLE_BUTTON:
    case RD_ROLE_LINK:
    case RD_ROLE_CHECKBOX:
    case RD_ROLE_RADIO_BUTTON:
      return true;
    default:
      return false;
  }
}

rd_node rd_node_create_text(rd_ctx* ctx, const char* utf8, size_t len) {
  rd_node n = rd_node_create(ctx);
  if (rd_node_is_null(n)) return n;
  if (!rd_node_set_text(ctx, n, utf8 ? utf8 : "", utf8 ? len : 0)) {
    rd_node_destroy(ctx, n);
    return (rd_node){0};
  }
  return n;
}

rd_node rd_node_create_button(rd_ctx* ctx, const char* utf8, size_t len) {
  rd_node n = utf8 ? rd_node_create_text(ctx, utf8, len) : rd_node_create(ctx);
  rd_node_set_role(ctx, n, RD_ROLE_BUTTON);
  return n;
}

rd_node rd_node_create_input(rd_ctx* ctx, uint32_t flags) {
  rd_node n = rd_node_create(ctx);
  if (!rd_node_is_null(n) && !rd_node_make_input(ctx, n, flags)) {
    rd_node_destroy(ctx, n);
    return (rd_node){0};
  }
  return n;
}
