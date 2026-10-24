# Builds against raydom the way a downstream project would: FetchContent by default, or an
# installed package with -DCONSUMER_FIND_PACKAGE=ON and CMAKE_PREFIX_PATH set.
cmake_minimum_required(VERSION 3.25)
project(raydom_consumer LANGUAGES C CXX)

option(CONSUMER_FIND_PACKAGE "Use find_package instead of FetchContent" OFF)

if (CONSUMER_FIND_PACKAGE)
    # The version of the source tree this test sits in: the installed package must accept it.
    file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/../../CMakeLists.txt line REGEX "^project\\(raydom VERSION")
    string(REGEX MATCH "[0-9]+\\.[0-9]+" want "${line}")
    find_package(raydom ${want} CONFIG REQUIRED)
else ()
    include(FetchContent)
    FetchContent_Declare(raydom SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
    FetchContent_MakeAvailable(raydom)
endif ()

# C11 with -pedantic: the C headers mustn't need C23 or warn.
add_executable(consumer_c main.c)
target_link_libraries(consumer_c PRIVATE raydom::router)
set_target_properties(consumer_c PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
target_compile_options(consumer_c PRIVATE -Wall -Wextra -pedantic -Werror)

add_executable(consumer_cpp main.cc)
target_link_libraries(consumer_cpp PRIVATE raydom::extras)
