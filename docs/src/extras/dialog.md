# File dialogs

`raydom::extras` wraps the operating system's file dialogs: Cocoa panels on macOS, the shell dialog on Windows, and
[xdg-desktop-portal](https://flatpak.github.io/xdg-desktop-portal/) on Linux, which shows the desktop's own dialog
(GNOME, KDE and others). The backends come from
[nativefiledialog-extended](https://github.com/btzy/nativefiledialog-extended) 1.4.1, vendored and compiled into
`raydom::extras`, so applications still link one static library.

```cmake
target_link_libraries(your_app PRIVATE raydom::extras)
```

```cpp
#include <raydom/extras/dialog.hh>  // or dialog.h for C
```

## C++

```cpp
{{#include ../../examples/dialog.cc:cpp}}
```

`rdx::open_file`, `open_files`, `save_file` and `pick_folder` take `rdx::DialogOptions` and return
`rdx::DialogResult`. The result is true when the user chose something. `status` is `RDX_DIALOG_OK`,
`RDX_DIALOG_CANCEL` or `RDX_DIALOG_ERROR`, `paths` holds the chosen paths, and `error` explains a failure.

| Option         | Meaning                                                       |
| -------------- | ------------------------------------------------------------- |
| `title`        | Window title                                                  |
| `filters`      | `{name, extensions}` pairs; extensions comma-separated, no dots |
| `default_path` | Folder to start in                                            |
| `default_name` | Save: suggested file name                                     |

## C

```c
{{#include ../../examples/dialog.cc:c}}
```

Each function writes paths that the caller owns: free one path with `rdx_dialog_free`, and the array from
`rdx_dialog_open_multiple` with `rdx_dialog_free_paths`. Paths are UTF-8 on every platform.

## Behavior

- **Blocking.** Each call returns when the dialog closes. The raydom window doesn't redraw meanwhile. Calling from a
  click listener is the usual pattern; the tree can be changed with the result before the listener returns.
- **Main thread.** Call after `InitWindow`, on the thread that owns the window. macOS requires it.
- **Parent window.** On macOS and Windows the dialog is attached to the raylib window. On Linux it isn't, because
  raylib's window handle there may be X11 or Wayland.
- **Unsupported builds.** On the web, or with `RAYDOM_FILE_DIALOG=OFF`, every call returns `RDX_DIALOG_ERROR`
  with a message saying so. The API is always present, so code compiles everywhere.

## Linux requirements

Building needs the libdbus development package (`libdbus-1-dev` on Debian and Ubuntu, `dbus-devel` on Fedora) and
`pkg-config`. At run time, the desktop must provide xdg-desktop-portal, which current GNOME, KDE and most other
desktops do. Set `RAYDOM_FILE_DIALOG=OFF` to build without either.
