#include <raydom/raydom.hh>
#include <string>

#include "check.h"

using namespace rd::literals;

int main() {
  rd::Context ctx;
  ctx.load_font_file(RAYDOM_TEST_FONT);
  rd::Node    form  = ctx.create(rd::Style().padding(10_px).row_gap(8_px));
  rd::Node    name  = ctx.create().make_input().placeholder("Name");
  rd::Node    notes = ctx.create(rd::Style().min_height(60_px)).make_input(true);
  rd::Node    save  = ctx.create(rd::Style().focusable(RD_FOCUSABLE_TAB).height(20_px));
  std::string echo;
  int         saves = 0;
  ctx.root().append(form.append(name).append(notes).append(save));

  name.on(RD_EVENT_INPUT, [&](rd::Event& e) { echo = std::string(e.target().text()); });
  save.on_click([&] { ++saves; });
  form.on(RD_EVENT_KEY_DOWN, [&](rd::Event& e) {
    if (e.key() == RD_KEY_ESCAPE) ctx.blur();
  });
  ctx.layout(400, 300);

  ctx.press(RD_KEY_TAB);
  CHECK(name.focused());
  ctx.type("Ada");
  CHECK(echo == "Ada" && name.text() == "Ada");
  ctx.press(RD_KEY_TAB);
  CHECK(notes.focused());
  ctx.type("line one");
  ctx.press(RD_KEY_ENTER);
  ctx.type("two");
  CHECK(notes.text() == "line one\ntwo");
  ctx.press(RD_KEY_TAB);
  ctx.press(RD_KEY_SPACE);
  CHECK(save.focused() && saves == 1);
  ctx.press(RD_KEY_ESCAPE);
  CHECK(ctx.focused().handle().index == 0);

  // Lines of a value, scrolling them and any node into view.
  rd::Node scroller = ctx.create(rd::Style().height(50_px).overflow_y(RD_OVERFLOW_SCROLL).line_height(20_px));
  rd::Node editor   = ctx.create(rd::Style().flex_shrink(rd::num(0))).make_input(true);
  rd::Node end      = ctx.create(rd::Style().height(10_px).flex_shrink(rd::num(0)));
  form.append(scroller.append(editor).append(end));
  editor.text("a\nb\nc\nd\ne");
  CHECK(!editor.line_rect(0));  // not laid out yet
  ctx.layout(400, 300);
  auto line = editor.line_rect(2);
  CHECK(line && line->y == 40 && line->height == 20);
  CHECK(!editor.line_rect(5) && !editor.scroll_to_line(5));
  CHECK(editor.scroll_to_line(3) && scroller.scroll().y == 30);
  end.scroll_into_view(5);
  CHECK(scroller.scroll().y == 60);
  return TEST_RESULT();
}
