// Runs headless as a test for the "Events" chapter: input is fed with rd_ctx_pointer instead of raylib.
#include <raydom/raydom.hh>

#include <cassert>
#include <string>
#include <vector>

using namespace rd::literals;

int main() {
  rd::Context ctx;
  rd::Node    list = ctx.create(rd::Style().width(300_px).padding(10_px).row_gap(4_px));
  rd::Node    item = ctx.create(rd::Style().height(30_px));
  ctx.root().append(list.append(item));

  std::vector<std::string> log;

  // ANCHOR: listeners
  item.on_click([&] { log.push_back("item"); });                                  // bubble phase, target
  list.on(RD_EVENT_CLICK, [&](rd::Event&) { log.push_back("list bubble"); });     // after the target
  list.on(RD_EVENT_CLICK, [&](rd::Event&) { log.push_back("list capture"); }, true);  // before the target
  // ANCHOR_END: listeners

  ctx.layout(800, 600);
  ctx.pointer(20, 20, RD_BUTTON_PRIMARY);
  ctx.pointer(20, 20, 0);
  assert((log == std::vector<std::string>{"list capture", "item", "list bubble"}));

  // ANCHOR: stop
  rd::Node row = ctx.create(rd::Style().height(30_px));
  row.on_click([](rd::Event& e) {
    e.stop_propagation();       // no bubble listeners run; capture listeners already have
    e.current().destroy();      // safe during dispatch: freed when dispatch ends
  });
  list.append(row);
  // ANCHOR_END: stop

  ctx.layout(800, 600);
  log.clear();
  rd_rect r = row.bounds();
  ctx.pointer(r.x + 5, r.y + 5, RD_BUTTON_PRIMARY);
  ctx.pointer(r.x + 5, r.y + 5, 0);
  assert((log == std::vector<std::string>{"list capture"}) && list.child_count() == 1);

  // ANCHOR: drag
  rd::Node handle = ctx.create(rd::Style().width(20_px).height(20_px));
  float    dragged = 0;
  handle.on(RD_EVENT_POINTER_DOWN, [](rd::Event& e) { e.current().capture_pointer(); });
  handle.on(RD_EVENT_POINTER_MOVE, [&](rd::Event& e) {
    if (e.buttons() & RD_BUTTON_PRIMARY) dragged = e.local_x();  // keeps arriving outside the handle
  });
  // ANCHOR_END: drag
  ctx.root().append(handle);
  ctx.layout(800, 600);
  rd_rect h = handle.bounds();
  ctx.pointer(h.x + 5, h.y + 5, RD_BUTTON_PRIMARY);
  ctx.pointer(h.x + 300, h.y + 5, RD_BUTTON_PRIMARY);
  ctx.pointer(h.x + 300, h.y + 5, 0);
  assert(dragged == 300);

  // ANCHOR: wheel
  rd::Node canvas = ctx.create(rd::Style().height(100_px).overflow_y(RD_OVERFLOW_SCROLL));
  canvas.on(RD_EVENT_WHEEL, [](rd::Event& e) { e.prevent_default(); });  // handle the wheel yourself, no scrolling
  // ANCHOR_END: wheel
  return 0;
}
