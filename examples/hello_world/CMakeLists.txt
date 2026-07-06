cmake_minimum_required(VERSION 3.16)

project(FluentQtHelloWorld VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(FluentQt CONFIG REQUIRED)

add_executable(fluentqt_hello_world
    main.cpp
)
target_link_libraries(fluentqt_hello_world
    PRIVATE
    FluentQt::FluentQt
    Qt${QT_VERSION_MAJOR}::Widgets
)
