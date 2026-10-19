# Writing tests

## Shape

Each subsystem has a test program in `tests/` (`test_layout.c`, `test_popups.c`, ...), listed in the `foreach` in
the top-level `CMakeLists.txt`. A test is a `main` calling functions that use `CHECK(cond)` from `tests/check.h`, which
prints the failing expression and line and counts failures; `TEST_RESULT()` returns non-zero if any failed. C++
tests (`*_cpp.cc`) exercise the C++ layer.

Tests link the core and extras and can include private headers from `src/` to check internals (`rd__display_list`,
`rd__scrollbar`, `ctx->sb_hot`). Prefer public API; reach inside only to check something the API doesn't expose.

## Headless by default

Nothing but `render.c` touches raylib, so tests don't open windows:

- Fonts: `rd_font_load_file(ctx, RAYDOM_TEST_FONT)`, a font from raylib's examples.
- Layout: `rd_ctx_layout(ctx, w, h)`, then `rd_node_rect` and `rd_node_bounds`.
- Input: `rd_ctx_pointer`, `rd_ctx_key`, `rd_ctx_text`; layout first so hit-testing has geometry.
- Time: `rd_ctx_set_time(ctx, t)` then `rd_ctx_layout` to advance timers and animations deterministically.
- Drawing: `rd__display_list` and check commands (`tests/test_render.c`, `tests/test_image.c`).
- Pixels: rasterize SVG with `rd__image_rasterize` and read `pixels`.

## What to cover

- The behavior and its edges (empty, one, many; clamped values; stale handles).
- Every default action and that `prevent_default` stops it.
- Destruction during dispatch, for anything with listeners.
- For components: keyboard paths, setters firing nothing, userdata left alone.

## Floats

Compare with a tolerance (`near(a, b)`, usually 0.01, or 0.6 for laid-out pixels after animation).

## Documentation examples

Programs in `docs/examples/` are tests too (`doc_<name>_<ext>`). They `assert` what the book claims; keep them
passing when changing behavior, and update the prose with them.

## Sanitizers

Run the suite under `just test`, which builds with ASan and UBSan. A test passing without sanitizers but failing
with them is a bug, not flakiness.
