// A tour of raydom: flexbox, grid, text, boxes and interactive states, in one window.
// The font is compiled into the binary; nothing is loaded from disk.

#include <raylib.h>

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <raydom/raydom.hh>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace rd::literals;

namespace {

const unsigned char kFont[] = {
#embed "anonymous_pro_bold.ttf"
};

constexpr std::uint32_t kBg = 0x0f1115, kPanel = 0x151922, kRaised = 0x1a1f2b, kLine = 0x272d3a, kText = 0xe4e7ee,
                        kMuted = 0x8a93a6, kAccent = 0x5b8cff, kAccentHi = 0x7aa2ff, kAccentLo = 0x456fe0;

// Attached to nodes that name themselves in the status bar.
struct Info {
  std::string label;
};

struct Theme {
  rd::Class card, chip, pill, nav, nav_selected, muted, heading, code, swatch;
  rd::Class btn, btn_primary, btn_secondary, btn_ghost, field;
};

struct App {
  rd::Node              main, status, counter;
  std::vector<rd::Node> nav, sections;
  std::string           last_status;
  std::size_t           selected = 0;
  int                   clicks   = 0;

  rd::Node                 log_text, rows_label, list, slider_fill, slider_thumb, slider_zone, slider_value, hover_label;
  std::vector<std::string> log_lines;
  bool                     stop_at_middle = false;
  int                      rows_made = 0, entered = 0, left = 0;

  rd::Node name, email, notes, notes_count, summary;
  float    drag_x = 0, drag_w = 0;

  void log(std::string line) {
    log_lines.push_back(std::move(line));
    std::string joined;
    for (const auto& l : log_lines) joined += l + "\n";
    log_text.text(joined);
  }
};

Theme make_theme(const rd::Context& ctx) {
  Theme t;
  t.card    = ctx.create_class(rd::Style()
                                   .background(rd::hex(kRaised))
                                   .border_width(1_px)
                                   .border_color(rd::hex(kLine))
                                   .border_radius(10_px)
                                   .padding(16_px)
                                   .row_gap(8_px)
                                   .box_shadow(0, 8, 20, rd::rgba(0, 0, 0, 110)));
  t.chip    = ctx.create_class(rd::Style().padding(4_px, 10_px).border_radius(6_px).background(rd::hex(0x232938)));
  t.pill    = ctx.create_class(rd::Style()
                                   .padding(3_px, 10_px)
                                   .border_radius(999_px)
                                   .border_width(1_px)
                                   .border_color(rd::hex(kLine))
                                   .color(rd::hex(kMuted))
                                   .font_size(12_px)
                                   .user_select(RD_USER_SELECT_NONE));
  t.muted   = ctx.create_class(rd::Style().color(rd::hex(kMuted)));
  t.heading = ctx.create_class(rd::Style().font_size(20_px));
  t.code    = ctx.create_class(rd::Style()
                                   .background(rd::hex(0x0b0d12))
                                   .border_width(1_px)
                                   .border_color(rd::hex(kLine))
                                   .border_radius(8_px)
                                   .padding(12_px)
                                   .white_space(RD_WHITE_SPACE_PRE)
                                   .color(rd::hex(0xc7d2fe))
                                   .font_size(13_px));
  t.swatch  = ctx.create_class(rd::Style()
                                   .width(120_px)
                                   .height(80_px)
                                   .align_items(RD_ALIGN_CENTER)
                                   .justify_content(RD_ALIGN_CENTER)
                                   .border_radius(8_px)
                                   .background(rd::hex(kAccent))
                                   .font_size(12_px));

  // Focus rings show for keyboard focus only.
  const rd::Style ring = rd::Style().box_shadow(0, 0, 0, rd::rgba(91, 140, 255, 200), 2);
  t.nav                = ctx.create_class(
      rd::Style().padding(8_px, 12_px).border_radius(6_px).color(rd::hex(kMuted)).focusable(RD_FOCUSABLE_TAB).user_select(RD_USER_SELECT_NONE));
  t.nav.on(RD_STATE_FOCUS_VISIBLE, ring);
  t.nav.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x1d2230)).color(rd::hex(kText)));
  t.nav.on(RD_STATE_ACTIVE, rd::Style().background(rd::hex(0x242b3c)));
  t.nav_selected = ctx.create_class(rd::Style().background(rd::hex(0x1e2a45)).color(rd::hex(kText)));
  t.nav_selected.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x223050)));

  t.btn = ctx.create_class(rd::Style()
                               .flex_direction(RD_FLEX_ROW)
                               .align_items(RD_ALIGN_CENTER)
                               .padding(8_px, 16_px)
                               .border_radius(8_px)
                               .border_width(1_px)
                               .focusable(RD_FOCUSABLE_TAB)
                               .user_select(RD_USER_SELECT_NONE));
  t.btn.on(RD_STATE_FOCUS_VISIBLE, ring);
  t.btn.on(RD_STATE_DISABLED, rd::Style().opacity(rd::num(0.4f)));

  t.field = ctx.create_class(rd::Style()
                                 .padding(8_px, 12_px)
                                 .border_radius(8_px)
                                 .border_width(1_px)
                                 .border_color(rd::hex(kLine))
                                 .background(rd::hex(0x0b0d12))
                                 .caret_color(rd::hex(kAccentHi)));
  t.field.on(RD_STATE_HOVER, rd::Style().border_color(rd::hex(0x39415a)));
  t.field.on(RD_STATE_FOCUS, rd::Style().border_color(rd::hex(kAccent)));
  t.btn_primary = ctx.create_class(
      rd::Style().background(rd::hex(kAccent)).border_color(rd::hex(kAccent)).color(rd::hex(0xffffff)));
  t.btn_primary.on(RD_STATE_HOVER, rd::Style().background(rd::hex(kAccentHi)));
  t.btn_primary.on(RD_STATE_ACTIVE, rd::Style().background(rd::hex(kAccentLo)));
  t.btn_primary.on(RD_STATE_DISABLED, rd::Style().background(rd::hex(kAccent)));
  t.btn_secondary = ctx.create_class(rd::Style().background(rd::hex(0x1d2230)).border_color(rd::hex(kLine)));
  t.btn_secondary.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x252c3d)));
  t.btn_secondary.on(RD_STATE_ACTIVE, rd::Style().background(rd::hex(kRaised)));
  t.btn_ghost = ctx.create_class(
      rd::Style().background(rd::rgba(0, 0, 0, 0)).border_color(rd::rgba(0, 0, 0, 0)).color(rd::hex(kMuted)));
  t.btn_ghost.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x1d2230)).color(rd::hex(kText)));
  return t;
}

rd::Node text(const rd::Context& ctx, std::string_view s, const rd::Style& style = {}) {
  return ctx.create_text(s, style);
}

rd::Node labeled(rd::Node n, std::string label) {
  n.emplace_userdata<Info>(Info{std::move(label)});
  return n;
}

std::pair<rd::Node, rd::Node> section(const rd::Context& ctx, const Theme& t, std::string_view title,
                                      std::string_view blurb) {
  rd::Node root = ctx.create(rd::Style().row_gap(14_px));
  rd::Node head = ctx.create(rd::Style().row_gap(4_px));
  head.append(text(ctx, title).add_class(t.heading)).append(text(ctx, blurb).add_class(t.muted));
  rd::Node body = ctx.create(rd::Style().row_gap(14_px));
  root.append(head).append(body);
  return {root, body};
}

rd::Node build_flexbox(const rd::Context& ctx, const Theme& t) {
  auto [root, body] = section(ctx, t, "Flexbox", "Direction, justification, wrapping and grow factors.");

  const std::pair<const char*, rd_align> modes[] = {
      {"start", RD_ALIGN_START},
      {"center", RD_ALIGN_CENTER},
      {"space-between", RD_ALIGN_SPACE_BETWEEN},
      {"space-evenly", RD_ALIGN_SPACE_EVENLY},
  };
  for (auto [name, mode] : modes) {
    rd::Node row   = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(12_px));
    rd::Node track = ctx.create(rd::Style()
                                    .flex(1)
                                    .flex_direction(RD_FLEX_ROW)
                                    .justify_content(mode)
                                    .column_gap(8_px)
                                    .padding(8_px)
                                    .background(rd::hex(kPanel))
                                    .border_radius(8_px)
                                    .border_width(1_px)
                                    .border_color(rd::hex(kLine)));
    for (int k = 1; k <= 3; k++) track.append(text(ctx, "item " + std::to_string(k)).add_class(t.chip));
    row.append(text(ctx, name, rd::Style().width(130_px)).add_class(t.muted)).append(track);
    body.append(labeled(row, std::string("justify-content: ") + name));
  }

  const char* tags[] = {"layout", "flexbox", "grid", "text wrapping", "baselines", "hover states",
                        "box shadows", "rounded corners", "borders", "scrolling", "percentages", "min-content",
                        "fr tracks", "auto margins", "absolute", "opacity", "UTF-8", "classes"};
  rd::Node    wrap   = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).gap(8_px));
  for (const char* tag : tags) wrap.append(text(ctx, tag).add_class(t.pill));
  body.append(labeled(wrap, "flex-wrap: wrap"));

  rd::Node grow = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).column_gap(8_px));
  for (int f : {1, 2, 1}) {
    rd::Node box = ctx.create(rd::Style()
                                  .flex(static_cast<float>(f))
                                  .padding(12_px)
                                  .align_items(RD_ALIGN_CENTER)
                                  .border_radius(8_px)
                                  .background(rd::hex(f == 2 ? kAccentLo : 0x232938)));
    box.append(text(ctx, "flex: " + std::to_string(f)));
    grow.append(labeled(box, "flex-grow " + std::to_string(f)));
  }
  body.append(grow);
  return root;
}

rd::Node build_grid(const rd::Context& ctx, const Theme& t) {
  auto [root, body] = section(ctx, t, "Grid", "Fractional tracks, spans, auto tracks sized to content.");

  rd::Node                                  cards   = ctx.create(rd::Style()
                                                                     .display(RD_DISPLAY_GRID)
                                                                     .grid_template_columns(ctx.tracks(rd::repeat(3, {1_fr})))
                                                                     .gap(16_px));
  const std::pair<const char*, const char*> items[] = {
      {"Track sizing",
       "Columns resolve min-content and max-content contributions before distributing free "
       "space to fr tracks, so a 1fr column never shrinks below its longest word."},
      {"Placement", "Items auto-place row by row, or sit on explicit lines."},
      {"Alignment", "justify-items and align-items position items inside their grid areas."},
      {"Spans", "This card spans two columns."},
      {"Gaps", "Row and column gaps resolve against the grid's content box."},
  };
  int index = 0;
  for (auto [title, blurb] : items) {
    rd::Node card = ctx.create().add_class(t.card);
    card.append(text(ctx, title, rd::Style().font_size(16_px))).append(text(ctx, blurb).add_class(t.muted));
    if (index == 3) card.merge_style(rd::Style().grid_column(rd::span(2)));
    if (index == 0) {
      card.append(text(ctx, "NEW",
                       rd::Style()
                           .position(RD_POSITION_ABSOLUTE)
                           .inset(10_px, 10_px, rd::auto_, rd::auto_)
                           .background(rd::hex(kAccent))
                           .color(rd::hex(0xffffff))
                           .border_color(rd::hex(kAccent)))
                      .add_class(t.pill));
    }
    cards.append(labeled(card, std::string("card: ") + title));
    index++;
  }
  body.append(cards);

  rd::Node table = ctx.create(rd::Style()
                                  .display(RD_DISPLAY_GRID)
                                  .grid_template_columns(ctx.tracks({rd::auto_, 1_fr}))
                                  .row_gap(8_px)
                                  .column_gap(24_px))
                       .add_class(t.card);
  const std::pair<const char*, const char*> rows[] = {
      {"Renderer", "raylib, one SDF shader for rounded boxes and shadows"},
      {"Layout", "flexbox and grid, cached per node"},
      {"Text", "stb_truetype, kerning, greedy line breaking"},
      {"Binary", "single executable, assets compiled in"},
  };
  for (auto [key, value] : rows) table.append(text(ctx, key).add_class(t.muted)).append(text(ctx, value));
  body.append(labeled(table, "grid: auto 1fr"));
  return root;
}

std::string lower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return out;
}

// Every occurrence of needle in hay, case-insensitively, as highlight ranges.
std::vector<rd_highlight> find_all(std::string_view hay, std::string_view needle, rd_color bg, rd_color fg) {
  std::vector<rd_highlight> out;
  std::string               h = lower(hay), n = lower(needle);
  if (n.empty()) return out;
  for (std::size_t at = h.find(n); at != std::string::npos; at = h.find(n, at + n.size()))
    out.push_back({at, at + n.size(), bg, fg});
  return out;
}

rd::Node caption(const rd::Context& ctx, std::string_view s) {
  return ctx.create_text(s, rd::Style().font_size(12_px).color(rd::hex(kMuted)));
}

rd::Node build_text(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Text", "Wrapping, alignment, white-space modes and baselines.");

  rd::Node para = ctx.create().add_class(t.card);
  para.append(text(ctx,
                   "Text nodes are leaves measured through the same contract as any other box: at a width, "
                   "at most a width, or at their min-content and max-content sizes. Resize the window and "
                   "this paragraph re-wraps on word boundaries; whitespace collapses as in CSS."));
  body.append(labeled(para, "paragraph"));

  rd::Node                                    aligns  = ctx.create(rd::Style()
                                                                       .display(RD_DISPLAY_GRID)
                                                                       .grid_template_columns(ctx.tracks(rd::repeat(3, {1_fr})))
                                                                       .gap(12_px));
  const std::pair<const char*, rd_text_align> modes[] = {
      {"start", RD_TEXT_ALIGN_START}, {"center", RD_TEXT_ALIGN_CENTER}, {"end", RD_TEXT_ALIGN_END}};
  for (auto [name, mode] : modes) {
    rd::Node box = ctx.create().add_class(t.card);
    box.append(text(ctx, std::string("text-align: ") + name + ". Lines align within the box.",
                    rd::Style().text_align(mode)));
    aligns.append(labeled(box, std::string("text-align: ") + name));
  }
  body.append(aligns);

  const char* sample =
      "auto card = ctx.create_class(rd::Style()\n"
      "    .padding(16_px)\n"
      "    .border_radius(10_px)\n"
      "    .background(rd::hex(0x1a1f2b)));\n"
      "\n"
      "ctx.root().append(ctx.create().add_class(card));";
  rd::Node                  code = text(ctx, sample).add_class(t.code);
  std::vector<rd_highlight> keywords;
  const rd_color            none = rd::rgba(0, 0, 0, 0);
  for (auto [word, color] : {std::pair{"auto", 0xc792ea}, {"rd::", 0x82aaff}, {"_px", 0xf78c6c}, {"0x1a1f2b", 0xf78c6c}}) {
    auto found = find_all(sample, word, none, rd::hex(static_cast<std::uint32_t>(color)));
    keywords.insert(keywords.end(), found.begin(), found.end());
  }
  code.highlights(keywords);
  body.append(labeled(code, "white-space: pre, highlighted keywords"));

  rd::Node baseline = ctx.create(
      rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_BASELINE).column_gap(16_px).padding(12_px));
  baseline.add_class(t.card);
  for (float size : {12.0f, 18.0f, 28.0f, 40.0f}) baseline.append(text(ctx, "Baseline", rd::Style().font_size(rd::px(size))));
  body.append(labeled(baseline, "align-items: baseline"));

  rd::Node clip = ctx.create(rd::Style().width(260_px).overflow_x(RD_OVERFLOW_HIDDEN)).add_class(t.card);
  clip.append(text(ctx, "white-space: nowrap keeps this on one line; overflow: hidden clips it.",
                   rd::Style().white_space(RD_WHITE_SPACE_NOWRAP)));
  body.append(labeled(clip, "overflow: hidden"));

  // One long line, twice: scrolling horizontally, and cut with an ellipsis.
  const char* log_line =
      "2026-10-18 10:58:03.142 INFO raydom.render frame=4182 nodes=220 commands=611 glyphs=3904 atlas=2048x2048 "
      "shader=sdf draw_calls=37 layout=0.21ms paint=0.48ms pointer=(612,404) buttons=0 focus=input:Name";
  rd::Node scroller = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).overflow_x(RD_OVERFLOW_SCROLL)).add_class(t.code);
  scroller.append(text(ctx, log_line, rd::Style().white_space(RD_WHITE_SPACE_NOWRAP)));
  rd::Node cut = text(ctx, log_line, rd::Style().white_space(RD_WHITE_SPACE_NOWRAP).text_overflow(RD_TEXT_OVERFLOW_ELLIPSIS));
  body.append(caption(ctx, "A long line scrolls horizontally: trackpad, or Shift+wheel."))
      .append(labeled(scroller, "long line, overflow-x: scroll"))
      .append(caption(ctx, "The same line with text-overflow: ellipsis."))
      .append(labeled(cut.add_class(t.code), "long line, ellipsis"));

  // A paragraph clamped to three lines in a box you can resize by dragging its edge.
  rd::Class handle_style = ctx.create_class(
      rd::Style().width(10_px).border_radius(5_px).margin(0_px, 0_px, 0_px, 6_px).background(rd::hex(0x232938)));
  handle_style.on(RD_STATE_HOVER, rd::Style().background(rd::hex(kAccentLo)));
  handle_style.on(RD_STATE_ACTIVE, rd::Style().background(rd::hex(kAccent)));
  rd::Node panel = ctx.create(rd::Style().width(460_px).row_gap(6_px)).add_class(t.card);
  rd::Node width = caption(ctx, "width: 460px, drag the handle");
  panel.append(text(ctx, "On the Analytical Engine", rd::Style().font_size(16_px)))
      .append(text(ctx,
                   "The Analytical Engine has no pretensions whatever to originate anything. It can do whatever we "
                   "know how to order it to perform. It can follow analysis; but it has no power of anticipating "
                   "any analytical relations or truths. Its province is to assist us in making available what we "
                   "are already acquainted with.",
                   rd::Style().line_clamp(rd::num(3)).text_overflow(RD_TEXT_OVERFLOW_ELLIPSIS))
                  .add_class(t.muted))
      .append(width);
  rd::Node handle = ctx.create().add_class(handle_style);
  handle.on(RD_EVENT_POINTER_DOWN, [&app, panel](rd::Event& e) {
    e.current().capture_pointer();
    app.drag_x = e.x();
    app.drag_w = panel.rect().width;
  });
  handle.on(RD_EVENT_POINTER_MOVE, [&ctx, &app, panel, width](rd::Event& e) mutable {
    if (!(ctx.pointer_capture() == e.current())) return;
    float w = std::fmin(std::fmax(app.drag_w + e.x() - app.drag_x, 220.0f), 760.0f);
    panel.merge_style(rd::Style().width(rd::px(w)));
    width.text("width: " + std::to_string(static_cast<int>(w)) + "px, drag the handle");
  });
  rd::Node frame = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW));
  body.append(caption(ctx, "A paragraph clamped to three lines with line-clamp and an ellipsis; it reflows as the box resizes."))
      .append(labeled(frame.append(panel).append(handle), "line-clamp: 3"));

  // Search highlighting: matches update on every keystroke.
  rd::Node passage = text(ctx,
                          "The engine weaves algebraic patterns just as the Jacquard loom weaves flowers and leaves. "
                          "The operating mechanism acts upon other things besides number, were objects found whose "
                          "mutual relations could be expressed by those of the abstract science of operations.");
  rd::Node search  = ctx.create(rd::Style().width(280_px)).make_input().placeholder("Search the passage").add_class(t.field);
  rd::Node matches = caption(ctx, "");
  auto     apply   = [passage, matches](std::string_view query) mutable {
    auto found = find_all(passage.text(), query, rd::hex(0xffd84d), rd::hex(0x15171c));
    passage.highlights(found);
    matches.text(query.empty() ? "type to highlight" : std::to_string(found.size()) + " matches");
  };
  search.on(RD_EVENT_INPUT, [apply](rd::Event& e) mutable { apply(e.target().text()); });
  search.text("the");
  apply("the");
  rd::Node bar = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(12_px));
  body.append(caption(ctx, "Highlights: byte ranges with a background and an optional text color."))
      .append(bar.append(labeled(search, "input: search")).append(matches))
      .append(labeled(ctx.create().add_class(t.card).append(passage), "highlighted passage"));
  return root;
}

rd::Node build_boxes(const rd::Context& ctx, const Theme& t) {
  auto [root, body] = section(ctx, t, "Boxes", "Per-corner radii, borders, shadows and opacity.");
  rd::Node row      = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).gap(16_px));

  auto swatch = [&](const char* label, const rd::Style& style) {
    rd::Node box = ctx.create(style).add_class(t.swatch);
    box.merge_style(style);
    box.append(text(ctx, label));
    row.append(labeled(box, label));
  };
  swatch("corners", rd::Style().border_radius(rd_corners{0_px, 28_px, 0_px, 28_px}));
  swatch("borders", rd::Style()
                        .background(rd::hex(kPanel))
                        .border_width(2_px, 6_px)
                        .border_color(rd::hex(kAccent))
                        .border_radius(12_px));
  swatch("square", rd::Style().border_radius(0_px).border_width(1_px).border_color(rd::hex(kAccentHi)));
  swatch("glow", rd::Style().box_shadow(0, 10, 28, rd::rgba(91, 140, 255, 140)));
  swatch("circle", rd::Style().width(80_px).border_radius(40_px));
  for (float o : {1.0f, 0.75f, 0.5f, 0.25f}) {
    char label[32];
    std::snprintf(label, sizeof label, "opacity %.2f", static_cast<double>(o));
    swatch(label, rd::Style().opacity(rd::num(o)));
  }
  body.append(row);
  return root;
}

rd::Node build_buttons(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body]                               = section(ctx, t, "States", "Classes carry hover, active and disabled variants.");
  rd::Node                                row     = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(12_px));
  const std::pair<const char*, rd::Class> kinds[] = {
      {"Primary", t.btn_primary}, {"Secondary", t.btn_secondary}, {"Ghost", t.btn_ghost}, {"Disabled", t.btn_primary}};
  int index = 0;
  for (auto [label, cls] : kinds) {
    rd::Node b = ctx.create().add_class(t.btn).add_class(cls);
    b.append(text(ctx, label));
    if (index == 3) b.set_state(RD_STATE_DISABLED, true);
    b.on_click([&app] { app.counter.text("clicks: " + std::to_string(++app.clicks)); });
    row.append(labeled(b, std::string("button: ") + label));
    index++;
  }
  app.counter = text(ctx, "clicks: 0").add_class(t.muted);
  row.append(app.counter);
  body.append(row);
  return root;
}

rd::Node button(const rd::Context& ctx, const Theme& t, rd::Class kind, std::string_view label) {
  rd::Node b = ctx.create().add_class(t.btn).add_class(kind);
  b.append(text(ctx, label, rd::Style().pointer_events(RD_POINTER_EVENTS_NONE)));
  return b;
}

const char* phase_name(rd_event_phase p) {
  return p == RD_PHASE_CAPTURE ? "capture" : p == RD_PHASE_TARGET ? "target "
                                                                  : "bubble ";
}

rd::Node build_events(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Events", "Capture and bubble phases, DOM changes from handlers, pointer capture.");

  // Propagation: every box listens in both phases and writes to the log.
  auto box = [&](std::uint32_t bg, float pad) {
    return ctx.create(rd::Style().padding(rd::px(pad)).border_radius(10_px).background(rd::hex(bg)).border_width(1_px).border_color(rd::hex(kLine)));
  };
  rd::Node outer = box(0x1d2333, 18), middle = box(0x243050, 18), inner = box(kAccentLo, 14);
  outer.merge_style(rd::Style().user_select(RD_USER_SELECT_NONE));
  inner.append(text(ctx, "click me", rd::Style().pointer_events(RD_POINTER_EVENTS_NONE)));
  middle.append(inner);
  outer.append(middle);
  const std::pair<rd::Node, const char*> boxes[] = {{outer, "outer"}, {middle, "middle"}, {inner, "inner"}};
  for (auto [node, name] : boxes) {
    bool is_outer = node == outer, is_middle = node == middle;
    node.on(RD_EVENT_CLICK, [&app, name, is_outer](rd::Event& e) {
      if (is_outer) app.log_lines.clear();
      app.log(std::string(phase_name(e.phase())) + "  " + name); }, true);
    node.on(RD_EVENT_CLICK, [&app, name, is_middle](rd::Event& e) {
      app.log(std::string(phase_name(e.phase())) + "  " + name);
      if (is_middle && app.stop_at_middle) {
        e.stop_propagation();
        app.log("stop_propagation() at middle");
      }
    });
  }
  rd::Node toggle = button(ctx, t, t.btn_secondary, "stop at middle: off");
  toggle.on_click([&app](rd::Event& e) {
    app.stop_at_middle = !app.stop_at_middle;
    e.current().first_child().text(app.stop_at_middle ? "stop at middle: on" : "stop at middle: off");
  });
  rd::Node left_col = ctx.create(rd::Style().row_gap(12_px).align_items(RD_ALIGN_START));
  left_col.append(labeled(outer, "propagation demo")).append(toggle);
  app.log_text = text(ctx, "click the boxes", rd::Style().flex(1).min_height(150_px)).add_class(t.code);
  body.append(ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).column_gap(16_px)).append(left_col).append(app.log_text));

  // DOM changes: rows are created by one handler and destroy themselves from another.
  app.list       = ctx.create(rd::Style().row_gap(6_px));
  app.rows_label = text(ctx, "rows: 0").add_class(t.muted);
  rd::Node add   = button(ctx, t, t.btn_primary, "Add row");
  add.on_click([&ctx, &t, &app] {
    rd::Node row = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).padding(6_px, 6_px, 6_px, 12_px)).add_class(t.chip);
    row.append(text(ctx, "Row " + std::to_string(++app.rows_made), rd::Style().flex(1)));
    rd::Node remove = button(ctx, t, t.btn_ghost, "remove");
    remove.on_click([&app](rd::Event& e) {
      e.current().parent().destroy();
      app.rows_label.text("rows: " + std::to_string(app.list.child_count()));
    });
    app.list.append(labeled(row, "row " + std::to_string(app.rows_made)).append(remove));
    app.rows_label.text("rows: " + std::to_string(app.list.child_count()));
  });
  rd::Node controls = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(12_px));
  body.append(controls.append(add).append(app.rows_label)).append(ctx.create(rd::Style().width(420_px)).append(app.list));

  // Pointer capture: the slider keeps receiving moves after the pointer leaves it.
  app.slider_zone  = ctx.create(rd::Style().width(320_px).padding(10_px, 0_px));
  rd::Node track   = ctx.create(rd::Style().height(8_px).border_radius(4_px).background(rd::hex(0x232938)));
  app.slider_fill  = ctx.create(rd::Style().width(40_pct).height(100_pct).border_radius(4_px).background(rd::hex(kAccent)));
  app.slider_thumb = ctx.create(rd::Style()
                                    .position(RD_POSITION_ABSOLUTE)
                                    .inset(-5_px, rd::auto_, rd::auto_, 40_pct)
                                    .margin(0_px, 0_px, 0_px, -9_px)
                                    .width(18_px)
                                    .height(18_px)
                                    .border_radius(9_px)
                                    .background(rd::hex(0xffffff))
                                    .box_shadow(0, 2, 6, rd::rgba(0, 0, 0, 120))
                                    .pointer_events(RD_POINTER_EVENTS_NONE));
  app.slider_zone.append(track.append(app.slider_fill).append(app.slider_thumb));
  app.slider_value = text(ctx, "value: 40%").add_class(t.muted);
  auto set_value   = [&app](float x) {
    float v = std::fmin(std::fmax(x / 320.0f, 0.0f), 1.0f) * 100.0f;
    app.slider_fill.merge_style(rd::Style().width(rd::pct(v)));
    app.slider_thumb.merge_style(rd::Style().inset(-5_px, rd::auto_, rd::auto_, rd::pct(v)));
    app.slider_value.text("value: " + std::to_string(static_cast<int>(v)) + "%");
  };
  app.slider_zone.on(RD_EVENT_POINTER_DOWN, [set_value](rd::Event& e) {
    e.current().capture_pointer();
    set_value(e.local_x());
  });
  app.slider_zone.on(RD_EVENT_POINTER_MOVE, [&ctx, &app, set_value](rd::Event& e) {
    if (ctx.pointer_capture() == app.slider_zone) set_value(e.local_x());
  });
  rd::Node slider_row = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(16_px));
  body.append(labeled(slider_row.append(app.slider_zone).append(app.slider_value), "slider (pointer capture)"));

  // Enter and leave don't bubble: moving onto the inner label doesn't leave the box.
  rd::Node hover = box(0x1d2333, 16);
  hover.merge_style(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(12_px).width(420_px));
  hover.append(text(ctx, "hover me").add_class(t.chip));
  app.hover_label = text(ctx, "entered 0 | left 0").add_class(t.muted);
  hover.append(app.hover_label);
  auto show = [&app] { app.hover_label.text("entered " + std::to_string(app.entered) + " | left " + std::to_string(app.left)); };
  hover.on(RD_EVENT_POINTER_ENTER, [&app, show] { app.entered++; show(); });
  hover.on(RD_EVENT_POINTER_LEAVE, [&app, show] { app.left++; show(); });
  body.append(labeled(hover, "enter/leave"));
  return root;
}

rd::Node build_forms(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Forms", "Focus, keyboard navigation and text input. Tab moves between controls.");

  rd::Node grid  = ctx.create(rd::Style()
                                  .display(RD_DISPLAY_GRID)
                                  .grid_template_columns(ctx.tracks({rd::auto_, 1_fr}))
                                  .column_gap(16_px)
                                  .row_gap(12_px)
                                  .align_items(RD_ALIGN_CENTER)
                                  .max_width(560_px));
  auto     field = [&](const char* label, rd::Node input) {
    grid.append(text(ctx, label).add_class(t.muted)).append(labeled(input.add_class(t.field), std::string("input: ") + label));
    return input;
  };
  app.name  = field("Name", ctx.create().make_input().placeholder("Ada Lovelace"));
  app.email = field("Email", ctx.create().make_input().placeholder("ada@example.com"));
  app.notes = field("Notes", ctx.create(rd::Style().min_height(88_px)).make_input(true).placeholder("Anything else?"));
  grid.append(ctx.create()).append(app.notes_count = text(ctx, "0 characters", rd::Style().font_size(12_px)).add_class(t.muted));
  app.notes.on(RD_EVENT_INPUT, [&app](rd::Event& e) {
    app.notes_count.text(std::to_string(e.target().text().size()) + " characters");
  });

  auto submit = [&app] {
    app.summary.text("Saved " + std::string(app.name.text().empty() ? "(no name)" : app.name.text()) + " <" +
                     std::string(app.email.text()) + ">, " + std::to_string(app.notes.text().size()) + " characters of notes.");
  };
  for (rd::Node n : {app.name, app.email}) n.on(RD_EVENT_CHANGE, submit);

  rd::Node save  = button(ctx, t, t.btn_primary, "Submit");
  rd::Node clear = button(ctx, t, t.btn_ghost, "Clear");
  save.on_click(submit);
  clear.on_click([&app] {
    for (rd::Node n : {app.name, app.email, app.notes}) n.text("");
    app.notes_count.text("0 characters");
    app.summary.text("Cleared.");
    app.name.focus();
  });
  app.summary      = text(ctx, "Enter in a single-line field also submits. Escape releases focus.").add_class(t.muted);
  rd::Node actions = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(12_px));
  body.append(grid).append(actions.append(save).append(clear).append(app.summary));
  return root;
}

void build(const rd::Context& ctx, const Theme& t, App& app) {
  ctx.root().style(rd::Style().background(rd::hex(kBg)).color(rd::hex(kText)).font_size(14_px));

  rd::Node header = ctx.create(rd::Style()
                                   .flex_direction(RD_FLEX_ROW)
                                   .align_items(RD_ALIGN_CENTER)
                                   .column_gap(12_px)
                                   .padding(12_px, 20_px)
                                   .background(rd::hex(kPanel))
                                   .border_width(0_px, 0_px, 1_px, 0_px)
                                   .border_color(rd::hex(kLine)));
  header.append(ctx.create(rd::Style()
                               .width(22_px)
                               .height(22_px)
                               .border_radius(6_px)
                               .background(rd::hex(kAccent))
                               .box_shadow(0, 0, 12, rd::rgba(91, 140, 255, 160))));
  header.append(text(ctx, "raydom", rd::Style().font_size(18_px)));
  header.append(text(ctx, "a DOM for raylib").add_class(t.muted));
  header.append(ctx.create(rd::Style().flex(1)));
  for (const char* p : {"flexbox", "grid", "text", "events", "forms"}) header.append(text(ctx, p).add_class(t.pill));

  rd::Node body    = ctx.create(rd::Style().flex(1).flex_direction(RD_FLEX_ROW));
  rd::Node sidebar = ctx.create(rd::Style()
                                    .width(200_px)
                                    .padding(12_px)
                                    .row_gap(4_px)
                                    .background(rd::hex(kPanel))
                                    .border_width(0_px, 1_px, 0_px, 0_px)
                                    .border_color(rd::hex(kLine)));
  sidebar.append(text(ctx, "SECTIONS", rd::Style().font_size(11_px).padding(8_px, 12_px)).add_class(t.muted));

  app.main            = ctx.create(rd::Style().flex(1).overflow_y(RD_OVERFLOW_SCROLL).padding(24_px).row_gap(36_px));
  app.sections        = {build_flexbox(ctx, t), build_grid(ctx, t), build_text(ctx, t, app),
                         build_boxes(ctx, t), build_buttons(ctx, t, app), build_events(ctx, t, app),
                         build_forms(ctx, t, app)};
  const char* names[] = {"Flexbox", "Grid", "Text", "Boxes", "States", "Events", "Forms"};
  for (int j = 0; j < 7; j++) {
    app.main.append(app.sections[static_cast<std::size_t>(j)]);
    rd::Node item = text(ctx, names[j]).add_class(t.nav);
    if (j == 0) item.add_class(t.nav_selected);
    item.on_click([&app, &t, item, j]() mutable {
      for (rd::Node n : app.nav) n.remove_class(t.nav_selected);
      item.add_class(t.nav_selected);
      app.main.scroll_to(0, app.sections[static_cast<std::size_t>(j)].rect().y - 24);
    });
    app.nav.push_back(item);
    sidebar.append(labeled(item, std::string("nav: ") + names[j]));
  }
  body.append(sidebar).append(app.main);

  rd::Node status = ctx.create(rd::Style()
                                   .flex_direction(RD_FLEX_ROW)
                                   .padding(6_px, 16_px)
                                   .background(rd::hex(kPanel))
                                   .border_width(1_px, 0_px, 0_px, 0_px)
                                   .border_color(rd::hex(kLine)));
  app.status      = text(ctx, "", rd::Style().font_size(12_px).user_select(RD_USER_SELECT_NONE)).add_class(t.muted);
  status.append(app.status);
  ctx.root().append(header).append(body).append(status);
  ctx.root().on(RD_EVENT_SELECTION_CHANGE, [&ctx, &app] { app.selected = rd_ctx_selection_text(ctx.get(), nullptr, 0); });
  ctx.root().on(RD_EVENT_KEY_DOWN, [&ctx](rd::Event& e) {
    if (e.key() == RD_KEY_ESCAPE) ctx.blur();
  });
}

std::pair<rd::Node, Info*> find_info(rd::Node n) {
  for (; n; n = n.parent())
    if (Info* info = n.userdata<Info>()) return {n, info};
  return {{}, nullptr};
}

void update_status(App& app, const rd::Context& ctx) {
  auto [node, info] = find_info(ctx.hovered());
  char line[256];
  std::snprintf(line, sizeof line, "hover: %-32s | scroll: %4.0f | selected: %zu chars | %dx%d | %u nodes",
                info ? info->label.c_str() : "-", static_cast<double>(app.main.scroll().y), app.selected, GetScreenWidth(),
                GetScreenHeight(), ctx.node_count());
  if (app.last_status != line) {
    app.last_status = line;
    app.status.text(app.last_status);
  }
}

}  // namespace

// RAYDOM_SCREENSHOT=<file.png> renders a few frames, saves one to the working directory and exits.
// RAYDOM_SCROLL=<y> scrolls the main panel first; RAYDOM_MOUSE=<x>,<y> places the pointer;
// RAYDOM_CLICK=<x>,<y>[;<x>,<y>...] clicks, then RAYDOM_TABS=<n> presses Tab and RAYDOM_TYPE=<text>
// types, and RAYDOM_DRAG=<x0>,<y0>,<x1>,<y1> drags with the primary button, before the capture.
int main() {  // NOLINT(bugprone-exception-escape): std::bad_alloc should terminate.
  const char* shot = std::getenv("RAYDOM_SCREENSHOT");
  SetConfigFlags(FLAG_WINDOW_HIGHDPI | FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
  InitWindow(1180, 780, "raydom showcase");
  SetWindowMinSize(760, 480);
  SetExitKey(KEY_NULL);
  if (!shot) EnableEventWaiting();
  {
    rd::Context ctx;
    ctx.load_font(kFont, sizeof kFont, false);
    Theme t = make_theme(ctx);
    App   app;
    build(ctx, t, app);
    float mx = -1, my = -1;
    if (const char* m = std::getenv("RAYDOM_MOUSE")) {
      char* end = nullptr;
      mx        = std::strtof(m, &end);
      my        = *end == ',' ? std::strtof(end + 1, nullptr) : -1;
    }
    for (int frame = 0; !WindowShouldClose(); frame++) {
      ctx.update();
      if (shot) {
        if (const char* s = std::getenv("RAYDOM_SCROLL")) app.main.scroll_to(0, std::strtof(s, nullptr));
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
          if (const char* typed = std::getenv("RAYDOM_TYPE")) ctx.type(typed);
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
      update_status(app, ctx);
      ctx.layout(static_cast<float>(GetScreenWidth()), static_cast<float>(GetScreenHeight()));
      BeginDrawing();
      ClearBackground(Color{0x0f, 0x11, 0x15, 0xff});
      ctx.draw();
      EndDrawing();
      if (shot && frame == 3) {
        TakeScreenshot(shot);
        break;
      }
    }
  }
  CloseWindow();
}
