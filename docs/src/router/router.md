# Router

```cpp
#include <raydom/router/router.hh>  // C: router.h
```

`raydom::router` is a separate library, linked as `raydom::router`, that shows one page at a time chosen by a
path, with a history, links, nested routers and transitions between pages. It depends only on the core;
the [breadcrumb](../extras/views.md#breadcrumb) in `raydom::extras` is the usual companion.

```cmake
target_link_libraries(your_app PRIVATE raydom::router)  # RAYDOM_BUILD_ROUTER, on by default
```

## Routes

```cpp
{{#include ../../examples/router.cc:routes}}
```

A router is a node. Give it a size, like any node; it is a flex column that clips its pages. Each route has a
pattern and a builder that returns the page. Routes take effect at the next navigation, so add them all and then
navigate.

| Pattern segment | Matches                                                               |
| --------------- | --------------------------------------------------------------------- |
| `users`         | That literal                                                          |
| `:id`           | Any one segment, as the parameter `id`                                |
| `:id?`, `edit?` | Optionally, a parameter or a literal                                  |
| `*`, `*rest`    | Last only: the remaining segments, zero or more. `*rest` is a parameter, the segments joined with `/` |

When several routes match, the most specific wins. Segment by segment, a literal beats a parameter and a
parameter beats a wildcard. Then a route without a wildcard wins, and then the route added first. `users/new`
beats `users/:id` for `/users/new`, and `*` catches whatever nothing else matches.

`rdr_param(ctx, node, name)` (C++ `router.param(name)` or `rdr::param(node, name)`) reads a parameter from the
router or any node inside its pages. A nested router sees its parents' parameters too.

## Navigation and history

`go` pushes a history entry and drops any entries after the current one. `replace` overwrites the current entry.
`back` and `forward` move through the history, which keeps the last 100 entries (`history_limit`). A path is
absolute (`/users/7`) or relative to the router's base (`profile`, `.`, `..`), and is normalized. `go` returns
false, and changes nothing, when no route matches.

Every navigation resolves the whole tree of routers, then fires `CHANGE` on the root router, once. Navigating from
a builder or from a `CHANGE` listener is queued until the current navigation finishes. A builder that redirects
should call `replace`, so that back doesn't lead to the redirect again.

The root router also takes these inputs, while the pointer or focus is inside it, or for the router used last:

- the mouse's back and forward side buttons (`RD_BUTTON_BACK`, `RD_BUTTON_FORWARD`);
- Cmd+[ and Cmd+] on macOS, or Super elsewhere;
- Alt+Left and Alt+Right outside macOS. On macOS Option+Left moves by word, so it isn't taken.

In a browser these inputs also mean "leave this page". Browsers act on them as they happen, before raydom sees
them on the next frame. So on the web, creating a router cancels the browser's handling: of the side buttons over
the canvas, and of the shortcuts while nothing else on the page has focus. The router's history doesn't sync with
the browser's yet.

## Page lifetime

A page is built when its route is entered and destroyed when it's left. A page is also left, and rebuilt, when
its parameters change: `/users/1` to `/users/2` builds a new page.

`keep_alive` (C `RDR_ROUTE_KEEP_ALIVE`) hides the page instead of destroying it. The page is shown again,
with its state, when the route is next entered, and lives as long as the router. A kept page isn't rebuilt
when its parameters change. It reads the new values on the root router's `CHANGE`.

## Nested routers

```cpp
{{#include ../../examples/router.cc:nested}}
```

A router created with a parent is nested. It sits inside a page whose route ends in an unnamed `*`, and matches
what that `*` captured. A named wildcard would make the remainder a parameter, so changing it would rebuild
the page. The nested router's routes are relative to its base (`/settings` here), so `""` matches `/settings`
itself. One history, in the root router, covers the whole tree, and any router in it navigates it.

## Links

```cpp
{{#include ../../examples/router.cc:links}}
```

`rdr::link` (C `rdr_link_create`) makes a focusable text node with the link role. `rdr_link_attach` turns any node
into a link. A click or Enter navigates. A link without a router uses the nearest router above it, or the router
used last.

A link has `RD_STATE_CHECKED` while the current path is its target or lies under it. A link to `/users` is
checked on `/users/7`. A link to `/`, or one with `RDR_LINK_EXACT`, is checked only on its exact path. Style
the checked state to mark the current section.

## Transitions

```c
rd_node app = rdr_router_create(ctx, &(rdr_router_desc){.transition = RDR_TRANSITION_SLIDE, .duration = 0.25});
```

| Transition               | Does                                                               |
| ------------------------ | ------------------------------------------------------------------ |
| `RDR_TRANSITION_NONE`    | Pages swap at once                                                 |
| `RDR_TRANSITION_FADE`    | A crossfade: the old page fades out as the new one fades in, together |
| `RDR_TRANSITION_SLIDE`   | Forward slides in from the right, back from the left               |

During a transition both pages are in the tree. The old one is positioned over the router, doesn't take pointer
input (`pointer_events: none` and `RD_STATE_DISABLED`), and loses focus. A navigation during a transition ends
it at once and starts the next one. Each router animates only its own page changes, so a nested router can fade
while its parent slides. The first page of a router never animates. Animations run on the context's clock, like
every [animation](../guide/animation.md).

## Breadcrumbs

```cpp
{{#include ../../examples/router.cc:breadcrumb}}
```

`trail()` (C `rdr_router_trail`) lists one crumb per level of the current path. Each crumb has a title and the
absolute path to navigate to. Strings are valid until the next navigation.

- **One crumb per prefix.** For each prefix of the path, the route that matches that prefix exactly gives a
  crumb, if it has a title. With routes `/`, `projects`, `projects/:id` and `projects/:id/files/:file`, the path
  `/projects/loom/files/main.c` gives Home, Projects, loom and main.c. `/projects/loom/files` has no route of its
  own, so it gets no crumb. Every crumb is a real route.
- **Wildcards.** A prefix that matches only because a wildcard swallowed part of it gets no crumb, and neither
  does a pattern that starts with a wildcard. A titled `*` catch-all names the page it shows, not every level
  above it.
- **Nested routers.** The parent names the prefixes up to its wildcard, and the nested router names the rest.
  A crumb whose path repeats the previous one is dropped, so a nested `""` route doesn't repeat its parent.
- **Titles.** `:name` in a title is replaced by the parameter matched at that prefix. To show data instead of
  the raw parameter, give the route a title function. It runs for ancestor crumbs too, whose pages aren't
  built:

```cpp
{{#include ../../examples/router.cc:title}}
```

In C, `rdr_title_fn` gets an `rdr_match` (`rdr_match_param`, `rdr_match_path`) and returns a string the router
copies. Null falls back to `title`.

## Reference

| C                                       | C++                       | Returns                                       |
| --------------------------------------- | ------------------------- | --------------------------------------------- |
| `rdr_router_path`                       | `path()`                  | The tree's current path                       |
| `rdr_router_base`                       | `base()`                  | Where the router is mounted: `/` for a root   |
| `rdr_router_route`                      | `matched_route()`         | The matched pattern, or null                  |
| `rdr_router_page`                       | `page()`                  | The page's wrapper; the page is its child     |
| `rdr_router_root`                       | `root()`                  | The root router                               |
| `rdr_router_can_back`, `_can_forward`   | `can_back()`, `can_forward()` | Whether there's history that way          |
| `rdr_router_transitioning`              | `transitioning()`         | Whether pages are changing places             |
| `rdr_router_set_transition`             | `set_transition()`        | Changes the transition; ends a running one    |

Browser history and URL sync for WebAssembly builds aren't implemented yet. In the browser, the router keeps its
own history like it does on the desktop.
