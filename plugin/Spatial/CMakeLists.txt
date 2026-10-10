fluent_qt_add_spatial_library()

if(FLUENT_QT_BUILD_EXAMPLES)
    include("${PROJECT_SOURCE_DIR}/cmake/FluentQtHelloWorldExamples.cmake")
    fluent_qt_add_hello_world_example(fluentqt_hello_world_spatial FluentQt::Spatial
        HelloWorldSpatialWindow.cpp "${PROJECT_BINARY_DIR}/examples/hello_world")
endif()
