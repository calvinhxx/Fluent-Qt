function(fluent_qt_find_qml_bridge_dependencies)
    # Qt Quick belongs to this optional host plugin, never to the Widgets core.
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Qml Quick QuickWidgets)
    if(QT_VERSION_MAJOR EQUAL 6)
        find_package(Qt6 REQUIRED COMPONENTS OpenGL)
    endif()

endfunction()

function(fluent_qt_add_qml_bridge_library)
    add_library(FluentQtQmlBridge STATIC ${FLUENT_QT_QML_BRIDGE_SOURCES})
    add_library(FluentQt::QmlBridge ALIAS FluentQtQmlBridge)
    fluent_qt_configure_cpp_target(FluentQtQmlBridge)
    fluent_qt_enable_project_warnings(FluentQtQmlBridge)
    fluent_qt_enable_sanitizers(FluentQtQmlBridge)
    target_link_libraries(FluentQtQmlBridge
        PUBLIC FluentQt::Plugins
        PRIVATE
            Qt${QT_VERSION_MAJOR}::Qml
            Qt${QT_VERSION_MAJOR}::Quick
            Qt${QT_VERSION_MAJOR}::QuickWidgets)
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(FluentQtQmlBridge PRIVATE Qt6::OpenGL)
    endif()
    target_compile_definitions(FluentQtQmlBridge PUBLIC FLUENT_QT_HAS_QML_BRIDGE=1)
    set_target_properties(FluentQtQmlBridge PROPERTIES
        EXPORT_NAME QmlBridge OUTPUT_NAME FluentQtQmlBridge
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        AUTOGEN_BUILD_DIR "${PROJECT_BINARY_DIR}/FluentQtQmlBridge_autogen"
        POSITION_INDEPENDENT_CODE ON)
endfunction()
