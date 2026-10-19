# Documentation

There are two books:

| Book        | Path        | Audience      | Published at              |
| ----------- | ----------- | ------------- | ------------------------- |
| User book   | `docs/`     | Applications  | `/<tag>/`, `/latest/`, `/dev/` |
| This book   | `devdocs/`  | Contributors  | `/internals/`, from `main` |

## Samples are tests

Every code sample in the user book is an `{{#include}}` of an anchored region of a program in `docs/examples/`. CMake
builds every program there; programs that don't open a window also run as tests (`doc_<name>_<ext>`). A sample that
stops compiling or stops doing what the prose says fails `ctest`. Write new samples the same way: put the snippet
between `// ANCHOR: name` and `// ANCHOR_END: name` in a program that asserts what the chapter claims.

## The preprocessor

`docs/tools/preprocess.py` is an mdbook preprocessor that:

- replaces `{{version}}` and `{{version_minor}}` with the version from `CMakeLists.txt`;
- replaces `{{properties}}` with the property reference, generated from the `RD_STYLE_PROPS` X-macro and the
  defaults in `src/style.c`;
- dedents included code blocks.

Each property needs a description in `DESCRIPTIONS` and a section in `GROUPS`; the build fails otherwise, so a new
property can't ship undocumented.

## Machine-readable output

`docs/tools/llms.py` runs after `mdbook build docs`: it copies the Markdown renderer's output next to each HTML page
and writes `llms.txt` (a page index) and `llms-full.txt` (the whole book). Each page needs a one-line description in
`DESCRIPTIONS`, again enforced.

## Publishing

`.github/workflows/docs.yml` builds on pushes to `main` (into `/dev/`) and on `v*` tags (into `/<tag>/`, and
`/latest/` when it's the newest tag), maintains `versions.json` for the version menu, and commits to `gh-pages`.
Tag folders are never rebuilt, so each version's docs describe that version.
