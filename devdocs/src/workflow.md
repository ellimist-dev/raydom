# Workflow

Everything runs through the `Justfile`.

| Command          | Does                                                                   |
| ---------------- | ---------------------------------------------------------------------- |
| `just build`     | Configures with `RAYDOM_SANITIZE=ON` and builds library, tests, examples |
| `just test`      | `build`, then `ctest --output-on-failure`                              |
| `just showcase`  | Runs the showcase                                                      |
| `just lint`      | clang-tidy over `src/`, `extras/src/` and `tests/` with the repo's `.clang-tidy` |
| `just consumer`  | Builds `tests/consumer` against raydom via FetchContent and via an installed package |
| `just docs`      | Serves the user book with live reload                                  |
| `just docs-build`| Builds the user book as published, with `llms.txt`                     |
| `just devdocs`   | Serves this book                                                       |
| `just clean`     | Removes build outputs, keeps the configuration and downloaded raylib   |
| `just distclean` | Removes build directories entirely                                     |
| `just build-web` | Emscripten build                                                       |

The first configure downloads raylib 6.0 into `build/_deps`. The tests and the showcase use a font from raylib's
examples (`anonymous_pro_bold.ttf`), which is why they need a downloaded raylib rather than an installed one.

## Sanitizers are on by default

`just build` compiles the library, tests and examples with AddressSanitizer and UBSan. Most memory bugs in this
codebase have surfaced as a sanitizer report in an ordinary test run, so leave them on while developing. Release
builds and installs don't use them: the sanitizer link flag reaches only this tree's programs, never an installed
package.

## Before sending a change

1. `just test`: every test, including the documentation examples, passes.
2. `just lint`: no new findings in the files you touched.
3. `just consumer` if you changed CMake, headers, or anything that affects linking.
4. If behavior changed, update the user book and, where relevant, this book. If a style property changed, the
   property reference regenerates from the source, but its description lives in `docs/tools/preprocess.py`.
5. Look at the showcase for anything visual. Screenshots help in review; see below.

CI (`.github/workflows/ci.yml`) runs the tests with sanitizers on Linux (GCC) and macOS (Apple Clang), `just consumer`,
`just check-version` and both book builds, on pull requests and before every docs publish (`docs.yml` calls it).
To reproduce the Linux job locally, run the same steps in an `ubuntu:24.04` container.

`-DRAYDOM_TEST_GRAPHICS=ON` adds `test_render_gl`, which opens a window, draws, and checks pixels read back from
the screen: the only test of `rd_update` and `rd_draw`. It's off by default because it needs a display; CI turns it
on, under `xvfb-run` on Linux. Without a display or OpenGL, as on GitHub's macOS runners, it reports itself
skipped.

## Screenshots

The showcase has a scripted mode for screenshots, used during development to check visual changes without clicking:

```sh
RAYDOM_SCREENSHOT=out.png RAYDOM_TAB=1 RAYDOM_CLICK="745,748" RAYDOM_TYPE="hello|" RAYDOM_FRAME=60 ./build/examples/showcase
```

| Variable            | Effect                                                                  |
| ------------------- | ----------------------------------------------------------------------- |
| `RAYDOM_SCREENSHOT` | Saves this file and exits                                               |
| `RAYDOM_TAB`        | Opens a top-level tab: 0 Tour, 1 Messages, 2 Network                    |
| `RAYDOM_SCROLL`     | Scrolls the tour                                                        |
| `RAYDOM_CLICK`      | Clicks at `x,y`, several separated by `;`, on frame 1                   |
| `RAYDOM_MOUSE`      | Leaves the pointer at `x,y`                                             |
| `RAYDOM_TABS`       | Presses Tab this many times                                             |
| `RAYDOM_TYPE`       | Types text; `|` presses Enter                                           |
| `RAYDOM_DRAG`       | Drags from `x0,y0` to `x1,y1`                                           |
| `RAYDOM_FRAME`      | Captures this frame (default 3), to let animations and timers finish    |

On a high-DPI display the screenshot is at device pixels with the window in one quadrant; crop the bottom-left
`2 x width` by `2 x height` region.

## Docs locally

The user book needs mdbook 0.5 (`brew install mdbook`) and Python 3, used by its preprocessor. See
[Documentation](architecture/docs.md).
