// Runs headless as a test for the "Menus, selects, modals and tooltips" chapter.
#include <raydom/extras/popups.hh>
#include <raydom/raydom.hh>

#include <cassert>
#include <string>

int main() {
  rd::Context ctx;
  ctx.load_font_file(FONT_PATH);
  ctx.set_time(0);
  rd::Node    file = ctx.create_button("File");
  rd::Node    area = ctx.create(rd::Style().height(rd::px(200)));
  std::string log;

  // ANCHOR: menu
  file.on_click([&] {
    rdx::menu(ctx, {{"New", "Ctrl+N"}, {"Open...", "Ctrl+O"}, {}, {"Quit", "Ctrl+Q"}},  // {} is a separator
              {.node = file.handle(), .offset = 4},                                       // below the button
              [&](int index) { log = "menu " + std::to_string(index); });
  });
  // ANCHOR_END: menu

  // ANCHOR: context
  rdx::context_menu(area, {{"Copy", "Ctrl+C"}, {"Paste", "Ctrl+V", /*disabled=*/true}},
                    [&](int index) { log = "context " + std::to_string(index); });
  // ANCHOR_END: context

  // ANCHOR: select
  rd::Node theme = rdx::select(ctx, {"System", "Light", "Dark"}, "Theme");
  theme.on(RD_EVENT_CHANGE, [&] { log = "theme " + std::to_string(rdx::selected(theme)); });
  // ANCHOR_END: select

  // ANCHOR: tooltip
  rdx::tooltip(file, "File operations");
  // ANCHOR_END: tooltip

  ctx.root().append(file).append(theme).append(area);
  ctx.layout(800, 600);
  file.focus();
  ctx.press(RD_KEY_ENTER);  // opens the menu
  ctx.press(RD_KEY_DOWN);
  ctx.press(RD_KEY_ENTER);
  assert(log == "menu 0" && file.focused());
  theme.focus();
  ctx.press(RD_KEY_DOWN);
  assert(log == "theme 0");

  // ANCHOR: modal
  rd::Node body = rdx::modal(ctx, {.title = "Rename", .on_close = [&] { log += ", closed"; }});
  rd::Node name = ctx.create_input().placeholder("New name");
  rd::Node done = ctx.create_button("Rename");
  done.on_click([body] { rdx::close_popup(body); });
  body.append(name).append(done);
  // ANCHOR_END: modal
  ctx.layout(800, 600);
  ctx.set_time(0.1);  // focus moves into the modal at the next clock update
  assert(name.focused());
  ctx.press(RD_KEY_ESCAPE);
  assert(!body.alive() && log == "theme 0, closed");
  return 0;
}
