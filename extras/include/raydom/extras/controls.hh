#pragma once

#include <raydom/extras/controls.h>
#include <raydom/extras/toast.h>

#include <raydom/raydom.hh>
#include <string>

// C++ forms of controls.h and toast.h: the controls are plain rd::Node values; listen for
// RD_EVENT_CHANGE and RD_EVENT_INPUT with node.on().
namespace rdx {

namespace detail {
inline const char* opt(const std::string& s) { return s.empty() ? nullptr : s.c_str(); }
}  // namespace detail

inline rd::Node checkbox(const rd::Context& ctx, const std::string& label = {}) {
  return {ctx.get(), rdx_checkbox_create(ctx.get(), detail::opt(label))};
}
inline rd::Node toggle_switch(const rd::Context& ctx, const std::string& label = {}) {
  return {ctx.get(), rdx_switch_create(ctx.get(), detail::opt(label))};
}
inline bool checked(rd::Node n) { return rdx_checked(n.ctx(), n.handle()); }
inline bool set_checked(rd::Node n, bool on) { return rdx_set_checked(n.ctx(), n.handle(), on); }

inline rd::Node radio_group(const rd::Context& ctx) { return {ctx.get(), rdx_radio_group_create(ctx.get())}; }
inline rd::Node add_radio(rd::Node group, const std::string& label) {
  return {group.ctx(), rdx_radio_add(group.ctx(), group.handle(), detail::opt(label))};
}
inline int  radio_selected(rd::Node group) { return rdx_radio_selected(group.ctx(), group.handle()); }
inline bool radio_select(rd::Node group, int index) { return rdx_radio_select(group.ctx(), group.handle(), index); }

inline rd::Node slider(const rd::Context& ctx, const rdx_slider_desc& desc = {}) {
  return {ctx.get(), rdx_slider_create(ctx.get(), &desc)};
}
inline float slider_value(rd::Node n) { return rdx_slider_value(n.ctx(), n.handle()); }
inline bool  set_slider_value(rd::Node n, float v) { return rdx_slider_set_value(n.ctx(), n.handle(), v); }

inline rd::Node toast(const rd::Context& ctx, const rdx_toast_desc& desc) {
  return {ctx.get(), rdx_toast_show(ctx.get(), &desc)};
}
inline void close_toast(rd::Node n) { rdx_toast_close(n.ctx(), n.handle()); }

}  // namespace rdx
