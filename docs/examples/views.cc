// Runs headless as a test for the "Tabs, splitters, lists, tables and trees" chapter.
#include <raydom/extras/views.hh>
#include <raydom/raydom.hh>

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace rd::literals;

int main() {
  rd::Context ctx;
  ctx.load_font_file(FONT_PATH);

  // ANCHOR: tabs
  rd::Node tabs    = rdx::tabs(ctx);
  rd::Node general = rdx::add_tab(tabs, "General");  // panels to fill
  rd::Node network = rdx::add_tab(tabs, "Network");
  general.append(ctx.create_text("General settings"));
  network.append(ctx.create_text("Network settings"));
  tabs.on(RD_EVENT_CHANGE, [&] { /* rdx::selected_tab(tabs) */ });
  // ANCHOR_END: tabs

  // ANCHOR: split
  rd::Node split = rdx::split(ctx, {.fraction = 0.3f, .min_first = 120, .min_second = 200});
  split.style(rd::Style().height(400_px));
  rd::Node sidebar = rdx::pane(split, 0), content = rdx::pane(split, 1);
  // ANCHOR_END: split

  // ANCHOR: list
  std::vector<std::string> names;
  for (int i = 0; i < 50000; i++) names.push_back("Contact " + std::to_string(i));
  rd::Node contacts = rdx::list(ctx, static_cast<int>(names.size()), 28, [&](rd::Node row, int i) {
    row.append(ctx.create_text(names[static_cast<std::size_t>(i)]));  // the row comes empty
  });
  contacts.style(rd::Style().flex(1));
  contacts.on(RD_EVENT_CHANGE, [&] { /* names[rdx::list_selected(contacts)] */ });
  // ANCHOR_END: list
  sidebar.append(contacts);

  // ANCHOR: table
  rd::Node table = rdx::table(ctx, {{"Name", 1_fr}, {"Size", 80_px}}, 10000, 28,
                              {.cell = [](rd::Node cell, int row, int col) {
                                 char buf[32];
                                 std::snprintf(buf, sizeof buf, col == 0 ? "file_%05d" : "%d KB", row);
                                 cell.text(buf);
                               },
                               .header = [](int column) { (void)column; /* sort by it, then rdx::list_refresh */ }});
  // ANCHOR_END: table
  content.append(table.style(rd::Style().flex(1)));

  // ANCHOR: tree
  rd::Node tree = rdx::tree(ctx);
  int      src  = rdx::tree_add(tree, -1, "src");
  rdx::tree_add(tree, src, "main.c");
  rdx::tree_expand(tree, src);
  tree.on(RD_EVENT_CHANGE, [&] { /* rdx::tree_label(tree, rdx::tree_selected(tree)) */ });
  // ANCHOR_END: tree

  ctx.root().append(tabs).append(split).append(tree);
  ctx.layout(1000, 1200);
  contacts.focus();
  ctx.press(RD_KEY_END);
  ctx.layout(1000, 1200);
  assert(rdx::list_selected(contacts) == 49999 && rdx::selected_tab(tabs) == 0);
  return 0;
}
