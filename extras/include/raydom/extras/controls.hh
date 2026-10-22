#pragma once

#include <raydom/extras/controls.h>
#include <raydom/extras/toast.h>

#include <raydom/raydom.hh>
#include <string>

// C++ forms of controls.h and toast.h. Each control is a typed handle: an rd::Node with the control's
// operations as members (slider.value()), which converts to rd::Node. The free functions taking an
// rd::Node do the same for any handle. Listen for RD_EVENT_CHANGE and RD_EVENT_INPUT with node.on().
namespace rdx {

namespace detail {
inline const char* opt(const std::string& s) { return s.empty() ? nullptr : s.c_str(); }
}  // namespace detail

// A checkbox or a switch.
class Toggle : public rd::Node {
public:
  using rd::Node::Node;
  Toggle() = default;
  explicit Toggle(rd::Node n) : rd::Node(n) {}
  bool checked() const { return rdx_checked(ctx(), handle()); }
  bool set_checked(bool on) const { return rdx_set_checked(ctx(), handle(), on); }
};

class RadioGroup : public rd::Node {
public:
  using rd::Node::Node;
  RadioGroup() = default;
  explicit RadioGroup(rd::Node n) : rd::Node(n) {}
  rd::Node add(const std::string& label) const { return {ctx(), rdx_radio_add(ctx(), handle(), detail::opt(label))}; }
  int      selected() const { return rdx_radio_selected(ctx(), handle()); }
  bool     select(int index) const { return rdx_radio_select(ctx(), handle(), index); }
};

class Slider : public rd::Node {
public:
  using rd::Node::Node;
  Slider() = default;
  explicit Slider(rd::Node n) : rd::Node(n) {}
  float value() const { return rdx_slider_value(ctx(), handle()); }
  bool  set_value(float v) const { return rdx_slider_set_value(ctx(), handle(), v); }
  bool  set_range(float min, float max) const { return rdx_slider_set_range(ctx(), handle(), min, max); }
};

class Number : public rd::Node {
public:
  using rd::Node::Node;
  Number() = default;
  explicit Number(rd::Node n) : rd::Node(n) {}
  float    value() const { return rdx_number_value(ctx(), handle()); }
  bool     set_value(float v) const { return rdx_number_set_value(ctx(), handle(), v); }
  bool     set_range(float min, float max) const { return rdx_number_set_range(ctx(), handle(), min, max); }
  rd::Node input() const { return {ctx(), rdx_number_input(ctx(), handle())}; }
};

inline Toggle checkbox(const rd::Context& ctx, const std::string& label = {}) {
  return {ctx.get(), rdx_checkbox_create(ctx.get(), detail::opt(label))};
}
inline Toggle toggle_switch(const rd::Context& ctx, const std::string& label = {}) {
  return {ctx.get(), rdx_switch_create(ctx.get(), detail::opt(label))};
}
inline bool checked(rd::Node n) { return rdx_checked(n.ctx(), n.handle()); }
inline bool set_checked(rd::Node n, bool on) { return rdx_set_checked(n.ctx(), n.handle(), on); }

inline RadioGroup radio_group(const rd::Context& ctx) { return {ctx.get(), rdx_radio_group_create(ctx.get())}; }
inline rd::Node add_radio(rd::Node group, const std::string& label) {
  return {group.ctx(), rdx_radio_add(group.ctx(), group.handle(), detail::opt(label))};
}
inline int  radio_selected(rd::Node group) { return rdx_radio_selected(group.ctx(), group.handle()); }
inline bool radio_select(rd::Node group, int index) { return rdx_radio_select(group.ctx(), group.handle(), index); }

inline Slider slider(const rd::Context& ctx, const rdx_slider_desc& desc = {}) {
  return {ctx.get(), rdx_slider_create(ctx.get(), &desc)};
}
inline float slider_value(rd::Node n) { return rdx_slider_value(n.ctx(), n.handle()); }
inline bool  set_slider_value(rd::Node n, float v) { return rdx_slider_set_value(n.ctx(), n.handle(), v); }
inline bool  set_slider_range(rd::Node n, float min, float max) { return rdx_slider_set_range(n.ctx(), n.handle(), min, max); }

inline Number number(const rd::Context& ctx, const rdx_number_desc& desc = {}) {
  return {ctx.get(), rdx_number_create(ctx.get(), &desc)};
}
inline float    number_value(rd::Node n) { return rdx_number_value(n.ctx(), n.handle()); }
inline bool     set_number_value(rd::Node n, float v) { return rdx_number_set_value(n.ctx(), n.handle(), v); }
inline bool     set_number_range(rd::Node n, float min, float max) { return rdx_number_set_range(n.ctx(), n.handle(), min, max); }
inline rd::Node number_input(rd::Node n) { return {n.ctx(), rdx_number_input(n.ctx(), n.handle())}; }

// rdx_toast_desc with strings; the C layer copies them.
struct ToastOptions {
  std::string        title{};  // empty for none
  std::string        text{};
  rdx_toast_position position = RDX_TOAST_BOTTOM_RIGHT;
  rdx_toast_kind     kind     = RDX_TOAST_INFO;
  double             seconds  = 0;  // 0 means 4; negative stays until closed
};

inline rd::Node toast(const rd::Context& ctx, const ToastOptions& o) {
  rdx_toast_desc d{detail::opt(o.title), o.text.c_str(), o.position, o.kind, o.seconds};
  return {ctx.get(), rdx_toast_show(ctx.get(), &d)};
}
inline void close_toast(rd::Node n) { rdx_toast_close(n.ctx(), n.handle()); }

}  // namespace rdx
