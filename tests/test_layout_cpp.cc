#include <cmath>
#include <raydom/raydom.hh>

#include "check.h"

using namespace rd::literals;

int main() {
  rd::Context ctx;
  rd::Node    row   = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).padding(8_px).gap(8_px));
  rd::Node    side  = ctx.create(rd::Style().width(100_px));
  rd::Node    main  = ctx.create(rd::Style().flex(1));
  int         calls = 0;
  rd::Node    label = ctx.create().measure([&calls](float w, rd_measure_mode wm, float, rd_measure_mode) {
    ++calls;
    float width = wm == RD_MEASURE_UNDEFINED ? 200.0f : std::fmin(w, 200.0f);
    return rd_size{width, 20.0f * std::ceil(200.0f / width)};
  });
  ctx.root().append(row.append(side).append(main.append(label)));
  ctx.layout(400, 300);

  CHECK(row.rect().height == 36);
  CHECK(side.rect().x == 8 && side.rect().width == 100);
  CHECK(main.rect().x == 116 && main.rect().width == 276);
  CHECK(label.rect().width == 276 && label.rect().height == 20);
  CHECK(label.bounds().x == 116 && label.bounds().y == 8);
  CHECK(calls > 0);

  ctx.layout(300, 300);
  CHECK(main.rect().width == 176);
  CHECK(label.rect().height == 40);
  CHECK(row.rect().height == 56);
  return TEST_RESULT();
}
