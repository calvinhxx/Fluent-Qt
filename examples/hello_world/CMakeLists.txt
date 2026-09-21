cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FluentQtHelloWorld VERSION 0.1.0 LANGUAGES CXX)
    option(FLUENT_QT_HELLO_WORLD_SPATIAL
        "Also build the Spatial example from an installed FluentQt SDK" OFF)
    if(FLUENT_QT_HELLO_WORLD_SPATIAL)
        find_package(FluentQt CONFIG REQUIRED COMPONENTS Spatial)
    endif()
endif()

if(NOT TARGET FluentQt::FluentQt)
    find_package(FluentQt CONFIG REQUIRED)
endif()

set(FLUENTQT_HELLO_WORLD_SOURCES
    main.cpp
    HelloWorldApplication.h
    HelloWorldWindow.h)
if(EMSCRIPTEN)
    list(APPEND FLUENTQT_HELLO_WORLD_SOURCES
        platform/wasm/HelloWorldApplication.cpp)
else()
    list(APPEND FLUENTQT_HELLO_WORLD_SOURCES
        platform/desktop/HelloWorldApplication.cpp)
endif()

function(add_hello_world target library window_source)
    if(COMMAND qt_add_executable)
        qt_add_executable(${target} ${FLUENTQT_HELLO_WORLD_SOURCES} ${window_source})
    else()
        add_executable(${target} ${FLUENTQT_HELLO_WORLD_SOURCES} ${window_source})
    endif()
    if(COMMAND fluent_qt_configure_cpp_target)
        fluent_qt_configure_cpp_target(${target})
    else()
        set_target_properties(${target} PROPERTIES
            AUTOMOC ON
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF)
    endif()
    target_link_libraries(${target} PRIVATE ${library})
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    if(EMSCRIPTEN)
        target_link_libraries(${target} PRIVATE FluentQt::WebAssembly)
        set_target_properties(${target} PROPERTIES
            QT_WASM_INITIAL_MEMORY "64MB"
            QT_WASM_MAXIMUM_MEMORY "256MB")
    endif()
endfunction()

# These are independent executables: the 2D target never links Spatial.
add_hello_world(fluentqt_hello_world FluentQt::FluentQt HelloWorldWindow.cpp)
if(TARGET FluentQt::Spatial)
    add_hello_world(fluentqt_hello_world_spatial FluentQt::Spatial HelloWorldSpatialWindow.cpp)
endif()

if(EMSCRIPTEN AND TARGET fluent_qt_gallery)
    set(_fluentqt_hello_world_web_destination
        "${CMAKE_BINARY_DIR}/app/hello-world")
    file(MAKE_DIRECTORY "${_fluentqt_hello_world_web_destination}")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/platform/wasm/index.html.in"
        "${_fluentqt_hello_world_web_destination}/index.html"
        @ONLY)
    add_custom_command(TARGET fluentqt_hello_world POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${_fluentqt_hello_world_web_destination}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/fluentqt_hello_world.js"
            "${_fluentqt_hello_world_web_destination}/fluentqt_hello_world.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/fluentqt_hello_world.wasm"
            "${_fluentqt_hello_world_web_destination}/fluentqt_hello_world.wasm"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/qtloader.js"
            "${_fluentqt_hello_world_web_destination}/qtloader.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/qtlogo.svg"
            "${_fluentqt_hello_world_web_destination}/qtlogo.svg"
        COMMENT "Stage the minimal FluentQt WebAssembly consumer")
endif()
