cmake_minimum_required(VERSION 3.16)

project(FluentQtExternalIntegration LANGUAGES CXX)

# CI integration fixture: verifies the three supported consumer paths without
# relying on Gallery, tests, or their optional dependencies.

set(FLUENT_QT_INTEGRATION_MODE "SOURCE" CACHE STRING
    "FluentQt consumer path: SOURCE, FETCHCONTENT, or INSTALLED")
set_property(CACHE FLUENT_QT_INTEGRATION_MODE PROPERTY STRINGS
    SOURCE FETCHCONTENT INSTALLED)
string(TOUPPER "${FLUENT_QT_INTEGRATION_MODE}" FLUENT_QT_INTEGRATION_MODE)
option(FLUENT_QT_INTEGRATION_SPATIAL "Exercise the optional Spatial consumer target" OFF)
if(FLUENT_QT_INTEGRATION_SPATIAL AND NOT FLUENT_QT_INTEGRATION_MODE STREQUAL "INSTALLED")
    set(FLUENT_QT_BUILD_SPATIAL ON CACHE BOOL "" FORCE)
endif()

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
    if(FLUENT_QT_INTEGRATION_SPATIAL)
        find_package(FluentQt CONFIG REQUIRED COMPONENTS Spatial)
    else()
        find_package(FluentQt CONFIG REQUIRED)
    endif()
else()
    message(FATAL_ERROR
        "Unsupported FLUENT_QT_INTEGRATION_MODE: ${FLUENT_QT_INTEGRATION_MODE}")
endif()

add_executable(fluentqt_external_integration
    main.cpp
    CategoryHeaderProbe.cpp
    BackdropHeaderProbe.cpp)
set_target_properties(fluentqt_external_integration PROPERTIES
    AUTOMOC ON
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)
if(FLUENT_QT_INTEGRATION_SPATIAL)
    target_link_libraries(fluentqt_external_integration PRIVATE FluentQt::Spatial)
else()
    target_link_libraries(fluentqt_external_integration PRIVATE FluentQt::FluentQt)
    if(TARGET Qt6::OpenGL OR TARGET Qt6::OpenGLWidgets OR TARGET FluentQt::Spatial)
        message(FATAL_ERROR "A base-only consumer unexpectedly loaded Spatial/Qt OpenGL modules")
    endif()
endif()
get_target_property(_base_dependencies FluentQt::FluentQt INTERFACE_LINK_LIBRARIES)
if(_base_dependencies MATCHES "OpenGL|Spatial")
    message(FATAL_ERROR "The base target leaked optional rendering dependencies: ${_base_dependencies}")
endif()
