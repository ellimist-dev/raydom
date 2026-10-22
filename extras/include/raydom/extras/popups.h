#ifndef RAYDOM_EXTRAS_POPUPS_H
#define RAYDOM_EXTRAS_POPUPS_H

#include <raydom/extras/theme.h>

#ifdef __cplusplus
extern "C" {
#endif

// Menus, selects, modals and tooltips. They live in the overlay and close the way desktop popups do:
// a press outside closes the topmost menu or list, Escape closes the topmost popup, and scrolling
// outside closes menus and lists. Closing returns focus to the node that opened the popup.

// Closes the popup that node belongs to: a menu, a select's list, a modal or a tooltip. Any node
// inside the popup works, such as a modal's body. False if node isn't in an open popup.
bool rdx_popup_close(rd_ctx* ctx, rd_node node);

// Menus.

typedef struct rdx_menu_item {
  const char* label RD_DEFAULT({});     // null for a separator
  const char* shortcut RD_DEFAULT({});  // optional hint at the right, such as "Ctrl+C"; it doesn't bind the key
  bool        disabled RD_DEFAULT({});
} rdx_menu_item;

// Called with the index into items of the chosen item, after the menu has closed. Not called when
// the menu is dismissed.
typedef void (*rdx_menu_fn)(rd_ctx* ctx, int index, void* data);

typedef struct rdx_menu_desc {
  const rdx_menu_item* items RD_DEFAULT({});  // copied
  size_t               count RD_DEFAULT({});
  rd_anchor            anchor RD_DEFAULT({});  // a node, for a menu button; or a point, for a context menu
  rdx_menu_fn          fn RD_DEFAULT({});
  void*                data RD_DEFAULT({});
  rd_destroy_fn        destroy RD_DEFAULT({});  // optional: destroy(data) when the menu is destroyed; not on failure
} rdx_menu_desc;

// Opens a menu and focuses it. Up and Down move through the enabled items, Home and End jump, Enter
// or Space chooses, Tab closes. Returns the menu's node.
rd_node rdx_menu_open(rd_ctx* ctx, const rdx_menu_desc* desc);

// Opens the menu at the pointer when target is pressed with the secondary button. The items are
// copied; destroy(data) runs when target is destroyed. False, keeping nothing, on failure.
bool rdx_context_menu_attach(rd_ctx* ctx, rd_node target, const rdx_menu_item* items, size_t count, rdx_menu_fn fn,
                             void* data, rd_destroy_fn destroy);

// Select: a button showing the chosen option that opens a list of options below it. Up and Down on
// the closed select change the choice. CHANGE fires on the select when the user chooses.
// placeholder shows while nothing is chosen.
rd_node rdx_select_create(rd_ctx* ctx, const char* const* options, size_t count, const char* placeholder);
int     rdx_select_selected(const rd_ctx* ctx, rd_node select);  // -1 for none
bool    rdx_select_set(rd_ctx* ctx, rd_node select, int index);  // fires nothing; -1 clears

// Modal dialog: a card over a dimmed backdrop, centered. Tab stays inside it. Escape, the × button and
// a press on the backdrop close it, unless persistent.
typedef struct rdx_modal_desc {
  const char* title RD_DEFAULT({});  // optional
  float       width RD_DEFAULT({});  // 0 means 440px
  bool        persistent RD_DEFAULT({});  // no × button; only rdx_popup_close closes it
  void (*on_close)(rd_ctx* ctx, void* data) RD_DEFAULT({});  // optional, before the modal is destroyed
  void*         data RD_DEFAULT({});
  rd_destroy_fn destroy RD_DEFAULT({});  // optional: destroy(data) when the modal is destroyed; not on failure
} rdx_modal_desc;

// Returns the modal's body, a column to fill with content and buttons. Focus moves to the first
// focusable node in the body once it's filled and laid out, or to the card.
rd_node rdx_modal_open(rd_ctx* ctx, const rdx_modal_desc* desc);

// Tooltip: text shown above node after the pointer rests on it for half a second, or at once when it
// gets keyboard focus. Calling again replaces the text.
bool rdx_tooltip_attach(rd_ctx* ctx, rd_node node, const char* text);

#ifdef __cplusplus
}
#endif

#endif
