# Focus, keyboard and text input

## Focus

A node can take focus when its `focusable` is `TAB` (pointer, code and Tab) or `POINTER` (pointer and code). Inputs
and nodes with an interactive [role](roles.md), such as buttons from `rd_node_create_button`, are focusable without
the style. Pressing the primary button focuses the target or its nearest focusable ancestor, and clears focus if there is
none. A listener that calls `rd_event_prevent_default` on `POINTER_DOWN` keeps focus where it was.

```cpp
rd::Class button_style = ctx.create_class(rd::Style().padding(6_px, 12_px));
button_style.on(RD_STATE_FOCUS_VISIBLE, rd::Style().box_shadow(0, 0, 0, rd::rgba(91, 140, 255, 200), 2));
rd::Node save  = ctx.create_button("Save").add_class(button_style);  // focusable by role
rd::Node badge = ctx.create(rd::Style().focusable(RD_FOCUSABLE_TAB));  // any node, by style
int      saves = 0;
save.on_click([&] { ++saves; });  // also fires on Enter or Space while focused
```

Tab and Shift+Tab move through `TAB` nodes in tree order, wrapping, skipping hidden and disabled subtrees.
`rd_node_focus(ctx, node)` focuses from code; a null node clears focus. `rd_ctx_focus_next` steps like Tab. A focus
change fires `BLUR`, `FOCUS_OUT`, `FOCUS`, `FOCUS_IN`, in that order.

`rd_ctx_set_focus_scope(ctx, node)` keeps Tab and Shift+Tab inside a subtree, as a modal dialog needs; null
restores the whole tree, and so does destroying the scope node. Pointer and programmatic focus aren't restricted.

Enter and Space on a focused node that isn't an input fire `CLICK`, so keyboard users can activate buttons without
extra code.

## Keyboard

```cpp
ctx.root().on(RD_EVENT_KEY_DOWN, [&](rd::Event& e) {
  if (e.key() == RD_KEY_ESCAPE) ctx.blur();
});
```

Keys go to the focused node, or the root when nothing has focus, and bubble. `ev->mods` is a mask of
`RD_MOD_SHIFT`, `RD_MOD_CTRL`, `RD_MOD_ALT`, `RD_MOD_SUPER`. Calling `rd_event_prevent_default` in a `KEY_DOWN`
listener suppresses Tab navigation, activation, and editing for that key.

## Text inputs

```cpp
rd::Node name  = ctx.create_input(false, rd::Style().width(240_px)).placeholder("Your name");
rd::Node notes = ctx.create_input(/*multiline=*/true, rd::Style().min_height(80_px));

std::string value;
name.on(RD_EVENT_INPUT, [&](rd::Event& e) { value = std::string(e.target().text()); });  // every edit
name.on(RD_EVENT_CHANGE, [&] { /* Enter, or leaving the field after editing */ });
```

`rd_node_create_input` creates an input, and `rd_node_make_input` turns an existing node into one: an editable text
leaf. A single-line input scrolls horizontally; a multi-line
input (`RD_INPUT_MULTILINE`) wraps and grows. `rd_node_text` reads the value and `rd_node_set_text` replaces it.
`rd_input_select` and `rd_input_selection` set and read the selection as byte offsets; the caret is the moving end.

`INPUT` fires after every edit. `CHANGE` fires when edits are committed: Enter in a single-line input, or blur after
an edit.

Editing keys, with Shortcut meaning Cmd on macOS and Ctrl elsewhere, and Word meaning Option on macOS and Ctrl
elsewhere:

| Keys                        | Action                                  |
| --------------------------- | --------------------------------------- |
| Arrows, Home, End           | Move; Shift extends the selection       |
| Word + Left/Right           | Move by word                            |
| Shortcut + Home/End         | Start or end of the value               |
| Backspace, Delete           | Delete; Word deletes a word; on macOS, Cmd deletes to the line edge |
| Shortcut + A                | Select all                              |
| Shortcut + C, X, V          | Copy, cut, paste                        |
| Shortcut + Z                | Undo; with Shift, redo                  |
| Shortcut + Y                | Redo                                    |
| Enter                       | Newline in multi-line; `CHANGE` in single-line |

Consecutive typed characters form one undo step.

## Clipboard

Inputs use an in-memory clipboard by default. `rd_update` installs raylib's system clipboard. Without raylib, provide
one:

```c
rd_ctx_set_clipboard(ctx, get_text, set_text, data);
```

## Driving input in tests

The C++ context has `press(key)` and `type(utf8)` helpers over `rd_ctx_key` and `rd_ctx_text`:

```cpp
ctx.press(RD_KEY_TAB);  // focus moves to the first focusable node: name
ctx.type("Ada");        // text events, one per codepoint
ctx.press(RD_KEY_TAB);  // notes
ctx.press(RD_KEY_TAB);  // save
ctx.press(RD_KEY_ENTER);
```
