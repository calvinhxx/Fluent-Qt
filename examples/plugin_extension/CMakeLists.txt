cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FluentQtPluginExtension VERSION 0.1.0 LANGUAGES CXX)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

if(NOT TARGET FluentQt::Plugins)
    find_package(FluentQt CONFIG REQUIRED COMPONENTS Plugins)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/FluentQtPluginExtensionExample.cmake")
fluent_qt_add_plugin_extension_example("${CMAKE_CURRENT_BINARY_DIR}")
