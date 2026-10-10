source_group(TREE ${PROJECT_SOURCE_DIR}/src FILES ${SRC})

add_library(FluentQt STATIC ${SRC} ${RESOURCES})
add_library(FluentQt::FluentQt ALIAS FluentQt)
# Backward-compatible build-tree alias for existing local tests and examples.
add_library(fluent_qt_lib ALIAS FluentQt)
fluent_qt_configure_cpp_target(FluentQt)
fluent_qt_enable_project_warnings(FluentQt)
fluent_qt_enable_sanitizers(FluentQt)
target_link_libraries(FluentQt
    PUBLIC
    Qt${QT_VERSION_MAJOR}::Widgets
)
if(APPLE)
    target_link_libraries(FluentQt PUBLIC FluentQt::ApplePlatformDependencies)
endif()
target_include_directories(FluentQt
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/FluentQt>
)
set_target_properties(FluentQt PROPERTIES
    EXPORT_NAME FluentQt
    OUTPUT_NAME FluentQt
    ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
    AUTOGEN_BUILD_DIR "${PROJECT_BINARY_DIR}/FluentQt_autogen"
)

if(FLUENT_QT_BUILD_EXAMPLES)
    include("${PROJECT_SOURCE_DIR}/cmake/FluentQtHelloWorldExamples.cmake")
    fluent_qt_add_hello_world_example(fluentqt_hello_world FluentQt::FluentQt
        HelloWorldWindow.cpp "${PROJECT_BINARY_DIR}/examples/hello_world")
endif()
