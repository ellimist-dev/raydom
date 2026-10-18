# Style properties

Every field of `rd_style`, generated from the `RD_STYLE_PROPS` X-macro in `include/raydom/style.h` and the defaults
in `src/style.c` at this version.

- **Default** is the value a node has when nothing sets the property and it isn't inherited.
- *inherited* properties take the parent's computed value when unset.
- *paint only* properties change drawing without triggering layout. All others trigger relayout when they change.

In C++, each property is a setter of the same name on `rd::Style`. Enumerated values are written in full in C, for
example `RD_DISPLAY_GRID`.

## Box model

| Property | Default | Description |
| -------- | ------- | ----------- |
| `display` | `FLEX` | Layout mode of the node's children; `NONE` removes the node from layout, drawing and hit-testing.<br>*[`rd_display`](#rd-display)* |
| `position` | `RELATIVE` | `ABSOLUTE` takes the node out of flow and places it against the parent's padding box with `inset`.<br>*[`rd_position`](#rd-position)* |
| `width` | `auto` | Border-box width.<br>*`rd_val`* |
| `height` | `auto` | Border-box height.<br>*`rd_val`* |
| `min_width` | `auto` | Lower bound on width. `auto` is 0.<br>*`rd_val`* |
| `min_height` | `auto` | Lower bound on height. `auto` is 0.<br>*`rd_val`* |
| `max_width` | `auto` | Upper bound on width. `auto` is none.<br>*`rd_val`* |
| `max_height` | `auto` | Upper bound on height. `auto` is none.<br>*`rd_val`* |
| `margin` | `0px` | Outer spacing. `auto` margins absorb free space in flex layout.<br>*`rd_edges`* |
| `padding` | `0px` | Inner spacing. Percentages resolve against the containing block's width.<br>*`rd_edges`* |
| `border_width` | `0px` | Border thickness per side, in pixels.<br>*`rd_edges`* |
| `inset` | `auto` | Offsets from the containing block for absolute nodes; relative offsets otherwise.<br>*`rd_edges`* |
| `overflow_x` | `VISIBLE` | `HIDDEN` clips children horizontally; `SCROLL` clips and scrolls.<br>*[`rd_overflow`](#rd-overflow)* |
| `overflow_y` | `VISIBLE` | `HIDDEN` clips children vertically; `SCROLL` clips and scrolls.<br>*[`rd_overflow`](#rd-overflow)* |

## Flexbox

| Property | Default | Description |
| -------- | ------- | ----------- |
| `flex_direction` | `COLUMN` | Main axis of a flex container.<br>*[`rd_flex_direction`](#rd-flex-direction)* |
| `flex_wrap` | `NOWRAP` | Whether flex items wrap onto several lines.<br>*[`rd_flex_wrap`](#rd-flex-wrap)* |
| `flex_grow` | `0` | Share of positive free space, as `rd_num`.<br>*`rd_val`* |
| `flex_shrink` | `0` | Share of negative free space, weighted by basis, as `rd_num`. Default 0: items don't shrink.<br>*`rd_val`* |
| `flex_basis` | `auto` | Main size before free space is distributed.<br>*`rd_val`* |

## Alignment and gaps

| Property | Default | Description |
| -------- | ------- | ----------- |
| `justify_content` | `NORMAL` | Distribution of items along the main axis (flex) or of tracks along the row axis (grid).<br>*[`rd_align`](#rd-align)* |
| `align_items` | `STRETCH` | Default cross-axis alignment of the children.<br>*[`rd_align`](#rd-align)* |
| `align_self` | `AUTO` | Cross-axis alignment of this item; `AUTO` uses the parent's `align_items`.<br>*[`rd_align`](#rd-align)* |
| `align_content` | `NORMAL` | Distribution of flex lines or grid rows along the cross axis.<br>*[`rd_align`](#rd-align)* |
| `justify_items` | `STRETCH` | Grid: default inline-axis alignment of items in their areas.<br>*[`rd_align`](#rd-align)* |
| `justify_self` | `AUTO` | Grid: inline-axis alignment of this item; `AUTO` uses the parent's `justify_items`.<br>*[`rd_align`](#rd-align)* |
| `row_gap` | `0px` | Space between rows, or between items in a column flex container.<br>*`rd_val`* |
| `column_gap` | `0px` | Space between columns, or between items in a row flex container.<br>*`rd_val`* |

## Grid

| Property | Default | Description |
| -------- | ------- | ----------- |
| `grid_template_columns` | none | Explicit column tracks, from `rd_tracks_create`.<br>*`rd_tracks`* |
| `grid_template_rows` | none | Explicit row tracks, from `rd_tracks_create`.<br>*`rd_tracks`* |
| `grid_auto_columns` | `auto` | Size of implicitly created columns.<br>*`rd_track`* |
| `grid_auto_rows` | `auto` | Size of implicitly created rows.<br>*`rd_track`* |
| `grid_auto_flow` | `ROW` | Whether auto-placement fills rows or columns first.<br>*[`rd_grid_flow`](#rd-grid-flow)* |
| `grid_column_start` | `auto` | Start line or span of the item's columns.<br>*`rd_grid_line`* |
| `grid_column_end` | `auto` | End line or span of the item's columns.<br>*`rd_grid_line`* |
| `grid_row_start` | `auto` | Start line or span of the item's rows.<br>*`rd_grid_line`* |
| `grid_row_end` | `auto` | End line or span of the item's rows.<br>*`rd_grid_line`* |

## Paint

| Property | Default | Description |
| -------- | ------- | ----------- |
| `background` | transparent | Fill of the border box, inside the rounded corners.<br>*`rd_color`, paint only* |
| `border_color` | transparent | Color of the border.<br>*`rd_color`, paint only* |
| `border_radius` | `0px` | Corner radii.<br>*`rd_corners`, paint only* |
| `opacity` | `1` | Opacity of the node and its subtree, 0 to 1, as `rd_num`.<br>*`rd_val`, paint only* |
| `box_shadow` | none | Outer shadow: offset, blur, spread and color.<br>*`rd_shadow`, paint only* |

## Text

| Property | Default | Description |
| -------- | ------- | ----------- |
| `color` | `rgba(0, 0, 0, 255)` | Text color.<br>*`rd_color`, inherited, paint only* |
| `font` | default font | Font handle from `rd_font_load`; `{0, true}` is the default font.<br>*`rd_font`, inherited* |
| `font_size` | `16px` | Text size in pixels.<br>*`rd_val`, inherited* |
| `line_height` | `1.2` | Line box height: `rd_num` times `font_size`, `rd_px`, or `rd_pct` of `font_size`.<br>*`rd_val`, inherited* |
| `white_space` | `NORMAL` | Collapsing of spaces and newlines, and whether lines wrap.<br>*[`rd_white_space`](#rd-white-space), inherited* |
| `text_overflow` | `CLIP` | Treatment of text cut by the box or by `line_clamp`.<br>*[`rd_text_overflow`](#rd-text-overflow), paint only* |
| `line_clamp` | `0` | Maximum number of lines shown, as `rd_num`; 0 is unlimited.<br>*`rd_val`* |
| `text_align` | `START` | Horizontal alignment of each line in the content box.<br>*[`rd_text_align`](#rd-text-align), inherited, paint only* |
| `user_select` | `AUTO` | Whether dragging over the text selects it.<br>*[`rd_user_select`](#rd-user-select), inherited, paint only* |
| `caret_color` | transparent | Input caret color; transparent uses `color`.<br>*`rd_color`, inherited, paint only* |
| `selection_color` | `rgba(91, 140, 255, 90)` | Background of selected text.<br>*`rd_color`, inherited, paint only* |

## Interaction

| Property | Default | Description |
| -------- | ------- | ----------- |
| `pointer_events` | `AUTO` | `NONE` makes the node invisible to hit-testing.<br>*[`rd_pointer_events`](#rd-pointer-events), inherited, paint only* |
| `focusable` | `NONE` | Whether the node takes focus, and whether Tab reaches it.<br>*[`rd_focusable`](#rd-focusable), paint only* |

## Enumerations

### rd_display

`RD_DISPLAY_*`: `FLEX`, `GRID`, `NONE`

### rd_position

`RD_POSITION_*`: `RELATIVE`, `ABSOLUTE`

### rd_overflow

`RD_OVERFLOW_*`: `VISIBLE`, `HIDDEN`, `SCROLL`

### rd_flex_direction

`RD_FLEX_*`: `ROW`, `COLUMN`, `ROW_REVERSE`, `COLUMN_REVERSE`

### rd_flex_wrap

`RD_FLEX_*`: `NOWRAP`, `WRAP`, `WRAP_REVERSE`

### rd_align

`RD_ALIGN_*`: `AUTO`, `NORMAL`, `START`, `END`, `CENTER`, `STRETCH`, `BASELINE`, `SPACE_BETWEEN`, `SPACE_AROUND`, `SPACE_EVENLY`

### rd_grid_flow

`RD_GRID_FLOW_*`: `ROW`, `COLUMN`

### rd_white_space

`RD_WHITE_SPACE_*`: `NORMAL`, `NOWRAP`, `PRE`, `PRE_WRAP`

### rd_text_overflow

`RD_TEXT_OVERFLOW_*`: `CLIP`, `ELLIPSIS`

### rd_text_align

`RD_TEXT_ALIGN_*`: `START`, `CENTER`, `END`

### rd_user_select

`RD_USER_SELECT_*`: `AUTO`, `NONE`

### rd_pointer_events

`RD_POINTER_EVENTS_*`: `AUTO`, `NONE`

### rd_focusable

`RD_FOCUSABLE_*`: `NONE`, `TAB`, `POINTER`

