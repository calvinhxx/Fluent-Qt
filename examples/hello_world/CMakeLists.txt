cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(FluentQtHelloWorld VERSION 0.1.0 LANGUAGES CXX)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET FluentQt::FluentQt)
    find_package(FluentQt CONFIG REQUIRED)
endif()

add_executable(fluentqt_hello_world
    main.cpp
)
target_link_libraries(fluentqt_hello_world
    PRIVATE
    FluentQt::FluentQt
)
