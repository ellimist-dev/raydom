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
{{#include ../../examples/graph.cc:create}}
```

`rdx_graph_create(ctx, host)` turns `host` into the view. The graph lives until the host node is destroyed; there is
no separate destroy call. Node labels are text nodes under the host, styled with the class returned by
`label_class()`.

## Nodes and edges

```cpp
{{#include ../../examples/graph.cc:nodes}}
```

Positions are world units on a plane, y down. `rdx_node` and `rdx_edge` are generational handles, like `rd_node`.
Removing a node removes its edges. `fit(padding)` frames every node; `view(center, zoom)` sets the camera.

Defaults: node radius 20 world units, edge width 2 screen pixels, message radius 6 world units.

## Interaction

Dragging the background pans, and the wheel zooms about the pointer, between 0.1x and 10x. With
`RDX_GRAPH_DRAG_NODES`, dragging a node moves it. `RDX_GRAPH_NO_GRID` hides the background grid.

```cpp
{{#include ../../examples/graph.cc:events}}
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
{{#include ../../examples/graph.cc:messages}}
```

The `network` example in the repository is a Paxos simulator built this way.
