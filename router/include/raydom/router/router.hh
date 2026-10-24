#pragma once

#include <raydom/router/router.h>

#include <functional>
#include <memory>
#include <new>
#include <raydom/raydom.hh>
#include <string>
#include <string_view>
#include <vector>

// C++ form of router.h. A router is a typed handle, an rd::Node with the router's operations as
// members; page builders are std::function, owned by the router. Strings are copied, so temporaries
// are fine; the string_views returned are valid until the next navigation.
namespace rdr {

class Router;

// Builds a page for a route that was entered; router is the one showing it.
using BuildFn = std::function<rd::Node(Router router)>;

// A route's match on a prefix of the path, for title functions.
class Match {
public:
  explicit Match(const rdr_match* m) : m_(m) {}
  // Empty if absent.
  std::string_view param(const std::string& name) const {
    const char* v = rdr_match_param(m_, name.c_str());
    return v ? std::string_view(v) : std::string_view();
  }
  std::string_view path() const { return rdr_match_path(m_); }

private:
  const rdr_match* m_;
};

// Names a route's breadcrumb; empty falls back to the static title.
using TitleFn = std::function<std::string(const Match& match)>;

struct RouterOptions {
  rdr_transition transition    = RDR_TRANSITION_NONE;
  double         duration      = 0;  // seconds; 0 means 0.2
  size_t         history_limit = 0;  // 0 means 100
};

struct RouteOptions {
  std::string title{};  // for trail(); :name is replaced by the parameter
  TitleFn     title_fn{};  // for trail(), from data; takes precedence over title
  bool        keep_alive = false;
};

struct Crumb {
  std::string_view title;
  std::string_view path;
};

namespace detail {
inline std::string_view view(const char* s) { return s ? std::string_view(s) : std::string_view(); }

struct RouteFns {
  BuildFn     build;
  TitleFn     title;
  std::string last;  // the title function's result, while the C layer copies it
};
}  // namespace detail

class Router : public rd::Node {
public:
  using rd::Node::Node;
  Router() = default;
  explicit Router(rd::Node n) : rd::Node(n) {}

  bool route(const std::string& pattern, BuildFn build, const RouteOptions& o = {}) const {
    using detail::RouteFns;
    std::unique_ptr<RouteFns> fn(new (std::nothrow) RouteFns{std::move(build), o.title_fn, {}});
    if (!fn) return false;
    rdr_title_fn title = nullptr;
    if (fn->title)
      title = [](rd_ctx*, const rdr_match* m, void* p) noexcept -> const char* {
        auto& f = *static_cast<RouteFns*>(p);
        f.last  = f.title(Match(m));
        return f.last.empty() ? nullptr : f.last.c_str();
      };
    rdr_route_desc d{pattern.c_str(), o.title.empty() ? nullptr : o.title.c_str(), title,
                     [](rd_ctx* c, rd_node r, void* p) noexcept {
                       auto& f = static_cast<RouteFns*>(p)->build;
                       return f ? f(Router(c, r)).handle() : rd_node{};
                     },
                     fn.get(), [](void* p) noexcept { delete static_cast<RouteFns*>(p); },
                     o.keep_alive ? uint32_t(RDR_ROUTE_KEEP_ALIVE) : 0u};
    if (!rdr_router_add(ctx(), handle(), &d)) return false;
    fn.release();
    return true;
  }

  bool go(const std::string& path) const { return rdr_router_go(ctx(), handle(), path.c_str()); }
  bool replace(const std::string& path) const { return rdr_router_replace(ctx(), handle(), path.c_str()); }
  bool back() const { return rdr_router_back(ctx(), handle()); }
  bool forward() const { return rdr_router_forward(ctx(), handle()); }
  bool can_back() const { return rdr_router_can_back(ctx(), handle()); }
  bool can_forward() const { return rdr_router_can_forward(ctx(), handle()); }

  std::string_view path() const { return detail::view(rdr_router_path(ctx(), handle())); }
  std::string_view base() const { return detail::view(rdr_router_base(ctx(), handle())); }
  std::string_view matched_route() const { return detail::view(rdr_router_route(ctx(), handle())); }
  // Empty if absent; has_param tells an absent parameter from an empty one.
  std::string_view param(const std::string& name) const { return detail::view(rdr_param(ctx(), handle(), name.c_str())); }
  bool     has_param(const std::string& name) const { return rdr_param(ctx(), handle(), name.c_str()) != nullptr; }
  rd::Node page() const { return {ctx(), rdr_router_page(ctx(), handle())}; }
  Router   root() const { return {ctx(), rdr_router_root(ctx(), handle())}; }
  bool     transitioning() const { return rdr_router_transitioning(ctx(), handle()); }
  bool     set_transition(rdr_transition t, double seconds = 0) const {
    return rdr_router_set_transition(ctx(), handle(), t, seconds);
  }

  std::vector<Crumb> trail() const {
    std::vector<rdr_crumb> c(rdr_router_trail(ctx(), handle(), nullptr, 0));
    c.resize(rdr_router_trail(ctx(), handle(), c.data(), c.size()));
    std::vector<Crumb> out;
    out.reserve(c.size());
    for (const rdr_crumb& k : c) out.push_back({detail::view(k.title), detail::view(k.path)});
    return out;
  }
};

inline Router router(const rd::Context& ctx, const RouterOptions& o = {}) {
  rdr_router_desc d{{}, o.transition, o.duration, o.history_limit};
  return {ctx.get(), rdr_router_create(ctx.get(), &d)};
}

// A router nested in one of parent's pages, usually created by the page's builder.
inline Router router(Router parent, const RouterOptions& o = {}) {
  rdr_router_desc d{parent.handle(), o.transition, o.duration, o.history_limit};
  return {parent.ctx(), rdr_router_create(parent.ctx(), &d)};
}

// A parameter seen from any node in a page.
inline std::string_view param(rd::Node n, const std::string& name) { return detail::view(rdr_param(n.ctx(), n.handle(), name.c_str())); }

// With a null router, the nearest router above the link, or the router used last.
inline bool link(rd::Node node, const std::string& path, Router r = {}, uint32_t flags = 0) {
  return rdr_link_attach(node.ctx(), node.handle(), r.handle(), path.c_str(), flags);
}
inline rd::Node link(const rd::Context& ctx, const std::string& label, const std::string& path, Router r = {}, uint32_t flags = 0) {
  return {ctx.get(), rdr_link_create(ctx.get(), label.c_str(), r.handle(), path.c_str(), flags)};
}

}  // namespace rdr
