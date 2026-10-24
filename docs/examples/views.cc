// Runs headless as a test for the "Tabs, splitters, lists, tables, trees and breadcrumbs" chapter.
#include <raydom/extras/popups.hh>
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

  // ANCHOR: collapsible
  // A 280px panel that keeps its width as the window resizes and snaps shut when dragged narrow.
  rd::Node panel = rdx::split(ctx, {.size = 280, .min_first = 200, .min_second = 300, .collapsible = true});
  panel.style(rd::Style().height(400_px));
  rd::Node reopen = ctx.create_button("Show panel");
  reopen.on_click([panel] { rdx::set_split_collapsed(panel, false); });
  panel.on(RD_EVENT_CHANGE, [panel, reopen] {
    reopen.merge_style(rd::Style().display(rdx::split_collapsed(panel) ? RD_DISPLAY_FLEX : RD_DISPLAY_NONE));
  });
  // On a narrow window: stacked, with the panel at the bottom.
  rdx::set_split_direction(panel, true, true);
  // ANCHOR_END: collapsible

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

  // ANCHOR: rows
  // One listener for every row: rows are reused as the table scrolls, so ask which row a node is in.
  rd::Node rows   = rdx::table_list(table);
  int      opened = -1;
  rows.on(RD_EVENT_CLICK, [&](rd::Event& e) {
    if (e.clicks() == 2) opened = rdx::list_index_of(rows, e.target());  // double click: open the file
  });
  rows.on(RD_EVENT_POINTER_DOWN, [&](rd::Event& e) {
    int row = rdx::list_index_of(rows, e.target());
    if (e.button() != RD_BUTTON_SECONDARY || row < 0) return;
    rdx::menu(ctx, {{"Open"}, {"Rename"}, {}, {"Delete"}}, {.x = e.x(), .y = e.y()},
              [row](int item) { (void)row, (void)item; /* act on row */ });
  });
  // ANCHOR_END: rows

  // ANCHOR: tree
  rd::Node tree = rdx::tree(ctx);
  int      src  = rdx::tree_add(tree, -1, "src");
  rdx::tree_add(tree, src, "main.c");
  rdx::tree_expand(tree, src);
  tree.on(RD_EVENT_CHANGE, [&] { /* rdx::tree_label(tree, rdx::tree_selected(tree)) */ });
  // ANCHOR_END: tree

  // ANCHOR: breadcrumb
  rdx::Breadcrumb crumbs = rdx::breadcrumb(ctx, {"Home", "Projects", "Loom"});
  crumbs.on(RD_EVENT_CHANGE, [] { /* go to item rdx::Breadcrumb::clicked() */ });
  // ANCHOR_END: breadcrumb

  ctx.root().append(tabs).append(split).append(tree).append(crumbs);
  ctx.layout(1000, 1200);
  contacts.focus();
  ctx.press(RD_KEY_END);
  ctx.layout(1000, 1200);
  assert(rdx::list_selected(contacts) == 49999 && rdx::selected_tab(tabs) == 0 && crumbs.count() == 3);

  rd_rect r = rows.bounds();  // a double click on the third row
  float   y = r.y + 28 * 2 + 10;
  ctx.set_time(1);
  ctx.pointer(r.x + 20, y, RD_BUTTON_PRIMARY);
  ctx.pointer(r.x + 20, y, 0);
  ctx.set_time(1.2);
  ctx.pointer(r.x + 20, y, RD_BUTTON_PRIMARY);
  ctx.pointer(r.x + 20, y, 0);
  assert(opened == 2);
  return 0;
}
