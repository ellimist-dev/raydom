#!/usr/bin/env python3
"""After `mdbook build docs`: copies each page's Markdown next to its HTML and writes llms.txt and llms-full.txt."""

import re
import shutil
import sys
from pathlib import Path

DOCS = Path(__file__).resolve().parents[1]
BOOK = DOCS / "book"
HTML, MD = BOOK / "html", BOOK / "markdown"


def summary():
    """(section, title, path) in SUMMARY.md order."""
    out, section = [], ""
    for line in (DOCS / "src/SUMMARY.md").read_text().splitlines():
        if line.startswith("# ") and line != "# Summary":
            section = line[2:].strip()
        m = re.search(r"\[([^\]]+)\]\(([^)]+\.md)\)", line)
        if m:
            out.append((section if line.lstrip().startswith("-") else "", m.group(1), m.group(2)))
    return out


# One line per page for llms.txt. A page missing here fails the build.
DESCRIPTIONS = {
    "introduction.md": "What raydom is and the context/node/style/frame-loop model.",
    "getting-started/installation.md": "CMake FetchContent and find_package setup, build options, headers.",
    "getting-started/quickstart.md": "Complete C and C++ programs that open a window and show styled, clickable text.",
    "getting-started/frame-loop.md": "What rd_update and rd_draw do each frame; driving input and layout without raylib.",
    "guide/tree.md": "Creating, moving, walking and destroying nodes; generational handles; userdata.",
    "guide/styles.md": "Inline styles, classes, state variants, precedence order, inheritance, computed styles.",
    "guide/layout.md": "Flexbox and grid, geometry queries, absolute positioning, scrolling, custom measurement.",
    "guide/text.md": "Fonts, text nodes, wrapping, ellipsis, line clamping, highlights, measuring, selection.",
    "guide/events.md": "Listeners, capture and bubble, event types and fields, pointer capture, default actions.",
    "guide/input.md": "Focus and Tab order, keyboard events, text inputs and their editing keys, clipboard.",
    "guide/roles.md": "Semantic roles and the button/text/input constructors; how roles affect focus.",
    "guide/canvas.md": "Custom raylib drawing inside a node with paint callbacks.",
    "cpp/overview.md": "The rd:: C++ wrapper: ownership, Style builder, literals, callable listeners.",
    "extras/graph.md": "rdx_graph network view: nodes, edges, messages, pan/zoom, graph events.",
    "reference/properties.md": "Every rd_style property with type, default, inheritance and enum values.",
    "reference/values.md": "rd_val units, colors, edges, corners, shadows, grid tracks and lines.",
    "reference/limits.md": "Features raydom does not implement in this version.",
    "agents.md": "Rules for generating correct raydom code, and where the machine-readable docs are.",
    "versioning.md": "Semver policy, per-tag documentation paths, version macros.",
}


def main():
    if not MD.is_dir():
        sys.exit("docs/tools/llms.py: run `mdbook build docs` first")
    pages = summary()
    for _, _, path in pages:
        dst = HTML / path
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(MD / path, dst)

    missing = [p for _, _, p in pages if p not in DESCRIPTIONS]
    if missing:
        sys.exit("docs/tools/llms.py: no llms.txt description for " + ", ".join(missing))

    lines = ["# raydom", "",
             "> A retained-mode UI tree for raylib, in C23 with a C++20 layer: flexbox and grid layout, CSS-like "
             "styles, text, input and drawing.", "",
             "Each page below is Markdown. `llms-full.txt` is the whole book in one file. Read `agents.md` first.", ""]
    groups = {"Start here": []}
    for section, title, path in pages:
        heading = section or ("Optional" if path == "versioning.md" else "Start here")
        groups.setdefault(heading, []).append(f"- [{title}]({path}): {DESCRIPTIONS[path]}")
    groups["Optional"] = groups.pop("Optional", [])  # last, per llmstxt.org
    for heading, items in groups.items():
        if items:
            lines += [f"## {heading}", "", *items, ""]
    (HTML / "llms.txt").write_text("\n".join(lines))

    full = []
    for _, _, path in pages:
        full.append(f"<!-- {path} -->\n\n" + (MD / path).read_text().strip() + "\n")
    (HTML / "llms-full.txt").write_text("\n\n".join(full))


if __name__ == "__main__":
    main()
