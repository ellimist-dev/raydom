# Versioning

raydom follows [semantic versioning](https://semver.org/). Before 1.0, a minor release may break the API; a patch
release doesn't. From 1.0, only a major release may break it.

Compatibility is at the source level, not binary: raydom is a static library built with your program, and public
structs such as `rd_style` grow when properties are added, so code compiled against one version must not be linked
against another. Rebuild against each release; FetchContent and the installed package both do.

## Documentation versions

Every release tag publishes its own copy of this book, built from the sources at that tag, under `/<tag>/`. A
published copy is never rebuilt by later releases, so it keeps describing the version it was built from. The
version menu in the top bar switches between them and keeps the current page when it exists in the target version.

| Path        | Built from                     |
| ----------- | ------------------------------ |
| `/vX.Y.Z/`  | The tag `vX.Y.Z`               |
| `/latest/`  | The newest release tag         |
| `/dev/`     | The `main` branch              |

The book's code samples are compiled, and the headless ones run, by the test suite at each tag, so a version's
samples match that version's API.

## Checking a version in code

```c
#include <raydom/version.h>

#if RD_VERSION_MAJOR == 0 && RD_VERSION_MINOR < 2
// ...
#endif
```

`RD_VERSION_STRING` holds the full version.
