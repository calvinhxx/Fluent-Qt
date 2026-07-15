cmake_minimum_required(VERSION 3.16)

project(FluentQtExternalIntegration LANGUAGES CXX)

# CI integration fixture: verifies FluentQt's defaults and exported target
# without relying on Gallery, tests, or their optional dependencies.

if(NOT FLUENT_QT_SOURCE_DIR)
    message(FATAL_ERROR "FLUENT_QT_SOURCE_DIR is required")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FLUENT_QT_INSTALL ON CACHE BOOL "" FORCE)

add_subdirectory("${FLUENT_QT_SOURCE_DIR}" fluentqt)

add_executable(fluentqt_external_integration main.cpp)
target_link_libraries(fluentqt_external_integration PRIVATE FluentQt::FluentQt)
