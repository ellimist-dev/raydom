// Compiles with the docs; opens a window and dialogs, so it doesn't run as a test.
#include <raydom/extras/dialog.hh>
#include <raydom/raydom.hh>
#include <raylib.h>

#include <stdio.h>

using namespace rd::literals;

// The C API; this function is also valid C.
// ANCHOR: c
static void open_c(void) {
  const rdx_file_filter filters[] = {{"Text", "txt,md"}, {"Images", "png,jpg"}};
  const rdx_dialog_desc desc      = {.title = "Open", .filters = filters, .filter_count = 2};
  char*                 path      = NULL;
  switch (rdx_dialog_open(&desc, &path)) {
    case RDX_DIALOG_OK:
      printf("chose %s\n", path);
      rdx_dialog_free(path);
      break;
    case RDX_DIALOG_CANCEL:
      break;
    case RDX_DIALOG_ERROR:
      printf("dialog failed: %s\n", rdx_dialog_error());
      break;
  }
}
// ANCHOR_END: c

int main() {
  InitWindow(640, 480, "dialogs");
  {
    rd::Context ctx;
    ctx.load_font_file(FONT_PATH);
    rd::Node label = ctx.create_text("Nothing open");

    // ANCHOR: cpp
    rd::Node open = ctx.create_button("Open...");
    open.on_click([&] {
      rdx::DialogResult r = rdx::open_file({.title = "Open", .filters = {{"Text", "txt,md"}}});
      if (r) label.text(r.path());
      else if (r.status == RDX_DIALOG_ERROR) label.text("error: " + r.error);
    });

    rd::Node save = ctx.create_button("Save as...");
    save.on_click([&] {
      if (rdx::DialogResult r = rdx::save_file({.default_name = "notes.txt"})) label.text("saving to " + r.path());
    });
    // ANCHOR_END: cpp

    rd::Node c = ctx.create_button("Open (C API)...");
    c.on_click([] { open_c(); });
    ctx.root().style(rd::Style().padding(24_px).row_gap(12_px).align_items(RD_ALIGN_START));
    ctx.root().append(open).append(save).append(c).append(label);

    while (!WindowShouldClose()) {
      ctx.update();
      BeginDrawing();
      ClearBackground(BLACK);
      ctx.draw();
      EndDrawing();
    }
  }
  CloseWindow();
}
