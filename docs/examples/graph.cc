// Compiles with the docs; opens a window, so it doesn't run as a test.
#include <raydom/extras/graph.hh>
#include <raydom/raydom.hh>
#include <raylib.h>

#include <vector>

using namespace rd::literals;

int main() {
  SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
  InitWindow(960, 640, "network");
  {
    rd::Context ctx;
    ctx.load_font_file(FONT_PATH);

    // ANCHOR: create
    rd::Node   canvas = ctx.create(rd::Style().flex(1));
    rdx::Graph graph  = rdx::Graph::create(canvas);
    graph.background(rd::hex(0x0b0d12));
    graph.label_class().set(rd::Style().font_size(12_px).color(rd::hex(0xe4e7ee)));
    ctx.root().append(canvas);
    // ANCHOR_END: create

    // ANCHOR: nodes
    std::vector<rdx_node> nodes;
    for (const char* name : {"A", "B", "C", "D", "E"}) nodes.push_back(graph.add_node({0, 0}, rd::hex(0x6c7385), name));
    graph.layout_circle(nodes, {0, 0}, 160);
    for (std::size_t i = 0; i < nodes.size(); i++)
      graph.connect(nodes[i], nodes[(i + 1) % nodes.size()], {.color = rd::hex(0x2a3142), .width = 1.5f});
    graph.fit(60);
    // ANCHOR_END: nodes

    // ANCHOR: events
    graph.on([&](rdx_graph_event ev, rdx_node n) {
      if (ev == RDX_EVENT_NODE_CLICK) graph.select(n);
    });
    // ANCHOR_END: events

    float t = 0;
    while (!WindowShouldClose()) {
      t += GetFrameTime() * 0.5f;
      // ANCHOR: messages
      float progress = t - static_cast<float>(static_cast<int>(t));  // your simulation decides this
      graph.messages({{nodes[0], nodes[1], progress, rd::hex(0x5b8cff), 6}});
      // ANCHOR_END: messages
      ctx.update();
      BeginDrawing();
      ClearBackground(BLACK);
      ctx.draw();
      EndDrawing();
    }
  }
  CloseWindow();
}
