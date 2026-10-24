# Router

`raydom::router` (`router/src/router.c`) is built only on the public core API, like extras, and doesn't depend on
extras. Its user-facing behavior is in the user book's Router chapter; this page covers how it's put together.

## State

Each router's state is attached to its node (`rd_node_attach`). It holds the routes, the current match (route
index, parameters, base, consumed path, the page's wrapper), the handles of nested routers created with it as
parent, and the transition in progress. Only the root router uses its history, path, pending navigation and trail
fields.

Nested routers find their parent by handle, not pointer, and the parent prunes dead children lazily. Destroying a
page that holds a nested router needs no unregistration.

A context-wide registry is attached to the root node. It holds the router used last and the link handles, and
installs the two root listeners: `KEY_DOWN` for Cmd+[ / Cmd+] (Alt+Left / Alt+Right outside macOS) and
`POINTER_DOWN` for the side buttons.

## Navigation

`request` runs a navigation on the root router, or queues it when one is already running (the `navigating`
flag). That flag covers the builders and the `CHANGE` dispatch. The queue holds one entry, the latest, and is
drained at most 16 times, which bounds redirect loops.

`apply` normalizes the path, checks that the root matches it, updates history, and calls `resolve` on the root.
Then it rebuilds the trail, marks links, and fires `CHANGE`. `resolve` matches one router against its segments.
It keeps the page if the route and parameters are unchanged, and otherwise swaps pages through `change_pages`.
Then it recurses into the nested routers inside the current page with the segments the route's wildcard left.
After each builder it re-finds its state, because a builder may destroy the router.

## Matching

A pattern is parsed into segments: literal, parameter, or wildcard, each possibly optional. `bind` tries every
choice of optional segments, at most 2^16, earlier ones included first. It ranks each binding by a per-path-segment
array (literal 3, parameter 2, wildcard 1), compared lexicographically. A binding without a wildcard wins ties.
Across routes, ties go to the route added first.

## Trail

`build_trail` walks the chain of current routers from the root. Each router stores the segments it resolved
against (`sub`) and how many its route consumed. `router_crumbs` re-runs `bind` for each prefix of `sub`. It
goes up to the consumed count when a nested router continues the trail, and up to the full length otherwise.
For each prefix it keeps the best route that matches without a wildcard capturing anything and that doesn't
start with a wildcard. The current route is skipped there and named last.

Each crumb gets an `rdr_match` for that prefix, holding its parameters with the router above for inherited
ones. That match is passed to the title function, or used to substitute `:name` in the static title. Crumbs
whose path equals the previous crumb's are dropped. A route with a title function can name ancestors from
data even though their pages aren't built.

## Transitions

Each page sits in a wrapper (`SLOT`): a relative flex column that grows to fill the router. On a change, the old
wrapper becomes absolute at inset 0 and is disabled and made pointer-transparent. Both wrappers are animated
with `rd_node_animate`: opacity for fade, `inset` percentages for slide. The leaving wrapper gets an explicit
`from`, because an unset inset doesn't interpolate. A kept-alive wrapper coming back from `display: none` gets
`display` in its `from`, since the animation system doesn't animate out of a hidden state.

The end is a context timer, not the animation's `done` callback, so it can carry a destroy function. Its data is
`{router handle, sequence}`. A timer that fires after the router is gone, or after a newer transition started,
does nothing. `finish` ends a transition at once. It's called at the start of every page change and by
`rdr_router_set_transition`, so at most one old page exists per router.

## Not yet

Browser history and URL hash sync for Emscripten builds; the router keeps its own history there.
