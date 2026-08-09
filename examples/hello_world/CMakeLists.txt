cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FluentQtHelloWorld VERSION 0.1.0 LANGUAGES CXX)
endif()

if(NOT TARGET FluentQt::FluentQt)
    find_package(FluentQt CONFIG REQUIRED)
endif()

set(FLUENTQT_HELLO_WORLD_SOURCES
    main.cpp
    HelloWorldApplication.h
    HelloWorldWindow.cpp
    HelloWorldWindow.h)
if(EMSCRIPTEN)
    list(APPEND FLUENTQT_HELLO_WORLD_SOURCES
        platform/wasm/HelloWorldApplication.cpp)
else()
    list(APPEND FLUENTQT_HELLO_WORLD_SOURCES
        platform/desktop/HelloWorldApplication.cpp)
endif()

if(COMMAND qt_add_executable)
    qt_add_executable(fluentqt_hello_world ${FLUENTQT_HELLO_WORLD_SOURCES})
else()
    add_executable(fluentqt_hello_world ${FLUENTQT_HELLO_WORLD_SOURCES})
endif()
if(COMMAND fluent_qt_configure_cpp_target)
    fluent_qt_configure_cpp_target(fluentqt_hello_world)
else()
    set_target_properties(fluentqt_hello_world PROPERTIES
        AUTOMOC ON
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
endif()
target_link_libraries(fluentqt_hello_world
    PRIVATE
    FluentQt::FluentQt
)
target_include_directories(fluentqt_hello_world PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
if(EMSCRIPTEN)
    target_link_libraries(fluentqt_hello_world PRIVATE FluentQt::WebAssembly)
    set_target_properties(fluentqt_hello_world PROPERTIES
        QT_WASM_INITIAL_MEMORY "64MB"
        QT_WASM_MAXIMUM_MEMORY "256MB")
    if(TARGET fluent_qt_gallery)
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
endif()
