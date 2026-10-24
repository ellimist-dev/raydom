# Introduction

This book is for people changing raydom: how it is put together, why it is put together that way, and how to
extend it without breaking the parts that depend on each other. The user book (`docs/`, published per release)
describes what raydom does; this one describes how.

It tracks `main`. Where it names a function or a file, the name is real; where behavior is subtle, the chapter says
which test pins it down.

## What raydom is, structurally

- **A C23 core** (`src/`, `include/raydom/*.h`): a retained tree of nodes, a style system, flexbox and grid layout,
  text shaping and wrapping, event dispatch, animation, and a renderer on raylib. One static library,
  `raydom::raydom`.
- **A C++20 layer** (`include/raydom/raydom.hh`): header-only wrappers that add RAII for the context, builder-style
  styles, and lambda callbacks. No exceptions.
- **Extras** (`extras/`): components built only on the public core API (controls, popups, views, toasts, a network
  view, native file dialogs), in a second library, `raydom::extras`.
- **Router** (`router/`): path routing, history, nested routers and page transitions, in a third library,
  `raydom::router`, also built only on the public core API. It doesn't depend on extras.
- **Docs** (`docs/`): the user book, whose code samples are programs compiled and run by the test suite.
- **The showcase** (`examples/showcase/`): one application with a feature tour, a desktop messaging app, a
  Paxos simulation and a routed app, used as the integration test you look at.

## Map of the repository

| Path                     | Contents                                                           |
| ------------------------ | ------------------------------------------------------------------ |
| `include/raydom/`        | Public C headers, one per subsystem, and `raydom.hh`               |
| `src/`                   | The core; `*_internal.h` are private headers                       |
| `src/external/`          | Vendored stb_truetype, stb_image, nanosvg                          |
| `extras/`                | `raydom::extras`: headers, sources, vendored nativefiledialog      |
| `router/`                | `raydom::router`: headers and source                               |
| `tests/`                 | One test program per subsystem, and `consumer/` for packaging      |
| `docs/`                  | The user book, its example programs and its tools                  |
| `devdocs/`               | This book                                                          |
| `examples/showcase/`     | The showcase                                                       |
| `cmake/`                 | raylib discovery, the version header, the package config           |
| `.github/workflows/`     | Docs publishing                                                    |

## Reading order

[Overview](architecture/overview.md) first: it walks one frame through every subsystem. Then the chapter for
whatever you are changing, and [Design decisions](decisions.md) before arguing with any of them.
