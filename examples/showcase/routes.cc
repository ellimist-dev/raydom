// The Router tab: a small app on raydom::router. Links in the sidebar, back and forward buttons and a
// breadcrumb above the page, pages sliding in and out, and a settings section with a nested router,
// kept alive so its fields keep what was typed. Alt+Left / Alt+Right (Cmd+[ / Cmd+] on macOS) and the
// mouse's side buttons go back and forward.

#include <raydom/extras/views.hh>
#include <raydom/router/router.hh>
#include <cstdint>
#include <string>
#include <vector>

#include "showcase.h"

using namespace rd::literals;

namespace {

struct Project {
  const char* id;
  const char* name;
  const char* blurb;
};

const Project kProjects[] = {
    {"orbit", "Orbit", "A gravity sandbox: drag planets, watch the orbits settle."},
    {"ledger", "Ledger", "Double-entry bookkeeping for one person, in a single window."},
    {"loom", "Loom", "A node editor for shader graphs."},
};

struct Routes {
  rdr::Router     app;
  rdx::Breadcrumb crumbs;
  rd::Node        back, forward, path;
  rd::Class       link, nav, title, card, muted, field, button;
};

Routes& state() {
  static Routes r;
  return r;
}

rd::Node heading(const rd::Context& ctx, const std::string& s) { return ctx.create_text(s).add_class(state().title); }
rd::Node para(const rd::Context& ctx, const std::string& s) { return ctx.create_text(s).add_class(state().muted); }

rd::Node page(const rd::Context& ctx) {
  return ctx.create(rd::Style().flex(1).padding(24_px).row_gap(12_px).overflow_y(RD_OVERFLOW_SCROLL).background(rd::hex(kBg)));
}

rd::Node home(const rd::Context& ctx) {
  rd::Node p = page(ctx);
  p.append(heading(ctx, "Home"));
  p.append(para(ctx, "Each page is built when its route is entered and destroyed when it's left. Settings is kept "
                     "alive: type in its fields, leave, and come back."));
  rd::Node row = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).column_gap(8_px));
  for (const char* to : {"/projects", "/projects/loom", "/settings/profile", "/nowhere"})
    row.append(rdr::link(ctx, to, to).add_class(state().button));
  return p.append(row);
}

rd::Node projects(const rd::Context& ctx) {
  rd::Node p = page(ctx);
  p.append(heading(ctx, "Projects"));
  for (const Project& pr : kProjects) {
    rd::Node card = ctx.create().add_class(state().card);
    card.append(ctx.create_text(pr.name)).append(para(ctx, pr.blurb));
    rdr::link(card.role(RD_ROLE_LINK), std::string("/projects/") + pr.id);
    p.append(card);
  }
  return p;
}

rd::Node project(const rd::Context& ctx, rdr::Router r) {
  rd::Node p  = page(ctx);
  auto     id = r.param("id");
  for (const Project& pr : kProjects)
    if (id == pr.id) {
      p.append(heading(ctx, pr.name)).append(para(ctx, pr.blurb));
      return p.append(rdr::link(ctx, "All projects", "/projects").add_class(state().button).merge_style(rd::Style().align_self(RD_ALIGN_START)));
    }
  return p.append(heading(ctx, "No project " + std::string(id)));
}

rd::Node field(const rd::Context& ctx, const char* label, const char* placeholder) {
  rd::Node row = ctx.create(rd::Style().row_gap(4_px).max_width(360_px));
  row.append(ctx.create_text(label).add_class(state().muted));
  return row.append(ctx.create_input().placeholder(placeholder).add_class(state().field));
}

// Settings: a nested router fading between its sections, with tabs as links.
rd::Node settings(const rd::Context& ctx, rdr::Router parent) {
  rd::Node p = page(ctx);
  p.append(heading(ctx, "Settings"));
  rd::Node    bar   = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).column_gap(4_px));
  rdr::Router inner = rdr::router(parent, {.transition = RDR_TRANSITION_FADE, .duration = 0.15});
  inner.style(rd::Style().flex(1).min_height(0_px));
  struct Tab {
    const char *label, *to;
    uint32_t    flags;
  };
  for (const Tab& t : {Tab{"General", ".", RDR_LINK_EXACT}, Tab{"Profile", "profile", 0}, Tab{"Notifications", "notifications", 0}})
    bar.append(rdr::link(ctx, t.label, t.to, inner, t.flags).add_class(state().nav));
  inner.route("", [&ctx](rdr::Router) {
    return ctx.create(rd::Style().row_gap(12_px).padding(12_px, 0_px)).append(field(ctx, "Workspace", "My workspace"));
  });
  inner.route("profile", [&ctx](rdr::Router) {
    return ctx.create(rd::Style().row_gap(12_px).padding(12_px, 0_px))
        .append(field(ctx, "Name", "Ada Lovelace"))
        .append(field(ctx, "Email", "ada@example.com"));
  }, {.title = "Profile"});
  inner.route("notifications", [&ctx](rdr::Router) {
    return ctx.create(rd::Style().padding(12_px, 0_px)).append(para(ctx, "Nothing to configure yet."));
  }, {.title = "Notifications"});
  return p.append(bar).append(inner);
}

// The trail, buttons and path follow every navigation.
void refresh(const rd::Context&) {
  Routes&                  s = state();
  std::vector<std::string> labels;
  for (const rdr::Crumb& c : s.app.trail()) labels.emplace_back(c.title);
  s.crumbs.set(labels);
  s.back.set_state(RD_STATE_DISABLED, !s.app.can_back());
  s.forward.set_state(RD_STATE_DISABLED, !s.app.can_forward());
  s.path.text(s.app.path());
}

}  // namespace

rd::Node build_routes(rd::Context& ctx) {
  Routes& s = state();
  s.title   = ctx.create_class(rd::Style().font_size(20_px));
  s.muted   = ctx.create_class(rd::Style().color(rd::hex(kMuted)));
  s.card    = ctx.create_class(rd::Style()
                                   .padding(12_px, 14_px)
                                   .row_gap(4_px)
                                   .border_radius(8_px)
                                   .background(rd::hex(kRaised))
                                   .border_width(1_px)
                                   .border_color(rd::hex(kLine))
                                   .cursor(RD_CURSOR_POINTER));
  s.card.on(RD_STATE_HOVER, rd::Style().border_color(rd::hex(kAccent)));
  s.nav = ctx.create_class(rd::Style().padding(7_px, 12_px).border_radius(6_px).color(rd::hex(kMuted)).cursor(RD_CURSOR_POINTER));
  s.nav.on(RD_STATE_HOVER, rd::Style().background(rd::hex(kRaised)).color(rd::hex(kText)));
  s.nav.on(RD_STATE_CHECKED, rd::Style().background(rd::hex(0x1e2a45)).color(rd::hex(kText)));
  s.button = ctx.create_class(rd::Style().padding(6_px, 12_px).border_radius(6_px).background(rd::hex(kRaised)).border_width(1_px)
                                  .border_color(rd::hex(kLine)).cursor(RD_CURSOR_POINTER));
  s.button.on(RD_STATE_HOVER, rd::Style().border_color(rd::hex(kAccent)));
  s.button.on(RD_STATE_DISABLED, rd::Style().color(rd::hex(kLine)).cursor(RD_CURSOR_DEFAULT));
  s.field = ctx.create_class(rd::Style().padding(7_px, 10_px).border_radius(6_px).background(rd::hex(kPanel)).border_width(1_px)
                                 .border_color(rd::hex(kLine)));
  s.field.on(RD_STATE_FOCUS, rd::Style().border_color(rd::hex(kAccent)));

  s.app = rdr::router(ctx, {.transition = RDR_TRANSITION_FADE, .duration = 0.25});
  s.app.style(rd::Style().flex(1).min_height(0_px));
  s.app.route("/", [&ctx](rdr::Router) { return home(ctx); }, {.title = "Home"});
  s.app.route("projects", [&ctx](rdr::Router) { return projects(ctx); }, {.title = "Projects"});
  s.app.route("projects/:id", [&ctx](rdr::Router r) { return project(ctx, r); },
              {.title = ":id", .title_fn = [](const rdr::Match& m) {
                 for (const Project& pr : kProjects)
                   if (m.param("id") == pr.id) return std::string(pr.name);
                 return std::string();  // unknown: the id
               }});
  s.app.route("settings/*", [&ctx](rdr::Router r) { return settings(ctx, r); }, {.title = "Settings", .keep_alive = true});
  s.app.route("*", [&ctx](rdr::Router r) {
    return page(ctx).append(heading(ctx, "Not found")).append(para(ctx, "Nothing at " + std::string(r.path())));
  }, {.title = "Not found"});

  rd::Node side = ctx.create(rd::Style().width(180_px).padding(16_px, 10_px).row_gap(2_px).background(rd::hex(kPanel))
                                 .border_width(0_px, 1_px, 0_px, 0_px).border_color(rd::hex(kLine)));
  side.append(rdr::link(ctx, "Home", "/", s.app).add_class(s.nav));
  side.append(rdr::link(ctx, "Projects", "/projects", s.app).add_class(s.nav));
  side.append(rdr::link(ctx, "Settings", "/settings", s.app).add_class(s.nav));

  rd::Node top = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).align_items(RD_ALIGN_CENTER).column_gap(8_px)
                                .padding(8_px, 16_px).background(rd::hex(kPanel)).border_width(0_px, 0_px, 1_px, 0_px)
                                .border_color(rd::hex(kLine)));
  s.back    = ctx.create_button("<").add_class(s.button).on_click([] { state().app.back(); });
  s.forward = ctx.create_button(">").add_class(s.button).on_click([] { state().app.forward(); });
  s.crumbs  = rdx::breadcrumb(ctx);
  s.crumbs.merge_style(rd::Style().flex(1));
  s.crumbs.on(RD_EVENT_CHANGE, [] {
    Routes& st    = state();
    auto    trail = st.app.trail();
    int     k     = st.crumbs.clicked();
    if (k >= 0 && k < static_cast<int>(trail.size())) st.app.go(std::string(trail[static_cast<size_t>(k)].path));
  });
  s.path = ctx.create_text("/", rd::Style().color(rd::hex(kMuted)));
  top.append(s.back).append(s.forward).append(s.crumbs).append(s.path);
  s.app.on(RD_EVENT_CHANGE, [&ctx] { refresh(ctx); });

  rd::Node main = ctx.create(rd::Style().flex(1).min_width(0_px)).append(top).append(s.app);
  rd::Node root = ctx.create(rd::Style().flex(1).flex_direction(RD_FLEX_ROW).min_height(0_px)).append(side).append(main);
  s.app.go("/");
  return root;
}
