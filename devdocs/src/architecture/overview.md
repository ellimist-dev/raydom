# Overview

## Layers

```text
 application
     │
     ├── raydom::extras   controls, popups, views, toasts, network view, file dialogs
     │        │           (public core API only, plus rd_ctx_extension and layout hooks)
     ▼        ▼
 raydom.hh ── C++ wrappers, header-only
     │
 raydom core (C23)
   tree ─ styles ─ layout ─ text ─ events/input ─ display list ─ renderer
     │                                                │
     └────────────── timers, animation ───────────────┘
                                                      ▼
                                                   raylib
```

The core is the only part that knows the private structures. Extras and the C++ layer use the public headers, so
anything they need is something an application could need too; when extras needed per-context storage or a
post-layout callback, those became public core features (`rd_ctx_set_extension`, `rd_ctx_on_layout`) rather than
private back doors.

Only `src/render.c` calls raylib. Everything else is plain C, which is why layout, text, events and animation run
headless in tests.

## One frame

A raylib application calls `rd_update`, then `rd_draw` inside `BeginDrawing`/`EndDrawing`. Here is what happens,
in order:

```text
rd_update
 ├─ rd_ctx_set_time(GetTime())   clock; fires due timers
 ├─ keyboard and text input       KEY_DOWN/UP and TEXT dispatch, editing
 ├─ rd_ctx_pointer(mouse)         scrollbars, hover, POINTER_*, CLICK, WHEEL, focus
 ├─ rd_ctx_layout(screen size)
 │   ├─ rd__anim_tick             animations into computed styles
 │   ├─ rd_ctx_compute_styles     dirty subtrees only; starts transitions
 │   ├─ rd__layout_node(root)     flex and grid, with caches
 │   ├─ rd__place_anchored        overlay popups next to their anchors
 │   ├─ layout hooks              rd_ctx_on_layout callbacks
 │   └─ second pass               only if a hook dirtied the tree
 ├─ apply the mouse cursor
 └─ raylib event waiting          on while idle, off while timers or animations run
rd_draw
 ├─ rd__display_list              the laid-out tree into commands
 └─ execute commands              SDF shader, glyph atlas, images, scissor
```

The [table below](#subsystems-at-a-glance) says which file owns each step.

Each stage reads what the previous one produced and marks dirty flags for the next. Nothing is recomputed eagerly:
mutating the tree or styles from a listener only sets flags, and the next `rd_ctx_layout` does the work once.

## Data flow

```text
inline style + classes + states + parent   ──build──▶  target style
target style + animations                   ──transition──▶  computed (shown) style
computed style + tree + measure callbacks   ──layout──▶  rects, scroll ranges, baselines
rects + computed styles + text layout       ──display list──▶  commands
commands                                     ──renderer──▶  pixels
```

The **computed style is what is shown**, not what was asked for: during a transition it holds the interpolated
value, and layout and drawing read it without knowing an animation exists.

## Subsystems at a glance

| File(s)                  | Owns                                                                  |
| ------------------------ | --------------------------------------------------------------------- |
| `tree.c`                 | The context, slots, handles, tree links, userdata, extensions, destruction |
| `style.c`                | The style X-macro machinery, classes, state variants, computation, dirty flags |
| `anim.c`                 | Transitions, explicit animations, the animation tick                  |
| `layout.c`, `grid.c`     | The measure contract, flexbox, grid, caches, absolute positioning, layout hooks |
| `overlay.c`              | The overlay node and anchored placement                               |
| `text.c`, `font.c`       | Text storage, tokens, line breaking, fonts, metrics                   |
| `input.c`, `selection.c` | Editable text, undo, the document selection                           |
| `events.c`               | Listeners, dispatch, pointer, hover, focus, keyboard routing, wheel, scrollbar input, cursors |
| `display.c`              | Display list, hit-testing, scrolling, scrollbar geometry              |
| `render.c`               | raylib integration: drawing, glyph atlas, textures, `rd_update`       |
| `image.c`                | Image decoding, SVG parsing and rasterizing, image nodes              |
| `timer.c`                | Timers                                                                |
| `role.c`                 | Roles and the semantic constructors                                  |
