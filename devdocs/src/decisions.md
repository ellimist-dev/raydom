# Design decisions and tradeoffs

Each entry: what was decided, why, what it costs, and what would make it worth revisiting.

## Retained mode, not immediate mode

raylib has raygui, an immediate-mode toolkit. raydom keeps a tree because layout (flexbox, grid, text wrapping)
needs the whole tree to size anything, animation needs state across frames, and accessibility needs a structure to
describe. **Cost:** applications manage node lifetimes and mirror their state into the tree. **Revisit:** never; it
is the premise.

## A C core with a C++ layer

C23 is callable from everything and keeps the library easy to embed; the C++ layer recovers ergonomics (RAII,
lambdas, builders) without making C users pay. **Cost:** every feature is written twice at the API surface, and the
C API is verbose. **Revisit:** if the C++ layer starts needing state the C API can't express.

## Generational handles instead of pointers

`rd_node` is a value with a generation. Stale handles are detectable, so a destroyed node is a no-op everywhere
instead of a use-after-free, and handles can be stored and copied freely. **Cost:** a check on every call, and
public structs hold handles instead of pointers. **Revisit:** no.

## Parallel arrays by subsystem

Node data is split into `slots`, `styles`, `layouts`, `events` rather than one struct per node, so each pass touches
dense memory and subsystems don't share a struct definition. **Cost:** a node's data is spread out, and arrays
reallocate together, so no pointer into them survives node creation. **Revisit:** if profiling shows the split
hurting a pass that reads several arrays per node.

## Zero means unset, and one X-macro

Partial styles as C designated initializers, merging that copies only what's set, and every per-property operation
generated from one list. **Cost:** "value zero" needs a unit or flag (`rd_px(0)`), and adding a value *kind* touches
several expansions. **Revisit:** no; it's what makes the C API pleasant.

## CSS semantics, with deliberate differences

Flexbox and grid follow the CSS specs closely, so knowledge and bug reports transfer. The defaults differ where CSS's
are web-specific: `flex_direction` column (most UI stacks vertically), `flex_shrink` 0 (items keep their size unless
asked; CSS's shrink-by-default is a frequent surprise), automatic minimum size 0. **Cost:** CSS users must learn the
differences, documented in the user book. **Revisit:** if users trip on them more than they would on CSS's.

## Classes and state variants, no selectors

Styles come from defaults, inheritance, classes in order, state variants, and the inline style. No selectors, no
specificity, no cascade across the tree. Selector matching is the expensive, surprising part of CSS; classes plus
variants cover component styling, and `share_states` covers the one common descendant case (`.row:hover .box`).
**Cost:** no structural selectors (`:nth-child`, `.a .b`); applications add classes instead. **Revisit:** if
component authors keep reaching for descendant selectors beyond what `share_states` gives.

## One recursive measure function with caches

Layout follows Yoga's model: `rd__layout_node(key, perform)` answers "how big would you be?" and "lay yourself out",
with per-node caches. It's simple to extend (grid, text and images plug in as measured leaves) and near-linear with
caching. **Cost:** callers must resolve a node's own size properties into the key, and cache keys must capture every
input (see `normalize_key`). **Revisit:** if a layout mode can't be expressed as measure-then-perform.

## Lazy recomputation

Mutations set dirty flags; work happens once in `rd_ctx_layout`. Listeners can make many changes cheaply, and an
unchanged frame costs almost nothing. **Cost:** geometry read between a mutation and the next layout is stale, and
some internal paths (focus) must compute styles on demand.

## A display list, rebuilt every frame

Drawing walks the tree into commands, then a renderer executes them. The split keeps raylib in one file, allows
headless render tests, and leaves room for other backends. Rebuilding every frame avoids retained render state and
its invalidation bugs. **Cost:** a full tree walk per frame even when nothing changed; `paint_dirty` is tracked but
not yet used to skip it. **Revisit:** for very large trees, by skipping unchanged frames first.

## An SDF shader for boxes

Rounded corners, borders and shadows come from one fragment shader computing distance to a rounded rectangle, not
from tessellated meshes. Antialiasing is exact and per-corner radii and per-side borders are cheap. **Cost:** a
batch flush per rounded box; square boxes avoid it. **Revisit:** if batch flushes dominate frame time.

## stb_truetype, no shaping

Fonts and text use stb_truetype: small, static, no dependencies, which fits single-binary apps. **Cost:** no
ligatures, contextual forms or bidirectional text; Arabic and Indic scripts render incorrectly. **Revisit:** when
those scripts matter; HarfBuzz is the answer and would be the largest dependency by far, so likely as an option.

## The overlay is a child of the root

Popups need to draw above everything and escape clipping. A second root would have meant special cases in every
ancestor walk (event paths, focus, selection order, "is this displayed"). As the root's forced-last child with fixed
geometry, the overlay needs none. **Cost:** it appears among the root's children, and the "always last" rule lives in
`rd_node_insert_before`.

## Overlay scrollbars

Scrollbars draw over the content edge and take no layout space, like macOS. Showing or hiding a bar never reflows
content, and layout didn't change. **Cost:** a bar can cover content; layouts add padding where it matters.
**Revisit:** if classic gutter bars become a requirement, as an option.

## Animation lives in the computed style

Transitions write interpolated values into the computed style, which everything else already reads. Layout
properties animate with no layout-side code, and hit-testing and anchoring follow. **Cost:** animating layout
properties relayouts the moving part of the tree every frame. **Revisit:** if a common animation is too expensive;
paint-only fast paths could skip layout for paint properties (they already don't mark layout dirty).

## An application-supplied clock

The context never reads the system clock; `rd_ctx_set_time` drives timers, animations and wheel gesture ends. Tests
are deterministic, and applications control time (pausing, replaying). **Cost:** forgetting to set time stops
everything; `rd_update` does it for raylib apps.

## Extras use only public API

Components are built like an application would build them. This keeps the core honest: every capability a
component needed became public. **Cost:** some features are slightly more general than one component needed.

## Component state outside userdata

Userdata is the application's. Components keep state in listener data plus a registry. **Cost:** a lookup per getter.
The alternative broke the showcase, which reads userdata on hovered nodes as its own type.

## Fixed-height virtualization

Lists and tables require one row height so any row's position is arithmetic and work is O(visible). **Cost:** no
variable-height rows. **Revisit:** with an estimated-height scheme that measures rows as they appear.

## Blocking native file dialogs

`rdx_dialog_*` block until the dialog closes. Native dialogs are modal anyway, macOS requires them on the main
thread, and blocking keeps the API one call. **Cost:** the window doesn't redraw while a dialog is open.

## Vendored code stays unmodified

stb, nanosvg and nativefiledialog are copied verbatim, with static linkage or `#define` renames to avoid symbol
clashes. Updating is copying; fixes go upstream (the nanosvg fractional-gradient bug, for example). **Cost:** known
upstream bugs stay until fixed there.

## Byte offsets in public APIs

Selections, highlights and carets use byte offsets into UTF-8, because applications index their strings in bytes.
Codepoints are internal. **Cost:** callers must not split a multi-byte character; the core clamps to character
boundaries where it can.

## Documentation samples are tests, and docs are per tag

Every sample in the user book is compiled, and most run, in the test suite, and each release tag publishes its own
copy of the book. Docs can't silently drift from the code, and old versions stay accurate. **Cost:** writing a
sample means writing a small test program.
