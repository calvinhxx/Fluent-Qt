cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FluentQtQmlBridgeExample VERSION 0.1.0 LANGUAGES CXX)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

if(NOT TARGET FluentQt::QmlBridge)
    find_package(FluentQt CONFIG REQUIRED COMPONENTS QmlBridge)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/FluentQtQmlBridgeExample.cmake")
fluent_qt_add_qml_bridge_example("${CMAKE_CURRENT_BINARY_DIR}")
