add_qt_test_module(test_qml_bridge TestQmlBridge.cpp QmlBridgeTestModel.h)
set_property(TEST ${test_qml_bridge_TESTS} APPEND PROPERTY ENVIRONMENT
    "QT_QUICK_BACKEND=software")
target_link_libraries(test_qml_bridge PRIVATE FluentQt::QmlBridge
    Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::QuickWidgets)
target_compile_definitions(test_qml_bridge PRIVATE
    FLUENT_QT_QML_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_qt_test_module(test_qml_bridge_gpu TestQmlTexture.cpp QmlBridgeTestModel.h)
target_link_libraries(test_qml_bridge_gpu PRIVATE FluentQt::QmlBridge
    Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::QuickWidgets)
if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    target_link_libraries(test_qml_bridge_gpu PRIVATE Qt6::OpenGLWidgets Qt6::OpenGL)
endif()
target_compile_definitions(test_qml_bridge_gpu PRIVATE
    FLUENT_QT_QML_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set_tests_properties(${test_qml_bridge_gpu_TESTS} PROPERTIES
    LABELS "qt;unit;qml_bridge;test_qml_bridge_gpu;local_desktop;native_window"
    ENVIRONMENT "SKIP_VISUAL_TEST=1;QT_QUICK_BACKEND="
    RUN_SERIAL TRUE TIMEOUT 90)
if(APPLE)
    set_property(TEST ${test_qml_bridge_gpu_TESTS} APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=cocoa")
elseif(WIN32)
    set_property(TEST ${test_qml_bridge_gpu_TESTS} APPEND PROPERTY ENVIRONMENT "QT_QPA_PLATFORM=windows")
endif()
