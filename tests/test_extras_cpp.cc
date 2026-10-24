#include <raydom/extras/controls.hh>
#include <raydom/extras/popups.hh>
#include <raydom/extras/views.hh>
#include <unordered_set>

#include "check.h"

using namespace rd::literals;

int main() {
  rd::Context ctx;
  ctx.load_font_file(RAYDOM_TEST_FONT);

  // Typed handles carry their operations, and are still rd::Nodes.
  rdx::Slider slider = rdx::slider(ctx, {.min = 0, .max = 10, .value = 3, .step = 1});
  CHECK(slider.value() == 3 && slider.set_value(7) && slider.value() == 7 && rdx::slider_value(slider) == 7);
  CHECK(slider.set_range(0, 5) && slider.value() == 5);
  rd::Node plain = slider;  // slicing to a plain node keeps the handle
  CHECK(plain == slider && rdx::slider_value(plain) == 5);

  rdx::Number number = rdx::number(ctx, {.min = 0, .max = 100, .value = 12.345f, .decimals = 1});
  CHECK(number.value() > 12.29f && number.value() < 12.31f && number.input());
  rdx::Toggle box = rdx::checkbox(ctx, "Remember");
  CHECK(!box.checked() && box.set_checked(true) && box.checked());
  rdx::RadioGroup group = rdx::radio_group(ctx);
  group.add("A");
  group.add("B");
  CHECK(group.select(1) && group.selected() == 1);

  rdx::Tabs tabs = rdx::tabs(ctx);
  rd::Node  one = tabs.add("One"), two = tabs.add("Two");
  CHECK(one && two && tabs.select(1) && tabs.selected() == 1);
  rdx::Split split = rdx::split(ctx, {.size = 200, .collapsible = true});
  CHECK(split.pane(0) && split.size() == 200 && split.set_collapsed(true) && split.collapsed());
  rdx::Select select = rdx::select(ctx, {"Red", "Green"});
  CHECK(select.set_selected(1) && select.selected() == 1);
  rdx::List list = rdx::list(ctx, 10, 20, [](rd::Node, int) {});
  CHECK(list.set_count(5) && list.select(3) && list.selected() == 3 && !list.select(9));
  rdx::Breadcrumb crumbs = rdx::breadcrumb(ctx, {"Home", "Docs"});
  CHECK(crumbs.count() == 2 && crumbs.clicked() == -1 && crumbs.set({"A", "B", "C"}) && crumbs.count() == 3);
  rdx::Table table = rdx::table(ctx, {{"Name"}}, 4, 20, {});
  CHECK(table.list() && table.list().set_count(2));
  rdx::Tree tree = rdx::tree(ctx);
  int       root = tree.add(-1, "root");
  CHECK(root == 0 && tree.add(root, "leaf") == 1 && tree.label(1) == "leaf");

  rdx::Slider none;  // default-constructible, null like rd::Node
  CHECK(!none && !rdx::Slider(rd::Node()).set_value(1));

  // Handles hash, and setters chain by value.
  std::unordered_set<rd::Node> seen{one, two, one};
  CHECK(seen.size() == 2);
  rd::Class a = ctx.create_class(rd::Style().width(10_px));
  CHECK(a == a && std::hash<rd::Class>{}(a) == std::hash<rd::Class>{}(a));
  rd::Node chained = ctx.create().style(rd::Style().width(5_px)).add_class(a);
  CHECK(chained.has_class(a));
  return TEST_RESULT();
}
