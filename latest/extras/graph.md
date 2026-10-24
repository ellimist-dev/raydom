# Network view

`raydom::extras` provides `rdx_graph`, a view of nodes and edges for simulators and tools: lit 3D spheres, straight
edges with optional arrows and labels, and messages drawn in flight. It is built on a [canvas](../guide/canvas.md)
and lives in its own library so the core stays free of widgets.

```cmake
target_link_libraries(your_app PRIVATE raydom::extras)
```

```cpp
#include <raydom/extras/graph.hh>  // or graph.h for C
```

## Creating a view

```cpp
rd::Node   canvas = ctx.create(rd::Style().flex(1));
rdx::Graph graph  = rdx::Graph::create(canvas);
graph.background(rd::hex(0x0b0d12));
graph.label_class().set(rd::Style().font_size(12_px).color(rd::hex(0xe4e7ee)));
ctx.root().append(canvas);
```

`rdx_graph_create(ctx, host)` turns `host` into the view. The graph lives until the host node is destroyed; there is
no separate destroy call. A host holds one graph: creating a second on the same host returns null and leaves the
first as it was, so to rebuild the view, destroy the host and create the graph on a new one. Replacing the host's
paint callback stops the graph drawing but doesn't free it. Node labels are text nodes under the host, styled with the class returned by
`label_class()`.

## Nodes and edges

```cpp
std::vector<rdx_node> nodes;
for (const char* name : {"A", "B", "C", "D", "E"}) nodes.push_back(graph.add_node({0, 0}, rd::hex(0x6c7385), name));
graph.layout_circle(nodes, {0, 0}, 160);
for (std::size_t i = 0; i < nodes.size(); i++)
  graph.connect(nodes[i], nodes[(i + 1) % nodes.size()], {.color = rd::hex(0x2a3142), .width = 1.5f});
graph.fit(60);
```

Positions are world units on a plane, y down. `rdx_node` and `rdx_edge` are generational handles, like `rd_node`.
Removing a node removes its edges. `fit(padding)` frames every node; `view(center, zoom)` sets the camera.

Defaults: node radius 20 world units, edge width 2 screen pixels, message radius 6 world units.

## Interaction

Dragging the background pans, and the wheel zooms about the pointer, between 0.1x and 10x. With
`RDX_GRAPH_DRAG_NODES`, dragging a node moves it. `RDX_GRAPH_NO_GRID` hides the background grid.

```cpp
graph.on([&](rdx_graph_event ev, rdx_node n) {
  if (ev == RDX_EVENT_NODE_CLICK) graph.select(n);
});
```

| Event                   | When                                                   |
| ----------------------- | ------------------------------------------------------ |
| `RDX_EVENT_NODE_ENTER`  | Pointer moves onto a node                              |
| `RDX_EVENT_NODE_LEAVE`  | Pointer leaves a node                                  |
| `RDX_EVENT_NODE_CLICK`  | Press and release on the same node without dragging    |
| `RDX_EVENT_NODE_DRAG`   | A node moved, with `RDX_GRAPH_DRAG_NODES`              |
| `RDX_EVENT_VIEW`        | The view panned or zoomed                              |

## Messages

The graph has no clock. Each frame, pass the messages in flight with their progress from 0 (at `from`) to 1 (at
`to`); the call replaces the previous set.

```cpp
float progress = t - static_cast<float>(static_cast<int>(t));  // your simulation decides this
graph.messages({{nodes[0], nodes[1], progress, rd::hex(0x5b8cff), 6}});
```

The showcase's Network tab is a Paxos simulator built this way (`examples/showcase/network.cc`).
