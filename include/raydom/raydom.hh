#pragma once

#include <raydom/raydom.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <new>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// Negation for offsets such as -5_px. Global so lookup finds it for the C type.
constexpr rd_val operator-(rd_val v) { return {-v.value, v.unit}; }

namespace rd {

inline rd_val   px(float v) { return rd_px(v); }
inline rd_val   pct(float v) { return rd_pct(v); }
inline rd_val   num(float v) { return rd_num(v); }
inline rd_color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return rd_rgb(r, g, b); }
inline rd_color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) { return rd_rgba(r, g, b, a); }
inline rd_color hex(std::uint32_t rgb) { return rd_hex(rgb); }

inline rd_val fr(float v) { return rd_fr(v); }

inline constexpr rd_val auto_       = {0, RD_UNIT_AUTO};
inline constexpr rd_val min_content = {0, RD_UNIT_MIN_CONTENT};
inline constexpr rd_val max_content = {0, RD_UNIT_MAX_CONTENT};

inline rd_grid_line line(std::int16_t n) { return rd_line(n); }
inline rd_grid_line span(std::int16_t n) { return rd_span(n); }

// A track is any rd_val (fr expands to minmax(auto, fr)) or an explicit minmax.
struct Track {
  rd_track t;
  Track(rd_val v) : t(rd_track_of(v)) {}
  Track(rd_track tr) : t(tr) {}
};

inline Track minmax(rd_val min, rd_val max) { return Track(rd_track_minmax(min, max)); }

inline std::vector<Track> repeat(int count, std::initializer_list<Track> tracks) {
  std::vector<Track> out;
  out.reserve(static_cast<std::size_t>(count) * tracks.size());
  for (int j = 0; j < count; j++) out.insert(out.end(), tracks.begin(), tracks.end());
  return out;
}

namespace literals {
constexpr rd_val operator""_px(long double v) { return {static_cast<float>(v), RD_UNIT_PX}; }
constexpr rd_val operator""_px(unsigned long long v) { return {static_cast<float>(v), RD_UNIT_PX}; }
constexpr rd_val operator""_pct(long double v) { return {static_cast<float>(v), RD_UNIT_PCT}; }
constexpr rd_val operator""_pct(unsigned long long v) { return {static_cast<float>(v), RD_UNIT_PCT}; }
constexpr rd_val operator""_fr(long double v) { return {static_cast<float>(v), RD_UNIT_FR}; }
constexpr rd_val operator""_fr(unsigned long long v) { return {static_cast<float>(v), RD_UNIT_FR}; }
// Durations, for transition_duration and transition_delay: 150_ms, 0.2_s.
constexpr rd_val operator""_ms(long double v) { return {static_cast<float>(v / 1000), RD_UNIT_NUM}; }
constexpr rd_val operator""_ms(unsigned long long v) { return {static_cast<float>(v) / 1000, RD_UNIT_NUM}; }
constexpr rd_val operator""_s(long double v) { return {static_cast<float>(v), RD_UNIT_NUM}; }
constexpr rd_val operator""_s(unsigned long long v) { return {static_cast<float>(v), RD_UNIT_NUM}; }
}  // namespace literals

class Style {
public:
  Style() = default;
  Style(const rd_style& s) : s_(s) {}

#define RD_HH_SETTER_ENUM(name, type) \
  Style& name(type v) {               \
    s_.name = v;                      \
    return *this;                     \
  }
#define RD_HH_SETTER_VAL(name, type)    RD_HH_SETTER_ENUM(name, type)
#define RD_HH_SETTER_COLOR(name, type)  RD_HH_SETTER_ENUM(name, type)
#define RD_HH_SETTER_TRACKS(name, type) RD_HH_SETTER_ENUM(name, type)
#define RD_HH_SETTER_FONT(name, type)   RD_HH_SETTER_ENUM(name, type)
#define RD_HH_SETTER_SHADOW(name, type)                                     \
  RD_HH_SETTER_ENUM(name, type)                                             \
  Style& name(float x, float y, float blur, rd_color c, float spread = 0) { \
    s_.name = {rd_px(x), rd_px(y), rd_px(blur), rd_px(spread), c};          \
    return *this;                                                           \
  }
#define RD_HH_SETTER_GRID_LINE(name, type) RD_HH_SETTER_ENUM(name, type)
#define RD_HH_SETTER_TRACK(name, type) \
  RD_HH_SETTER_ENUM(name, type)        \
  Style& name(rd_val v) {              \
    s_.name = rd_track_of(v);          \
    return *this;                      \
  }
#define RD_HH_SETTER_EDGES(name, type)                  \
  RD_HH_SETTER_ENUM(name, type)                         \
  Style& name(rd_val all) {                             \
    s_.name = rd_edges_all(all);                        \
    return *this;                                       \
  }                                                     \
  Style& name(rd_val vertical, rd_val horizontal) {     \
    s_.name = rd_edges_vh(vertical, horizontal);        \
    return *this;                                       \
  }                                                     \
  Style& name(rd_val t, rd_val r, rd_val b, rd_val l) { \
    s_.name = {t, r, b, l};                             \
    return *this;                                       \
  }
#define RD_HH_SETTER_CORNERS(name, type) \
  RD_HH_SETTER_ENUM(name, type)          \
  Style& name(rd_val all) {              \
    s_.name = rd_corners_all(all);       \
    return *this;                        \
  }
#define RD_HH_SETTER(name, type, kind, inherited, layout) RD_HH_SETTER_##kind(name, type)
  RD_STYLE_PROPS(RD_HH_SETTER)
#undef RD_HH_SETTER
#undef RD_HH_SETTER_ENUM
#undef RD_HH_SETTER_VAL
#undef RD_HH_SETTER_COLOR
#undef RD_HH_SETTER_TRACKS
#undef RD_HH_SETTER_FONT
#undef RD_HH_SETTER_SHADOW
#undef RD_HH_SETTER_GRID_LINE
#undef RD_HH_SETTER_TRACK
#undef RD_HH_SETTER_EDGES
#undef RD_HH_SETTER_CORNERS

  Style& gap(rd_val both) {
    s_.row_gap = s_.column_gap = both;
    return *this;
  }

  // CSS `flex: <grow>`: grow, shrink 1, basis 0.
  Style& flex(float grow) {
    s_.flex_grow   = rd_num(grow);
    s_.flex_shrink = rd_num(1);
    s_.flex_basis  = rd_px(0);
    return *this;
  }

  Style& grid_column(rd_grid_line start, rd_grid_line end = {0, RD_GRID_LINE_AUTO}) {
    s_.grid_column_start = start;
    s_.grid_column_end   = end;
    return *this;
  }
  Style& grid_row(rd_grid_line start, rd_grid_line end = {0, RD_GRID_LINE_AUTO}) {
    s_.grid_row_start = start;
    s_.grid_row_end   = end;
    return *this;
  }

  Style& merge(const Style& o) {
    rd_style_merge(&s_, &o.s_);
    return *this;
  }

  const rd_style& get() const { return s_; }

private:
  rd_style s_{};
};

class Class {
public:
  Class() = default;
  Class(rd_ctx* ctx, rd_class handle) : ctx_(ctx), handle_(handle) {}

  rd_class handle() const { return handle_; }
  explicit operator bool() const { return handle_.id != 0; }

  Class& set(const Style& base) {
    rd_class_set(ctx_, handle_, &base.get());
    return *this;
  }
  Class& on(rd_state state, const Style& style) {
    rd_class_set_state(ctx_, handle_, state, &style.get());
    return *this;
  }

private:
  rd_ctx*  ctx_    = nullptr;
  rd_class handle_ = {};
};

class Node;

// A view of an event during dispatch. Valid only inside the listener.
class Event {
public:
  Event(rd_ctx* ctx, rd_event* ev) : ctx_(ctx), ev_(ev) {}

  rd_event_type  type() const { return ev_->type; }
  rd_event_phase phase() const { return ev_->phase; }
  Node           target() const;
  Node           current() const;
  float          x() const { return ev_->x; }
  float          y() const { return ev_->y; }
  float          local_x() const { return ev_->local_x; }
  float          local_y() const { return ev_->local_y; }
  float          wheel_x() const { return ev_->wheel_x; }
  float          wheel_y() const { return ev_->wheel_y; }
  std::uint8_t   button() const { return ev_->button; }
  std::uint8_t   buttons() const { return ev_->buttons; }
  rd_key         key() const { return ev_->key; }
  std::uint8_t   mods() const { return ev_->mods; }
  bool           repeat() const { return ev_->repeat; }
  std::uint32_t  codepoint() const { return ev_->codepoint; }
  Node           related() const;

  void stop_propagation() { rd_event_stop_propagation(ev_); }
  void stop_immediate_propagation() { rd_event_stop_immediate_propagation(ev_); }
  void prevent_default() { rd_event_prevent_default(ev_); }
  bool default_prevented() const { return ev_->default_prevented; }

  rd_event* get() const { return ev_; }

private:
  rd_ctx*   ctx_;
  rd_event* ev_;
};

class Node {
public:
  Node() = default;
  Node(rd_ctx* ctx, rd_node handle) : ctx_(ctx), handle_(handle) {}

  rd_ctx*  ctx() const { return ctx_; }
  rd_node  handle() const { return handle_; }
  bool     alive() const { return ctx_ && rd_node_alive(ctx_, handle_); }
  explicit operator bool() const { return alive(); }

  Node          parent() const { return wrap(rd_node_parent(ctx_, handle_)); }
  Node          first_child() const { return wrap(rd_node_first_child(ctx_, handle_)); }
  Node          last_child() const { return wrap(rd_node_last_child(ctx_, handle_)); }
  Node          next_sibling() const { return wrap(rd_node_next_sibling(ctx_, handle_)); }
  Node          prev_sibling() const { return wrap(rd_node_prev_sibling(ctx_, handle_)); }
  std::uint32_t child_count() const { return rd_node_child_count(ctx_, handle_); }

  Node& append(Node child) {
    [[maybe_unused]] bool ok = rd_node_append(ctx_, handle_, child.handle_);
    assert(ok);
    return *this;
  }

  Node& insert_before(Node child, Node ref) {
    [[maybe_unused]] bool ok = rd_node_insert_before(ctx_, handle_, child.handle_, ref.handle_);
    assert(ok);
    return *this;
  }

  Node& style(const Style& s) {
    rd_node_set_style(ctx_, handle_, &s.get());
    return *this;
  }
  Node& merge_style(const Style& s) {
    rd_node_merge_style(ctx_, handle_, &s.get());
    return *this;
  }
  Node& clear_style() {
    rd_node_set_style(ctx_, handle_, nullptr);
    return *this;
  }
  Node& add_class(Class c) {
    rd_node_add_class(ctx_, handle_, c.handle());
    return *this;
  }
  Node& remove_class(Class c) {
    rd_node_remove_class(ctx_, handle_, c.handle());
    return *this;
  }
  bool  has_class(Class c) const { return rd_node_has_class(ctx_, handle_, c.handle()); }
  Node& set_state(rd_state s, bool on) {
    rd_node_set_states(ctx_, handle_, s, on);
    return *this;
  }
  std::uint8_t states() const { return rd_node_states(ctx_, handle_); }

  // Null for a dead node. Valid until the next node creation.
  const rd_style* computed() const { return rd_node_computed_style(ctx_, handle_); }

  rd_rect rect() const { return rd_node_rect(ctx_, handle_); }
  rd_rect bounds() const { return rd_node_bounds(ctx_, handle_); }
  rd_box  box() const { return rd_node_box(ctx_, handle_); }
  rd_size content_size() const { return rd_node_content_size(ctx_, handle_); }
  float   baseline() const { return rd_node_baseline(ctx_, handle_); }
  rd_vec2 scroll() const { return rd_node_scroll(ctx_, handle_); }
  Node&   scroll_to(float x, float y) {
    rd_node_set_scroll(ctx_, handle_, x, y);
    return *this;
  }

  Node& text(std::string_view s) {
    rd_node_set_text(ctx_, handle_, s.data(), s.size());
    return *this;
  }
  // Ranges are byte offsets into the text; an empty list clears them.
  Node& highlights(const std::vector<rd_highlight>& ranges) {
    rd_node_set_highlights(ctx_, handle_, ranges.data(), ranges.size());
    return *this;
  }

  // Empty for a node that isn't a text leaf.
  std::string_view text() const {
    std::size_t len = 0;
    const char* p   = rd_node_text(ctx_, handle_, &len);
    return p ? std::string_view(p, len) : std::string_view();
  }

  // f(float width, rd_measure_mode, float height, rd_measure_mode) -> rd_size
  template <class F>
  Node& measure(F&& f) {
    using Fn = std::decay_t<F>;
    Fn* p    = new (std::nothrow) Fn(std::forward<F>(f));
    if (!p) return *this;
    rd_measure_fn call = [](void* d, float w, rd_measure_mode wm, float h, rd_measure_mode hm) -> rd_size {
      return (*static_cast<Fn*>(d))(w, wm, h, hm);
    };
    if (!rd_node_set_measure(ctx_, handle_, call, p, [](void* d) { delete static_cast<Fn*>(d); })) delete p;
    return *this;
  }
  Node& clear_measure() {
    rd_node_set_measure(ctx_, handle_, nullptr, nullptr, nullptr);
    return *this;
  }
  // f(const rd_paint_info&), called during draw. See rd_node_set_paint.
  template <class F>
  Node& paint(F&& f) {
    using Fn = std::decay_t<F>;
    Fn* p    = new (std::nothrow) Fn(std::forward<F>(f));
    if (!p) return *this;
    rd_paint_fn call = [](rd_ctx*, const rd_paint_info* info, void* d) { (*static_cast<Fn*>(d))(*info); };
    if (!rd_node_set_paint(ctx_, handle_, call, p, [](void* d) { delete static_cast<Fn*>(d); })) delete p;
    return *this;
  }

  Node& mark_layout_dirty() {
    rd_node_mark_layout_dirty(ctx_, handle_);
    return *this;
  }

  // f(Event&) or f(). Null listener on failure.
  template <class F>
  rd_listener on(rd_event_type type, F&& f, bool capture = false) {
    using Fn = std::decay_t<F>;
    Fn* p    = new (std::nothrow) Fn(std::forward<F>(f));
    if (!p) return {};
    rd_event_fn call = [](rd_ctx* c, rd_event* e, void* d) {
      auto& fn = *static_cast<Fn*>(d);
      if constexpr (std::is_invocable_v<Fn&, Event&>) {
        Event ev(c, e);
        fn(ev);
      } else {
        fn();
      }
    };
    rd_listener l = rd_node_on(ctx_, handle_, type, call, p, [](void* d) { delete static_cast<Fn*>(d); }, capture ? RD_LISTEN_CAPTURE : 0);
    if (!l.id) delete p;
    return l;
  }
  template <class F>
  Node& on_click(F&& f) {
    on(RD_EVENT_CLICK, std::forward<F>(f));
    return *this;
  }
  bool off(rd_listener l) const { return rd_node_off(ctx_, l); }

  Node& capture_pointer() {
    rd_node_set_pointer_capture(ctx_, handle_);
    return *this;
  }
  Node& release_pointer() {
    rd_node_release_pointer_capture(ctx_, handle_);
    return *this;
  }

  Node& make_input(bool multiline = false) {
    rd_node_make_input(ctx_, handle_, multiline ? RD_INPUT_MULTILINE : 0);
    return *this;
  }
  bool  is_input() const { return rd_node_is_input(ctx_, handle_); }
  Node& placeholder(std::string_view s) {
    rd_node_set_placeholder(ctx_, handle_, s.data(), s.size());
    return *this;
  }
  Node& select(std::size_t anchor, std::size_t caret) {
    rd_input_select(ctx_, handle_, anchor, caret);
    return *this;
  }
  // Null clears it. See rd_node_set_anchor.
  Node& anchor(const rd_anchor& a) {
    rd_node_set_anchor(ctx_, handle_, &a);
    return *this;
  }
  Node& clear_anchor() {
    rd_node_set_anchor(ctx_, handle_, nullptr);
    return *this;
  }
  rd_side anchor_side() const { return rd_node_anchor_side(ctx_, handle_); }
  // True if other is this node or one of its descendants.
  bool contains(Node other) const {
    for (; other; other = other.parent())
      if (other == *this) return true;
    return false;
  }

  // Null image clears it.
  Node& image(rd_image img) {
    rd_node_set_image(ctx_, handle_, img);
    return *this;
  }
  rd_image image() const { return rd_node_image(ctx_, handle_); }

  // Class variants also apply for the parent's states. See rd_node_set_share_states.
  Node& share_states(bool share = true) {
    rd_node_set_share_states(ctx_, handle_, share);
    return *this;
  }
  // Fires an event at the node; false if a listener prevented the default.
  bool dispatch(rd_event_type type) { return rd_node_dispatch(ctx_, handle_, type); }

  // Animates to `to` over seconds; see rd_node_animate. done(), if given, runs afterward and is owned
  // by the timer.
  Node& animate(const Style& to, double seconds, rd_easing easing = RD_EASING_EASE) {
    rd_node_animate(ctx_, handle_, nullptr, &to.get(), seconds, easing, nullptr, nullptr);
    return *this;
  }
  Node& animate(const Style& from, const Style& to, double seconds, rd_easing easing = RD_EASING_EASE) {
    rd_node_animate(ctx_, handle_, &from.get(), &to.get(), seconds, easing, nullptr, nullptr);
    return *this;
  }
  template <class F>
  Node& animate(const Style& to, double seconds, rd_easing easing, F&& done) {
    using Fn = std::decay_t<F>;
    Fn* p    = new (std::nothrow) Fn(std::forward<F>(done));
    if (!p) return animate(to, seconds, easing);
    rd_timer_fn call = [](rd_ctx*, void* d) { (*static_cast<Fn*>(d))(); };
    rd_destroy_fn del = [](void* d) { delete static_cast<Fn*>(d); };
    rd_node_animate(ctx_, handle_, nullptr, &to.get(), seconds, easing, nullptr, nullptr);
    if (!rd_ctx_set_timer(ctx_, seconds, 0, call, p, del).index) delete p;
    return *this;
  }

  Node& role(rd_role r) {
    rd_node_set_role(ctx_, handle_, r);
    return *this;
  }
  rd_role role() const { return rd_node_role(ctx_, handle_); }

  // False if the node can't take focus.
  bool focus() { return rd_node_focus(ctx_, handle_); }
  bool focused() const { return alive() && rd_node_eq(rd_ctx_focused(ctx_), handle_); }

  void detach() { rd_node_detach(ctx_, handle_); }
  void destroy() { rd_node_destroy(ctx_, handle_); }

  // Null on allocation failure or dead node.
  template <class T, class... Args>
  T* emplace_userdata(Args&&... args) {
    T* p = new (std::nothrow) T(std::forward<Args>(args)...);
    if (p && !rd_node_set_userdata(ctx_, handle_, p, [](void* d) { delete static_cast<T*>(d); })) {
      delete p;
      return nullptr;
    }
    return p;
  }

  template <class T>
  T* userdata() const { return static_cast<T*>(rd_node_userdata(ctx_, handle_)); }

  struct Children;
  // Not stable under mutation of the iterated sibling list.
  Children children() const;

  bool operator==(const Node& o) const { return ctx_ == o.ctx_ && rd_node_eq(handle_, o.handle_); }

private:
  Node wrap(rd_node n) const { return {ctx_, n}; }

  rd_ctx* ctx_    = nullptr;
  rd_node handle_ = {};
};

class ChildIterator {
public:
  using value_type      = Node;
  using difference_type = std::ptrdiff_t;

  ChildIterator() = default;
  explicit ChildIterator(Node n) : n_(n) {}

  Node           operator*() const { return n_; }
  ChildIterator& operator++() {
    n_ = n_.next_sibling();
    return *this;
  }
  ChildIterator operator++(int) {
    auto t = *this;
    ++*this;
    return t;
  }
  bool operator==(std::default_sentinel_t) const { return rd_node_is_null(n_.handle()); }

private:
  Node n_;
};

struct Node::Children {
  Node                    first;
  ChildIterator           begin() const { return ChildIterator(first); }
  std::default_sentinel_t end() const { return {}; }
};

inline Node::Children Node::children() const { return {first_child()}; }

inline Node Event::target() const { return {ctx_, ev_->target}; }
inline Node Event::current() const { return {ctx_, ev_->current}; }
inline Node Event::related() const { return {ctx_, ev_->related}; }

class Context {
public:
  // Check operator bool for allocation failure.
  Context() : ctx_(rd_ctx_create()) {}
  ~Context() {
    if (ctx_) rd_ctx_destroy(ctx_);
  }
  Context(Context&& o) noexcept : ctx_(std::exchange(o.ctx_, nullptr)) {}
  Context& operator=(Context&& o) noexcept {
    if (this != &o) {
      if (ctx_) rd_ctx_destroy(ctx_);
      ctx_ = std::exchange(o.ctx_, nullptr);
    }
    return *this;
  }
  Context(const Context&)            = delete;
  Context& operator=(const Context&) = delete;

  explicit operator bool() const { return ctx_ != nullptr; }

  rd_ctx*       get() const { return ctx_; }
  Node          root() const { return {ctx_, rd_ctx_root(ctx_)}; }
  // The layer above the tree for popups; created on first use.
  Node          overlay() const { return {ctx_, rd_ctx_overlay(ctx_)}; }
  std::uint32_t node_count() const { return rd_ctx_node_count(ctx_); }

  // Null node on allocation failure.
  Node create() const { return {ctx_, rd_node_create(ctx_)}; }
  Node create(const Style& s) const { return create().style(s); }
  Node create_text(std::string_view s, const Style& style = {}) const { return create(style).text(s); }
  // A focusable node with the button role; with text, a text leaf, otherwise empty for children.
  Node create_button(std::string_view label, const Style& style = {}) const {
    return create_text(label, style).role(RD_ROLE_BUTTON);
  }
  Node create_button(const Style& style = {}) const { return create(style).role(RD_ROLE_BUTTON); }
  Node create_input(bool multiline = false, const Style& style = {}) const {
    return create(style).make_input(multiline);
  }

  // Null font on failure. Without copy, data must outlive the context.
  rd_font load_font(const void* data, std::size_t size, bool copy = true) const {
    return rd_font_load(ctx_, data, size, copy);
  }
  rd_font load_font_file(const char* path) const { return rd_font_load_file(ctx_, path); }

  // Null image on failure. See image.h.
  rd_image load_image(const void* data, std::size_t size) const { return rd_image_load(ctx_, data, size); }
  rd_image load_image_file(const char* path) const { return rd_image_load_file(ctx_, path); }
  rd_image image_from_pixels(const void* rgba, int width, int height) const {
    return rd_image_from_pixels(ctx_, rgba, width, height);
  }
  bool    update_image(rd_image img, const void* rgba, int width, int height) const {
    return rd_image_update(ctx_, img, rgba, width, height);
  }
  void    unload_image(rd_image img) const { rd_image_unload(ctx_, img); }
  rd_size image_size(rd_image img) const { return rd_image_size(ctx_, img); }
  Node    create_image(rd_image img, const Style& style = {}) const { return create(style).image(img).role(RD_ROLE_IMAGE); }
  void    set_default_font(rd_font f) const { rd_ctx_set_default_font(ctx_, f); }
  float   measure_text(std::string_view s, float size, rd_font f = {0, true}) const {
    return rd_font_measure(ctx_, f, size, s.data(), s.size());
  }

  // Null class on allocation failure.
  Class create_class(const Style& base = {}) const { return {ctx_, rd_class_create(ctx_, &base.get())}; }

  // Interns a track list. tracks_none() on allocation failure.
  rd_tracks tracks(std::initializer_list<Track> list) const { return tracks(list.begin(), list.size()); }
  rd_tracks tracks(const std::vector<Track>& list) const { return tracks(list.data(), list.size()); }

  void compute_styles() const { rd_ctx_compute_styles(ctx_); }
  void layout(float width, float height) const { rd_ctx_layout(ctx_, width, height); }

  // f() after delay seconds, or every delay seconds. The callable is owned by the timer. Null timer
  // on allocation failure.
  template <class F>
  rd_timer set_timeout(double delay, F&& f) const {
    return set_timer(delay, 0, std::forward<F>(f));
  }
  template <class F>
  rd_timer set_interval(double delay, F&& f) const {
    return set_timer(delay, RD_TIMER_REPEAT, std::forward<F>(f));
  }
  bool clear_timer(rd_timer t) const { return rd_ctx_clear_timer(ctx_, t); }
  // The clock timers run on; rd_update sets it from raylib.
  void set_time(double seconds) const { rd_ctx_set_time(ctx_, seconds); }

  Node      hovered() const { return {ctx_, rd_ctx_hovered(ctx_)}; }
  rd_cursor cursor() const { return rd_ctx_cursor(ctx_); }
  Node pointer_capture() const { return {ctx_, rd_ctx_pointer_capture(ctx_)}; }
  Node focused() const { return {ctx_, rd_ctx_focused(ctx_)}; }
  void blur() const { rd_node_focus(ctx_, {}); }
  bool focus_next(bool backward = false) const { return rd_ctx_focus_next(ctx_, backward); }
  void key(rd_key k, bool down, std::uint8_t mods = 0, bool repeat = false) const {
    rd_ctx_key(ctx_, k, down, repeat, mods);
  }
  // A key press and release.
  void press(rd_key k, std::uint8_t mods = 0) const {
    key(k, true, mods);
    key(k, false, mods);
  }
  // Text input, one event per codepoint.
  void type(std::string_view utf8) const {
    for (std::size_t i = 0; i < utf8.size();) {
      std::uint32_t c = static_cast<unsigned char>(utf8[i]);
      int           n = c < 0x80 ? 1 : c < 0xE0 ? 2
                                   : c < 0xF0   ? 3
                                                : 4;
      if (n > 1) c &= 0x3Fu >> (n - 1);
      for (int j = 1; j < n && i + static_cast<std::size_t>(j) < utf8.size(); j++)
        c = (c << 6) | (static_cast<unsigned char>(utf8[i + static_cast<std::size_t>(j)]) & 0x3Fu);
      rd_ctx_text(ctx_, c);
      i += static_cast<std::size_t>(n);
    }
  }
  Node node_at(float x, float y) const { return {ctx_, rd_node_at(ctx_, x, y)}; }
  void pointer(float x, float y, std::uint8_t buttons, float wheel_x = 0, float wheel_y = 0) const {
    rd_ctx_pointer(ctx_, x, y, buttons, wheel_x, wheel_y);
  }
  // raylib: input and layout at the screen size, then drawing inside BeginDrawing/EndDrawing.
  void update() const { rd_update(ctx_); }
  void draw() const { rd_draw(ctx_); }

private:
  template <class F>
  rd_timer set_timer(double delay, std::uint32_t flags, F&& f) const {
    using Fn = std::decay_t<F>;
    Fn* p    = new (std::nothrow) Fn(std::forward<F>(f));
    if (!p) return {};
    rd_timer t = rd_ctx_set_timer(
        ctx_, delay, flags, [](rd_ctx*, void* d) { (*static_cast<Fn*>(d))(); }, p,
        [](void* d) { delete static_cast<Fn*>(d); });
    if (!t.index) delete p;
    return t;
  }

  rd_tracks tracks(const Track* t, std::size_t n) const {
    static_assert(sizeof(Track) == sizeof(rd_track));
    rd_tracks r = rd_tracks_create(ctx_, reinterpret_cast<const rd_track*>(t), static_cast<std::uint32_t>(n));
    return r.id ? r : rd_tracks_none();
  }

  rd_ctx* ctx_;
};

}  // namespace rd
