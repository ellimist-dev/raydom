#include <raydom/router/router.hh>

#include "check.h"

using namespace rd::literals;

int main() {
  rd::Context ctx;
  ctx.load_font_file(RAYDOM_TEST_FONT);

  int         builds = 0;
  rdr::Router app    = rdr::router(ctx, {.transition = RDR_TRANSITION_FADE});
  ctx.root().append(app.style(rd::Style().width(400_px).height(300_px)));
  CHECK(app && app.route("/", [&](rdr::Router) { return ctx.create_text("Home"); }, {.title = "Home"}));
  CHECK(app.route("users/:id", [&](rdr::Router r) {
    builds++;
    return ctx.create_text(std::string("User ") + std::string(r.param("id")));
  }, {.title = "User :id", .title_fn = [](const rdr::Match& m) {
    return m.param("id") == "8" ? std::string("Eight at ") + std::string(m.path()) : std::string();
  }}));
  CHECK(app.route("docs/*", [&](rdr::Router r) {
    rdr::Router inner = rdr::router(r);
    inner.route("", [&](rdr::Router) { return ctx.create_text("Index"); });
    inner.route(":page", [&](rdr::Router) { return ctx.create_text("Page"); }, {.title = ":page", .keep_alive = true});
    return ctx.create().append(inner);
  }, {.title = "Docs"}));
  CHECK(!app.route("a/*/b", {}));

  CHECK(app.go("/users/7") && app.path() == "/users/7" && app.param("id") == "7" && builds == 1);
  CHECK(app.matched_route() == "users/:id" && app.has_param("id") && !app.has_param("x") && app.param("x").empty());
  CHECK(rdr::param(app.page(), "id") == "7");
  auto trail = app.trail();
  CHECK(trail.size() == 2 && trail[0].title == "Home" && trail[1].title == "User 7" && trail[1].path == "/users/7");

  CHECK(app.go("/docs/intro") && app.trail().size() == 3 && app.trail()[2].title == "intro");
  CHECK(app.back() && app.path() == "/users/7" && app.can_forward() && app.forward() && !app.can_forward());

  rd::Node link = rdr::link(ctx, "Home", "/", app, RDR_LINK_EXACT);
  rd::Node any  = ctx.create_text("Seven");
  CHECK(link && rdr::link(any, "/users/7") && !rdr::link(any, "/again"));
  ctx.root().append(link);
  CHECK(app.go("/") && (link.states() & RD_STATE_CHECKED));
  CHECK(app.transitioning() && app.set_transition(RDR_TRANSITION_NONE) && !app.transitioning());
  CHECK(app.replace("/users/8") && builds == 3 && app.root() == app && app.base() == "/");
  CHECK(app.trail().size() == 2 && app.trail()[1].title == "Eight at /users/8");
  return TEST_RESULT();
}
