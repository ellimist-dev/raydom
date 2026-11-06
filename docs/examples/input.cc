// Runs headless as a test for the "Focus, keyboard and text input" chapter.
#include <raydom/raydom.hh>

#include <cassert>
#include <cmath>
#include <optional>
#include <string>

using namespace rd::literals;

int main() {
  rd::Context ctx;
  ctx.load_font_file(FONT_PATH);

  // ANCHOR: inputs
  rd::Node name  = ctx.create_input(false, rd::Style().width(240_px)).placeholder("Your name");
  rd::Node notes = ctx.create_input(/*multiline=*/true, rd::Style().min_height(80_px));

  std::string value;
  name.on(RD_EVENT_INPUT, [&](rd::Event& e) { value = std::string(e.target().text()); });  // every edit
  name.on(RD_EVENT_CHANGE, [&] { /* Enter, or leaving the field after editing */ });
  // ANCHOR_END: inputs

  // ANCHOR: focusable
  rd::Class button_style = ctx.create_class(rd::Style().padding(6_px, 12_px));
  button_style.on(RD_STATE_FOCUS_VISIBLE, rd::Style().box_shadow(0, 0, 0, rd::rgba(91, 140, 255, 200), 2));
  rd::Node save  = ctx.create_button("Save").add_class(button_style);  // focusable by role
  rd::Node badge = ctx.create(rd::Style().focusable(RD_FOCUSABLE_TAB));  // any node, by style
  int      saves = 0;
  save.on_click([&] { ++saves; });  // also fires on Enter or Space while focused
  // ANCHOR_END: focusable

  ctx.root().append(name).append(notes).append(save).append(badge);
  ctx.layout(800, 600);

  // ANCHOR: keys
  ctx.root().on(RD_EVENT_KEY_DOWN, [&](rd::Event& e) {
    if (e.key() == RD_KEY_ESCAPE) ctx.blur();
  });
  // ANCHOR_END: keys

  // ANCHOR: drive
  ctx.press(RD_KEY_TAB);  // focus moves to the first focusable node: name
  ctx.type("Ada");        // text events, one per codepoint
  ctx.press(RD_KEY_TAB);  // notes
  ctx.press(RD_KEY_TAB);  // save
  ctx.press(RD_KEY_ENTER);
  // ANCHOR_END: drive
  assert(value == "Ada" && name.text() == "Ada" && saves == 1 && save.focused());

  ctx.press(RD_KEY_TAB);
  assert(badge.focused());

  // ANCHOR: lines
  rd::Node pane   = ctx.create(rd::Style().height(120_px).overflow_y(RD_OVERFLOW_SCROLL));
  rd::Node editor = ctx.create_input(/*multiline=*/true, rd::Style().flex_shrink(rd::num(0)));
  ctx.root().append(pane.append(editor));
  std::string source;
  for (int i = 0; i < 100; i++) source += "line\n";
  editor.text(source);
  editor.scroll_to_line(40);  // waits for the layout, since the value just changed
  ctx.layout(800, 600);
  // Where to put line 40's number or marker, in the same coordinates however the pane scrolls:
  std::optional<rd_rect> marker = editor.line_rect(40);
  // ANCHOR_END: lines
  assert(marker && std::fabs(pane.scroll().y - (marker->y + marker->height - 120)) < 0.01f);
  ctx.press(RD_KEY_ESCAPE);
  assert(!ctx.focused());
  return 0;
}
