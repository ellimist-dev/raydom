#!/usr/bin/env python3
"""mdbook preprocessor: expands {{version}}, {{version_minor}} and {{properties}} from the sources at this commit."""

import json
import textwrap
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

DESCRIPTIONS = {
    "display": "Layout mode of the node's children; `NONE` removes the node from layout, drawing and hit-testing.",
    "position": "`ABSOLUTE` takes the node out of flow and places it against the parent's padding box with `inset`.",
    "overflow_x": "`HIDDEN` clips children horizontally; `SCROLL` clips and scrolls.",
    "overflow_y": "`HIDDEN` clips children vertically; `SCROLL` clips and scrolls.",
    "flex_direction": "Main axis of a flex container.",
    "flex_wrap": "Whether flex items wrap onto several lines.",
    "justify_content": "Distribution of items along the main axis (flex) or of tracks along the row axis (grid).",
    "align_items": "Default cross-axis alignment of the children.",
    "align_self": "Cross-axis alignment of this item; `AUTO` uses the parent's `align_items`.",
    "align_content": "Distribution of flex lines or grid rows along the cross axis.",
    "justify_items": "Grid: default inline-axis alignment of items in their areas.",
    "justify_self": "Grid: inline-axis alignment of this item; `AUTO` uses the parent's `justify_items`.",
    "grid_template_columns": "Explicit column tracks, from `rd_tracks_create`.",
    "grid_template_rows": "Explicit row tracks, from `rd_tracks_create`.",
    "grid_auto_columns": "Size of implicitly created columns.",
    "grid_auto_rows": "Size of implicitly created rows.",
    "grid_auto_flow": "Whether auto-placement fills rows or columns first.",
    "grid_column_start": "Start line or span of the item's columns.",
    "grid_column_end": "End line or span of the item's columns.",
    "grid_row_start": "Start line or span of the item's rows.",
    "grid_row_end": "End line or span of the item's rows.",
    "flex_grow": "Share of positive free space, as `rd_num`.",
    "flex_shrink": "Share of negative free space, weighted by basis, as `rd_num`. Default 0: items don't shrink.",
    "flex_basis": "Main size before free space is distributed.",
    "width": "Border-box width.",
    "height": "Border-box height.",
    "min_width": "Lower bound on width. `auto` is 0.",
    "min_height": "Lower bound on height. `auto` is 0.",
    "max_width": "Upper bound on width. `auto` is none.",
    "max_height": "Upper bound on height. `auto` is none.",
    "margin": "Outer spacing. `auto` margins absorb free space in flex layout.",
    "padding": "Inner spacing. Percentages resolve against the containing block's width.",
    "border_width": "Border thickness per side, in pixels.",
    "inset": "Offsets from the containing block for absolute nodes; relative offsets otherwise.",
    "row_gap": "Space between rows, or between items in a column flex container.",
    "column_gap": "Space between columns, or between items in a row flex container.",
    "background": "Fill of the border box, inside the rounded corners.",
    "border_color": "Color of the border.",
    "border_radius": "Corner radii.",
    "opacity": "Opacity of the node and its subtree, 0 to 1, as `rd_num`.",
    "box_shadow": "Outer shadow: offset, blur, spread and color.",
    "color": "Text color.",
    "font": "Font handle from `rd_font_load`; `{0, true}` is the default font.",
    "font_size": "Text size in pixels.",
    "line_height": "Line box height: `rd_num` times `font_size`, `rd_px`, or `rd_pct` of `font_size`.",
    "white_space": "Collapsing of spaces and newlines, and whether lines wrap.",
    "text_overflow": "Treatment of text cut by the box or by `line_clamp`.",
    "user_select": "Whether dragging over the text selects it.",
    "line_clamp": "Maximum number of lines shown, as `rd_num`; 0 is unlimited.",
    "pointer_events": "`NONE` makes the node invisible to hit-testing.",
    "focusable": "Whether the node takes focus, and whether Tab reaches it.",
    "caret_color": "Input caret color; transparent uses `color`.",
    "selection_color": "Background of selected text.",
    "text_align": "Horizontal alignment of each line in the content box.",
}


GROUPS = [
    ("Box model", ["display", "position", "width", "height", "min_width", "min_height", "max_width", "max_height",
                   "margin", "padding", "border_width", "inset", "overflow_x", "overflow_y"]),
    ("Flexbox", ["flex_direction", "flex_wrap", "flex_grow", "flex_shrink", "flex_basis"]),
    ("Alignment and gaps", ["justify_content", "align_items", "align_self", "align_content", "justify_items",
                            "justify_self", "row_gap", "column_gap"]),
    ("Grid", ["grid_template_columns", "grid_template_rows", "grid_auto_columns", "grid_auto_rows", "grid_auto_flow",
              "grid_column_start", "grid_column_end", "grid_row_start", "grid_row_end"]),
    ("Paint", ["background", "border_color", "border_radius", "opacity", "box_shadow"]),
    ("Text", ["color", "font", "font_size", "line_height", "white_space", "text_overflow", "line_clamp",
              "text_align", "user_select", "caret_color", "selection_color"]),
    ("Interaction", ["pointer_events", "focusable"]),
]


def version():
    m = re.search(r"project\(raydom\s+VERSION\s+(\d+)\.(\d+)\.(\d+)", (ROOT / "CMakeLists.txt").read_text())
    return m.group(1), m.group(2), m.group(3)


def enum_values(header):
    out = {}
    for m in re.finditer(r"typedef enum (\w+)\s*:\s*\w+\s*\{(.*?)\}\s*\w+;", header, re.S):
        names = re.findall(r"^\s*(RD_\w+)", m.group(2), re.M)
        out[m.group(1)] = [n for n in names if not n.endswith("_UNSET")]
    return out


def short(enum_name, values):
    # Longest common prefix up to an underscore: RD_FLEX_ROW, RD_FLEX_COLUMN -> RD_FLEX_
    prefix = values[0] if values else enum_name
    for v in values[1:]:
        while not v.startswith(prefix):
            prefix = prefix[:-1]
    prefix = prefix[: prefix.rfind("_") + 1] if "_" in prefix else ""
    return prefix, [v[len(prefix):] for v in values]


UNIT = {"RD_UNIT_PX": "px", "RD_UNIT_PCT": "%", "RD_UNIT_NUM": "", "RD_UNIT_FR": "fr"}


def fmt_val(tok):
    tok = tok.strip()
    if tok == "AUTO":
        return "auto"
    if tok == "PX0":
        return "0px"
    m = re.fullmatch(r"\{\s*([-\d.f]+)\s*,\s*(RD_UNIT_\w+)\s*\}", tok)
    if m:
        n = m.group(1).rstrip("f")
        if m.group(2) == "RD_UNIT_AUTO":
            return "auto"
        return n + UNIT.get(m.group(2), "")
    return tok


def split_top(s):
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    if cur.strip():
        parts.append(cur)
    return [p.strip() for p in parts]


def fmt_default(kind, raw, prefixes):
    raw = raw.strip()
    if kind == "ENUM":
        for prefix in prefixes:
            if raw.startswith(prefix):
                return "`" + raw[len(prefix):] + "`"
        return "`" + raw + "`"
    if kind == "VAL":
        return "`" + fmt_val(raw) + "`"
    inner = split_top(raw[1:-1]) if raw.startswith("{") else [raw]
    if kind == "EDGES" or kind == "CORNERS":
        vals = [fmt_val(p) for p in inner]
        return "`" + vals[0] + "`" if len(set(vals)) == 1 else "`" + " ".join(vals) + "`"
    if kind == "COLOR":
        r, g, b, a = (int(x) for x in inner[:4])
        return "transparent" if a == 0 else f"`rgba({r}, {g}, {b}, {a})`"
    if kind == "SHADOW":
        return "none"
    if kind == "TRACKS":
        return "none"
    if kind == "TRACK":
        vals = [fmt_val(p) for p in inner]
        return "`" + vals[0] + "`" if vals[0] == vals[1] else f"`minmax({vals[0]}, {vals[1]})`"
    if kind == "GRID_LINE":
        return "`auto`"
    if kind == "FONT":
        return "default font"
    return "`" + raw + "`"


TYPES = {
    "VAL": "`rd_val`", "EDGES": "`rd_edges`", "CORNERS": "`rd_corners`", "COLOR": "`rd_color`",
    "SHADOW": "`rd_shadow`", "TRACKS": "`rd_tracks`", "TRACK": "`rd_track`", "GRID_LINE": "`rd_grid_line`",
    "FONT": "`rd_font`",
}


def properties():
    header = (ROOT / "include/raydom/style.h").read_text()
    source = (ROOT / "src/style.c").read_text()
    props = re.findall(r"X\((\w+),\s*(\w+),\s*(\w+),\s*([01]),\s*([01])\)", header)
    enums = enum_values(header)
    body = re.search(r"default_style\s*=\s*\{(.*?)\n\};", source, re.S).group(1)
    defaults = {}
    for m in re.finditer(r"\.(\w+)\s*=\s*(.+?),\s*$", body, re.M):
        defaults[m.group(1)] = m.group(2)

    missing = [p[0] for p in props if p[0] not in DESCRIPTIONS]
    if missing:
        sys.exit("docs/tools/preprocess.py: no description for style properties: " + ", ".join(missing))

    grouped = [n for _, names in GROUPS for n in names]
    if sorted(grouped) != sorted(p[0] for p in props):
        sys.exit("docs/tools/preprocess.py: GROUPS doesn't list every style property exactly once")

    prefixes = sorted((short(n, v)[0] for n, v in enums.items() if v), key=len, reverse=True)
    by_name = {p[0]: p for p in props}
    out, enum_sections, seen = [], [], set()
    for title, names in GROUPS:
        out += [f"## {title}", "", "| Property | Default | Description |", "| -------- | ------- | ----------- |"]
        for name in names:
            _, ctype, kind, inherited, layout = by_name[name]
            if kind == "ENUM":
                prefix, values = short(ctype, enums.get(ctype, []))
                t = f"[`{ctype}`](#{ctype.replace('_', '-')})"
                if ctype not in seen:
                    seen.add(ctype)
                    enum_sections.append(f"### {ctype}\n\n`{prefix}*`: " + ", ".join(f"`{v}`" for v in values) + "\n")
            else:
                t = TYPES.get(kind, f"`{ctype}`")
            flags = [t] + (["inherited"] if inherited == "1" else []) + ([] if layout == "1" else ["paint only"])
            d = fmt_default(kind, defaults.get(name, "?"), prefixes)
            out.append(f"| `{name}` | {d} | {DESCRIPTIONS[name]}<br>*{', '.join(flags)}* |")
        out.append("")
    rows = out
    return "\n".join(rows) + "\n## Enumerations\n\n" + "\n".join(enum_sections)


def expand(text, subs):
    for k, v in subs.items():
        if "{{" + k + "}}" in text:
            text = text.replace("{{" + k + "}}", v() if callable(v) else v)
    return text


FENCE = re.compile(r"^(```[^\n]*\n)(.*?)(^```)", re.S | re.M)


def dedent_code(text):
    # Anchored includes keep the indentation they have inside a function body.
    return FENCE.sub(lambda m: m.group(1) + textwrap.dedent(m.group(2)) + m.group(3), text)


def walk(node, subs):
    if isinstance(node, dict):
        ch = node.get("Chapter")
        if isinstance(ch, dict) and isinstance(ch.get("content"), str):
            ch["content"] = dedent_code(expand(ch["content"], subs))
        for v in node.values():
            walk(v, subs)
    elif isinstance(node, list):
        for v in node:
            walk(v, subs)


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "supports":
        return 0
    if len(sys.argv) > 1 and sys.argv[1] == "properties":
        print(properties())
        return 0
    major, minor, patch = version()
    subs = {"version": f"{major}.{minor}.{patch}", "version_minor": f"{major}.{minor}", "properties": properties}
    _, book = json.load(sys.stdin)
    walk(book, subs)
    json.dump(book, sys.stdout)
    return 0


if __name__ == "__main__":
    sys.exit(main())
