#ifndef RAYDOM_OVERLAY_H
#define RAYDOM_OVERLAY_H

#include <raydom/render.h>
#include <raydom/style.h>

#ifdef __cplusplus
extern "C" {
#endif

// The overlay: a layer above the rest of the tree for menus, popovers, tooltips and modals. It is a
// child of the root, kept last, and never clipped by other nodes. Appends to the root go before it
// while it is the root's child; if code moves it elsewhere, the next call puts it back last. It always covers the root: its
// position, inset, margin and size properties are fixed, whatever its styles set. It isn't a hit
// target itself, so it doesn't block the content under it; only its children are. Created on first
// use. Give overlay children position: absolute so they don't stack in its flex layout.
rd_node rd_ctx_overlay(rd_ctx* ctx);

RD_ENUM typedef enum rd_side : uint8_t {
  RD_SIDE_BOTTOM,
  RD_SIDE_TOP,
  RD_SIDE_RIGHT,
  RD_SIDE_LEFT,
} rd_side;

// Places a node next to another node, or at a point, after every layout. Zero is the common case:
// below the anchor, start-aligned, flipped and shifted to stay inside the root.
typedef struct rd_anchor {
  rd_node  node RD_DEFAULT({});      // the anchor; null to anchor to (x, y) instead
  float    x RD_DEFAULT({}), y RD_DEFAULT({});      // root coordinates, used when node is null: a context menu at the pointer
  rd_side  side RD_DEFAULT({});      // which side of the anchor the node goes on
  rd_align align RD_DEFAULT({});     // START, CENTER or END along that side; UNSET is START
  float    offset RD_DEFAULT({});    // gap between the anchor and the node, in pixels
  bool     no_flip RD_DEFAULT({});   // keep side even when the node only fits on the opposite one
  bool     no_shift RD_DEFAULT({});  // don't slide the node along the side to keep it inside the root
  bool     match_width RD_DEFAULT({});  // lay the node out at the anchor's width, for dropdowns
} rd_anchor;

// Null anchor clears it. Anchored nodes are positioned in tree order, whatever order they were
// anchored in, so a submenu after its menu in the tree lands next to the item's final position. The node should be absolutely positioned.
bool rd_node_set_anchor(rd_ctx* ctx, rd_node node, const rd_anchor* anchor);
// The side used after flipping, as of the last layout. For pointing an arrow at the anchor.
rd_side rd_node_anchor_side(const rd_ctx* ctx, rd_node node);

#ifdef __cplusplus
}
#endif

#endif
