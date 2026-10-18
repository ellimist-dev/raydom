// ANCHOR: all
#include <raydom/raydom.hh>
#include <raylib.h>

#include <string>

using namespace rd::literals;

int main() {
  SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE);
  InitWindow(800, 600, "hello");
  {
    rd::Context ctx;
    ctx.load_font_file(FONT_PATH);  // any .ttf or .otf file

    rd::Class button_style = ctx.create_class(rd::Style()
                                                  .padding(8_px, 16_px)
                                                  .border_radius(8_px)
                                                  .background(rd::hex(0x5b8cff))
                                                  .color(rd::hex(0xffffff)));
    button_style.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x7aa2ff)));

    int      clicks = 0;
    rd::Node label  = ctx.create_text("Clicked 0 times", rd::Style().color(rd::hex(0xe4e7ee)));
    rd::Node button = ctx.create_button("Click me").add_class(button_style);  // Tab focuses it; Enter and Space click it
    button.on_click([&] { label.text("Clicked " + std::to_string(++clicks) + " times"); });

    ctx.root()
        .style(rd::Style().padding(24_px).row_gap(12_px).align_items(RD_ALIGN_START))
        .append(button)
        .append(label);

    while (!WindowShouldClose()) {
      ctx.update();
      BeginDrawing();
      ClearBackground(BLACK);
      ctx.draw();
      EndDrawing();
    }
  }  // the context is destroyed here, before CloseWindow
  CloseWindow();
}
// ANCHOR_END: all
