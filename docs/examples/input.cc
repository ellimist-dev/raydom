// Runs headless as a test for the "Focus, keyboard and text input" chapter.
#include <raydom/raydom.hh>

#include <cassert>
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
  ctx.press(RD_KEY_ESCAPE);
  assert(!ctx.focused());
  return 0;
}
