#pragma once

#include <raydom/extras/views.h>

#include <functional>
#include <memory>
#include <new>
#include <raydom/raydom.hh>
#include <string>
#include <vector>

// C++ forms of views.h; callbacks are std::function, owned by the view. Each view is a typed handle, an
// rd::Node with its operations as members; the free functions do the same for any rd::Node.
namespace rdx {

class Tabs : public rd::Node {
public:
  using rd::Node::Node;
  Tabs() = default;
  explicit Tabs(rd::Node n) : rd::Node(n) {}
  rd::Node add(const std::string& label) const { return {ctx(), rdx_tabs_add(ctx(), handle(), label.c_str())}; }
  int      selected() const { return rdx_tabs_selected(ctx(), handle()); }
  bool     select(int index) const { return rdx_tabs_select(ctx(), handle(), index); }
};

class Split : public rd::Node {
public:
  using rd::Node::Node;
  Split() = default;
  explicit Split(rd::Node n) : rd::Node(n) {}
  rd::Node pane(int index) const { return {ctx(), rdx_split_pane(ctx(), handle(), index)}; }
  float    fraction() const { return rdx_split_fraction(ctx(), handle()); }
  bool     set_fraction(float f) const { return rdx_split_set_fraction(ctx(), handle(), f); }
  float    size() const { return rdx_split_size(ctx(), handle()); }
  bool     set_size(float px) const { return rdx_split_set_size(ctx(), handle(), px); }
  bool     collapsed() const { return rdx_split_collapsed(ctx(), handle()); }
  bool     set_collapsed(bool c) const { return rdx_split_set_collapsed(ctx(), handle(), c); }
  bool     set_direction(bool vertical, bool reverse = false) const { return rdx_split_set_direction(ctx(), handle(), vertical, reverse); }
};

class List : public rd::Node {
public:
  using rd::Node::Node;
  List() = default;
  explicit List(rd::Node n) : rd::Node(n) {}
  int  selected() const { return rdx_list_selected(ctx(), handle()); }
  bool select(int index) const { return rdx_list_select(ctx(), handle(), index); }
  bool set_count(int count) const { return rdx_list_set_count(ctx(), handle(), count); }
  void refresh() const { rdx_list_refresh(ctx(), handle()); }
  void scroll_to(int index) const { rdx_list_scroll_to(ctx(), handle(), index); }
  int  index_of(rd::Node node) const { return rdx_list_index_of(ctx(), handle(), node.handle()); }
};

class Table : public rd::Node {
public:
  using rd::Node::Node;
  Table() = default;
  explicit Table(rd::Node n) : rd::Node(n) {}
  List list() const { return {ctx(), rdx_table_list(ctx(), handle())}; }
};

class Tree : public List {
public:
  using List::List;
  Tree() = default;
  explicit Tree(rd::Node n) : List(n) {}
  int  add(int parent, const std::string& label) const { return rdx_tree_add(ctx(), handle(), parent, label.c_str()); }
  int  selected_item() const { return rdx_tree_selected(ctx(), handle()); }
  int  item_of(rd::Node node) const { return rdx_tree_item_of(ctx(), handle(), node.handle()); }
  bool expand(int item, bool on = true) const { return rdx_tree_expand(ctx(), handle(), item, on); }
  std::string label(int item) const {
    const char* s = rdx_tree_label(ctx(), handle(), item);
    return s ? s : "";
  }
};

inline Tabs tabs(const rd::Context& ctx) { return {ctx.get(), rdx_tabs_create(ctx.get())}; }
inline rd::Node add_tab(rd::Node tabs, const std::string& label) {
  return {tabs.ctx(), rdx_tabs_add(tabs.ctx(), tabs.handle(), label.c_str())};
}
inline int  selected_tab(rd::Node tabs) { return rdx_tabs_selected(tabs.ctx(), tabs.handle()); }
inline bool select_tab(rd::Node tabs, int i) { return rdx_tabs_select(tabs.ctx(), tabs.handle(), i); }

inline Split split(const rd::Context& ctx, const rdx_split_desc& d = {}) {
  return {ctx.get(), rdx_split_create(ctx.get(), &d)};
}
inline rd::Node pane(rd::Node split, int i) { return {split.ctx(), rdx_split_pane(split.ctx(), split.handle(), i)}; }
inline float split_fraction(rd::Node s) { return rdx_split_fraction(s.ctx(), s.handle()); }
inline bool  set_split_fraction(rd::Node s, float f) { return rdx_split_set_fraction(s.ctx(), s.handle(), f); }
inline float split_size(rd::Node s) { return rdx_split_size(s.ctx(), s.handle()); }
inline bool  set_split_size(rd::Node s, float px) { return rdx_split_set_size(s.ctx(), s.handle(), px); }
inline bool  split_collapsed(rd::Node s) { return rdx_split_collapsed(s.ctx(), s.handle()); }
inline bool  set_split_collapsed(rd::Node s, bool c) { return rdx_split_set_collapsed(s.ctx(), s.handle(), c); }
inline bool  set_split_direction(rd::Node s, bool vertical, bool reverse = false) {
  return rdx_split_set_direction(s.ctx(), s.handle(), vertical, reverse);
}

namespace detail {
template <class Fn>
inline void destroy_fn(void* p) noexcept {
  delete static_cast<Fn*>(p);
}
}  // namespace detail

// fill(row, index) populates a row.
using RowFn = std::function<void(rd::Node, int)>;
inline List list(const rd::Context& ctx, int count, float row_height, RowFn fill) {
  std::unique_ptr<RowFn> fn(new (std::nothrow) RowFn(std::move(fill)));
  if (!fn) return {};
  rdx_list_desc d{count, row_height,
                  [](rd_ctx* c, rd_node row, int index, void* p) noexcept { (*static_cast<RowFn*>(p))(rd::Node(c, row), index); },
                  fn.get(), detail::destroy_fn<RowFn>};
  List          l{ctx.get(), rdx_list_create(ctx.get(), &d)};
  if (l) fn.release();
  return l;
}
inline int  list_selected(rd::Node list) { return rdx_list_selected(list.ctx(), list.handle()); }
inline bool list_select(rd::Node list, int i) { return rdx_list_select(list.ctx(), list.handle(), i); }
inline bool list_set_count(rd::Node list, int n) { return rdx_list_set_count(list.ctx(), list.handle(), n); }
inline void list_refresh(rd::Node list) { rdx_list_refresh(list.ctx(), list.handle()); }
// The row node is in, or -1; for tables pass table_list(table).
inline int  list_index_of(rd::Node list, rd::Node node) { return rdx_list_index_of(list.ctx(), list.handle(), node.handle()); }

struct Column {
  std::string title{};
  rd_val      width{};  // px or fr; unset means 1fr
};

struct TableFns {
  std::function<void(rd::Node cell, int row, int column)> cell{};
  std::function<void(int column)>                         header{};
};

inline Table table(const rd::Context& ctx, const std::vector<Column>& columns, int count, float row_height, TableFns fns) {
  std::unique_ptr<TableFns> fn(new (std::nothrow) TableFns(std::move(fns)));
  if (!fn) return {};
  std::vector<rdx_column> cols;
  for (const Column& c : columns) cols.push_back({c.title.c_str(), c.width});
  rdx_table_desc d{cols.data(),
                   static_cast<int>(cols.size()),
                   count,
                   row_height,
                   [](rd_ctx* c, rd_node cell, int row, int col, void* p) noexcept {
                     auto* f = static_cast<TableFns*>(p);
                     if (f->cell) f->cell(rd::Node(c, cell), row, col);
                   },
                   [](rd_ctx*, int col, void* p) noexcept {
                     auto* f = static_cast<TableFns*>(p);
                     if (f->header) f->header(col);
                   },
                   fn.get(),
                   detail::destroy_fn<TableFns>};
  Table t{ctx.get(), rdx_table_create(ctx.get(), &d)};
  if (t) fn.release();
  return t;
}
inline rd::Node table_list(rd::Node table) { return {table.ctx(), rdx_table_list(table.ctx(), table.handle())}; }

inline Tree tree(const rd::Context& ctx, float row_height = 0) { return {ctx.get(), rdx_tree_create(ctx.get(), row_height)}; }
inline int  tree_add(rd::Node tree, int parent, const std::string& label) {
  return rdx_tree_add(tree.ctx(), tree.handle(), parent, label.c_str());
}
inline int  tree_selected(rd::Node tree) { return rdx_tree_selected(tree.ctx(), tree.handle()); }
inline int  tree_item_of(rd::Node tree, rd::Node node) { return rdx_tree_item_of(tree.ctx(), tree.handle(), node.handle()); }
inline bool tree_expand(rd::Node tree, int item, bool on = true) { return rdx_tree_expand(tree.ctx(), tree.handle(), item, on); }
inline std::string tree_label(rd::Node tree, int item) {
  const char* s = rdx_tree_label(tree.ctx(), tree.handle(), item);
  return s ? s : "";
}

}  // namespace rdx
