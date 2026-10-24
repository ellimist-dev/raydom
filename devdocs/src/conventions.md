# Conventions

## Naming

| Prefix      | Meaning                                                                 |
| ----------- | ----------------------------------------------------------------------- |
| `rd_`       | Public core API                                                         |
| `RD_`       | Public enumerators and macros                                           |
| `rd__`      | Internal to the core, shared between core files (`src/*_internal.h`)    |
| `rdx_`, `RDX_` | Public extras API                                                    |
| `rdx__`     | Internal to extras (`extras/src/extras_internal.h`)                     |
| `rdr_`, `RDR_` | Public router API                                                    |
| `rd::`, `rdx::`, `rdr::` | C++ layers                                                          |

Everything else in a `.c` file is `static`. Internal headers are not installed.

Names are snake_case in C and C++. Types are lowercase (`rd_node`, `rd_style`). File-local types drop the prefix
(`vlist`, `popup`).

## Files

- C sources are `.c`, C headers `.h`; C++ sources `.cc`, headers `.hh`.
- One public header per subsystem; `raydom.h` includes them all.
- A subsystem's private interface goes in `src/<name>_internal.h` when more than one file uses it, otherwise it
  stays `static`.

## Style

- clang-format with the repository's `.clang-format`. It sets no column limit; keep lines near 120 by hand.
- Comments are few and say why, or what a reader can't see from the code: invariants, units, ownership, the CSS
  section an algorithm follows. Don't narrate code.
- Public headers document each function in one or two lines above it: what it does, what it returns on failure,
  who owns what.
- Keep stack frames small. Emscripten's default stack is 64 KB, and an overflow there corrupts memory silently
  rather than crashing where it happens. Each `rd_style` is about 500 bytes, and every compound literal takes its
  own slot, so a function building many styles adds up: extras' theme definitions are split into non-inlined
  parts for this. Check with `emcc -O3 -Wframe-larger-than=16384`. The vendored `stbi__load_main` takes 38 KB,
  which is acceptable only because image loading runs from shallow call stacks.

## Errors

The core never aborts on bad input. A function that can fail returns `false`, a null handle, or does nothing, and
says so in its header comment. Stale node handles are always safe to pass. Allocation failure is handled where it
is reasonable to recover (a list that can't grow keeps its old contents), and `assert` is reserved for invariants
the library itself maintains.

The C++ layer never throws. It allocates callables with `new (std::nothrow)` and reports failure the same way as C.
Code that builds with `-fno-exceptions` must keep building.

## Ownership

- The context owns everything created through it. `rd_ctx_destroy` frees it all, in a defined order (see
  [The context and the tree](architecture/tree.md#destruction)).
- Anything raydom keeps a pointer to has a destroy callback (`rd_destroy_fn`) next to it: userdata, listener data,
  measure and paint data, timer data, extension data.
- **Node userdata belongs to the application.** Nothing in the core or extras may store its own data there; the
  showcase reads userdata on hovered nodes as its own type, and a component that wrote an index into it crashed it.
  Components keep state in their own tables (see [Extras](architecture/extras.md#state)).

## Tests

Every behavior change comes with a test. Tests are plain C or C++ programs using `tests/check.h`; see
[Writing tests](guides/tests.md).

## Commits

Small, focused commits with a message that says what changed and why. Keep vendored code unmodified; upstream fixes
instead (see [Design decisions](decisions.md#vendored-code-stays-unmodified)).
