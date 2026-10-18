// The Tour tab: one section per feature, with a sidebar to jump between them and a status bar.

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <raydom/extras/controls.hh>
#include <raydom/extras/dialog.hh>
#include <raydom/extras/popups.hh>
#include <raydom/extras/views.hh>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "showcase.h"

using namespace rd::literals;

namespace {

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
  rd::Node file_result;
  rd::Node popover, popover_toggle, popups_result;
  rd::Node anim_box, anim_easing, views_status, files_rows;
  bool     files_reversed = false;
  bool     anim_wide = false;
  rd::Node controls_summary, check_a, check_b, switch_a, switch_b, sizes, volume, toast_position;
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
      rd::Style().padding(8_px, 12_px).border_radius(6_px).color(rd::hex(kMuted)).focusable(RD_FOCUSABLE_TAB).user_select(RD_USER_SELECT_NONE).cursor(RD_CURSOR_POINTER).transition_duration(120_ms));
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
                               .user_select(RD_USER_SELECT_NONE)
                               .cursor(RD_CURSOR_POINTER)
                               .transition_duration(120_ms));
  t.btn.on(RD_STATE_FOCUS_VISIBLE, ring);
  t.btn.on(RD_STATE_DISABLED, rd::Style().opacity(rd::num(0.4f)).cursor(RD_CURSOR_NOT_ALLOWED));

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
  scroller.merge_style(rd::Style().padding(12_px, 12_px, 20_px, 12_px));  // room for the scrollbar under the line
  scroller.append(text(ctx, log_line, rd::Style().white_space(RD_WHITE_SPACE_NOWRAP)));
  rd::Node cut = text(ctx, log_line, rd::Style().white_space(RD_WHITE_SPACE_NOWRAP).text_overflow(RD_TEXT_OVERFLOW_ELLIPSIS));
  body.append(caption(ctx, "A long line scrolls horizontally: trackpad, Shift+wheel, or drag the scrollbar."))
      .append(labeled(scroller, "long line, overflow-x: scroll"))
      .append(caption(ctx, "The same line with text-overflow: ellipsis."))
      .append(labeled(cut.add_class(t.code), "long line, ellipsis"));

  // A paragraph clamped to three lines in a box you can resize by dragging its edge.
  rd::Class handle_style = ctx.create_class(
      rd::Style().width(10_px).border_radius(5_px).margin(0_px, 0_px, 0_px, 6_px).background(rd::hex(0x232938)).cursor(RD_CURSOR_EW_RESIZE));
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
  rd::Node b = ctx.create_button().add_class(t.btn).add_class(kind);
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
  // Pointer on hover, a resize cursor while dragging: the active variant applies for the whole drag.
  rd::Class slider_style = ctx.create_class(rd::Style().cursor(RD_CURSOR_POINTER));
  slider_style.on(RD_STATE_ACTIVE, rd::Style().cursor(RD_CURSOR_EW_RESIZE));
  app.slider_zone  = ctx.create(rd::Style().width(320_px).padding(10_px, 0_px)).add_class(slider_style);
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

// A 2:1 test pattern: a hue gradient, a checker band and a ring, so cropping and scaling are visible.
std::vector<unsigned char> pattern(int w, int h, float hue) {
  std::vector<unsigned char> px(static_cast<std::size_t>(w * h * 4));
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      float u = static_cast<float>(x) / static_cast<float>(w), v = static_cast<float>(y) / static_cast<float>(h);
      float a = (hue + u * 0.35f + v * 0.15f) * 6.2832f;
      float r = 0.5f + 0.5f * std::cos(a), g = 0.5f + 0.5f * std::cos(a - 2.094f), b = 0.5f + 0.5f * std::cos(a + 2.094f);
      float dx = static_cast<float>(x - w / 2), dy = static_cast<float>(y - h / 2);
      float d  = std::sqrt(dx * dx + dy * dy) / static_cast<float>(h);
      if (d > 0.30f && d < 0.36f) r = g = b = 1;
      if (y < h / 8 && ((x / 16 + y / 16) % 2)) r = g = b = 0.1f;
      unsigned char* p = &px[static_cast<std::size_t>((y * w + x) * 4)];
      p[0] = static_cast<unsigned char>(r * 255);
      p[1] = static_cast<unsigned char>(g * 255);
      p[2] = static_cast<unsigned char>(b * 255);
      p[3] = 255;
    }
  }
  return px;
}

// Stroke icons drawn with currentColor, so the node's color tints them.
#define ICON_SVG(body)                                                                                         \
  "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' width='24' height='24' fill='none' "            \
  "stroke='currentColor' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'>" body "</svg>"
const char* const kIcons[] = {
    ICON_SVG("<path d='M3 11 L12 4 L21 11'/><path d='M5 10 V20 H19 V10'/><path d='M10 20 V14 H14 V20'/>"),
    ICON_SVG("<circle cx='11' cy='11' r='7'/><path d='M16 16 L21 21'/>"),
    ICON_SVG("<path d='M12 3 L14.6 8.8 L21 9.5 L16.2 13.8 L17.6 20.1 L12 16.9 L6.4 20.1 L7.8 13.8 L3 9.5 L9.4 8.8 Z'/>"),
    ICON_SVG("<circle cx='12' cy='12' r='9'/><path d='M12 7 V12 L15 14'/>"),
    ICON_SVG("<path d='M4 7 H20'/><path d='M4 12 H20'/><path d='M4 17 H20'/>"),
};
#undef ICON_SVG

// A multicolor SVG with a gradient: drawn as authored.
const char kBadge[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 120 60'>"
    "<defs><linearGradient id='g' x1='0%' y1='0%' x2='100%' y2='100%'>"
    "<stop offset='0' stop-color='#5b8cff'/><stop offset='1' stop-color='#c77dff'/></linearGradient></defs>"
    "<rect x='2' y='2' width='116' height='56' rx='14' fill='url(#g)'/>"
    "<circle cx='30' cy='30' r='14' fill='#ffffff' fill-opacity='0.9'/>"
    "<path d='M52 22 H104 M52 38 H88' stroke='#ffffff' stroke-width='6' stroke-linecap='round'/></svg>";

rd::Node build_images(const rd::Context& ctx, const Theme& t) {
  auto [root, body] = section(ctx, t, "Images",
                              "Decoded on load, uploaded on first draw. object-fit places a 2:1 image in square boxes; "
                              "border-radius clips it.");
  constexpr int W = 320, H = 160;
  rd_image      img = ctx.image_from_pixels(pattern(W, H, 0.55f).data(), W, H);

  rd::Node fits = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).gap(16_px));
  const std::pair<const char*, rd_object_fit> modes[] = {{"fill", RD_OBJECT_FIT_FILL},
                                                         {"contain", RD_OBJECT_FIT_CONTAIN},
                                                         {"cover", RD_OBJECT_FIT_COVER},
                                                         {"none", RD_OBJECT_FIT_NONE},
                                                         {"scale-down", RD_OBJECT_FIT_SCALE_DOWN}};
  for (auto [name, fit] : modes) {
    rd::Node cell  = ctx.create(rd::Style().row_gap(6_px).align_items(RD_ALIGN_CENTER));
    rd::Node frame = ctx.create_image(img, rd::Style()
                                               .width(120_px)
                                               .height(120_px)
                                               .object_fit(fit)
                                               .background(rd::hex(kPanel))
                                               .border_width(1_px)
                                               .border_color(rd::hex(kLine))
                                               .border_radius(8_px));
    cell.append(frame).append(text(ctx, name).add_class(t.muted));
    fits.append(labeled(cell, std::string("object-fit: ") + name));
  }
  body.append(fits);

  // Natural aspect ratio from one dimension, a round avatar, and replacing the pixels in place.
  rd::Node row    = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).gap(16_px));
  rd::Node banner = ctx.create_image(img, rd::Style().width(240_px).border_radius(10_px));
  rd::Node avatar = ctx.create_image(img, rd::Style()
                                              .width(72_px)
                                              .height(72_px)
                                              .object_fit(RD_OBJECT_FIT_COVER)
                                              .border_radius(999_px)
                                              .box_shadow(0, 4, 12, rd::rgba(0, 0, 0, 140)));
  rd::Node recolor = button(ctx, t, t.btn_secondary, "Recolor");
  recolor.on_click([&ctx, img, hue = 0.55f]() mutable {
    hue += 0.13f;
    ctx.update_image(img, pattern(W, H, hue).data(), W, H);
  });
  row.append(labeled(banner, "width only: height from the ratio"))
      .append(labeled(avatar, "avatar: cover + radius"))
      .append(recolor);
  body.append(row);

  // SVG: rasterized at the size it's drawn, so every size is sharp. currentColor follows `color`.
  body.append(caption(ctx, "SVG icons at 16, 24, 32 and 48px, tinted by color; a multicolor SVG with a gradient."));
  rd::Node icons = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).align_items(RD_ALIGN_CENTER).gap(14_px));
  const std::uint32_t colors[] = {kText, kAccent, 0xc792ea, 0xf78c6c};
  int                 k        = 0;
  for (const char* src : kIcons) {
    rd_image icon = ctx.load_image(src, std::strlen(src));
    for (float size : {16.0f, 24.0f, 32.0f, 48.0f})
      icons.append(ctx.create_image(icon, rd::Style().height(rd::px(size)).color(rd::hex(colors[k++ % 4]))));
  }
  rd_image badge = ctx.load_image(kBadge, sizeof kBadge - 1);
  icons.append(labeled(ctx.create_image(badge, rd::Style().height(60_px)), "svg: gradient"));
  body.append(labeled(icons, "svg icons"));
  return root;
}

void describe_controls(App& app) {
  const char* sizes[] = {"small", "medium", "large"};
  int         size    = rdx::radio_selected(app.sizes);
  char        line[200];
  std::snprintf(line, sizeof line, "newsletter: %s | beta: %s | sync: %s | size: %s | volume: %.0f",
                rdx::checked(app.check_a) ? "yes" : "no", rdx::checked(app.check_b) ? "yes" : "no",
                rdx::checked(app.switch_a) ? "on" : "off", size >= 0 ? sizes[size] : "none",
                static_cast<double>(rdx::slider_value(app.volume)));
  app.controls_summary.text(line);
}

rd::Node build_controls(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Controls",
                              "Checkboxes, switches, radio groups and sliders from raydom::extras. Each fires CHANGE; "
                              "Tab, Space and the arrow keys work throughout.");
  auto     column = [&] { return ctx.create(rd::Style().row_gap(10_px).min_width(200_px)); };
  rd::Node grid   = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).gap(32_px));

  rd::Node checks = column();
  app.check_a     = rdx::checkbox(ctx, "Email me updates");
  app.check_b     = rdx::checkbox(ctx, "Join the beta");
  rd::Node locked = rdx::checkbox(ctx, "Disabled");
  rdx::set_checked(locked, true);
  locked.set_state(RD_STATE_DISABLED, true);
  checks.append(labeled(app.check_a, "checkbox")).append(labeled(app.check_b, "checkbox")).append(labeled(locked, "checkbox: disabled"));

  rd::Node switches = column();
  app.switch_a      = rdx::toggle_switch(ctx, "Sync");
  app.switch_b      = rdx::toggle_switch(ctx, "Dark mode");
  rdx::set_checked(app.switch_b, true);
  switches.append(labeled(app.switch_a, "switch")).append(labeled(app.switch_b, "switch"));

  app.sizes = rdx::radio_group(ctx);
  for (const char* s : {"Small", "Medium", "Large"}) rdx::add_radio(app.sizes, s);
  rdx::radio_select(app.sizes, 1);

  rd::Node sliders = column();
  app.volume       = rdx::slider(ctx, {.min = 0, .max = 100, .value = 40, .step = 1});
  rd::Node live    = text(ctx, "volume: 40").add_class(t.muted);
  app.volume.on(RD_EVENT_INPUT, [&app, live]() mutable {
    live.text("volume: " + std::to_string(static_cast<int>(rdx::slider_value(app.volume))));
  });
  rd::Node fine = rdx::slider(ctx, {.min = 0, .max = 1, .value = 0.25f});
  sliders.append(labeled(app.volume, "slider: step 1")).append(live).append(labeled(fine, "slider: continuous"));

  grid.append(checks).append(switches).append(labeled(app.sizes, "radio group")).append(sliders);
  app.controls_summary = text(ctx, "").add_class(t.code);
  for (rd::Node n : {app.check_a, app.check_b, app.switch_a, app.sizes, app.volume})
    n.on(RD_EVENT_CHANGE, [&app] { describe_controls(app); });
  describe_controls(app);
  body.append(grid).append(app.controls_summary);

  // Toasts: pick a corner, then a kind.
  body.append(caption(ctx, "Toasts stack in a corner, pause while hovered, and close themselves after four seconds."));
  rd::Node toasts    = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).gap(32_px));
  app.toast_position = rdx::radio_group(ctx);
  for (const char* p : {"Bottom right", "Bottom left", "Top right", "Top left", "Top center", "Bottom center"})
    rdx::add_radio(app.toast_position, p);
  rdx::radio_select(app.toast_position, 0);
  rd::Node kinds = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_START).gap(10_px));
  const std::pair<const char*, rdx_toast_kind> kinds_list[] = {
      {"Info", RDX_TOAST_INFO}, {"Success", RDX_TOAST_SUCCESS}, {"Warning", RDX_TOAST_WARNING}, {"Error", RDX_TOAST_ERROR}};
  for (auto [name, kind] : kinds_list) {
    rd::Node b = button(ctx, t, t.btn_secondary, name);
    b.on_click([&ctx, &app, name, kind] {
      static const char* bodies[] = {"Your export is ready to download.", "Settings saved.",
                                     "Storage is almost full.", "Couldn't reach the server."};
      rdx::toast(ctx, {.title    = name,
                       .text     = bodies[kind],
                       .position = static_cast<rdx_toast_position>(rdx::radio_selected(app.toast_position)),
                       .kind     = kind});
    });
    kinds.append(b);
  }
  toasts.append(labeled(app.toast_position, "toast position")).append(kinds);
  body.append(toasts);
  return root;
}

rd::Node build_animation(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Animation",
                              "Transitions animate style changes, layout included: sizes, spacing, insets, colors, "
                              "shadows. Explicit animations take a start and an end.");

  // Hover: the card lifts (a relative inset moves only the card) and its shadow and border change.
  rd::Class lift = ctx.create_class(rd::Style()
                                        .width(150_px)
                                        .padding(14_px)
                                        .row_gap(4_px)
                                        .inset(0_px, rd::auto_, rd::auto_, rd::auto_)
                                        .border_width(1_px)
                                        .border_color(rd::hex(kLine))
                                        .border_radius(10_px)
                                        .background(rd::hex(kRaised))
                                        .box_shadow(0, 2, 6, rd::rgba(0, 0, 0, 80))
                                        .cursor(RD_CURSOR_POINTER)
                                        .user_select(RD_USER_SELECT_NONE)
                                        .transition_duration(180_ms)
                                        .transition_easing(RD_EASING_EASE_OUT));
  lift.on(RD_STATE_HOVER, rd::Style()
                              .inset(-6_px, rd::auto_, rd::auto_, rd::auto_)
                              .border_color(rd::hex(kAccent))
                              .box_shadow(0, 14, 28, rd::rgba(0, 0, 0, 150)));
  rd::Node cards = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).gap(16_px).padding(8_px, 0_px));
  for (const char* name : {"Hover me", "And me", "Me too"})
    cards.append(labeled(ctx.create().add_class(lift).append(text(ctx, name)).append(text(ctx, "inset, shadow, border").add_class(t.muted)),
                         "transition: hover lift"));
  body.append(cards);

  // Accordion: height animates between 0 and auto.
  rd::Class panel = ctx.create_class(rd::Style()
                                         .overflow_y(RD_OVERFLOW_HIDDEN)
                                         .transition_duration(250_ms)
                                         .transition_easing(RD_EASING_EASE_IN_OUT));
  rd::Node accordion = ctx.create(rd::Style().max_width(560_px).row_gap(6_px));
  const std::pair<const char*, const char*> sections[] = {
      {"What animates?", "Lengths with the same unit on both ends, colors, edges, corners and shadows. Height also "
                         "animates to and from auto, which is how this panel opens."},
      {"What doesn't?", "Enumerations such as display and flex direction change at once, as do lengths whose unit "
                        "changes, such as px to percent."},
      {"Does layout follow?", "Yes: everything below this panel moves every frame while it opens or closes, and hit "
                              "testing follows too."}};
  for (auto [q, a] : sections) {
    rd::Node header = button(ctx, t, t.btn_secondary, q);
    rd::Node content = ctx.create(rd::Style().height(0_px)).add_class(panel);
    content.append(text(ctx, a, rd::Style().padding(10_px, 14_px)).add_class(t.muted));
    header.on_click([content]() mutable {
      bool open = content.computed()->height.unit == RD_UNIT_AUTO ||
                  (content.computed()->height.unit == RD_UNIT_PX && content.computed()->height.value > 0);
      content.merge_style(rd::Style().height(open ? 0_px : rd::auto_));
    });
    accordion.append(labeled(header, "accordion header")).append(labeled(content, "height: 0 <-> auto"));
  }
  body.append(accordion);

  // Width, with siblings reflowing, under each easing.
  app.anim_easing = rdx::radio_group(ctx);
  app.anim_easing.merge_style(rd::Style().flex_direction(RD_FLEX_ROW).column_gap(16_px));
  for (const char* e : {"linear", "ease", "ease-in", "ease-out", "ease-in-out"}) rdx::add_radio(app.anim_easing, e);
  rdx::radio_select(app.anim_easing, 1);
  rd::Node lane = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(10_px));
  app.anim_box  = ctx.create(rd::Style()
                                 .width(120_px)
                                 .height(36_px)
                                 .border_radius(8_px)
                                 .background(rd::hex(kAccentLo))
                                 .transition_duration(600_ms));
  lane.append(app.anim_box).append(text(ctx, "siblings reflow").add_class(t.chip));
  rd::Node go = button(ctx, t, t.btn_primary, "Grow / shrink");
  go.on_click([&app] {
    static const rd_easing curves[] = {RD_EASING_LINEAR, RD_EASING_EASE, RD_EASING_EASE_IN, RD_EASING_EASE_OUT,
                                       RD_EASING_EASE_IN_OUT};
    app.anim_wide = !app.anim_wide;
    app.anim_box.merge_style(rd::Style()
                                 .transition_easing(curves[std::max(0, rdx::radio_selected(app.anim_easing))])
                                 .width(app.anim_wide ? 420_px : 120_px)
                                 .background(rd::hex(app.anim_wide ? kAccent : kAccentLo)));
  });
  body.append(labeled(app.anim_easing, "easing")).append(ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).column_gap(12_px).align_items(RD_ALIGN_CENTER)).append(go).append(labeled(lane, "transition: width")));
  return root;
}

rd::Node build_views(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Views",
                              "Tabs, a splitter, a tree, and virtualized lists and tables: only the rows in view exist, "
                              "so 100,000 rows cost the same as 20.");
  rd::Node tabs = rdx::tabs(ctx);
  tabs.merge_style(rd::Style().max_width(900_px));

  // Files: a tree and a table side by side.
  rd::Node files = rdx::add_tab(tabs, "Files");
  rd::Node split = rdx::split(ctx, {.fraction = 0.3f, .min_first = 140, .min_second = 240});
  split.merge_style(rd::Style().height(320_px).border_width(1_px).border_color(rd::hex(kLine)).border_radius(8_px).overflow_y(RD_OVERFLOW_HIDDEN));
  rd::Node tree = rdx::tree(ctx, 26);
  tree.merge_style(rd::Style().flex(1));
  int src = rdx::tree_add(tree, -1, "src"), docs = rdx::tree_add(tree, -1, "docs"), ex = rdx::tree_add(tree, -1, "examples");
  for (const char* f : {"tree.c", "style.c", "layout.c", "grid.c", "text.c", "render.c", "anim.c"}) rdx::tree_add(tree, src, f);
  int ext = rdx::tree_add(tree, src, "external");
  for (const char* f : {"stb_truetype.h", "stb_image.h", "nanosvg.h"}) rdx::tree_add(tree, ext, f);
  for (const char* f : {"book.toml", "src", "examples"}) rdx::tree_add(tree, docs, f);
  for (const char* f : {"showcase", "network", "messages"}) rdx::tree_add(tree, ex, f);
  rdx::tree_expand(tree, src);
  rdx::pane(split, 0).append(tree);

  rd::Node table = rdx::table(ctx, {{"Name", 1_fr}, {"Size", 90_px}, {"Modified", 140_px}}, 10000, 28,
                              {.cell =
                                   [&app](rd::Node cell, int row, int col) {
                                     int  n = app.files_reversed ? 9999 - row : row;
                                     char buf[48];
                                     if (col == 0) std::snprintf(buf, sizeof buf, "file_%05d.txt", n);
                                     if (col == 1) std::snprintf(buf, sizeof buf, "%d KB", (n * 37) % 900 + 4);
                                     if (col == 2) std::snprintf(buf, sizeof buf, "2026-%02d-%02d", n % 12 + 1, n % 28 + 1);
                                     cell.text(buf);
                                   },
                               .header =
                                   [&app](int col) {
                                     if (col != 0) return;  // Name sorts; the others don't
                                     app.files_reversed = !app.files_reversed;
                                     rdx::list_refresh(app.files_rows);
                                   }});
  table.merge_style(rd::Style().flex(1).border_width(0_px).border_radius(0_px));
  rdx::pane(split, 1).append(table);
  files.append(split);

  app.views_status = text(ctx, "Select a folder or a file.").add_class(t.muted);
  tree.on(RD_EVENT_CHANGE, [&app, tree] { app.views_status.text("tree: " + rdx::tree_label(tree, rdx::tree_selected(tree))); });
  rd::Node rows  = rdx::table_list(table);
  app.files_rows = rows;
  rows.on(RD_EVENT_CHANGE, [&app, rows] {
    int  r = rdx::list_selected(rows);
    char buf[64];
    std::snprintf(buf, sizeof buf, "table: row %d of 10,000", r + 1);
    app.views_status.text(buf);
  });
  files.append(app.views_status);

  // A long list.
  rd::Node many = rdx::add_tab(tabs, "100,000 rows");
  rd::Node list = rdx::list(ctx, 100000, 30, [&ctx, &t](rd::Node row, int i) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "Item %d", i + 1);
    row.append(text(ctx, buf, rd::Style().flex(1)));
    std::snprintf(buf, sizeof buf, "#%05x", (i * 2654435761u) & 0xfffff);
    row.append(text(ctx, buf).add_class(t.muted));
  });
  list.merge_style(rd::Style().height(320_px).border_width(1_px).border_color(rd::hex(kLine)).border_radius(8_px));
  many.append(list).append(text(ctx, "Scroll, drag the scrollbar, or focus the list and use the arrow keys, Page Up/Down, "
                                     "Home and End.").add_class(t.muted));

  rd::Node about = rdx::add_tab(tabs, "About");
  about.append(text(ctx, "A virtualized list keeps a spacer as tall as every row and positions the few rows in view "
                         "over it. A layout hook rebinds rows as the list scrolls, in the same frame.").add_class(t.muted));
  body.append(labeled(tabs, "tabs"));
  return root;
}

void set_shown(rd::Node n, bool shown) { n.merge_style(rd::Style().display(shown ? RD_DISPLAY_FLEX : RD_DISPLAY_NONE)); }
bool shown(rd::Node n) { return n.computed()->display != RD_DISPLAY_NONE; }

rd::Node build_overlay(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Overlay",
                              "Popups live in a layer above the page, anchored to a node or a point. They flip and "
                              "shift to stay inside the window. Click outside or press Escape to close.");
  auto popup = [&](float width) {
    rd::Node n = ctx.create(rd::Style().position(RD_POSITION_ABSOLUTE).width(rd::px(width)).display(RD_DISPLAY_NONE))
                     .add_class(t.card);
    ctx.overlay().append(n);
    return n;
  };

  // A popover anchored below its button.
  app.popover_toggle = button(ctx, t, t.btn_primary, "Toggle popover");
  app.popover        = popup(280);
  app.popover.merge_style(rd::Style().align_items(RD_ALIGN_START));
  rd::Node close     = button(ctx, t, t.btn_secondary, "Close");
  app.popover.append(text(ctx, "Anchored popover", rd::Style().font_size(16_px)))
      .append(text(ctx, "Below its button, start-aligned, 8px away. Scroll the page so the button nears the bottom "
                        "edge and it flips above.")
                  .add_class(t.muted))
      .append(close);
  app.popover.anchor({.node = app.popover_toggle.handle(), .offset = 8});
  app.popover_toggle.on_click([&app] { set_shown(app.popover, !shown(app.popover)); });
  close.on_click([&app] { set_shown(app.popover, false); });

  rd::Node row = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(12_px));
  row.append(labeled(app.popover_toggle, "popover button"));
  body.append(row);
  return root;
}

rd::Node build_popups(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Popups",
                              "Menus, selects, context menus, modals and tooltips from raydom::extras, built on the "
                              "overlay. Arrow keys, Enter and Escape work in all of them.");
  rd::Node row = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).align_items(RD_ALIGN_CENTER).gap(12_px));

  // A menu button.
  rd::Node file = button(ctx, t, t.btn_secondary, "File");
  file.on_click([&ctx, &app, file] {
    rdx::menu(ctx,
              {{"New", "Ctrl+N"}, {"Open...", "Ctrl+O"}, {"Save", "Ctrl+S"}, {"Save as...", "", true}, {}, {"Quit", "Ctrl+Q"}},
              {.node = file.handle(), .offset = 4},
              [&app](int i) {
                const char* names[] = {"New", "Open", "Save", "Save as", "", "Quit"};
                app.popups_result.text(std::string("menu: ") + names[i]);
              });
  });
  rdx::tooltip(file, "A menu anchored to its button");

  // A select.
  rd::Node theme = rdx::select(ctx, {"System", "Light", "Dark", "High contrast"}, "Theme");
  theme.on(RD_EVENT_CHANGE, [&app, theme] {
    const char* names[] = {"System", "Light", "Dark", "High contrast"};
    app.popups_result.text(std::string("select: ") + names[rdx::selected(theme)]);
  });
  rdx::tooltip(theme, "Up and Down change it without opening");

  // A modal.
  rd::Node del = button(ctx, t, t.btn_primary, "Delete project...");
  del.on_click([&ctx, &t, &app] {
    rd::Node body = rdx::modal(ctx, {.title = "Delete project?", .on_close = [&app] {
                                       if (app.popups_result.text().substr(0, 6) != "modal:") app.popups_result.text("modal: dismissed");
                                     }});
    body.append(text(ctx, "This removes the project and its history. It can't be undone.").add_class(t.muted));
    rd::Node actions = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).justify_content(RD_ALIGN_END).gap(8_px));
    rd::Node cancel  = button(ctx, t, t.btn_secondary, "Cancel");
    rd::Node confirm = button(ctx, t, t.btn_primary, "Delete");
    cancel.on_click([body] { rdx::close_popup(body); });
    confirm.on_click([&app, body] {
      app.popups_result.text("modal: deleted");
      rdx::close_popup(body);
    });
    body.append(actions.append(cancel).append(confirm));
  });
  rdx::tooltip(del, "Opens a modal; Tab stays inside it");
  row.append(labeled(file, "menu button")).append(labeled(theme, "select")).append(labeled(del, "modal button"));

  // A context menu.
  rd::Node area = ctx.create(rd::Style()
                                 .height(100_px)
                                 .align_items(RD_ALIGN_CENTER)
                                 .justify_content(RD_ALIGN_CENTER)
                                 .border_width(1_px)
                                 .border_color(rd::hex(kLine))
                                 .border_radius(8_px)
                                 .background(rd::hex(kPanel))
                                 .user_select(RD_USER_SELECT_NONE)
                                 .cursor(RD_CURSOR_CROSSHAIR));
  area.append(text(ctx, "Right-click anywhere in this box").add_class(t.muted));
  rdx::context_menu(area, {{"Copy", "Ctrl+C"}, {"Rename"}, {}, {"Delete", "Del"}}, [&app](int i) {
    const char* names[] = {"Copy", "Rename", "", "Delete"};
    app.popups_result.text(std::string("context menu: ") + names[i]);
  });

  app.popups_result = text(ctx, "Nothing chosen yet.").add_class(t.code);
  body.append(row).append(labeled(area, "context menu area")).append(app.popups_result);
  return root;
}

// Light dismiss: a press outside an open popup closes it. Runs in the capture phase, before the target.
void close_popups(App& app, rd::Node target) {
  if (shown(app.popover) && !app.popover.contains(target) && !app.popover_toggle.contains(target))
    set_shown(app.popover, false);
}

void show_result(App& app, const char* action, const rdx::DialogResult& r) {
  std::string out = std::string(action) + ": ";
  if (r.status == RDX_DIALOG_CANCEL) out += "cancelled";
  if (r.status == RDX_DIALOG_ERROR) out += "error: " + r.error;
  for (const std::string& p : r.paths) out += "\n  " + p;
  app.file_result.text(out);
}

rd::Node build_files(const rd::Context& ctx, const Theme& t, App& app) {
  auto [root, body] = section(ctx, t, "Files",
                              "The operating system's open, save and folder dialogs, from raydom::extras. "
                              "Each call blocks until the dialog closes.");

  const std::vector<rdx_file_filter> filters = {{"Text", "txt,md"}, {"Images", "png,jpg,jpeg"}};
  rd::Node open    = button(ctx, t, t.btn_primary, "Open file...");
  rd::Node many    = button(ctx, t, t.btn_secondary, "Open files...");
  rd::Node save    = button(ctx, t, t.btn_secondary, "Save as...");
  rd::Node folder  = button(ctx, t, t.btn_secondary, "Choose folder...");
  open.on_click([&app, filters] { show_result(app, "open", rdx::open_file({.title = "Open a file", .filters = filters})); });
  many.on_click([&app] { show_result(app, "open multiple", rdx::open_files({.title = "Open files"})); });
  save.on_click([&app, filters] {
    show_result(app, "save", rdx::save_file({.title = "Save as", .filters = filters, .default_name = "untitled.txt"}));
  });
  folder.on_click([&app] { show_result(app, "folder", rdx::pick_folder({.title = "Choose a folder"})); });

  rd::Node row = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).flex_wrap(RD_FLEX_WRAP).gap(12_px));
  for (rd::Node b : {open, many, save, folder}) row.append(labeled(b, "dialog button"));
  app.file_result = text(ctx, "Nothing chosen yet.").add_class(t.code);
  body.append(row).append(labeled(app.file_result, "dialog result"));
  return root;
}

rd::Node build(const rd::Context& ctx, const Theme& t, App& app) {
  rd::Node body    = ctx.create(rd::Style().flex(1).min_height(0_px).flex_direction(RD_FLEX_ROW));
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
                         build_boxes(ctx, t), build_images(ctx, t), build_buttons(ctx, t, app),
                         build_controls(ctx, t, app), build_animation(ctx, t, app), build_views(ctx, t, app), build_events(ctx, t, app),
                         build_forms(ctx, t, app), build_overlay(ctx, t, app), build_popups(ctx, t, app),
                         build_files(ctx, t, app)};
  const char* names[] = {"Flexbox", "Grid",  "Text",  "Boxes",   "Images", "States",
                         "Controls", "Animation", "Views", "Events", "Forms", "Overlay", "Popups", "Files"};
  for (int j = 0; j < 14; j++) {
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
  rd::Node tour = ctx.create(rd::Style().flex(1).min_height(0_px));
  tour.append(body).append(status);
  ctx.root().on(RD_EVENT_SELECTION_CHANGE, [&ctx, &app] { app.selected = rd_ctx_selection_text(ctx.get(), nullptr, 0); });
  ctx.root().on(RD_EVENT_KEY_DOWN, [&ctx, &app](rd::Event& e) {
    if (e.key() != RD_KEY_ESCAPE) return;
    ctx.blur();
    set_shown(app.popover, false);
  });
  ctx.root().on(RD_EVENT_POINTER_DOWN, [&app](rd::Event& e) { close_popups(app, e.target()); }, true);
  return tour;
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

struct Tour {
  Theme theme;
  App   app;
};

Tour* tour = nullptr;

}  // namespace

rd::Node build_tour(rd::Context& ctx) {
  tour        = new Tour{make_theme(ctx), {}};  // lives as long as the program
  return build(ctx, tour->theme, tour->app);
}

void tour_frame(const rd::Context& ctx) { update_status(tour->app, ctx); }

rd::Node tour_scroller() { return tour->app.main; }
