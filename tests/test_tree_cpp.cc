#include <raydom/raydom.hh>
#include <vector>

#include "check.h"

struct Tracker {
  int* count;
  ~Tracker() { ++*count; }
};

int main() {
  int dtors = 0;
  {
    rd::Context ctx;
    CHECK(ctx);
    rd::Node root = ctx.root();
    rd::Node a = ctx.create(), b = ctx.create(), c = ctx.create();
    root.append(a).append(b).append(c);

    std::vector<rd::Node> seen;
    for (rd::Node n : root.children()) seen.push_back(n);
    CHECK(seen.size() == 3);
    CHECK(seen[0] == a && seen[1] == b && seen[2] == c);

    // Node and Class are handles: their setters are const, so a copy captured by value in a
    // non-mutable lambda, or a const Node, can change the node.
    const rd::Node label = ctx.create_text("before");
    auto           set   = [label] { label.text("after").merge_style(rd::Style().width(rd::px(10))); };
    set();
    CHECK(label.text() == "after" && label.computed()->width.value == 10);
    const rd::Class look = ctx.create_class();
    look.set(rd::Style().height(rd::px(4))).on(RD_STATE_HOVER, rd::Style().height(rd::px(8)));
    label.add_class(look);
    CHECK(label.computed()->height.value == 4);
    label.destroy();

    b.emplace_userdata<Tracker>(&dtors);
    CHECK(b.userdata<Tracker>()->count == &dtors);
    b.destroy();
    CHECK(dtors == 1);
    CHECK(!b);
    CHECK(root.child_count() == 2);

    // A painter is freed by clear_paint; rd_paint_text's order with the font last; context hooks.
    int painters = 0;
    a.paint([t = Tracker{&painters}](const rd_paint_info&) { (void)t; });
    CHECK(painters == 1);  // the moved-from temporary
    a.clear_paint();
    CHECK(painters == 2);
    ctx.paint_text(rd_paint_info{}, 12.0f, 1.0f, 2.0f, "text", rd::rgb(0, 0, 0));  // no-op before drawing
    int layouts = 0;
    CHECK(ctx.on_layout([&layouts] { layouts++; }));
    ctx.layout(100, 100);
    CHECK(layouts == 1);
    ctx.set_event_waiting(false);

    rd::Node words = ctx.create_text("hello world");
    root.append(words);
    ctx.layout(400, 100);
    CHECK(!ctx.selection());
    CHECK(ctx.select(words, 0, words, 5));
    auto sel = ctx.selection();
    CHECK(sel && sel->anchor == words && sel->anchor_offset == 0 && sel->focus == words && sel->focus_offset == 5);
    CHECK(ctx.selection_text() == "hello");
    ctx.clear_selection();
    CHECK(!ctx.selection() && ctx.selection_text().empty());

    CHECK(!ctx.focus_scope());
    ctx.set_focus_scope(a);
    CHECK(ctx.focus_scope() == a);
    ctx.set_focus_scope({});
    CHECK(!ctx.focus_scope());
    words.destroy();

    c.emplace_userdata<Tracker>(&dtors);
    rd::Context moved = std::move(ctx);
    CHECK(ctx.get() == nullptr);  // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    CHECK(moved.node_count() == 3);
  }
  CHECK(dtors == 2);
  return TEST_RESULT();
}
