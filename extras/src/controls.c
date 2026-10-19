#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "extras_internal.h"

typedef enum kind : uint8_t { ANY, CHECKBOX, SWITCH, RADIO_GROUP, RADIO, SLIDER, NUMBER } kind;

typedef struct control {
  rd_node node;
  kind    kind;
  bool    checked;
  rd_node group;  // RADIO: its group; RADIO_GROUP: unused
  rd_node rail, fill, thumb;  // SLIDER
  float   min, max, value, step;
  float   drag_start;  // SLIDER: value when the drag began; NUMBER: when a stepper was pressed
  rd_node input;       // NUMBER: the text field
  int     decimals;    // NUMBER
  rd_timer repeat;     // NUMBER: a held stepper's
} control;

// Controls by node slot index, so lookups are O(1) and the nodes' userdata stays the application's.
typedef struct registry {
  control** items;
  uint32_t  cap;
} registry;

static const char key = 0;

static void registry_free(void* data) {
  registry* r = data;
  free((void*)r->items);
  free(r);
}

static registry* registry_get(rd_ctx* ctx) {
  registry* r = rd_ctx_extension(ctx, &key);
  if (r || !ctx) return r;
  r = calloc(1, sizeof *r);
  if (r && !rd_ctx_set_extension(ctx, &key, r, registry_free)) {
    free(r);
    r = NULL;
  }
  return r;
}

static control* find(const rd_ctx* ctx, rd_node n, kind k) {
  const registry* r = rd_ctx_extension(ctx, &key);
  if (!r || !rd_node_alive(ctx, n) || n.index >= r->cap) return NULL;
  control* c = r->items[n.index];
  return c && rd_node_eq(c->node, n) && (k == ANY || c->kind == k) ? c : NULL;
}

// A control lives as long as its node: it's the data of the control's main listener, which frees it.
static void control_free(void* data) { free(data); }

// Registers the control for n, with fn as the listener that owns it. Destroys n on failure.
static control* attach(rd_ctx* ctx, rd_node n, kind k, rd_event_type type, rd_event_fn fn) {
  registry* r = rd_node_is_null(n) ? NULL : registry_get(ctx);
  control*  c = r ? calloc(1, sizeof *c) : NULL;
  if (!c) {
    rd_node_destroy(ctx, n);
    return NULL;
  }
  if (n.index >= r->cap) {
    uint32_t  cap = r->cap ? r->cap : 64;
    while (cap <= n.index) cap *= 2;
    control** p = (control**)realloc((void*)r->items, cap * sizeof *p);
    if (!p) {
      free(c);
      rd_node_destroy(ctx, n);
      return NULL;
    }
    memset((void*)(p + r->cap), 0, (cap - r->cap) * sizeof *p);
    r->items = p;
    r->cap   = cap;
  }
  *c = (control){.node = n, .kind = k};
  if (!rd_node_on(ctx, n, type, fn, c, control_free, 0).id) {
    free(c);
    rd_node_destroy(ctx, n);
    return NULL;
  }
  r->items[n.index] = c;
  return c;
}

static rd_node part(rd_ctx* ctx, rd_node parent, rd_class cls) {
  rd_node n = rd_node_create(ctx);
  rdx__part(ctx, n, cls);
  rd_node_append(ctx, parent, n);
  return n;
}

static void add_label(rd_ctx* ctx, rd_node row, const char* label, rd_class cls) {
  if (!label) return;
  rd_node t = rd_node_create_text(ctx, label, RD_TEXT_NUL);
  rdx__part(ctx, t, cls);
  rd_node_append(ctx, row, t);
}

// Checkbox and switch.

static void set_checked(rd_ctx* ctx, control* c, bool on) {
  c->checked = on;
  rd_node_set_states(ctx, c->node, RD_STATE_CHECKED, on);
}

static void toggle_click(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ev;
  control* c = data;
  set_checked(ctx, c, !c->checked);
  rd_node_dispatch(ctx, c->node, RD_EVENT_CHANGE);
}

static rd_node toggle_create(rd_ctx* ctx, const char* label, kind k) {
  rdx__theme* t = rdx__theme_get(ctx);
  if (!t) return (rd_node){0};
  rd_node row = rd_node_create(ctx);
  if (!attach(ctx, row, k, RD_EVENT_CLICK, toggle_click)) return (rd_node){0};
  const rdx_classes* cl = &t->classes;
  rd_node_set_role(ctx, row, k == CHECKBOX ? RD_ROLE_CHECKBOX : RD_ROLE_SWITCH);
  rd_node_add_class(ctx, row, k == CHECKBOX ? cl->check_row : cl->switch_row);
  if (k == CHECKBOX) {
    rd_node box  = part(ctx, row, cl->check_box);
    rd_node mark = part(ctx, box, cl->check_mark);
    rd_node_set_image(ctx, mark, t->check);
  } else {
    part(ctx, part(ctx, row, cl->switch_track), cl->switch_thumb);
  }
  add_label(ctx, row, label, cl->label);
  return row;
}

rd_node rdx_checkbox_create(rd_ctx* ctx, const char* label) { return toggle_create(ctx, label, CHECKBOX); }
rd_node rdx_switch_create(rd_ctx* ctx, const char* label) { return toggle_create(ctx, label, SWITCH); }

static control* toggle_find(const rd_ctx* ctx, rd_node n) {
  control* c = find(ctx, n, ANY);
  return c && (c->kind == CHECKBOX || c->kind == SWITCH) ? c : NULL;
}

bool rdx_checked(const rd_ctx* ctx, rd_node control) {
  struct control* c = toggle_find(ctx, control);
  return c && c->checked;
}

bool rdx_set_checked(rd_ctx* ctx, rd_node control, bool checked) {
  struct control* c = toggle_find(ctx, control);
  if (!c) return false;
  set_checked(ctx, c, checked);
  return true;
}

// Radio group. Only the selected option (or the first, with none selected) is in Tab order, so Tab
// enters and leaves the group in one step; the arrow keys move within it.

static int option_count(const rd_ctx* ctx, rd_node group) {
  int n = 0;
  for (rd_node o = rd_node_first_child(ctx, group); !rd_node_is_null(o); o = rd_node_next_sibling(ctx, o))
    if (find(ctx, o, RADIO)) n++;
  return n;
}

static rd_node option_at(const rd_ctx* ctx, rd_node group, int index) {
  int k = 0;
  for (rd_node o = rd_node_first_child(ctx, group); !rd_node_is_null(o); o = rd_node_next_sibling(ctx, o))
    if (find(ctx, o, RADIO) && k++ == index) return o;
  return (rd_node){0};
}

static void update_group(rd_ctx* ctx, rd_node group) {
  int sel = rdx_radio_selected(ctx, group), k = 0;
  for (rd_node o = rd_node_first_child(ctx, group); !rd_node_is_null(o); o = rd_node_next_sibling(ctx, o)) {
    control* c = find(ctx, o, RADIO);
    if (!c) continue;
    bool tab = sel >= 0 ? c->checked : k == 0;
    rd_node_merge_style(ctx, o, &(rd_style){.focusable = tab ? RD_FOCUSABLE_TAB : RD_FOCUSABLE_POINTER});
    k++;
  }
}

static bool select_index(rd_ctx* ctx, rd_node group, int index) {
  int  k       = 0;
  bool changed = false;
  for (rd_node o = rd_node_first_child(ctx, group); !rd_node_is_null(o); o = rd_node_next_sibling(ctx, o)) {
    control* c = find(ctx, o, RADIO);
    if (!c) continue;
    bool on = k++ == index;
    changed |= on != c->checked;
    c->checked = on;
    rd_node_set_states(ctx, o, RD_STATE_CHECKED, on);
  }
  update_group(ctx, group);
  return changed;
}

static int index_of(const rd_ctx* ctx, rd_node group, rd_node option) {
  int k = 0;
  for (rd_node o = rd_node_first_child(ctx, group); !rd_node_is_null(o); o = rd_node_next_sibling(ctx, o)) {
    if (!find(ctx, o, RADIO)) continue;
    if (rd_node_eq(o, option)) return k;
    k++;
  }
  return -1;
}

static void radio_click(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ev;
  control* c = data;
  if (select_index(ctx, c->group, index_of(ctx, c->group, c->node))) rd_node_dispatch(ctx, c->group, RD_EVENT_CHANGE);
}

static bool option_disabled(const rd_ctx* ctx, rd_node o) { return rd_node_states(ctx, o) & RD_STATE_DISABLED; }

static void radio_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  rd_node group = ((control*)data)->node;
  int     dir   = ev->key == RD_KEY_DOWN || ev->key == RD_KEY_RIGHT ? 1 : ev->key == RD_KEY_UP || ev->key == RD_KEY_LEFT ? -1 : 0;
  int     n     = option_count(ctx, group);
  if (!dir || !n) return;
  rd_event_prevent_default(ev);
  int from = index_of(ctx, group, rd_ctx_focused(ctx));
  if (from < 0) from = rdx_radio_selected(ctx, group);
  for (int step = 1; step <= n; step++) {
    int     i = ((from < 0 ? (dir > 0 ? -1 : 0) : from) + dir * step % n + n) % n;
    rd_node o = option_at(ctx, group, i);
    if (option_disabled(ctx, o)) continue;
    bool changed = select_index(ctx, group, i);
    rd_node_focus(ctx, o);
    if (changed) rd_node_dispatch(ctx, group, RD_EVENT_CHANGE);
    return;
  }
}

rd_node rdx_radio_group_create(rd_ctx* ctx) {
  rdx__theme* t = rdx__theme_get(ctx);
  if (!t) return (rd_node){0};
  rd_node g = rd_node_create(ctx);
  if (!attach(ctx, g, RADIO_GROUP, RD_EVENT_KEY_DOWN, radio_keys)) return (rd_node){0};
  rd_node_set_role(ctx, g, RD_ROLE_RADIO_GROUP);
  rd_node_add_class(ctx, g, t->classes.radio_group);
  return g;
}

rd_node rdx_radio_add(rd_ctx* ctx, rd_node group, const char* label) {
  rdx__theme* t = find(ctx, group, RADIO_GROUP) ? rdx__theme_get(ctx) : NULL;
  if (!t) return (rd_node){0};
  rd_node  row = rd_node_create(ctx);
  control* c   = attach(ctx, row, RADIO, RD_EVENT_CLICK, radio_click);
  if (!c) return (rd_node){0};
  const rdx_classes* cl = &t->classes;
  c->group              = group;
  rd_node_set_role(ctx, row, RD_ROLE_RADIO_BUTTON);
  rd_node_add_class(ctx, row, cl->radio_row);
  part(ctx, part(ctx, row, cl->radio_ring), cl->radio_dot);
  add_label(ctx, row, label, cl->label);
  rd_node_append(ctx, group, row);
  update_group(ctx, group);
  return row;
}

int rdx_radio_selected(const rd_ctx* ctx, rd_node group) {
  if (!find(ctx, group, RADIO_GROUP)) return -1;
  int k = 0;
  for (rd_node o = rd_node_first_child(ctx, group); !rd_node_is_null(o); o = rd_node_next_sibling(ctx, o)) {
    control* c = find(ctx, o, RADIO);
    if (!c) continue;
    if (c->checked) return k;
    k++;
  }
  return -1;
}

bool rdx_radio_select(rd_ctx* ctx, rd_node group, int index) {
  if (!find(ctx, group, RADIO_GROUP) || index >= option_count(ctx, group)) return false;
  select_index(ctx, group, index < 0 ? -1 : index);
  return true;
}

// Slider.

// Values are min + n * step, as for an HTML range input: when the range isn't a whole number of steps,
// the largest value is the last step below max, not max.
static float snap(const control* c, float v) {
  v = fminf(c->max, fmaxf(c->min, v));
  if (c->step <= 0) return v;
  float last = c->min + floorf((c->max - c->min) / c->step + 1e-4f) * c->step;
  return fminf(last, c->min + roundf((v - c->min) / c->step) * c->step);
}

static void show_value(rd_ctx* ctx, control* c) {
  float pct = c->max > c->min ? (c->value - c->min) / (c->max - c->min) * 100 : 0;
  rd_node_merge_style(ctx, c->fill, &(rd_style){.width = rd_pct(pct)});
  rd_node_merge_style(ctx, c->thumb, &(rd_style){.inset.left = rd_pct(pct)});
}

// Sets the value; fires INPUT if it changed.
static void move_to(rd_ctx* ctx, control* c, float v) {
  v = snap(c, v);
  if (v == c->value) return;
  c->value = v;
  show_value(ctx, c);
  rd_node_dispatch(ctx, c->node, RD_EVENT_INPUT);
}

static void slider_pointer(rd_ctx* ctx, rd_event* ev, void* data) {
  control* c = data;
  rd_rect  r = rd_node_bounds(ctx, c->rail);
  float    t = r.width > 0 ? (ev->x - r.x) / r.width : 0;
  if (ev->type == RD_EVENT_POINTER_DOWN) {
    if (ev->button != RD_BUTTON_PRIMARY) return;
    rd_node_set_pointer_capture(ctx, c->node);
    c->drag_start = c->value;
  } else if (ev->type == RD_EVENT_POINTER_UP) {
    if (ev->button == RD_BUTTON_PRIMARY && c->value != c->drag_start) rd_node_dispatch(ctx, c->node, RD_EVENT_CHANGE);
    return;
  } else if (!(ev->buttons & RD_BUTTON_PRIMARY) || !rd_node_eq(rd_ctx_pointer_capture(ctx), c->node)) {
    return;
  }
  move_to(ctx, c, c->min + t * (c->max - c->min));
}

static void slider_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  control* c     = data;
  float    range = c->max - c->min, step = c->step > 0 ? c->step : range / 100;
  float    page  = fmaxf(step, c->step > 0 ? roundf(range / 10 / c->step) * c->step : range / 10), to;
  switch (ev->key) {
    case RD_KEY_LEFT:
    case RD_KEY_DOWN:
      to = c->value - step;
      break;
    case RD_KEY_RIGHT:
    case RD_KEY_UP:
      to = c->value + step;
      break;
    case RD_KEY_PAGE_DOWN:
      to = c->value - page;
      break;
    case RD_KEY_PAGE_UP:
      to = c->value + page;
      break;
    case RD_KEY_HOME:
      to = c->min;
      break;
    case RD_KEY_END:
      to = c->max;
      break;
    default:
      return;
  }
  rd_event_prevent_default(ev);
  float before = c->value;
  move_to(ctx, c, to);
  if (c->value != before) rd_node_dispatch(ctx, c->node, RD_EVENT_CHANGE);
}

rd_node rdx_slider_create(rd_ctx* ctx, const rdx_slider_desc* desc) {
  rdx__theme* t = rdx__theme_get(ctx);
  if (!t) return (rd_node){0};
  rd_node  root = rd_node_create(ctx);
  control* c    = attach(ctx, root, SLIDER, RD_EVENT_KEY_DOWN, slider_keys);
  if (!c) return (rd_node){0};
  const rdx_classes* cl = &t->classes;
  rdx_slider_desc    d  = desc ? *desc : (rdx_slider_desc){0};
  if (!(d.max > d.min)) d.min = 0, d.max = 100;
  c->min  = d.min;
  c->max  = d.max;
  c->step = d.step > 0 ? d.step : 0;
  rd_node_set_role(ctx, root, RD_ROLE_SLIDER);
  rd_node_add_class(ctx, root, cl->slider);
  c->rail  = part(ctx, root, cl->slider_track);
  c->fill  = part(ctx, c->rail, cl->slider_fill);
  c->thumb = part(ctx, c->rail, cl->slider_thumb);
  c->value = snap(c, d.value);
  show_value(ctx, c);
  rd_event_type types[] = {RD_EVENT_POINTER_DOWN, RD_EVENT_POINTER_MOVE, RD_EVENT_POINTER_UP};
  for (int j = 0; j < 3; j++) rd_node_on(ctx, root, types[j], slider_pointer, c, NULL, 0);
  return root;
}

float rdx_slider_value(const rd_ctx* ctx, rd_node slider) {
  control* c = find(ctx, slider, SLIDER);
  return c ? c->value : 0;
}

bool rdx_slider_set_value(rd_ctx* ctx, rd_node slider, float value) {
  control* c = find(ctx, slider, SLIDER);
  if (!c) return false;
  c->value = snap(c, value);
  show_value(ctx, c);
  return true;
}

bool rdx_slider_set_range(rd_ctx* ctx, rd_node slider, float min, float max) {
  control* c = find(ctx, slider, SLIDER);
  if (!c || !(max > min)) return false;
  c->min   = min;
  c->max   = max;
  c->value = snap(c, c->value);
  show_value(ctx, c);
  return true;
}

// Numeric input.

static bool bounded(const control* c) { return c->max > c->min; }

static bool disabled(const rd_ctx* ctx, const control* c) { return rd_node_states(ctx, c->node) & RD_STATE_DISABLED; }

// Clamped to the range and rounded to the places shown.
static float number_fit(const control* c, double v) {
  if (bounded(c)) v = fmin(c->max, fmax(c->min, v));
  double k = pow(10, c->decimals);
  v        = round(v * k) / k;
  return (float)(v == 0 ? 0 : v);  // no "-0"
}

static void number_show(rd_ctx* ctx, const control* c) {
  char s[64];
  int  n = snprintf(s, sizeof s, "%.*f", c->decimals, (double)c->value);
  rd_node_set_text(ctx, c->input, s, n > 0 && (size_t)n < sizeof s ? (size_t)n : 0);
}

// Sets the value and shows it; fires INPUT, and CHANGE with `settle`, if it changed.
static void number_to(rd_ctx* ctx, control* c, double v, bool settle) {
  float f = number_fit(c, v);
  bool  changed = f != c->value;
  c->value      = f;
  number_show(ctx, c);
  if (!changed) return;
  rd_node_dispatch(ctx, c->node, RD_EVENT_INPUT);
  if (settle) rd_node_dispatch(ctx, c->node, RD_EVENT_CHANGE);
}

// Reads the field: the whole of it must be a finite number, or the value stays as it was.
static void number_commit(rd_ctx* ctx, rd_event* ev, void* data) {
  rd_event_stop_propagation(ev);  // the field's own CHANGE; the control fires its own
  control*    c = data;
  size_t      len;
  const char* s = rd_node_text(ctx, c->input, &len);
  char        buf[64];
  if (!s || len == 0 || len >= sizeof buf) {
    number_show(ctx, c);
    return;
  }
  memcpy(buf, s, len);
  buf[len]  = 0;
  char*  end = NULL;
  double v   = strtod(buf, &end);
  if (end == buf || *end || !isfinite(v))
    number_show(ctx, c);
  else
    number_to(ctx, c, v, true);
}

static void number_keep_inside(rd_ctx* ctx, rd_event* ev, void* data) {
  (void)ctx;
  (void)data;
  rd_event_stop_propagation(ev);  // edits are the field's business; the control reports values
}

// Digits always; a point with decimals; a minus where the range allows negatives.
static void number_text(rd_ctx* ctx, rd_event* ev, void* data) {
  const control* c  = data;
  uint32_t       cp = ev->codepoint;
  bool ok = (cp >= '0' && cp <= '9') || (cp == '.' && c->decimals > 0) || (cp == '-' && (!bounded(c) || c->min < 0));
  if (!ok || disabled(ctx, c)) rd_event_prevent_default(ev);
}

static float number_step(const control* c) { return c->step > 0 ? c->step : (float)pow(10, -c->decimals); }

static void number_keys(rd_ctx* ctx, rd_event* ev, void* data) {
  control* c = data;
  if (disabled(ctx, c)) {
    if (ev->key != RD_KEY_TAB) rd_event_prevent_default(ev);
    return;
  }
  double step = number_step(c), by;
  switch (ev->key) {
    case RD_KEY_UP:
      by = ev->mods & RD_MOD_SHIFT ? 10 * step : step;
      break;
    case RD_KEY_DOWN:
      by = ev->mods & RD_MOD_SHIFT ? -10 * step : -step;
      break;
    case RD_KEY_PAGE_UP:
      by = 10 * step;
      break;
    case RD_KEY_PAGE_DOWN:
      by = -10 * step;
      break;
    case RD_KEY_ESCAPE:
      rd_event_prevent_default(ev);
      number_show(ctx, c);  // drops what was typed
      return;
    default:
      return;
  }
  rd_event_prevent_default(ev);
  number_to(ctx, c, (double)c->value + by, true);
}

// A held stepper: one step on press, then repeats after a pause until release. The timer finds the
// control through its node, so a control destroyed meanwhile just ends it.
typedef struct stepping {
  rd_node  number;
  int      dir;
  rd_timer self;
} stepping;

static void number_repeat(rd_ctx* ctx, void* data) {
  stepping* s = data;
  control*  c = find(ctx, s->number, NUMBER);
  if (!c) {
    rd_ctx_clear_timer(ctx, s->self);
    return;
  }
  number_to(ctx, c, (double)c->value + s->dir * (double)number_step(c), false);
}

static void number_start_repeat(rd_ctx* ctx, void* data) {
  stepping* first = data;
  control*  c     = find(ctx, first->number, NUMBER);
  stepping* s     = c ? malloc(sizeof *s) : NULL;
  if (!s) return;
  *s       = *first;
  c->repeat = s->self = rd_ctx_set_timer(ctx, 0.06, RD_TIMER_REPEAT, number_repeat, s, free);
  number_repeat(ctx, s);
}

static void number_stepper(rd_ctx* ctx, rd_event* ev, void* data) {
  rd_node   button = ev->current;
  stepping* s      = data;
  control*  c      = find(ctx, s->number, NUMBER);
  if (!c) return;
  if (ev->type == RD_EVENT_POINTER_DOWN) {
    if (ev->button != RD_BUTTON_PRIMARY || disabled(ctx, c)) return;
    rd_node_set_pointer_capture(ctx, button);
    c->drag_start = c->value;
    number_to(ctx, c, (double)c->value + s->dir * (double)number_step(c), false);
    stepping* first = malloc(sizeof *first);
    if (!first) return;
    *first    = *s;
    c->repeat = rd_ctx_set_timer(ctx, 0.4, 0, number_start_repeat, first, free);
    return;
  }
  if (!rd_node_eq(rd_ctx_pointer_capture(ctx), button)) return;  // POINTER_UP
  rd_ctx_clear_timer(ctx, c->repeat);
  c->repeat = (rd_timer){0};
  if (c->value != c->drag_start) rd_node_dispatch(ctx, c->node, RD_EVENT_CHANGE);
}

static void add_stepper(rd_ctx* ctx, control* c, rd_node column, int dir, rd_image icon, const rdx_classes* cl) {
  rd_node b = rd_node_create(ctx);
  rd_node_add_class(ctx, b, cl->number_step);
  rd_node_append(ctx, column, b);
  rd_node img = rd_node_create(ctx);
  rd_node_set_style(ctx, img, &(rd_style){.height = rd_px(12), .pointer_events = RD_POINTER_EVENTS_NONE});
  rd_node_set_image(ctx, img, icon);
  rd_node_append(ctx, b, img);
  stepping* s = malloc(sizeof *s);
  if (!s) return;
  *s = (stepping){c->node, dir, {0}};
  if (!rd_node_on(ctx, b, RD_EVENT_POINTER_DOWN, number_stepper, s, free, 0).id) {
    free(s);
    return;
  }
  rd_node_on(ctx, b, RD_EVENT_POINTER_UP, number_stepper, s, NULL, 0);
}

rd_node rdx_number_create(rd_ctx* ctx, const rdx_number_desc* desc) {
  rdx__theme* t = rdx__theme_get(ctx);
  if (!t) return (rd_node){0};
  rd_node  root = rd_node_create(ctx);
  control* c    = attach(ctx, root, NUMBER, RD_EVENT_KEY_DOWN, number_keys);
  if (!c) return (rd_node){0};
  const rdx_classes* cl = &t->classes;
  rdx_number_desc    d  = desc ? *desc : (rdx_number_desc){0};
  c->min      = d.min;
  c->max      = d.max;
  c->step     = d.step > 0 ? d.step : 0;
  c->decimals = d.decimals < 0 ? 0 : d.decimals > 6 ? 6 : d.decimals;
  rd_node_set_role(ctx, root, RD_ROLE_SPIN_BUTTON);
  rd_node_add_class(ctx, root, cl->number);
  c->input = rd_node_create_input(ctx, 0);
  rd_node_add_class(ctx, c->input, cl->number_input);
  rd_node_append(ctx, root, c->input);
  rd_node_on(ctx, c->input, RD_EVENT_TEXT, number_text, c, NULL, 0);
  rd_node_on(ctx, c->input, RD_EVENT_CHANGE, number_commit, c, NULL, 0);
  rd_node_on(ctx, c->input, RD_EVENT_INPUT, number_keep_inside, NULL, NULL, 0);
  rd_node steppers = rd_node_create(ctx);
  rd_node_add_class(ctx, steppers, cl->number_steppers);
  rd_node_append(ctx, root, steppers);
  add_stepper(ctx, c, steppers, 1, t->chevron_up, cl);
  add_stepper(ctx, c, steppers, -1, t->chevron, cl);
  c->value = number_fit(c, d.value);
  number_show(ctx, c);
  return root;
}

float rdx_number_value(const rd_ctx* ctx, rd_node number) {
  control* c = find(ctx, number, NUMBER);
  return c ? c->value : 0;
}

bool rdx_number_set_value(rd_ctx* ctx, rd_node number, float value) {
  control* c = find(ctx, number, NUMBER);
  if (!c || !isfinite(value)) return false;
  c->value = number_fit(c, value);
  number_show(ctx, c);
  return true;
}

bool rdx_number_set_range(rd_ctx* ctx, rd_node number, float min, float max) {
  control* c = find(ctx, number, NUMBER);
  if (!c) return false;
  c->min   = min;
  c->max   = max;
  c->value = number_fit(c, c->value);
  number_show(ctx, c);
  return true;
}

rd_node rdx_number_input(const rd_ctx* ctx, rd_node number) {
  control* c = find(ctx, number, NUMBER);
  return c ? c->input : (rd_node){0};
}
