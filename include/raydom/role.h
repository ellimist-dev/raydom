#ifndef RAYDOM_ROLE_H
#define RAYDOM_ROLE_H

#include <raydom/input.h>

#ifdef __cplusplus
extern "C" {
#endif

// What a node is, for assistive technology. A subset of AccessKit's roles, so a future
// accessibility backend maps them directly. Roles carry no behavior, except that interactive
// roles (button, link, checkbox, radio button, switch, slider, menu item, combo box, tab, splitter)
// take focus by default, as inputs do.
typedef enum rd_role : uint8_t {
  RD_ROLE_AUTO,  // Derived: text input for inputs, text for text leaves, canvas for paint nodes, else generic.
  RD_ROLE_GENERIC,
  RD_ROLE_GROUP,
  RD_ROLE_BUTTON,
  RD_ROLE_LINK,
  RD_ROLE_CHECKBOX,
  RD_ROLE_RADIO_BUTTON,
  RD_ROLE_RADIO_GROUP,
  RD_ROLE_SWITCH,
  RD_ROLE_SLIDER,
  RD_ROLE_TEXT,
  RD_ROLE_LABEL,
  RD_ROLE_HEADING,
  RD_ROLE_TEXT_INPUT,
  RD_ROLE_MULTILINE_TEXT_INPUT,
  RD_ROLE_LIST,
  RD_ROLE_LIST_ITEM,
  RD_ROLE_IMAGE,
  RD_ROLE_CANVAS,
  RD_ROLE_MENU,
  RD_ROLE_MENU_ITEM,
  RD_ROLE_COMBO_BOX,
  RD_ROLE_DIALOG,
  RD_ROLE_TOOLTIP,
  RD_ROLE_TAB_LIST,
  RD_ROLE_TAB,
  RD_ROLE_TAB_PANEL,
  RD_ROLE_SPLITTER,
  RD_ROLE_TABLE,
  RD_ROLE_ROW,
  RD_ROLE_CELL,
  RD_ROLE_COLUMN_HEADER,
  RD_ROLE_TREE,
  RD_ROLE_TREE_ITEM,
} rd_role;

bool rd_node_set_role(rd_ctx* ctx, rd_node node, rd_role role);
// The resolved role: never RD_ROLE_AUTO for a live node.
rd_role rd_node_role(const rd_ctx* ctx, rd_node node);

// Constructors that set a role along with the content. Each returns a detached node, or null on
// failure. A button with null text is empty, for icon or composite content added as children.
rd_node rd_node_create_text(rd_ctx* ctx, const char* utf8, size_t len);
rd_node rd_node_create_button(rd_ctx* ctx, const char* utf8, size_t len);
rd_node rd_node_create_input(rd_ctx* ctx, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif
