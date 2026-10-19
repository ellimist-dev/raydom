#include <raydom/raydom.hh>

#include "check.h"

using namespace rd::literals;

int main() {
  rd::Context ctx;
  auto        cols   = ctx.tracks(rd::repeat(3, {1_fr}));
  auto        rows   = ctx.tracks({40_px, rd::minmax(20_px, rd::auto_)});
  rd::Node    grid   = ctx.create(rd::Style()
                                      .display(RD_DISPLAY_GRID)
                                      .width(320_px)
                                      .gap(10_px)
                                      .grid_template_columns(cols)
                                      .grid_template_rows(rows));
  rd::Node    header = ctx.create(rd::Style().grid_column(rd::line(1), rd::line(-1)));
  rd::Node    a      = ctx.create();
  rd::Node    b      = ctx.create(rd::Style().grid_column(rd::span(2)));
  ctx.root().append(grid.append(header).append(a).append(b));
  ctx.layout(800, 600);

  CHECK(header.rect().width == 320 && header.rect().height == 40);
  CHECK(a.rect().x == 0 && a.rect().y == 50 && a.rect().width == 100 && a.rect().height == 20);
  CHECK(b.rect().x == 110 && b.rect().width == 210);
  CHECK(grid.rect().height == 70);
  {
    // A responsive card grid: as many 260px-or-wider columns as fit.
    rd::Context c2;
    rd::Node    cards = c2.create(rd::Style()
                                      .display(RD_DISPLAY_GRID)
                                      .width(860_px)
                                      .column_gap(20_px)
                                      .grid_template_columns(c2.tracks(rd::repeat(rd::auto_fill, {rd::minmax(260_px, 1_fr)}))));
    for (int j = 0; j < 5; j++) cards.append(c2.create(rd::Style().height(10_px)));
    c2.root().append(cards);
    c2.layout(1000, 800);
    CHECK(cards.first_child().rect().width > 270 && cards.first_child().rect().width < 274);  // three columns
    CHECK(c2.tracks(rd::repeat(rd::auto_fit, {100_px})).id != c2.tracks(rd::repeat(rd::auto_fill, {100_px})).id);
  }
  return TEST_RESULT();
}
