// The Messages tab: a desktop messaging app. Conversations on the left, the open one on the right.
// Enter sends, Shift+Enter starts a new line, and every contact answers after a moment of typing.

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <raydom/extras/views.hh>
#include <string>
#include <string_view>
#include <vector>

#include "showcase.h"

using namespace rd::literals;

namespace {

constexpr std::uint32_t kMine = 0x2f7bf6, kTheirs = 0x252a35;

const char kSendIcon[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' width='24' height='24' fill='none' stroke='currentColor' "
    "stroke-width='2.2' stroke-linecap='round' stroke-linejoin='round'><path d='M12 19 V5'/><path d='M6 11 L12 5 L18 11'/></svg>";

enum class Who { me, them };

struct Msg {
  Who         who;
  std::string text;
  std::time_t at;
};

struct Convo {
  std::string              name;
  std::uint32_t            color;
  std::vector<Msg>         msgs;
  std::vector<std::string> replies;
  int                      next_reply = 0, unread = 0;
  bool                     typing = false, read = false;
  rd_timer                 timer{};
  rd::Node                 row, time, preview, badge;
};

struct Messages {
  rd::Context*       ctx = nullptr;
  std::vector<Convo> convos;
  int                active = -1;
  rd::Node           list, search, log, input, send, avatar, name, status, typing;
  rd::Class          row, bubble_mine, bubble_theirs, line_mine, line_theirs, dot, receipt, separator;
  // The open conversation's rendering state: runs of bubbles from one sender join up.
  Who      last = Who::them;
  rd::Node last_bubble, receipt_node;
  bool     last_joins_above = false;
  float    last_max         = 0;
  bool     to_bottom        = false;
  int      dot_phase        = 0;
  rd_timer dots{};
};

// Time labels.

std::tm local(std::time_t t) {
  std::tm out{};
  localtime_r(&t, &out);
  return out;
}

int days_ago(std::time_t t) {
  std::tm a = local(t), b = local(std::time(nullptr));
  a.tm_hour = a.tm_min = a.tm_sec = 0;
  b.tm_hour = b.tm_min = b.tm_sec = 0;
  return static_cast<int>(std::difftime(std::mktime(&b), std::mktime(&a)) / 86400);
}

std::string format(std::time_t t, const char* fmt) {
  std::tm tm = local(t);
  char    buf[48];
  std::strftime(buf, sizeof buf, fmt, &tm);
  return buf;
}

std::string separator_label(std::time_t t) {
  int d = days_ago(t);
  return d == 0 ? "Today " + format(t, "%H:%M") : d == 1 ? "Yesterday " + format(t, "%H:%M") : format(t, "%a %d %b, %H:%M");
}

std::string list_time(std::time_t t) {
  int d = days_ago(t);
  return d == 0 ? format(t, "%H:%M") : d == 1 ? "Yesterday" : format(t, "%d %b");
}

std::string first_line(std::string_view s) {
  std::string out(s.substr(0, s.find('\n')));
  return out;
}

// The sidebar.

void update_row(Messages& m, Convo& c) {
  const Msg* last = c.msgs.empty() ? nullptr : &c.msgs.back();
  c.time.text(last ? list_time(last->at) : "");
  c.preview.text(c.typing ? "typing..." : last ? (last->who == Who::me ? "You: " : "") + first_line(last->text) : "");
  c.badge.text(std::to_string(c.unread));
  c.badge.merge_style(rd::Style().display(c.unread ? RD_DISPLAY_FLEX : RD_DISPLAY_NONE));
  c.row.set_state(RD_STATE_CHECKED, &c == &m.convos[static_cast<std::size_t>(m.active)]);
}

void to_top(Messages& m, Convo& c) {
  rd::Node first = m.list.first_child();
  if (first && !(first == c.row)) m.list.insert_before(c.row, first);
}

void filter(Messages& m) {
  std::string q(m.search.text());
  std::transform(q.begin(), q.end(), q.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  for (Convo& c : m.convos) {
    std::string n = c.name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    c.row.merge_style(rd::Style().display(n.find(q) != std::string::npos ? RD_DISPLAY_FLEX : RD_DISPLAY_NONE));
  }
}

rd::Node avatar(rd::Context& ctx, const Convo& c, float size) {
  return ctx.create_text(c.name.substr(0, 1), rd::Style()
                                                  .width(rd::px(size))
                                                  .height(rd::px(size))
                                                  .flex_shrink(rd::num(0))
                                                  .border_radius(rd::px(size / 2))
                                                  .background(rd::hex(c.color))
                                                  .color(rd::hex(0xffffff))
                                                  .text_align(RD_TEXT_ALIGN_CENTER)
                                                  .line_height(rd::px(size))
                                                  .font_size(rd::px(size * 0.42f))
                                                  .user_select(RD_USER_SELECT_NONE));
}

// The conversation.

// Corners: 18px, except 6px where a bubble joins the one above or below from the same sender.
void corners(rd::Node bubble, Who who, bool above, bool below) {
  rd_val     big = 18_px, small = 6_px;
  rd_corners c   = {big, big, big, big};
  if (above) (who == Who::me ? c.top_right : c.top_left) = small;
  if (below) (who == Who::me ? c.bottom_right : c.bottom_left) = small;
  bubble.merge_style(rd::Style().border_radius(c));
}

void insert(Messages& m, rd::Node n) {
  if (m.typing.alive())
    m.log.insert_before(n, m.typing);  // the typing indicator stays last
  else
    m.log.append(n);
}

void show_receipt(Messages& m) {
  if (m.receipt_node.alive()) m.receipt_node.destroy();
  Convo& c = m.convos[static_cast<std::size_t>(m.active)];
  if (c.msgs.empty() || c.msgs.back().who != Who::me) return;
  m.receipt_node = m.ctx->create_text(c.read ? "Read" : "Delivered").add_class(m.receipt);
  insert(m, m.receipt_node);
}

// Renders one message at the end of the log; `previous` is the message before it, if any.
void render(Messages& m, const Msg& msg, const Msg* previous, bool animate) {
  rd::Context& ctx = *m.ctx;
  if (!previous || msg.at - previous->at > 15 * 60) {
    insert(m, ctx.create_text(separator_label(msg.at)).add_class(m.separator));
    m.last_bubble = {};
  }
  bool same = m.last_bubble.alive() && m.last == msg.who && previous && msg.at - previous->at < 120;
  if (same) corners(m.last_bubble, msg.who, m.last_joins_above, true);
  rd::Node line   = ctx.create().add_class(msg.who == Who::me ? m.line_mine : m.line_theirs);
  rd::Node bubble = ctx.create_text(msg.text).add_class(msg.who == Who::me ? m.bubble_mine : m.bubble_theirs);
  corners(bubble, msg.who, same, false);
  line.merge_style(rd::Style().margin(same ? 2_px : 10_px, 0_px, 0_px, 0_px)).append(bubble);
  insert(m, line);
  if (animate) {
    rd::Style from = rd::Style().opacity(rd::num(0));
    from.margin(8_px, msg.who == Who::me ? -24_px : 0_px, 0_px, msg.who == Who::me ? 0_px : -24_px);
    bubble.animate(from, rd::Style().opacity(rd::num(1)).margin(0_px), 0.22, RD_EASING_EASE_OUT);
  }
  m.last             = msg.who;
  m.last_bubble      = bubble;
  m.last_joins_above = same;
}

void show_typing(Messages& m, bool on) {
  rd::Context& ctx = *m.ctx;
  if (!on) {
    if (m.typing.alive()) m.typing.destroy();
    m.typing = {};
    ctx.clear_timer(m.dots);
    return;
  }
  if (m.typing.alive()) return;
  m.typing        = ctx.create().add_class(m.line_theirs);
  rd::Node bubble = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).column_gap(5_px).padding(12_px, 14_px).border_radius(18_px))
                        .add_class(m.bubble_theirs);
  for (int j = 0; j < 3; j++) bubble.append(ctx.create().add_class(m.dot));
  m.typing.merge_style(rd::Style().margin(10_px, 0_px, 0_px, 0_px)).append(bubble);
  m.log.append(m.typing);
  bubble.animate(rd::Style().opacity(rd::num(0)), rd::Style().opacity(rd::num(1)), 0.2);
  // The dots take turns rising; the dot class transitions each move.
  m.dots = ctx.set_interval(0.18, [&m] {
    if (!m.typing.alive()) return;
    int j = 0;
    for (rd::Node d : m.typing.first_child().children()) d.set_state(RD_STATE_CHECKED, j++ == m.dot_phase % 3);
    m.dot_phase++;
  });
}

void open(Messages& m, int index) {
  rd::Context& ctx = *m.ctx;
  m.active         = index;
  Convo& c         = m.convos[static_cast<std::size_t>(index)];
  c.unread         = 0;
  while (m.log.first_child()) m.log.first_child().destroy();
  m.typing      = {};
  m.last_bubble = {};
  ctx.clear_timer(m.dots);
  for (std::size_t j = 0; j < c.msgs.size(); j++) render(m, c.msgs[j], j ? &c.msgs[j - 1] : nullptr, false);
  if (c.typing) show_typing(m, true);
  show_receipt(m);
  m.avatar.text(c.name.substr(0, 1));
  m.avatar.merge_style(rd::Style().background(rd::hex(c.color)));
  m.name.text(c.name);
  m.status.text(c.typing ? "typing..." : "online");
  m.to_bottom = true;
  for (Convo& other : m.convos) update_row(m, other);
}

// A message into a conversation, shown if it's open.
void deliver(Messages& m, Convo& c, Who who, std::string text) {
  c.msgs.push_back({who, std::move(text), std::time(nullptr)});
  bool active = &c == &m.convos[static_cast<std::size_t>(m.active)];
  if (active) {
    render(m, c.msgs.back(), c.msgs.size() > 1 ? &c.msgs[c.msgs.size() - 2] : nullptr, true);
    show_receipt(m);
  } else if (who == Who::them) {
    c.unread++;
  }
  update_row(m, c);
  to_top(m, c);
}

void set_typing(Messages& m, Convo& c, bool on) {
  c.typing    = on;
  bool active = &c == &m.convos[static_cast<std::size_t>(m.active)];
  if (active) {
    show_typing(m, on);
    m.status.text(on ? "typing..." : "online");
  }
  update_row(m, c);
}

// Each contact reads, types for a moment, and answers, whether or not the conversation stays open.
void answer(Messages& m, Convo& c) {
  rd::Context& ctx = *m.ctx;
  ctx.clear_timer(c.timer);
  c.timer = ctx.set_timeout(0.6, [&m, &c, &ctx] {
    c.read = true;
    if (&c == &m.convos[static_cast<std::size_t>(m.active)]) show_receipt(m);
    set_typing(m, c, true);
    const std::string& text = c.replies[static_cast<std::size_t>(c.next_reply++) % c.replies.size()];
    c.timer                 = ctx.set_timeout(0.9 + 0.02 * static_cast<double>(text.size()), [&m, &c, text] {
      set_typing(m, c, false);
      deliver(m, c, Who::them, text);
    });
  });
}

bool blank(std::string_view s) { return s.find_first_not_of(" \t\n") == std::string_view::npos; }

void submit(Messages& m) {
  std::string text(m.input.text());
  if (blank(text)) return;
  while (!text.empty() && (text.back() == '\n' || text.back() == ' ')) text.pop_back();
  Convo& c = m.convos[static_cast<std::size_t>(m.active)];
  c.read   = false;
  deliver(m, c, Who::me, text);
  m.input.text("");
  m.send.set_state(RD_STATE_DISABLED, true);
  answer(m, c);
}

// After each layout: follow the newest message while the user is at the bottom, and jump there on opening.
void follow(rd_ctx* ctx, void* data) {
  Messages& m = *static_cast<Messages*>(data);
  if (!m.log.alive() || m.log.computed()->display == RD_DISPLAY_NONE) return;
  float view = rd_node_rect(ctx, m.log.handle()).height;
  float max  = std::max(0.0f, rd_node_content_size(ctx, m.log.handle()).height - view);
  if (m.to_bottom || rd_node_scroll(ctx, m.log.handle()).y >= m.last_max - 4) rd_node_set_scroll(ctx, m.log.handle(), 0, max);
  m.to_bottom = false;
  m.last_max  = max;
}

void seed(Messages& m) {
  std::time_t now = std::time(nullptr), h = 3600, d = 86400;
  auto        convo = [&](std::string name, std::uint32_t color, std::vector<Msg> msgs, std::vector<std::string> replies) {
    m.convos.push_back({std::move(name), color, std::move(msgs), std::move(replies)});
  };
  convo("Ada Lovelace", 0x8b5cf6,
        {{Who::them, "Morning! Did the grid layout land?", now - 2 * h},
         {Who::me, "It did. fr tracks, spans and auto placement.", now - 2 * h + 40},
         {Who::them, "Lovely. Next: can it animate height to auto?", now - 2 * h + 70},
         {Who::them, "Accordions are the real test.", now - 2 * h + 80},
         {Who::me, "It can now. Try the Animation section in the tour.", now - 20 * 60}},
        {"Oh nice, I'll take a look.", "Does it work with auto height in both directions?", "That's the part I'd read first.",
         "Ha, fair enough."});
  convo("Grace Hopper", 0x2ec4b6,
        {{Who::them, "Standup notes are in the doc.", now - d - 3 * h},
         {Who::them, "Also: the bug was a moth again.", now - d - 3 * h + 30},
         {Who::me, "Of course it was.", now - d - 2 * h}},
        {"Taped it into the log book.", "Ship it and we'll fix it in the next one.", "Don't forget the retro."});
  convo("Alan Turing", 0xf4a261,
        {{Who::me, "Is the halting problem solved yet?", now - 3 * d},
         {Who::them, "Ask me again when it stops.", now - 3 * d + 600}},
        {"Still thinking.", "Can you make the test suite decide that for us?", "Interesting."});
  convo("Linus", 0xef476f, {{Who::them, "Sent you the patch. Review when you can.", now - 5 * h}},
        {"Looks good to me.", "Rebase it, please.", "Thanks!"});
  convo("Margaret Hamilton", 0x5b8cff,
        {{Who::them, "The priority display saved the landing.", now - 6 * d},
         {Who::me, "Error handling first, always.", now - 6 * d + 120}},
        {"Exactly.", "Test the paths nobody thinks will run.", "Agreed."});
  convo("raydom team", 0x6bd968,
        {{Who::them, "Release notes for 0.2 are drafted.", now - 30 * 60},
         {Who::them, "Anyone want to proofread?", now - 29 * 60}},
        {"I'll take the docs part.", "Merged.", "Tagging tonight."});
  m.convos[3].unread = 1;
  m.convos[5].unread = 2;
}

rd::Node build(rd::Context& ctx, Messages& m) {
  m.ctx = &ctx;
  seed(m);

  m.row = ctx.create_class(rd::Style()
                               .flex_direction(RD_FLEX_ROW)
                               .align_items(RD_ALIGN_CENTER)
                               .column_gap(12_px)
                               .padding(10_px, 12_px)
                               .border_radius(10_px)
                               .cursor(RD_CURSOR_POINTER)
                               .user_select(RD_USER_SELECT_NONE)
                               .transition_duration(120_ms));
  m.row.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x1d2230)));
  m.row.on(RD_STATE_CHECKED, rd::Style().background(rd::hex(0x26355a)));
  m.row.on(RD_STATE_FOCUS_VISIBLE, rd::Style().box_shadow(0, 0, 0, rd::rgba(91, 140, 255, 200), 2));
  rd::Style bubble = rd::Style().max_width(rd::pct(62)).padding(9_px, 13_px).line_height(rd::num(1.35f));
  m.bubble_mine    = ctx.create_class(rd::Style(bubble).background(rd::hex(kMine)).color(rd::hex(0xffffff)));
  m.bubble_theirs  = ctx.create_class(rd::Style(bubble).background(rd::hex(kTheirs)));
  m.line_mine      = ctx.create_class(rd::Style().flex_direction(RD_FLEX_ROW).justify_content(RD_ALIGN_END));
  m.line_theirs    = ctx.create_class(rd::Style().flex_direction(RD_FLEX_ROW).justify_content(RD_ALIGN_START));
  m.receipt   = ctx.create_class(rd::Style().align_self(RD_ALIGN_END).margin(3_px, 6_px, 0_px, 0_px).font_size(11_px).color(rd::hex(kMuted)));
  m.separator = ctx.create_class(rd::Style().align_self(RD_ALIGN_CENTER).margin(14_px, 0_px, 4_px, 0_px).font_size(11_px).color(rd::hex(kMuted)));
  m.dot       = ctx.create_class(rd::Style()
                                     .width(7_px)
                                     .height(7_px)
                                     .border_radius(4_px)
                                     .background(rd::hex(kMuted))
                                     .inset(0_px, rd::auto_, rd::auto_, rd::auto_)
                                     .transition_duration(160_ms));
  m.dot.on(RD_STATE_CHECKED, rd::Style().inset(-3_px, rd::auto_, rd::auto_, rd::auto_).background(rd::hex(kText)));
  rd::Style field = rd::Style()
                        .padding(9_px, 14_px)
                        .border_radius(19_px)
                        .border_width(1_px)
                        .border_color(rd::hex(kLine))
                        .background(rd::hex(kBg))
                        .line_height(rd::num(1.35f));

  // Sidebar: search and conversations, most recent first.
  rd::Node split = rdx::split(ctx, {.fraction = 0.3f, .min_first = 240, .min_second = 380});
  split.merge_style(rd::Style().flex(1).min_height(0_px));
  rd::Node side = rdx::pane(split, 0);
  side.merge_style(rd::Style().background(rd::hex(kPanel)).padding(14_px, 10_px).row_gap(10_px));
  side.append(ctx.create_text("Messages", rd::Style().font_size(20_px).padding(0_px, 6_px)));
  m.search = ctx.create_input(false, rd::Style(field).margin(0_px, 4_px)).placeholder("Search");
  side.append(m.search);
  m.list = ctx.create(rd::Style().flex(1).min_height(0_px).overflow_y(RD_OVERFLOW_SCROLL).row_gap(2_px));
  side.append(m.list);
  std::vector<int> order(m.convos.size());
  for (std::size_t j = 0; j < order.size(); j++) order[j] = static_cast<int>(j);
  std::sort(order.begin(), order.end(), [&m](int a, int b) {
    return m.convos[static_cast<std::size_t>(a)].msgs.back().at > m.convos[static_cast<std::size_t>(b)].msgs.back().at;
  });
  for (int j : order) {
    Convo& c  = m.convos[static_cast<std::size_t>(j)];
    c.row     = ctx.create_button().add_class(m.row);
    rd::Node mid  = ctx.create(rd::Style().flex(1).min_width(0_px).row_gap(3_px));
    rd::Node top  = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(8_px));
    c.time    = ctx.create_text("", rd::Style().font_size(11_px).color(rd::hex(kMuted)));
    c.preview = ctx.create_text("", rd::Style()
                                        .font_size(12_px)
                                        .color(rd::hex(kMuted))
                                        .white_space(RD_WHITE_SPACE_NOWRAP)
                                        .text_overflow(RD_TEXT_OVERFLOW_ELLIPSIS));
    c.badge   = ctx.create_text("", rd::Style()
                                      .min_width(20_px)
                                      .height(20_px)
                                      .padding(0_px, 6_px)
                                      .border_radius(10_px)
                                      .background(rd::hex(kAccent))
                                      .color(rd::hex(0xffffff))
                                      .font_size(11_px)
                                      .line_height(20_px)
                                      .text_align(RD_TEXT_ALIGN_CENTER));
    top.append(ctx.create_text(c.name, rd::Style().flex(1).white_space(RD_WHITE_SPACE_NOWRAP).text_overflow(RD_TEXT_OVERFLOW_ELLIPSIS)))
        .append(c.time);
    mid.append(top).append(c.preview);
    c.row.append(avatar(ctx, c, 40)).append(mid).append(c.badge);
    c.row.on_click([&m, j] {
      open(m, j);
      m.input.focus();
    });
    m.list.append(c.row);
  }
  m.search.on(RD_EVENT_INPUT, [&m] { filter(m); });

  // The conversation: header, log and composer.
  rd::Node chat = rdx::pane(split, 1);
  rd::Node head = ctx.create(rd::Style()
                                 .flex_direction(RD_FLEX_ROW)
                                 .align_items(RD_ALIGN_CENTER)
                                 .column_gap(12_px)
                                 .padding(12_px, 18_px)
                                 .border_width(0_px, 0_px, 1_px, 0_px)
                                 .border_color(rd::hex(kLine)));
  m.avatar = avatar(ctx, m.convos[0], 36);
  m.name   = ctx.create_text("", rd::Style().font_size(16_px));
  m.status = ctx.create_text("online", rd::Style().font_size(12_px).color(rd::hex(kMuted)));
  head.append(m.avatar).append(ctx.create(rd::Style().row_gap(2_px)).append(m.name).append(m.status));
  m.log = ctx.create(rd::Style().flex(1).min_height(0_px).overflow_y(RD_OVERFLOW_SCROLL).padding(8_px, 24_px, 16_px, 24_px));

  rd::Node bar = ctx.create(rd::Style()
                                .flex_direction(RD_FLEX_ROW)
                                .align_items(RD_ALIGN_END)
                                .column_gap(10_px)
                                .padding(12_px, 18_px)
                                .border_width(1_px, 0_px, 0_px, 0_px)
                                .border_color(rd::hex(kLine)));
  m.input = ctx.create_input(true, rd::Style(field).flex(1).min_height(38_px).max_height(140_px).overflow_y(RD_OVERFLOW_SCROLL))
                .placeholder("Message (Enter sends, Shift+Enter for a new line)");
  rd::Class send = ctx.create_class(rd::Style()
                                        .width(38_px)
                                        .height(38_px)
                                        .flex_shrink(rd::num(0))
                                        .border_radius(19_px)
                                        .align_items(RD_ALIGN_CENTER)
                                        .justify_content(RD_ALIGN_CENTER)
                                        .background(rd::hex(kMine))
                                        .color(rd::hex(0xffffff))
                                        .cursor(RD_CURSOR_POINTER)
                                        .transition_duration(120_ms));
  send.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x4a8dfa)));
  send.on(RD_STATE_DISABLED, rd::Style().background(rd::hex(kTheirs)).color(rd::hex(kMuted)).cursor(RD_CURSOR_DEFAULT));
  m.send = ctx.create_button(rd::Style()).add_class(send);
  m.send.append(ctx.create_image(ctx.load_image(kSendIcon, sizeof kSendIcon - 1), rd::Style().height(20_px)));
  m.send.set_state(RD_STATE_DISABLED, true);
  bar.append(m.input).append(m.send);
  chat.merge_style(rd::Style().min_width(0_px));
  chat.append(head).append(m.log).append(bar);

  m.input.on(RD_EVENT_KEY_DOWN, [&m](rd::Event& e) {
    if (e.key() != RD_KEY_ENTER || (e.mods() & RD_MOD_SHIFT)) return;
    e.prevent_default();  // Enter sends; Shift+Enter keeps the newline
    submit(m);
  });
  m.input.on(RD_EVENT_INPUT, [&m] { m.send.set_state(RD_STATE_DISABLED, blank(m.input.text())); });
  m.send.on_click([&m] {
    submit(m);
    m.input.focus();
  });
  rd_ctx_on_layout(ctx.get(), follow, &m, nullptr);
  open(m, 0);
  return split;
}

}  // namespace

rd::Node build_messages(rd::Context& ctx) {
  auto* m = new Messages;  // lives as long as the program
  return build(ctx, *m);
}
