#pragma once

#include <raydom/extras/popups.h>

#include <functional>
#include <memory>
#include <new>
#include <raydom/raydom.hh>
#include <string>
#include <vector>

// C++ forms of popups.h. Callbacks are std::function, owned by the popup or the node they're attached to.
namespace rdx {

struct MenuItem {
  std::string label{};  // empty for a separator
  std::string shortcut{};
  bool        disabled = false;
};

namespace detail {
using MenuFn = std::function<void(int)>;

inline std::vector<rdx_menu_item> c_items(const std::vector<MenuItem>& items) {
  std::vector<rdx_menu_item> out;
  out.reserve(items.size());
  for (const MenuItem& i : items)
    out.push_back({i.label.empty() ? nullptr : i.label.c_str(), i.shortcut.empty() ? nullptr : i.shortcut.c_str(),
                   i.disabled});
  return out;
}
inline void call_menu(rd_ctx*, int index, void* data) noexcept { (*static_cast<MenuFn*>(data))(index); }
inline void free_menu(void* data) noexcept { delete static_cast<MenuFn*>(data); }
}  // namespace detail

// f(index) runs with the index of the chosen item.
inline rd::Node menu(const rd::Context& ctx, const std::vector<MenuItem>& items, const rd_anchor& anchor,
                     detail::MenuFn f) {
  std::unique_ptr<detail::MenuFn> fn(new (std::nothrow) detail::MenuFn(std::move(f)));  // the menu's on success
  if (!fn) return {};
  auto          c = detail::c_items(items);
  rdx_menu_desc d{c.data(), c.size(), anchor, detail::call_menu, fn.get(), detail::free_menu};
  rd::Node      m{ctx.get(), rdx_menu_open(ctx.get(), &d)};
  if (m) fn.release();
  return m;
}

inline bool context_menu(rd::Node target, const std::vector<MenuItem>& items, detail::MenuFn f) {
  std::unique_ptr<detail::MenuFn> fn(new (std::nothrow) detail::MenuFn(std::move(f)));
  if (!fn) return false;
  auto c  = detail::c_items(items);
  bool ok = rdx_context_menu_attach(target.ctx(), target.handle(), c.data(), c.size(), detail::call_menu, fn.get(),
                                    detail::free_menu);
  if (ok) fn.release();
  return ok;
}

class Select : public rd::Node {
public:
  using rd::Node::Node;
  Select() = default;
  explicit Select(rd::Node n) : rd::Node(n) {}
  int  selected() const { return rdx_select_selected(ctx(), handle()); }
  bool set_selected(int index) const { return rdx_select_set(ctx(), handle(), index); }
};

inline Select select(const rd::Context& ctx, const std::vector<std::string>& options, const std::string& placeholder = {}) {
  std::vector<const char*> c;
  for (const std::string& o : options) c.push_back(o.c_str());
  return {ctx.get(), rdx_select_create(ctx.get(), c.data(), c.size(), placeholder.empty() ? nullptr : placeholder.c_str())};
}
inline int  selected(rd::Node select) { return rdx_select_selected(select.ctx(), select.handle()); }
inline bool set_selected(rd::Node select, int index) { return rdx_select_set(select.ctx(), select.handle(), index); }

struct ModalOptions {
  std::string           title{};
  float                 width      = 0;
  bool                  persistent = false;
  std::function<void()> on_close{};
};

// Returns the modal's body.
inline rd::Node modal(const rd::Context& ctx, ModalOptions o) {
  using Fn = std::function<void()>;
  std::unique_ptr<Fn> fn(o.on_close ? new (std::nothrow) Fn(std::move(o.on_close)) : nullptr);
  rdx_modal_desc      d{o.title.empty() ? nullptr : o.title.c_str(), o.width, o.persistent, nullptr, fn.get(), nullptr};
  if (fn) {
    d.on_close = [](rd_ctx*, void* p) noexcept { (*static_cast<Fn*>(p))(); };
    d.destroy  = [](void* p) noexcept { delete static_cast<Fn*>(p); };
  }
  rd::Node body{ctx.get(), rdx_modal_open(ctx.get(), &d)};
  if (body) fn.release();
  return body;
}

inline bool close_popup(rd::Node n) { return rdx_popup_close(n.ctx(), n.handle()); }
inline bool tooltip(rd::Node n, const std::string& text) { return rdx_tooltip_attach(n.ctx(), n.handle(), text.c_str()); }

}  // namespace rdx
