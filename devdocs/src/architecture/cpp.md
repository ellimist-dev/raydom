# The C++ layer

`include/raydom/raydom.hh` is header-only and adds no state: every wrapper holds the context pointer and a handle.

| Type           | Owns                                                  |
| -------------- | ----------------------------------------------------- |
| `rd::Context`  | The context (RAII, move-only)                          |
| `rd::Node`     | Nothing: `{rd_ctx*, rd_node}`; setters are `const`     |
| `rd::Class`    | Nothing: `{rd_ctx*, rd_class}`; setters are `const`    |
| `rd::Style`    | An `rd_style` value, with a setter per property        |
| `rd::Event`    | A view of an `rd_event*`, valid during the listener    |

## Handles are const

`rd::Node` and `rd::Class` setters are `const` and return `const Node&` / `const Class&` for chaining. They change
the node or class in the context, not the handle, like `std::span`'s element access. This lets a lambda that
captures a node by value call setters without `mutable` or a copy. New methods on handle types follow suit.

## Callables

Lambdas go through one pattern: allocate the decayed callable with `new (std::nothrow)`, pass a captureless trampoline
as the C callback and the allocation as data, and pass a deleter as the destroy callback. If registration fails, the
wrapper deletes the allocation itself. The C side owns the callable from then on, so captures live exactly as long as
the listener, timer or measure function.

## Setters from the X-macro

`rd::Style` gets its setters by expanding `RD_STYLE_PROPS`, with extra overloads per kind (edges from one, two or
four values; corners from one; tracks from an `rd_val`; shadows from numbers). A new property gets its C++ setter
without touching `raydom.hh`.

## No exceptions

The layer compiles with `-fno-exceptions`. Allocation failures return null handles or `false`, as in C. Keep it
that way: applications on platforms without exceptions are a target.
