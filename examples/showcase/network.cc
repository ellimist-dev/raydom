// The Network tab: a deterministic single-decree Paxos run drawn with raydom_extras' network view. Two proposers
// compete for five acceptors. Message delays come from a seeded generator and the simulation
// advances in whole logical ticks, so a seed always produces the same run; the view only
// interpolates messages between ticks.

#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <raydom/extras/graph.hh>
#include <raydom/raydom.hh>
#include <string>
#include <vector>

#include "showcase.h"

using namespace rd::literals;

namespace {

constexpr int kAcceptors = 5, kProposers = 2, kMajority = kAcceptors / 2 + 1;


// Simulation.

enum Kind { Prepare,
            Promise,
            Accept,
            Accepted,
            Nack };
const char*         kKindName[]   = {"prepare", "promise", "accept", "accepted", "nack"};
const std::uint32_t kKindColor[]  = {0x5b8cff, 0x2ec4b6, 0xf4a261, 0x6bd968, 0xef476f};
const char*         kValueName[]  = {"-", "x", "y"};
const std::uint32_t kValueColor[] = {0x6c7385, 0x2ec4b6, 0xf4a261};

struct Message {
  int  from, to;  // node ids: proposers 0..1, acceptors 2..6
  Kind kind;
  int  ballot, value, accepted_ballot, accepted_value;
  int  sent, deliver, seq;
};

struct Acceptor {
  int promised = 0, accepted_ballot = 0, accepted_value = 0;
};

struct Proposer {
  int              ballot = 0, value = 0, phase = 0;  // 0 waiting, 1 preparing, 2 accepting, 3 chosen
  int              start = 0, best_ballot = 0, best_value = 0;
  std::vector<int> promises, accepts;
};

struct Sim {
  std::uint64_t                        rng;
  int                                  tick = 0, seq = 0;
  Proposer                             proposers[kProposers];
  Acceptor                             acceptors[kAcceptors];
  std::vector<Message>                 inflight;
  std::vector<std::deque<std::string>> logs   = std::vector<std::deque<std::string>>(kProposers + kAcceptors);
  int                                  chosen = 0;

  explicit Sim(std::uint64_t seed) : rng(seed) {
    proposers[0] = {.ballot = 1, .value = 1, .start = 0};
    proposers[1] = {.ballot = 2, .value = 2, .start = 7};
  }

  int delay() {
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return 3 + static_cast<int>(rng % 6);
  }

  void log(int node, const std::string& line) {
    auto& l = logs[static_cast<std::size_t>(node)];
    l.push_back("t" + std::to_string(tick) + "  " + line);
    if (l.size() > 14) l.pop_front();
  }

  void send(int from, int to, Kind kind, int ballot, int value = 0, int ab = 0, int av = 0) {
    inflight.push_back({from, to, kind, ballot, value, ab, av, tick, tick + delay(), seq++});
    log(from, std::string("sent ") + kKindName[kind] + " b" + std::to_string(ballot) + " to " + name(to));
  }

  static std::string name(int id) {
    return id < kProposers ? "P" + std::to_string(id + 1) : "A" + std::to_string(id - kProposers + 1);
  }

  void start(int p) {
    Proposer& pr = proposers[p];
    pr.phase     = 1;
    pr.promises.clear();
    pr.accepts.clear();
    pr.best_ballot = pr.best_value = 0;
    for (int a = 0; a < kAcceptors; a++) send(p, kProposers + a, Prepare, pr.ballot);
  }

  void receive(const Message& m) {
    log(m.to, std::string("got ") + kKindName[m.kind] + " b" + std::to_string(m.ballot) + " from " + name(m.from));
    if (m.to >= kProposers) {
      Acceptor& a = acceptors[m.to - kProposers];
      if (m.kind == Prepare) {
        if (m.ballot > a.promised) {
          a.promised = m.ballot;
          send(m.to, m.from, Promise, m.ballot, 0, a.accepted_ballot, a.accepted_value);
        } else {
          send(m.to, m.from, Nack, a.promised);
        }
      } else if (m.kind == Accept) {
        if (m.ballot >= a.promised) {
          a.promised        = m.ballot;
          a.accepted_ballot = m.ballot;
          a.accepted_value  = m.value;
          send(m.to, m.from, Accepted, m.ballot, m.value);
        } else {
          send(m.to, m.from, Nack, a.promised);
        }
      }
      return;
    }
    Proposer& p = proposers[m.to];
    if (m.kind == Nack && p.phase != 3 && m.ballot >= p.ballot) {
      p.ballot = (m.ballot / 10 + 1) * 10 + m.to + 1;
      p.phase  = 0;
      p.start  = tick + 4 + static_cast<int>(delay());
      log(m.to, "preempted; retry with b" + std::to_string(p.ballot));
    } else if (m.kind == Promise && p.phase == 1 && m.ballot == p.ballot) {
      p.promises.push_back(m.from);
      if (m.accepted_ballot > p.best_ballot) {
        p.best_ballot = m.accepted_ballot;
        p.best_value  = m.accepted_value;
      }
      if (static_cast<int>(p.promises.size()) == kMajority) {
        p.phase = 2;
        if (p.best_value) p.value = p.best_value;
        for (int a = 0; a < kAcceptors; a++) send(m.to, kProposers + a, Accept, p.ballot, p.value);
      }
    } else if (m.kind == Accepted && p.phase == 2 && m.ballot == p.ballot) {
      p.accepts.push_back(m.from);
      if (static_cast<int>(p.accepts.size()) == kMajority) {
        p.phase = 3;
        chosen  = p.value;
        log(m.to, std::string("chosen: ") + kValueName[p.value]);
      }
    }
  }

  void step() {
    tick++;
    for (int p = 0; p < kProposers; p++)
      if (proposers[p].phase == 0 && proposers[p].start <= tick) start(p);
    std::vector<Message> due;
    for (const Message& m : inflight)
      if (m.deliver <= tick) due.push_back(m);
    std::erase_if(inflight, [&](const Message& m) { return m.deliver <= tick; });
    std::sort(due.begin(), due.end(), [](const Message& a, const Message& b) { return a.seq < b.seq; });
    for (const Message& m : due) receive(m);
  }
};

// View.

struct App {
  rd::Context*          ctx = nullptr;
  rdx::Graph            graph;
  std::vector<rdx_node> nodes;
  Sim                   sim{42};
  std::uint64_t         seed    = 42;
  bool                  playing = false;
  float                 speed = 1, frac = 0;
  int                   selected = -1;
  rd::Node              tick_label, play_label, speed_label, title, role, state, log_text, verdict;
  rd_timer              ticker{};
  double                last = 0;
};

std::string node_label(const Sim& s, int id) {
  if (id < kProposers) {
    const Proposer& p       = s.proposers[id];
    const char*     phase[] = {"waiting", "phase 1", "phase 2", "chosen"};
    return Sim::name(id) + "  b" + std::to_string(p.ballot) + "  " + phase[p.phase];
  }
  const Acceptor& a = s.acceptors[id - kProposers];
  return Sim::name(id) + "  p" + std::to_string(a.promised) + "  " +
         (a.accepted_ballot ? std::string(kValueName[a.accepted_value]) + "@" + std::to_string(a.accepted_ballot) : "-");
}

rd_color node_color(const Sim& s, int id) {
  if (id < kProposers) return s.proposers[id].phase == 3 ? rd::hex(0x6bd968) : rd::hex(id ? 0xc77dff : 0x7aa2ff);
  return rd::hex(kValueColor[s.acceptors[id - kProposers].accepted_value]);
}

void refresh(App& app) {
  const Sim& s = app.sim;
  for (int id = 0; id < kProposers + kAcceptors; id++) {
    rdx_node n = app.nodes[static_cast<std::size_t>(id)];
    app.graph.label(n, node_label(s, id));
    app.graph.color(n, node_color(s, id));
  }
  app.tick_label.text("tick " + std::to_string(s.tick));
  app.verdict.text(s.chosen ? std::string("chosen: ") + kValueName[s.chosen] : "no value chosen yet");

  if (app.selected < 0) {
    app.title.text("Select a node");
    app.role.text("Click a sphere to inspect it. Drag to pan, scroll to zoom.");
    app.state.text("");
    app.log_text.text("");
    return;
  }
  int id = app.selected;
  app.title.text(Sim::name(id));
  if (id < kProposers) {
    const Proposer& p = s.proposers[id];
    app.role.text("proposer, wants " + std::string(kValueName[id + 1]));
    app.state.text("ballot " + std::to_string(p.ballot) + "\npromises " + std::to_string(p.promises.size()) + "/" +
                   std::to_string(kMajority) + "\naccepts " + std::to_string(p.accepts.size()) + "/" +
                   std::to_string(kMajority) + "\nproposing " + kValueName[p.value]);
  } else {
    const Acceptor& a = s.acceptors[id - kProposers];
    app.role.text("acceptor");
    app.state.text("promised b" + std::to_string(a.promised) + "\naccepted " +
                   (a.accepted_ballot ? std::string(kValueName[a.accepted_value]) + " at b" + std::to_string(a.accepted_ballot)
                                      : std::string("nothing")));
  }
  std::string log;
  for (const std::string& line : s.logs[static_cast<std::size_t>(id)]) log += line + "\n";
  app.log_text.text(log.empty() ? "no activity yet" : log);
}

void push_messages(App& app) {
  std::vector<rdx_message> out;
  float                    t = static_cast<float>(app.sim.tick) + app.frac;
  for (const Message& m : app.sim.inflight) {
    float span = static_cast<float>(m.deliver - m.sent);
    out.push_back({app.nodes[static_cast<std::size_t>(m.from)], app.nodes[static_cast<std::size_t>(m.to)],
                   (t - static_cast<float>(m.sent)) / span, rd::hex(kKindColor[m.kind]), 7});
  }
  app.graph.messages(out);
}

void reset(App& app) {
  app.sim  = Sim(app.seed);
  app.frac = 0;
  push_messages(app);
  refresh(app);
}

void step(App& app) {
  app.sim.step();
  refresh(app);
}

rd::Node button(rd::Context& ctx, rd::Class cls, std::string_view label, rd::Node* text_out = nullptr) {
  rd::Node b = ctx.create().add_class(cls);
  rd::Node t = ctx.create_text(label, rd::Style().pointer_events(RD_POINTER_EVENTS_NONE));
  b.append(t);
  if (text_out) *text_out = t;
  return b;
}

rd::Node build(rd::Context& ctx, App& app) {

  rd::Class btn = ctx.create_class(rd::Style()
                                       .padding(6_px, 14_px)
                                       .border_radius(7_px)
                                       .border_width(1_px)
                                       .border_color(rd::hex(kLine))
                                       .background(rd::hex(0x1d2230))
                                       .focusable(RD_FOCUSABLE_TAB)
                                       .user_select(RD_USER_SELECT_NONE));
  btn.on(RD_STATE_HOVER, rd::Style().background(rd::hex(0x252c3d)));
  btn.on(RD_STATE_ACTIVE, rd::Style().background(rd::hex(0x1a1f2b)));
  btn.on(RD_STATE_FOCUS_VISIBLE, rd::Style().box_shadow(0, 0, 0, rd::rgba(91, 140, 255, 200), 2));
  rd::Class muted = ctx.create_class(rd::Style().color(rd::hex(kMuted)));

  rd::Node header = ctx.create(rd::Style()
                                   .flex_direction(RD_FLEX_ROW)
                                   .align_items(RD_ALIGN_CENTER)
                                   .column_gap(10_px)
                                   .padding(10_px, 16_px)
                                   .background(rd::hex(kPanel))
                                   .border_width(0_px, 0_px, 1_px, 0_px)
                                   .border_color(rd::hex(kLine)));
  header.append(ctx.create_text("Paxos", rd::Style().font_size(18_px)));
  header.append(ctx.create_text("single decree, 2 proposers, 5 acceptors").add_class(muted));
  header.append(ctx.create(rd::Style().flex(1)));
  app.tick_label = ctx.create_text("tick 0", rd::Style().width(80_px)).add_class(muted);
  rd::Node play  = button(ctx, btn, "Play", &app.play_label);
  rd::Node next  = button(ctx, btn, "Step");
  rd::Node again = button(ctx, btn, "Reset");
  rd::Node speed = button(ctx, btn, "1x", &app.speed_label);
  header.append(app.tick_label).append(play).append(next).append(again).append(speed);

  // Playing advances on a timer, which keeps frames coming while it runs.
  auto toggle = [&app] {
    app.playing = !app.playing;
    app.play_label.text(app.playing ? "Pause" : "Play");
    if (!app.playing) {
      app.ctx->clear_timer(app.ticker);
      return;
    }
    app.last   = GetTime();
    app.ticker = app.ctx->set_interval(1.0 / 120, [&app] {
      double now = GetTime();
      app.frac += static_cast<float>(now - app.last) * app.speed * 4;
      app.last = now;
      while (app.frac >= 1) {
        app.frac -= 1;
        step(app);
      }
      push_messages(app);
    });
  };
  play.on_click(toggle);
  next.on_click([&app] {
    app.frac = 0;
    step(app);
    push_messages(app);
  });
  again.on_click([&app] { reset(app); });
  speed.on_click([&app] {
    app.speed = app.speed >= 4 ? 0.5f : app.speed * 2;
    char s[16];
    std::snprintf(s, sizeof s, app.speed < 1 ? "%.1fx" : "%.0fx", static_cast<double>(app.speed));
    app.speed_label.text(s);
  });
  // Shortcuts while the tab has focus (click anywhere in it).
  rd::Node root = ctx.create(rd::Style().flex(1).min_height(0_px).focusable(RD_FOCUSABLE_POINTER));
  root.on(RD_EVENT_KEY_DOWN, [&app, toggle](rd::Event& e) {
    if (e.key() == RD_KEY_SPACE) toggle();
    if (e.key() == RD_KEY_RIGHT) {
      app.frac = 0;
      step(app);
      push_messages(app);
    }
    if (e.key() == RD_KEY_R) reset(app);
  });

  rd::Node body   = ctx.create(rd::Style().flex(1).min_height(0_px).flex_direction(RD_FLEX_ROW));
  rd::Node canvas = ctx.create(rd::Style().flex(1));
  app.graph       = rdx::Graph::create(canvas);
  app.graph.background(rd::hex(0x0b0d12));
  app.graph.label_class().set(rd::Style()
                                  .font_size(12_px)
                                  .white_space(RD_WHITE_SPACE_NOWRAP)
                                  .color(rd::hex(kText))
                                  .padding(2_px, 6_px)
                                  .border_radius(4_px)
                                  .background(rd::rgba(15, 17, 21, 200)));

  for (int id = 0; id < kProposers + kAcceptors; id++) {
    float r = id < kProposers ? 26.0f : 22.0f;
    app.nodes.push_back(app.graph.add_node({0, 0}, rd::hex(kMuted), Sim::name(id), r));
  }
  std::vector<rdx_node> acceptors(app.nodes.begin() + kProposers, app.nodes.end());
  app.graph.layout_circle(acceptors, {0, 0}, 190);
  app.graph.position(app.nodes[0], {-420, 0});
  app.graph.position(app.nodes[1], {420, 0});
  for (int p = 0; p < kProposers; p++)
    for (rdx_node a : acceptors) app.graph.connect(app.nodes[static_cast<std::size_t>(p)], a, {.color = rd::hex(0x2a3142), .width = 1.5f});
  app.graph.fit(70);
  app.graph.on([&app](rdx_graph_event ev, rdx_node n) {
    if (ev != RDX_EVENT_NODE_CLICK) return;
    for (std::size_t j = 0; j < app.nodes.size(); j++)
      if (app.nodes[j] == n) app.selected = app.selected == static_cast<int>(j) ? -1 : static_cast<int>(j);
    app.graph.select(app.selected >= 0 ? app.nodes[static_cast<std::size_t>(app.selected)] : rdx_node{});
    refresh(app);
  });

  rd::Node side   = ctx.create(rd::Style()
                                   .width(320_px)
                                   .padding(16_px)
                                   .row_gap(10_px)
                                   .background(rd::hex(kPanel))
                                   .border_width(0_px, 0_px, 0_px, 1_px)
                                   .border_color(rd::hex(kLine))
                                   .overflow_y(RD_OVERFLOW_SCROLL));
  app.verdict     = ctx.create_text("", rd::Style().font_size(16_px).color(rd::hex(0x6bd968)));
  app.title       = ctx.create_text("", rd::Style().font_size(20_px));
  app.role        = ctx.create_text("").add_class(muted);
  app.state       = ctx.create_text("", rd::Style().white_space(RD_WHITE_SPACE_PRE));
  app.log_text    = ctx.create_text("", rd::Style()
                                            .white_space(RD_WHITE_SPACE_PRE)
                                            .font_size(12_px)
                                            .padding(10_px)
                                            .border_radius(8_px)
                                            .background(rd::hex(0x0b0d12))
                                            .color(rd::hex(0xc7d2fe)));
  rd::Node legend = ctx.create(rd::Style().row_gap(6_px).padding(10_px, 0_px));
  for (int k = 0; k < 5; k++) {
    rd::Node row = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(8_px));
    row.append(ctx.create(rd::Style().width(10_px).height(10_px).border_radius(5_px).background(rd::hex(kKindColor[k]))))
        .append(ctx.create_text(kKindName[k]).add_class(muted));
    legend.append(row);
  }
  side.append(app.verdict).append(legend).append(app.title).append(app.role).append(app.state).append(app.log_text);
  body.append(canvas).append(side);
  root.append(header).append(body);
  reset(app);
  return root;
}

}  // namespace

rd::Node build_network(rd::Context& ctx) {
  App* app = new App;  // lives as long as the program
  app->ctx = &ctx;
  return build(ctx, *app);
}
