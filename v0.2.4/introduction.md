# Introduction

raydom is a retained-mode UI tree for [raylib](https://www.raylib.com/). An application builds a tree of nodes,
assigns CSS-like styles, and attaches event listeners. raydom computes layout with flexbox and grid, shapes and wraps
text, routes pointer and keyboard input, and draws the result with raylib. The core is C23; a header-only C++20 layer
wraps it. Applications link it statically and ship as one binary.

This book documents the version shown in the version menu. Each release tag has its own copy of the book, so the
pages you read match the headers you compile against.

## Model

- **Context.** `rd_ctx` owns every node, style, class, font and listener. Destroying it frees all of them.
- **Nodes.** `rd_node` is a small value handle. A handle to a destroyed node is stale; every call on it is a no-op.
- **Styles.** A node's computed style merges defaults, inherited properties, classes, state variants, and the inline
  style, in that order.
- **Frame loop.** Each frame, `rd_update` feeds raylib's input and lays the tree out at the window size, and `rd_draw`
  draws it.

## How to read this book

[Getting started](getting-started/installation.md) covers installation and a first window. The
[guide](guide/tree.md) has one chapter per subsystem. The [reference](reference/properties.md) lists every
style property with its type, default and inheritance.

Every code sample in this book is a fragment of a program under `docs/examples/` in the repository. Those programs
are compiled by the test suite, and the ones that don't open a window run as tests, so the samples compile and
behave as described for this version.

Agents and tools can read the book as plain Markdown; see [For AI agents](agents.md).
