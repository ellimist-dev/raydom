// Runs headless as a test for the "Controls and toasts" chapter.
#include <raydom/extras/controls.hh>
#include <raydom/raydom.hh>

#include <cassert>
#include <string>

using namespace rd::literals;

int main() {
  rd::Context ctx;
  ctx.load_font_file(FONT_PATH);
  ctx.set_time(0);

  // ANCHOR: toggles
  rd::Node remember = rdx::checkbox(ctx, "Remember me");
  rd::Node sync     = rdx::toggle_switch(ctx, "Sync");
  rdx::set_checked(sync, true);  // setters don't fire CHANGE
  bool remembered = false;
  remember.on(RD_EVENT_CHANGE, [&] { remembered = rdx::checked(remember); });
  // ANCHOR_END: toggles

  // ANCHOR: radio
  rd::Node size = rdx::radio_group(ctx);
  for (const char* s : {"Small", "Medium", "Large"}) rdx::add_radio(size, s);
  rdx::radio_select(size, 1);
  size.on(RD_EVENT_CHANGE, [&] { /* rdx::radio_selected(size) */ });
  // ANCHOR_END: radio

  // ANCHOR: slider
  rd::Node    volume = rdx::slider(ctx, {.min = 0, .max = 100, .value = 40, .step = 5});
  std::string shown;
  volume.on(RD_EVENT_INPUT, [&] { shown = std::to_string(static_cast<int>(rdx::slider_value(volume))); });  // while moving
  volume.on(RD_EVENT_CHANGE, [&] { /* save it */ });  // drag released or key pressed
  // ANCHOR_END: slider

  // ANCHOR: number
  rd::Node particles = rdx::number(ctx, {.min = 1000, .max = 100000, .value = 12000, .step = 1000});
  rd::Node exposure  = rdx::number(ctx, {.min = 0.2f, .max = 3, .value = 1, .step = 0.05f, .decimals = 2});
  exposure.on(RD_EVENT_INPUT, [&] { /* rdx::number_value(exposure) */ });  // every change
  // ANCHOR_END: number

  ctx.root().append(remember).append(sync).append(size).append(volume).append(particles).append(exposure);
  ctx.layout(800, 600);
  remember.focus();
  ctx.press(RD_KEY_SPACE);
  volume.focus();
  ctx.press(RD_KEY_END);
  assert(remembered && rdx::checked(sync) && rdx::radio_selected(size) == 1 && shown == "100");

  // ANCHOR: theme
  const rdx_classes* theme = rdx_theme(ctx.get());
  rd::Class(ctx.get(), theme->check_box).on(RD_STATE_CHECKED, rd::Style().background(rd::hex(0x4cc38a)));  // green
  // ANCHOR_END: theme

  // ANCHOR: toast
  rd::Node t = rdx::toast(ctx, {.title    = "Exported",
                                .text     = "report.csv is in your Downloads folder.",
                                .position = RDX_TOAST_TOP_RIGHT,
                                .kind     = RDX_TOAST_SUCCESS});
  std::string host = "db.internal";
  rdx::toast(ctx, {.text = "Lost connection to " + host, .kind = RDX_TOAST_ERROR, .seconds = -1});  // until closed
  // ANCHOR_END: toast
  assert(t.alive());
  ctx.set_time(4.0);  // the toast's four seconds are up: it fades out
  ctx.set_time(4.5);
  assert(!t.alive());
  return 0;
}
