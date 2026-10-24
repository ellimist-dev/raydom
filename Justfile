set dotenv-load := true

# Development build: tests, examples, sanitizers.
build:
    cmake -S . -B build -DRAYDOM_SANITIZE=ON && cmake --build build

showcase: build
    ./build/examples/showcase

test: build
    ctest --test-dir build --output-on-failure

# The web build (emcmake on the PATH). Linking at -O1 without HTML minification works with an emsdk
# whose npm packages aren't installed.
build-web:
    emcmake cmake -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-O1 -sMINIFY_HTML=0" -S . -B build-web
    cmake --build build-web --target showcase

# Runs the Emscripten smoke test under Node: the extras theme and the router within half the default stack.
web-smoke:
    emcmake cmake -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXE_LINKER_FLAGS="-O1 -sMINIFY_HTML=0" -DRAYDOM_BUILD_TESTS=ON -S . -B build-web
    cmake --build build-web --target web_smoke
    node build-web/web_smoke.js

# Builds the showcase for the browser and serves it at http://127.0.0.1:8090. Needs Go.
serve: build-web
    cd serve && go run . -dir ../build-web/examples

# Serves the book at http://localhost:3000 and opens it; rebuilds on save. Needs mdbook (brew install mdbook).
docs:
    mdbook serve docs --open

# Serves the contributor book (devdocs/) and opens it.
devdocs:
    mdbook serve devdocs --open --port 3001

# Builds the book as published, with llms.txt and Markdown copies, into docs/book/html.
docs-build:
    mdbook build docs && python3 docs/tools/llms.py

# clang-tidy from $CLANG_TIDY, the PATH, or Homebrew's LLVM.
lint: build
    "${CLANG_TIDY:-$(command -v clang-tidy || echo /opt/homebrew/opt/llvm/bin/clang-tidy)}" -p build src/*.c extras/src/*.c router/src/*.c tests/*.c tests/*.cc

# Fails when the README's install snippets don't match the version in CMakeLists.txt.
check-version:
    cmake -P cmake/check_version.cmake

# Removes build outputs; keeps the configuration and downloaded raylib.
clean:
    if [ -d build ]; then cmake --build build --target clean; fi

# Removes the build directories entirely; the next build re-downloads raylib.
distclean:
    rm -rf build build-web docs/book devdocs/book

# Builds tests/consumer against raydom through FetchContent and through an installed package.
consumer:
    cmake -S tests/consumer -B build/consumer-fetch -DFETCHCONTENT_SOURCE_DIR_RAYLIB=$PWD/build/_deps/raylib-src
    cmake --build build/consumer-fetch
    ./build/consumer-fetch/consumer_c && ./build/consumer-fetch/consumer_cpp
    cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release -DRAYDOM_BUILD_TESTS=OFF -DRAYDOM_BUILD_EXAMPLES=OFF -DRAYDOM_INSTALL=ON -DFETCHCONTENT_SOURCE_DIR_RAYLIB=$PWD/build/_deps/raylib-src
    cmake --build build/release
    cmake --install build/release --prefix build/install
    cmake -S tests/consumer -B build/consumer-package -DCONSUMER_FIND_PACKAGE=ON -DCMAKE_PREFIX_PATH=$PWD/build/install
    cmake --build build/consumer-package
    ./build/consumer-package/consumer_c && ./build/consumer-package/consumer_cpp
