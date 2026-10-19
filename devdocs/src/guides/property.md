# Adding a style property

Example: a `letter_spacing` length, inherited, affecting layout.

1. **Declare it.** Add a line to `RD_STYLE_PROPS` in `include/raydom/style.h`:

   ```c
   X(letter_spacing, rd_val, VAL, 1, 1)  \
   ```

   Pick the kind (`VAL`, `COLOR`, `EDGES`, `CORNERS`, `SHADOW`, `ENUM`, ...), whether it's inherited, and whether it
   affects layout. Position in the list is the struct layout; append near related properties.

2. **Default it.** Add it to `default_style` in `src/style.c`. A debug build asserts at context creation that every
   property has a default.

3. **Use it.** Read `ctx->styles[i].computed.letter_spacing` where it matters (here, `text.c` tokenizing and the
   renderer's glyph placement). Changes already mark layout dirty because of the `affects_layout` flag.

4. **Animate it, if it should.** For kinds `VAL`, `COLOR`, `EDGES`, `CORNERS`, `SHADOW`, add it to `GROUP[]` in
   `src/anim.c` under the right `RD_TRANSITION_*` group. Without a group it changes at once.

5. **Document it.** Add a one-line description to `DESCRIPTIONS` and the name to a section of `GROUPS` in
   `docs/tools/preprocess.py`. The docs build fails until you do.

6. **Test it.** Cover inheritance and the default in `tests/test_style.c`, and the behavior in the subsystem's test.

The C++ setter (`rd::Style::letter_spacing`) and the property reference page generate themselves.

A property with a new **enum** type also needs the `typedef enum ... : uint8_t` with `_UNSET = 0` first. A new
**kind** of value needs a `MERGE_`, `EQ_`, `SET_` and `RD_HH_SETTER_` case, a kind in `anim.c`, and a default
formatter in `preprocess.py`.
