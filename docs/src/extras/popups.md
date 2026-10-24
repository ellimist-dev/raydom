# Menus, selects, modals and tooltips

These popups live in the [overlay](../guide/overlay.md), use [anchoring](../guide/overlay.md#anchoring) to sit next
to what opened them, and share one set of dismissal rules:

- A press outside closes the topmost menu or list; a press on the node that opened it doesn't.
- Escape closes the topmost popup.
- Scrolling outside closes menus and lists.
- Closing returns focus to the node that opened the popup, if focus was inside it.

`rdx_popup_close(ctx, node)` closes the popup any node belongs to, so a button inside a modal can close it.

```cpp
#include <raydom/extras/popups.hh>  // C: popups.h
```

## Menus

```cpp
{{#include ../../examples/popups.cc:menu}}
```

A menu takes a list of items: a label, an optional shortcut hint shown at the right (it doesn't bind the key), and a
disabled flag. An item with no label is a separator. The callback gets the index into the list of the chosen item,
after the menu has closed, so it can open a modal or another menu. Dismissing the menu calls nothing.

The menu takes focus when it opens. Up and Down move through the enabled items and wrap, Home and End jump, Enter or
Space chooses, and Tab closes the menu. Hovering an item highlights it.

## Context menus

```cpp
{{#include ../../examples/popups.cc:context}}
```

`rdx_context_menu_attach` opens the menu at the pointer when the node is pressed with the secondary button. The items
are copied, and the callback lives as long as the node. A menu still open when the node is destroyed stays open, and
choosing from it does nothing. Likewise a select's list that outlives its select ignores the choice, and closes at
the next press, key or pointer move.

## Select

```cpp
{{#include ../../examples/popups.cc:select}}
```

A select shows the chosen option, or the placeholder, with a chevron. Clicking it, or Enter or Space, opens a list
as wide as the select, with the chosen option checked and focused; clicking the select again closes it. Up and Down
on the closed select change the choice directly. `CHANGE` fires on the select when the user chooses;
`rdx::set_selected` (`rdx_select_set`) doesn't fire it.

## Modals

```cpp
{{#include ../../examples/popups.cc:modal}}
```

A modal dims the window and centers a card with an optional title and × button. `rdx::modal` returns the card's
body, a column for your content and buttons. While it's open:

- Tab and Shift+Tab stay inside it (it sets the [focus scope](../guide/input.md#focus)).
- At the next clock update, focus moves to the first focusable node in the body, or the card.
- Escape, the × button and a press on the backdrop close it, unless it's `persistent`.

`on_close` runs whenever it closes, from any of those or from `rdx_popup_close`. `width` sets the card's width
(default 440px); the card never grows beyond the window and its body scrolls.

## Tooltips

```cpp
{{#include ../../examples/popups.cc:tooltip}}
```

The tooltip appears above the node, flipping below near the top edge, after the pointer rests on it for half a
second, and at once when the node gets keyboard focus. Leaving, pressing, scrolling or blurring hides it. Attaching
again replaces the text. The tooltip ignores the pointer, so it never covers what it describes.

## Restyling

The popups' classes are in `rdx_theme(ctx)` with the other extras classes:

| Classes                                                         | Parts                               |
| --------------------------------------------------------------- | ----------------------------------- |
| `menu`, `menu_item`, `menu_label`, `menu_shortcut`, `menu_separator` | Menus and select lists          |
| `menu_check`                                                    | The checkmark column of select lists |
| `select`, `select_value`, `select_chevron`                      | The select button                   |
| `modal_backdrop`, `modal`, `modal_header`, `modal_title`, `modal_close`, `modal_body` | Modal parts  |
| `tooltip`                                                       | Tooltips                            |

Menu items highlight on `RD_STATE_FOCUS` as well as hover, so keyboard and pointer show the same highlight.
