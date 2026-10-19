#ifndef RAYDOM_LAYOUT_H
#define RAYDOM_LAYOUT_H

#include <raydom/tree.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rd_rect {
  float x, y, width, height;
} rd_rect;

typedef struct rd_size {
  float width, height;
} rd_size;

typedef struct rd_insets {
  float top, right, bottom, left;
} rd_insets;

typedef struct rd_box {
  rd_insets margin, border, padding;
} rd_box;

typedef enum rd_measure_mode : uint8_t {
  RD_MEASURE_UNDEFINED,  // Size freely (max-content). The size argument is NaN.
  RD_MEASURE_EXACTLY,
  RD_MEASURE_AT_MOST,
  RD_MEASURE_MIN_CONTENT,  // Smallest size without overflow, e.g. the longest word. The size argument is NaN.
} rd_measure_mode;

// Sizes are content-box: padding and border excluded. Must not mutate the tree.
typedef rd_size (*rd_measure_fn)(void* data, float width, rd_measure_mode width_mode, float height,
                                 rd_measure_mode height_mode);

// Advances animations, computes styles, then lays out the tree with the root sized to width x height,
// places anchored nodes, and runs the layout hooks.
void rd_ctx_layout(rd_ctx* ctx, float width, float height);

// Called after every layout, with this layout's geometry and scroll offsets. For content that depends
// on its own geometry: a virtualized list adding the rows that scrolled into view, a chat staying
// scrolled to the bottom. If hooks change the tree, layout runs once more before rd_ctx_layout returns;
// hooks don't run again for that pass.
typedef void (*rd_layout_hook)(rd_ctx* ctx, void* data);
// For the context's lifetime.
bool rd_ctx_on_layout(rd_ctx* ctx, rd_layout_hook fn, void* data, rd_destroy_fn destroy);
// For node's lifetime: removed, and destroy(data) run, when the node is destroyed. Skipped while the node
// is detached from the root. Prefer this for hooks that belong to a view.
bool rd_node_on_layout(rd_ctx* ctx, rd_node node, rd_layout_hook fn, void* data, rd_destroy_fn destroy);

// Border box relative to the parent's border box.
rd_rect rd_node_rect(const rd_ctx* ctx, rd_node node);
// Border box relative to the root, after ancestors' scroll offsets.
rd_rect rd_node_bounds(const rd_ctx* ctx, rd_node node);
rd_box  rd_node_box(const rd_ctx* ctx, rd_node node);
// Extent of the padding box and the children's margin boxes, in padding-box coordinates.
rd_size rd_node_content_size(const rd_ctx* ctx, rd_node node);

// Makes the node a measured leaf; its children are not laid out. Null fn clears it.
bool rd_node_set_measure(rd_ctx* ctx, rd_node node, rd_measure_fn fn, void* data, rd_destroy_fn destroy);
// For changes layout can't observe, such as a measured leaf's content changing.
void rd_node_mark_layout_dirty(rd_ctx* ctx, rd_node node);

#ifdef __cplusplus
}
#endif

#endif
