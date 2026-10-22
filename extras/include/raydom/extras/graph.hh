#pragma once

#include <raydom/extras/graph.h>

#include <memory>
#include <raydom/raydom.hh>
#include <string>
#include <type_traits>
#include <vector>

namespace rdx {

// A non-owning view of a graph; the graph lives as long as its host node.
class Graph {
public:
  Graph() = default;
  explicit Graph(rdx_graph* g) : g_(g) {}
  static Graph create(rd::Node host) { return Graph(rdx_graph_create(host.ctx(), host.handle())); }

  rdx_graph* get() const { return g_; }
  explicit   operator bool() const { return g_ != nullptr; }
  rd::Node   host() const { return {rdx_graph_ctx(g_), rdx_graph_host(g_)}; }
  rd::Class  label_class() const { return {rdx_graph_ctx(g_), rdx_graph_label_class(g_)}; }

  Graph& flags(std::uint32_t f) {
    rdx_graph_set_flags(g_, f);
    return *this;
  }
  Graph& background(rd_color c) {
    rdx_graph_set_background(g_, c);
    return *this;
  }

  // f(rdx_graph_event, rdx_node)
  template <class F>
  Graph& on(F&& f) {
    using Fn = std::decay_t<F>;
    std::unique_ptr<Fn> p(new (std::nothrow) Fn(std::forward<F>(f)));
    if (!p) return *this;
    if (rdx_graph_on(
            g_, [](rdx_graph*, rdx_graph_event ev, rdx_node n, void* d) noexcept { (*static_cast<Fn*>(d))(ev, n); }, p.get(),
            [](void* d) noexcept { delete static_cast<Fn*>(d); }))
      p.release();
    return *this;
  }

  rdx_node add_node(rd_vec2 position, rd_color color, const std::string& label = {}, float radius = 0) {
    rdx_node_desc d = {position, radius, color, label.empty() ? nullptr : label.c_str()};
    return rdx_graph_add_node(g_, &d);
  }
  bool    remove(rdx_node n) { return rdx_graph_remove_node(g_, n); }
  bool    alive(rdx_node n) const { return rdx_graph_node_alive(g_, n); }
  bool    position(rdx_node n, rd_vec2 p) { return rdx_graph_set_position(g_, n, p); }
  rd_vec2 position(rdx_node n) const { return rdx_graph_position(g_, n); }
  bool    color(rdx_node n, rd_color c) { return rdx_graph_set_color(g_, n, c); }
  bool    radius(rdx_node n, float r) { return rdx_graph_set_radius(g_, n, r); }
  bool    label(rdx_node n, const std::string& s) { return rdx_graph_set_label(g_, n, s.c_str()); }
  void    select(rdx_node n) { rdx_graph_set_selected(g_, n); }

  rdx_edge connect(rdx_node a, rdx_node b, rdx_edge_desc d = {}) { return rdx_graph_connect(g_, a, b, &d); }
  bool     remove(rdx_edge e) { return rdx_graph_remove_edge(g_, e); }
  bool     color(rdx_edge e, rd_color c) { return rdx_graph_set_edge_color(g_, e, c); }
  bool     label(rdx_edge e, const std::string& s) { return rdx_graph_set_edge_label(g_, e, s.c_str()); }

  void messages(const std::vector<rdx_message>& m) { rdx_graph_set_messages(g_, m.data(), m.size()); }

  void     view(rd_vec2 center, float zoom) { rdx_graph_set_view(g_, center, zoom); }
  void     fit(float padding = 40) { rdx_graph_fit(g_, padding); }
  rdx_node node_at(float x, float y) const { return rdx_graph_node_at(g_, x, y); }
  void     layout_circle(const std::vector<rdx_node>& nodes, rd_vec2 center, float radius) {
    rdx_graph_layout_circle(g_, nodes.data(), nodes.size(), center, radius);
  }

private:
  rdx_graph* g_ = nullptr;
};

}  // namespace rdx

// Global so lookup finds them for the C handle types.
inline bool operator==(rdx_node a, rdx_node b) { return a.index == b.index && a.gen == b.gen; }
inline bool operator==(rdx_edge a, rdx_edge b) { return a.index == b.index && a.gen == b.gen; }
