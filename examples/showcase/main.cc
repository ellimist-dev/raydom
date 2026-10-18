// The raydom showcase: a tour of every feature, a desktop messaging app and a Paxos simulation, in tabs.
// The font is compiled into the binary; nothing is loaded from disk.

#include <raylib.h>

#include <cstdlib>
#include <raydom/extras/views.hh>
#include <string>

#include "showcase.h"

using namespace rd::literals;

namespace {

const unsigned char kFont[] = {
#embed "anonymous_pro_bold.ttf"
};

// Returns the top-level tabs.
rd::Node build(rd::Context& ctx) {
  ctx.root().style(rd::Style().background(rd::hex(kBg)).color(rd::hex(kText)).font_size(14_px));

  // The header holds the logo, and the top-level tabs sit under it.
  rd::Node header = ctx.create(rd::Style()
                                   .flex_direction(RD_FLEX_ROW)
                                   .align_items(RD_ALIGN_CENTER)
                                   .column_gap(12_px)
                                   .padding(12_px, 20_px, 0_px, 20_px)
                                   .background(rd::hex(kPanel)));
  header.append(ctx.create(rd::Style()
                               .width(22_px)
                               .height(22_px)
                               .border_radius(6_px)
                               .background(rd::hex(kAccent))
                               .box_shadow(0, 0, 12, rd::rgba(91, 140, 255, 160))));
  header.append(ctx.create_text("raydom", rd::Style().font_size(18_px)));
  header.append(ctx.create_text("a DOM for raylib", rd::Style().color(rd::hex(kMuted))));

  rd::Node tabs = rdx::tabs(ctx);
  tabs.merge_style(rd::Style().flex(1).min_height(0_px));
  rd::Node bar = tabs.first_child();
  bar.merge_style(rd::Style().padding(10_px, 16_px, 0_px, 16_px).background(rd::hex(kPanel)).border_color(rd::hex(kLine)));
  rd::Style panel = rd::Style().flex(1).min_height(0_px).padding(0_px).row_gap(0_px);
  rd::Node  tour = rdx::add_tab(tabs, "Tour"), messages = rdx::add_tab(tabs, "Messages"), network = rdx::add_tab(tabs, "Network");
  for (rd::Node p : {tour, messages, network}) p.merge_style(panel);
  tour.append(build_tour(ctx));
  messages.append(build_messages(ctx));
  network.append(build_network(ctx));
  ctx.root().append(header).append(tabs);
  return tabs;
}

}  // namespace

// RAYDOM_SCREENSHOT=<file.png> renders a few frames, saves one to the working directory and exits.
// RAYDOM_TAB=<n> opens a top-level tab first (0 Tour, 1 Messages, 2 Network). RAYDOM_SCROLL=<y> scrolls the
// tour; RAYDOM_MOUSE=<x>,<y> places the pointer; RAYDOM_CLICK=<x>,<y>[;<x>,<y>...] clicks, then
// RAYDOM_TABS=<n> presses Tab and RAYDOM_TYPE=<text> types ('|' presses Enter), and
// RAYDOM_DRAG=<x0>,<y0>,<x1>,<y1> drags with the primary button, before the capture.
// RAYDOM_FRAME=<n> captures frame n instead of 3, to let animations finish.
int main() {  // NOLINT(bugprone-exception-escape): std::bad_alloc should terminate.
  const char* shot          = std::getenv("RAYDOM_SCREENSHOT");
  const char* frame_env     = std::getenv("RAYDOM_FRAME");
  const int   capture_frame = frame_env ? std::atoi(frame_env) : 3;
  SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(1180, 780, "raydom showcase");
  SetWindowMinSize(760, 480);
  SetExitKey(KEY_NULL);
  {
    rd::Context ctx;
    // Idle frames wait for input, except while a timer is pending, such as a toast counting down.
    if (!shot) rd_set_event_waiting(ctx.get(), true);
    ctx.load_font(kFont, sizeof kFont, false);
    rd::Node tabs = build(ctx);
    if (const char* tab = std::getenv("RAYDOM_TAB")) rdx::select_tab(tabs, std::atoi(tab));
    float mx = -1, my = -1;
    if (const char* m = std::getenv("RAYDOM_MOUSE")) {
      char* end = nullptr;
      mx        = std::strtof(m, &end);
      my        = *end == ',' ? std::strtof(end + 1, nullptr) : -1;
    }
    for (int frame = 0; !WindowShouldClose(); frame++) {
      ctx.update();
      if (shot) {
        if (const char* s = std::getenv("RAYDOM_SCROLL")) tour_scroller().scroll_to(0, std::strtof(s, nullptr));
        if (const char* c = std::getenv("RAYDOM_CLICK"); c && frame == 1) {
          while (*c) {
            char* end = nullptr;
            float cx  = std::strtof(c, &end);
            float cy  = std::strtof(end + 1, &end);
            ctx.layout(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
            ctx.pointer(cx, cy, RD_BUTTON_PRIMARY);
            ctx.pointer(cx, cy, 0);
            c = *end == ';' ? end + 1 : end;
          }
        }
        if (frame == 1) {
          if (const char* n = std::getenv("RAYDOM_TABS"))
            for (long j = std::strtol(n, nullptr, 10); j > 0; j--) ctx.press(RD_KEY_TAB);
          if (const char* typed = std::getenv("RAYDOM_TYPE"))
            for (const char* p = typed; *p; p++) {
              if (*p == '|')
                ctx.press(RD_KEY_ENTER);
              else
                ctx.type(std::string_view(p, 1));
            }
          if (const char* d = std::getenv("RAYDOM_DRAG")) {
            float p[4] = {0, 0, 0, 0};
            char* end  = const_cast<char*>(d);
            for (float& v : p) {
              v = std::strtof(end, &end);
              if (*end == ',') end++;
            }
            ctx.pointer(p[0], p[1], RD_BUTTON_PRIMARY);
            for (int step = 1; step <= 8; step++) {
              float f = static_cast<float>(step) / 8;
              ctx.pointer(p[0] + (p[2] - p[0]) * f, p[1] + (p[3] - p[1]) * f, RD_BUTTON_PRIMARY);
              ctx.layout(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
            }
            ctx.pointer(p[2], p[3], 0);
          }
        }
        if (mx >= 0) ctx.pointer(mx, my, 0);
      }
      tour_frame(ctx);
      ctx.layout(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
      BeginDrawing();
      ClearBackground(Color{0x0f, 0x11, 0x15, 0xff});
      ctx.draw();
      EndDrawing();
      if (shot && frame == capture_frame) {
        TakeScreenshot(shot);
        break;
      }
    }
  }
  CloseWindow();
}
