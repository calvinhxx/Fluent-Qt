cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FluentQtHelloWorld VERSION 0.1.0 LANGUAGES CXX)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    option(FLUENT_QT_HELLO_WORLD_SPATIAL
        "Also build the Spatial example from an installed FluentQt SDK" OFF)
    if(FLUENT_QT_HELLO_WORLD_SPATIAL)
        find_package(FluentQt CONFIG REQUIRED COMPONENTS Spatial)
    endif()
endif()

if(NOT TARGET FluentQt::FluentQt)
    find_package(FluentQt CONFIG REQUIRED)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/FluentQtHelloWorldExamples.cmake")

# Keep this installed-SDK entry point; the root build declares the examples in
# uilib and plugin/Spatial instead.
fluent_qt_add_hello_world_example(fluentqt_hello_world FluentQt::FluentQt
    HelloWorldWindow.cpp "${CMAKE_CURRENT_BINARY_DIR}")
if(TARGET FluentQt::Spatial)
    fluent_qt_add_hello_world_example(fluentqt_hello_world_spatial FluentQt::Spatial
        HelloWorldSpatialWindow.cpp "${CMAKE_CURRENT_BINARY_DIR}")
endif()
