#include <raydom/raydom.hh>
#include <string>
#include <vector>

#include "check.h"

using namespace rd::literals;

int main() {
  rd::Context              ctx;
  std::vector<std::string> log;
  rd::Node                 list   = ctx.create(rd::Style().width(200_px).padding(10_px).row_gap(4_px));
  rd::Node                 adder  = ctx.create(rd::Style().height(20_px));
  int                      clicks = 0;
  ctx.root().append(list.append(adder));

  list.on(RD_EVENT_CLICK, [&](rd::Event& e) { log.push_back(e.phase() == RD_PHASE_BUBBLE ? "list:bubble" : "list:?"); });
  list.on(RD_EVENT_CLICK, [&] { log.push_back("list:capture"); }, true);
  adder.on_click([&, list](rd::Event& e) mutable {
    ++clicks;
    rd::Node row = ctx.create(rd::Style().height(20_px));
    row.on_click([&](rd::Event& ev) {
      log.push_back("row");
      ev.current().destroy();
      ev.stop_propagation();
    });
    list.append(row);
    CHECK(e.target() == e.current());
  });

  ctx.layout(400, 300);
  ctx.pointer(15, 15, RD_BUTTON_PRIMARY);
  ctx.pointer(15, 15, 0);
  ctx.layout(400, 300);
  CHECK(clicks == 1 && list.child_count() == 2);
  CHECK(log.size() == 2 && log[0] == "list:capture" && log[1] == "list:bubble");

  log.clear();
  ctx.pointer(15, 40, RD_BUTTON_PRIMARY);
  ctx.pointer(15, 40, 0);
  CHECK(log.size() == 2 && log[0] == "list:capture" && log[1] == "row");
  CHECK(list.child_count() == 1);

  rd::Node drag = ctx.create(rd::Style().width(50_px).height(50_px));
  float    dx   = 0;
  drag.on(RD_EVENT_POINTER_DOWN, [&](rd::Event& e) { e.current().capture_pointer(); });
  drag.on(RD_EVENT_POINTER_MOVE, [&](rd::Event& e) {
    if (e.buttons() & RD_BUTTON_PRIMARY) dx = e.local_x();
  });
  ctx.root().append(drag);
  ctx.layout(400, 300);
  rd_rect r = drag.bounds();
  ctx.pointer(r.x + 5, r.y + 5, RD_BUTTON_PRIMARY);
  ctx.pointer(r.x + 300, r.y + 5, RD_BUTTON_PRIMARY);
  ctx.pointer(r.x + 300, r.y + 5, 0);
  CHECK(dx == 300);
  return TEST_RESULT();
}
