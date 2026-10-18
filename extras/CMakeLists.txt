add_library(raydom_extras STATIC src/graph.c)
add_library(raydom::extras ALIAS raydom_extras)
set_target_properties(raydom_extras PROPERTIES EXPORT_NAME extras)
target_include_directories(raydom_extras PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(raydom_extras PUBLIC raydom)
target_compile_options(raydom_extras PRIVATE ${RAYDOM_WARNINGS} ${RAYDOM_SANITIZERS})
