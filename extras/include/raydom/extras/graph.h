#ifndef RAYDOM_EXTRAS_GRAPH_H
#define RAYDOM_EXTRAS_GRAPH_H

#include <raydom/raydom.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// A network view: lit 3D spheres joined by edges, on a canvas you can pan and zoom. Positions are
// in world units on a plane, y down; the view maps them to the screen. Nothing here advances time:
// messages in flight are drawn at whatever progress the caller sets.

typedef struct rdx_graph rdx_graph;

// Generational handles; zero is null.
typedef struct rdx_node {
  uint32_t index, gen;
} rdx_node;

typedef struct rdx_edge {
  uint32_t index, gen;
} rdx_edge;

typedef struct rdx_node_desc {
  rd_vec2     position RD_DEFAULT({});
  float       radius RD_DEFAULT({});  // world units; 0 means 20
  rd_color    color RD_DEFAULT({});
  const char* label RD_DEFAULT({});  // optional
} rdx_node_desc;

typedef struct rdx_edge_desc {
  rd_color    color RD_DEFAULT({});
  float       width RD_DEFAULT({});  // screen pixels; 0 means 2
  bool        directed RD_DEFAULT({});
  const char* label RD_DEFAULT({});  // optional, drawn at the midpoint
} rdx_edge_desc;

typedef struct rdx_message {
  rdx_node from RD_DEFAULT({}), to RD_DEFAULT({});
  float    progress RD_DEFAULT({});  // 0 at from, 1 at to
  rd_color color RD_DEFAULT({});
  float    radius RD_DEFAULT({});  // world units; 0 means 6
} rdx_message;

RD_ENUM typedef enum rdx_graph_event : uint8_t {
  RDX_EVENT_NODE_ENTER,
  RDX_EVENT_NODE_LEAVE,
  RDX_EVENT_NODE_CLICK,  // press and release on the same node without dragging
  RDX_EVENT_NODE_DRAG,   // with RDX_GRAPH_DRAG_NODES
  RDX_EVENT_VIEW,        // panned or zoomed
} rdx_graph_event;

typedef void (*rdx_graph_fn)(rdx_graph* g, rdx_graph_event ev, rdx_node node, void* data);

enum { RDX_GRAPH_DRAG_NODES = 1u << 0,
       RDX_GRAPH_NO_GRID    = 1u << 1 };

// Turns host into the view's canvas, setting its paint callback. The graph lives until host is
// destroyed. Labels are text nodes added under host; style them through rdx_graph_label_class. Null
// if host already has a graph: a host holds one graph, to rebuild the view, destroy the host and
// make a new one.
rdx_graph* rdx_graph_create(rd_ctx* ctx, rd_node host);
rd_node    rdx_graph_host(const rdx_graph* g);
rd_ctx*    rdx_graph_ctx(const rdx_graph* g);
void       rdx_graph_set_flags(rdx_graph* g, uint32_t flags);
// Cleared to this before drawing; transparent by default.
void     rdx_graph_set_background(rdx_graph* g, rd_color color);
rd_class rdx_graph_label_class(const rdx_graph* g);
// One callback for all events, replacing any earlier one. False, keeping nothing, for a null graph.
bool rdx_graph_on(rdx_graph* g, rdx_graph_fn fn, void* data, rd_destroy_fn destroy);

rdx_node rdx_graph_add_node(rdx_graph* g, const rdx_node_desc* desc);
// Also removes the node's edges.
bool    rdx_graph_remove_node(rdx_graph* g, rdx_node node);
bool    rdx_graph_node_alive(const rdx_graph* g, rdx_node node);
bool    rdx_graph_set_position(rdx_graph* g, rdx_node node, rd_vec2 position);
rd_vec2 rdx_graph_position(const rdx_graph* g, rdx_node node);
bool    rdx_graph_set_color(rdx_graph* g, rdx_node node, rd_color color);
bool    rdx_graph_set_radius(rdx_graph* g, rdx_node node, float radius);
// Null removes the label.
bool rdx_graph_set_label(rdx_graph* g, rdx_node node, const char* label);
// Draws a ring around the node; null clears it.
void rdx_graph_set_selected(rdx_graph* g, rdx_node node);

rdx_edge rdx_graph_connect(rdx_graph* g, rdx_node from, rdx_node to, const rdx_edge_desc* desc);
bool     rdx_graph_remove_edge(rdx_graph* g, rdx_edge edge);
bool     rdx_graph_set_edge_color(rdx_graph* g, rdx_edge edge, rd_color color);
bool     rdx_graph_set_edge_label(rdx_graph* g, rdx_edge edge, const char* label);

// Copied; replaces the previous set.
void rdx_graph_set_messages(rdx_graph* g, const rdx_message* messages, size_t count);

// The world point at the canvas center, and screen pixels per world unit.
void rdx_graph_set_view(rdx_graph* g, rd_vec2 center, float zoom);
void rdx_graph_view(const rdx_graph* g, rd_vec2* center, float* zoom);
// Frames every node, leaving padding in screen pixels; applied once the canvas has a size.
void rdx_graph_fit(rdx_graph* g, float padding);
// The topmost node under a point in root coordinates; null if none.
rdx_node rdx_graph_node_at(const rdx_graph* g, float x, float y);
// Places nodes evenly on a circle, in the order given.
void rdx_graph_layout_circle(rdx_graph* g, const rdx_node* nodes, size_t count, rd_vec2 center, float radius);

#ifdef __cplusplus
}
#endif

#endif
