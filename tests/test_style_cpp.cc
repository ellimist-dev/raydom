#include <raydom/raydom.hh>

#include "check.h"

using namespace rd::literals;

int main() {
  rd::Context ctx;
  rd::Class   button = ctx.create_class(rd::Style()
                                            .padding(6_px, 12_px)
                                            .border_radius(4_px)
                                            .background(rd::hex(0x333333)));
  button.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x444444)));

  rd::Node row = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).gap(8_px).width(100_pct));
  rd::Node ok  = ctx.create().add_class(button);
  ctx.root().append(row.append(ok));

  const rd_style* s = ok.computed();
  CHECK(s->padding.top.value == 6 && s->padding.left.value == 12);
  CHECK(s->border_radius.bottom_left.value == 4);
  CHECK(s->background.r == 0x33);

  ok.set_state(RD_STATE_HOVER, true);
  CHECK(ok.computed()->background.r == 0x44);

  const rd_style* rs = row.computed();
  CHECK(rs->flex_direction == RD_FLEX_ROW);
  CHECK(rs->row_gap.value == 8 && rs->column_gap.value == 8);
  CHECK(rs->width.unit == RD_UNIT_PCT && rs->width.value == 100);

  // Corners clockwise from the top left.
  rd_corners c = rd::Style().border_radius(1_px, 2_px, 3_px, 4_px).get().border_radius;
  CHECK(c.top_left.value == 1 && c.top_right.value == 2 && c.bottom_right.value == 3 && c.bottom_left.value == 4);
  CHECK(rs->height.unit == rd::auto_.unit);
  return TEST_RESULT();
}
