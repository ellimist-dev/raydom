# Builds against raydom the way a downstream project would: FetchContent by default, or an
# installed package with -DCONSUMER_FIND_PACKAGE=ON and CMAKE_PREFIX_PATH set.
cmake_minimum_required(VERSION 3.25)
project(raydom_consumer LANGUAGES C CXX)

option(CONSUMER_FIND_PACKAGE "Use find_package instead of FetchContent" OFF)

if (CONSUMER_FIND_PACKAGE)
    find_package(raydom 0.1 CONFIG REQUIRED)
else ()
    include(FetchContent)
    FetchContent_Declare(raydom SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/../..)
    FetchContent_MakeAvailable(raydom)
endif ()

add_executable(consumer_c main.c)
target_link_libraries(consumer_c PRIVATE raydom::raydom)

add_executable(consumer_cpp main.cc)
target_link_libraries(consumer_cpp PRIVATE raydom::extras)
