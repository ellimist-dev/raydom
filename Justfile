set dotenv-load := true

# Development build: tests, examples, sanitizers.
build:
    cmake -S . -B build -DRAYDOM_SANITIZE=ON && cmake --build build

showcase: build
    ./build/examples/showcase

test: build
    ctest --test-dir build --output-on-failure

build-web:
    emcmake cmake -DPLATFORM=Web -S . -B build-web && cmake --build build-web

# Serves the book at http://localhost:3000 and opens it; rebuilds on save. Needs mdbook (brew install mdbook).
docs:
    mdbook serve docs --open

# Builds the book as published, with llms.txt and Markdown copies, into docs/book/html.
docs-build:
    mdbook build docs && python3 docs/tools/llms.py

lint: build
    /opt/homebrew/opt/llvm/bin/clang-tidy -p build src/*.c tests/*.c tests/*.cc

# Removes build outputs; keeps the configuration and downloaded raylib.
clean:
    if [ -d build ]; then cmake --build build --target clean; fi

# Removes the build directories entirely; the next build re-downloads raylib.
distclean:
    rm -rf build build-web docs/book

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
