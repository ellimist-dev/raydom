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

    b.emplace_userdata<Tracker>(&dtors);
    CHECK(b.userdata<Tracker>()->count == &dtors);
    b.destroy();
    CHECK(dtors == 1);
    CHECK(!b);
    CHECK(root.child_count() == 2);

    c.emplace_userdata<Tracker>(&dtors);
    rd::Context moved = std::move(ctx);
    CHECK(ctx.get() == nullptr);  // NOLINT(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    CHECK(moved.node_count() == 3);
  }
  CHECK(dtors == 2);
  return TEST_RESULT();
}
