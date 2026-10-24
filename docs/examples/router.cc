// Runs headless as a test for the "Router" chapter.
#include <raydom/extras/views.hh>
#include <raydom/raydom.hh>
#include <raydom/router/router.hh>

#include <cassert>
#include <map>
#include <string>
#include <vector>

using namespace rd::literals;

// ANCHOR: title
// Crumbs show names, not ids. An empty result falls back to the static title, "User :id".
std::map<std::string, std::string, std::less<>> names = {{"7", "Ada"}, {"8", "Grace"}};

std::string user_title(const rdr::Match& m) {
  auto it = names.find(m.param("id"));
  return it != names.end() ? it->second : std::string();
}
// ANCHOR_END: title

int main() {
  rd::Context ctx;
  ctx.load_font_file(FONT_PATH);

  // ANCHOR: routes
  rdr::Router app = rdr::router(ctx, {.transition = RDR_TRANSITION_SLIDE});
  app.style(rd::Style().flex(1));
  app.route("/", [&](rdr::Router) { return ctx.create_text("Home"); }, {.title = "Home"});
  app.route("users", [&](rdr::Router) { return ctx.create_text("Everyone"); }, {.title = "Users"});
  app.route("users/:id", [&](rdr::Router r) {
    return ctx.create_text("User " + std::string(r.param("id")));
  }, {.title = "User :id", .title_fn = user_title});
  app.route("*", [&](rdr::Router) { return ctx.create_text("Not found"); });
  app.go("/");
  // ANCHOR_END: routes

  // ANCHOR: nested
  // /settings, /settings/profile, ...: the outer page holds a router for the rest of the path.
  app.route("settings/*", [&](rdr::Router outer) {
    rdr::Router inner = rdr::router(outer, {.transition = RDR_TRANSITION_FADE});
    inner.route("", [&](rdr::Router) { return ctx.create_text("General"); });
    inner.route("profile", [&](rdr::Router) { return ctx.create_text("Profile"); }, {.title = "Profile"});
    rd::Node tabs = ctx.create(rd::Style().flex_direction(RD_FLEX_ROW).column_gap(8_px));
    tabs.append(rdr::link(ctx, "General", ".", inner, RDR_LINK_EXACT)).append(rdr::link(ctx, "Profile", "profile", inner));
    return ctx.create().append(tabs).append(inner.style(rd::Style().flex(1)));
  }, {.title = "Settings", .keep_alive = true});
  // ANCHOR_END: nested

  // ANCHOR: links
  rd::Node nav = ctx.create(rd::Style().width(160_px));
  nav.append(rdr::link(ctx, "Home", "/", app));
  nav.append(rdr::link(ctx, "Users", "/users", app));  // checked on /users and /users/7
  nav.append(rdr::link(ctx, "Settings", "/settings", app));
  // ANCHOR_END: links

  // ANCHOR: breadcrumb
  rdx::Breadcrumb crumbs = rdx::breadcrumb(ctx);
  app.on(RD_EVENT_CHANGE, [app, crumbs] {
    std::vector<std::string> labels;
    for (const rdr::Crumb& c : app.trail()) labels.emplace_back(c.title);
    crumbs.set(labels);
  });
  crumbs.on(RD_EVENT_CHANGE, [app, crumbs] {
    auto trail = app.trail();
    app.go(std::string(trail[static_cast<size_t>(crumbs.clicked())].path));
  });
  // ANCHOR_END: breadcrumb

  rd::Node main = ctx.create(rd::Style().flex(1)).append(crumbs).append(app);
  ctx.root().style(rd::Style().flex_direction(RD_FLEX_ROW)).append(nav).append(main);
  ctx.layout(800, 600);

  assert(app.go("/users/7") && app.param("id") == "7" && crumbs.count() == 3);  // Home, Users, Ada
  assert(app.trail()[2].title == "Ada");
  assert(app.go("/settings/profile") && app.trail().size() == 3 && crumbs.count() == 3);
  assert(app.back() && app.path() == "/users/7" && app.forward() && app.path() == "/settings/profile");
  assert(app.go("/users/9") && app.trail()[2].title == "User 9");  // no name: the static title
  return 0;
}
