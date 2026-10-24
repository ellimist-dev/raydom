// The raydom showcase: a tour of every feature, a desktop messaging app, a Paxos simulation and a routed
// app, in tabs.
// The font is compiled into the binary; nothing is loaded from disk.

#include <raylib.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#include <cmath>
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
rdx::Tabs build(rd::Context& ctx) {
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

  rdx::Tabs tabs = rdx::tabs(ctx);
  tabs.merge_style(rd::Style().flex(1).min_height(0_px));
  rd::Node bar = tabs.first_child();
  bar.merge_style(rd::Style().padding(10_px, 16_px, 0_px, 16_px).background(rd::hex(kPanel)).border_color(rd::hex(kLine)));
  rd::Style panel = rd::Style().flex(1).min_height(0_px).padding(0_px).row_gap(0_px);
  rd::Node  tour = tabs.add("Tour"), messages = tabs.add("Messages"), network = tabs.add("Network");
  rd::Node  routes = tabs.add("Router");
  for (rd::Node p : {tour, messages, network, routes}) p.merge_style(panel);
  tour.append(build_tour(ctx));
  messages.append(build_messages(ctx));
  network.append(build_network(ctx));
  routes.append(build_routes(ctx));
  ctx.root().append(header).append(tabs);
  return tabs;
}

// RAYDOM_SCREENSHOT=<file.png> renders a few frames, saves one to the working directory and exits.
// RAYDOM_TAB=<n> opens a top-level tab first (0 Tour, 1 Messages, 2 Network, 3 Router). RAYDOM_SCROLL=<y> scrolls the
// tour; RAYDOM_MOUSE=<x>,<y> places the pointer; RAYDOM_CLICK=<x>,<y>[;<x>,<y>...] clicks, then
// RAYDOM_TABS=<n> presses Tab and RAYDOM_TYPE=<text> types ('|' presses Enter), and
// RAYDOM_DRAG=<x0>,<y0>,<x1>,<y1> drags with the primary button, before the capture.
// RAYDOM_FRAME=<n> captures frame n instead of 3, to let animations finish.
struct Program {
  rd::Context ctx;
  rdx::Tabs   tabs;
  const char* shot          = std::getenv("RAYDOM_SCREENSHOT");
  int         capture_frame = 3;
  float       mx = -1, my = -1;
  int         frame = 0;
};

void start(Program& p) {
  if (const char* f = std::getenv("RAYDOM_FRAME")) p.capture_frame = std::atoi(f);
  // Idle frames wait for input, except while a timer is pending, such as a toast counting down.
  if (!p.shot) rd_set_event_waiting(p.ctx.get(), true);
  p.ctx.load_font(kFont, sizeof kFont, false);
  p.tabs = build(p.ctx);
  if (const char* tab = std::getenv("RAYDOM_TAB")) p.tabs.select(std::atoi(tab));
  if (const char* m = std::getenv("RAYDOM_MOUSE")) {
    char* end = nullptr;
    p.mx      = std::strtof(m, &end);
    p.my      = *end == ',' ? std::strtof(end + 1, nullptr) : -1;
  }
}

// Lays the tree out at the window's size in logical pixels, as rd_update does.
void layout(const rd::Context& ctx) {
  float ratio = ctx.pixel_ratio();
  ctx.layout(static_cast<float>(GetScreenWidth()) / ratio, static_cast<float>(GetScreenHeight()) / ratio);
}

// The screenshot mode's scripted input, on the first frame.
void script(Program& p) {
  rd::Context& ctx = p.ctx;
  if (const char* s = std::getenv("RAYDOM_SCROLL")) tour_scroller().scroll_to(0, std::strtof(s, nullptr));
  if (p.frame == 1) {
    if (const char* c = std::getenv("RAYDOM_CLICK"))
      while (*c) {
        char* end = nullptr;
        float cx  = std::strtof(c, &end);
        float cy  = std::strtof(end + 1, &end);
        layout(ctx);
        ctx.pointer(cx, cy, RD_BUTTON_PRIMARY);
        ctx.pointer(cx, cy, 0);
        c = *end == ';' ? end + 1 : end;
      }
    if (const char* n = std::getenv("RAYDOM_TABS"))
      for (long j = std::strtol(n, nullptr, 10); j > 0; j--) ctx.press(RD_KEY_TAB);
    if (const char* typed = std::getenv("RAYDOM_TYPE"))
      for (const char* q = typed; *q; q++) {
        if (*q == '|')
          ctx.press(RD_KEY_ENTER);
        else
          ctx.type(std::string_view(q, 1));
      }
    if (const char* d = std::getenv("RAYDOM_DRAG")) {
      float pt[4] = {0, 0, 0, 0};
      char* end   = const_cast<char*>(d);
      for (float& v : pt) {
        v = std::strtof(end, &end);
        if (*end == ',') end++;
      }
      ctx.pointer(pt[0], pt[1], RD_BUTTON_PRIMARY);
      for (int step = 1; step <= 8; step++) {
        float f = static_cast<float>(step) / 8;
        ctx.pointer(pt[0] + (pt[2] - pt[0]) * f, pt[1] + (pt[3] - pt[1]) * f, RD_BUTTON_PRIMARY);
        layout(ctx);
      }
      ctx.pointer(pt[2], pt[3], 0);
    }
  }
  if (p.mx >= 0) ctx.pointer(p.mx, p.my, 0);
}

#ifdef __EMSCRIPTEN__
// raylib renders the web at the canvas's own resolution. Keeping the canvas at the window's size in
// device pixels, shown at CSS size, and telling raydom the ratio keeps text and edges sharp.
void fit_canvas(const rd::Context& ctx) {
  double ratio = emscripten_get_device_pixel_ratio();
  int    w     = static_cast<int>(std::lround(EM_ASM_DOUBLE(return window.innerWidth;) * ratio));
  int    h     = static_cast<int>(std::lround(EM_ASM_DOUBLE(return window.innerHeight;) * ratio));
  if (w != GetScreenWidth() || h != GetScreenHeight()) SetWindowSize(w, h);
  ctx.set_pixel_ratio(static_cast<float>(ratio));
}
#endif

// One frame; false when the program should end.
bool frame(Program& p) {
#ifdef __EMSCRIPTEN__
  fit_canvas(p.ctx);
#endif
  p.ctx.update();
  if (p.shot) script(p);
  tour_frame(p.ctx);
  layout(p.ctx);
  BeginDrawing();
  ClearBackground(Color{0x0f, 0x11, 0x15, 0xff});
  p.ctx.draw();
  EndDrawing();
  if (p.shot && p.frame == p.capture_frame) {
    TakeScreenshot(p.shot);
    return false;
  }
  p.frame++;
  return true;
}

}  // namespace

int main() {  // NOLINT(bugprone-exception-escape): std::bad_alloc should terminate.
#ifdef __EMSCRIPTEN__
  // fit_canvas sizes the canvas. raylib's resizable handler would set it back to CSS pixels, and its
  // high-DPI flag scales the scissor without rendering at high density.
  SetConfigFlags(FLAG_VSYNC_HINT);
#else
  SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
#endif
  InitWindow(1180, 780, "raydom showcase");
  SetWindowMinSize(760, 480);
  SetExitKey(KEY_NULL);
#ifdef __EMSCRIPTEN__
  // The browser drives the frames, and main returns while they continue: the program lives as long as
  // the page.
  auto* p = new Program;
  start(*p);
  emscripten_set_main_loop_arg([](void* data) { frame(*static_cast<Program*>(data)); }, p, 0, false);
#else
  {
    Program p;
    start(p);
    while (!WindowShouldClose() && frame(p)) {
    }
  }
  CloseWindow();
#endif
}
