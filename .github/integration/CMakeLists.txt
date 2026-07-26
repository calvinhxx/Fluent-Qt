cmake_minimum_required(VERSION 3.16)

project(FluentQtExternalIntegration LANGUAGES CXX)

# CI integration fixture: verifies the three supported consumer paths without
# relying on Gallery, tests, or their optional dependencies.

set(FLUENT_QT_INTEGRATION_MODE "SOURCE" CACHE STRING
    "FluentQt consumer path: SOURCE, FETCHCONTENT, or INSTALLED")
set_property(CACHE FLUENT_QT_INTEGRATION_MODE PROPERTY STRINGS
    SOURCE FETCHCONTENT INSTALLED)
string(TOUPPER "${FLUENT_QT_INTEGRATION_MODE}" FLUENT_QT_INTEGRATION_MODE)

if(FLUENT_QT_INTEGRATION_MODE STREQUAL "SOURCE")
    if(NOT FLUENT_QT_SOURCE_DIR)
        message(FATAL_ERROR "FLUENT_QT_SOURCE_DIR is required for SOURCE mode")
    endif()
    set(FLUENT_QT_INSTALL ON CACHE BOOL "" FORCE)
    add_subdirectory("${FLUENT_QT_SOURCE_DIR}" fluentqt)
elseif(FLUENT_QT_INTEGRATION_MODE STREQUAL "FETCHCONTENT")
    if(NOT FLUENT_QT_SOURCE_DIR)
        message(FATAL_ERROR "FLUENT_QT_SOURCE_DIR is required for the local FETCHCONTENT fixture")
    endif()
    include(FetchContent)
    set(FLUENT_QT_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(fluentqt SOURCE_DIR "${FLUENT_QT_SOURCE_DIR}")
    FetchContent_MakeAvailable(fluentqt)
elseif(FLUENT_QT_INTEGRATION_MODE STREQUAL "INSTALLED")
    find_package(FluentQt CONFIG REQUIRED)
else()
    message(FATAL_ERROR
        "Unsupported FLUENT_QT_INTEGRATION_MODE: ${FLUENT_QT_INTEGRATION_MODE}")
endif()

add_executable(fluentqt_external_integration
    main.cpp
    CategoryHeaderProbe.cpp)
set_target_properties(fluentqt_external_integration PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
target_link_libraries(fluentqt_external_integration PRIVATE FluentQt::FluentQt)
