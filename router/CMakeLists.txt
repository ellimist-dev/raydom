add_library(raydom_router STATIC src/router.c)
add_library(raydom::router ALIAS raydom_router)
set_target_properties(raydom_router PROPERTIES EXPORT_NAME router)
target_include_directories(raydom_router PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                                                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(raydom_router PUBLIC raydom)
target_compile_features(raydom_router PRIVATE c_std_23)
target_compile_options(raydom_router PRIVATE ${RAYDOM_WARNINGS} ${RAYDOM_SANITIZERS})
