fluent_qt_add_qml_bridge_library()

if(FLUENT_QT_BUILD_EXAMPLES)
    include("${PROJECT_SOURCE_DIR}/cmake/FluentQtQmlBridgeExample.cmake")
    fluent_qt_add_qml_bridge_example("${PROJECT_BINARY_DIR}/examples/qml_bridge")
endif()
