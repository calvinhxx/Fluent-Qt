cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FluentQtHelloWorld VERSION 0.1.0 LANGUAGES CXX)
endif()

if(NOT TARGET FluentQt::FluentQt)
    find_package(FluentQt CONFIG REQUIRED)
endif()

add_executable(fluentqt_hello_world
    main.cpp
)
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
