#include <raydom/extras/dialog.hh>
#include <raydom/extras/graph.hh>
#include <raydom/raydom.hh>

#include <cstdio>

using namespace rd::literals;

int main() {
  rd::Context ctx;
  rd::Node    canvas = ctx.create(rd::Style().width(400_px).height(300_px));
  ctx.root().append(canvas);
  rdx::Graph g = rdx::Graph::create(canvas);
  rdx_node   a = g.add_node({0, 0}, rd::hex(0x5b8cff));
  rdx_node   b = g.add_node({100, 0}, rd::hex(0xc77dff));
  g.connect(a, b);
  ctx.layout(800, 600);
  // Links the dialog backend without opening a dialog: a null output is an argument error.
  bool dialog = rdx_dialog_open(nullptr, nullptr) == RDX_DIALOG_ERROR;
  bool ok     = g.alive(a) && g.alive(b) && dialog;
  std::printf("raydom %s (C++): %s\n", RD_VERSION_STRING, ok ? "ok" : "failed");
  return ok ? 0 : 1;
}
