#include <cmath>
#include <raydom/raydom.hh>

#include "check.h"

using namespace rd::literals;

int main() {
  rd::Context ctx;
  rd_font     font = ctx.load_font_file(RAYDOM_TEST_FONT);
  CHECK(font.id != 0);
  float cw = ctx.measure_text("a", 16);

  rd::Node label = ctx.create_text("hello world", rd::Style().font_size(16_px).flex_shrink(rd::num(1)));
  rd::Node row   = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).width(rd::px(6 * cw)));
  ctx.root().append(row.append(label));
  ctx.layout(800, 600);

  CHECK(label.text() == "hello world");
  CHECK(std::fabs(label.rect().width - 6 * cw) < 0.01f);
  CHECK(std::fabs(label.rect().height - 2 * 16 * 1.2f) < 0.01f);
  CHECK(!std::isnan(label.baseline()));

  label.text("hi");
  ctx.layout(800, 600);
  CHECK(std::fabs(label.rect().width - 2 * cw) < 0.01f);
  CHECK(ctx.create().text().empty());
  return TEST_RESULT();
}
