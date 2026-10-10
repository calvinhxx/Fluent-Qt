include_guard(GLOBAL)

get_filename_component(_fluent_qt_qml_bridge_example_source_dir
    "${CMAKE_CURRENT_LIST_DIR}/../examples/qml_bridge" ABSOLUTE)
set_property(GLOBAL PROPERTY FLUENT_QT_QML_BRIDGE_EXAMPLE_SOURCE_DIR
    "${_fluent_qt_qml_bridge_example_source_dir}")

function(fluent_qt_add_qml_bridge_example runtime_output_dir)
    get_property(_source_dir GLOBAL PROPERTY FLUENT_QT_QML_BRIDGE_EXAMPLE_SOURCE_DIR)
    get_filename_component(_output_dir "${runtime_output_dir}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    if(NOT TARGET FluentQt::QmlBridge)
        find_package(FluentQt CONFIG REQUIRED COMPONENTS QmlBridge)
    endif()
    if(EMSCRIPTEN AND NOT TARGET FluentQt::WebAssembly)
        message(FATAL_ERROR "This browser example requires the FluentQt WebAssembly runtime")
    endif()

    set(_sources
        "${_source_dir}/main.cpp"
        "${_source_dir}/QmlBridgeApplication.h"
        "${_source_dir}/QmlBridgeModel.h"
        "${_source_dir}/QmlBridgePanel.h"
        "${_source_dir}/QmlBridgePanel.cpp"
        "${_source_dir}/QmlBridgeWindow.h"
        "${_source_dir}/QmlBridgeWindow.cpp"
        "${_source_dir}/qml_bridge.qrc")
    if(EMSCRIPTEN)
        list(APPEND _sources "${_source_dir}/platform/wasm/QmlBridgeApplication.cpp")
    else()
        list(APPEND _sources "${_source_dir}/platform/desktop/QmlBridgeApplication.cpp")
    endif()

    if(COMMAND qt_add_executable)
        qt_add_executable(fluentqt_qml_bridge ${_sources})
    else()
        add_executable(fluentqt_qml_bridge ${_sources})
    endif()
    if(COMMAND fluent_qt_configure_cpp_target)
        fluent_qt_configure_cpp_target(fluentqt_qml_bridge)
    else()
        set_target_properties(fluentqt_qml_bridge PROPERTIES
            AUTOMOC ON AUTORCC ON CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    endif()
    set_target_properties(fluentqt_qml_bridge PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${_output_dir}")
    target_link_libraries(fluentqt_qml_bridge PRIVATE FluentQt::QmlBridge)
    if(NOT EMSCRIPTEN)
        target_link_libraries(fluentqt_qml_bridge PRIVATE Qt${QT_VERSION_MAJOR}::QuickWidgets)
    endif()
    target_include_directories(fluentqt_qml_bridge PRIVATE "${_source_dir}")

    if(EMSCRIPTEN)
        target_link_libraries(fluentqt_qml_bridge PRIVATE FluentQt::WebAssembly)
        # The example is declared in plugin/QmlBridge, but its QML files stay
        # beside the installed-consumer sources. Scan that directory explicitly.
        set_target_properties(fluentqt_qml_bridge PROPERTIES
            QT_QML_IMPORT_SCANNER_EXTRA_ROOT_PATHS "${_source_dir}/qml")
        if(COMMAND qt_import_qml_plugins)
            qt_import_qml_plugins(fluentqt_qml_bridge)
        endif()
        configure_file("${_source_dir}/platform/wasm/index.html.in"
            "${_output_dir}/index.html" @ONLY)
        if(FLUENT_QT_BUILD_GALLERY)
            set(_destination "${PROJECT_BINARY_DIR}/app/qml-bridge")
            file(MAKE_DIRECTORY "${_destination}")
            configure_file("${_source_dir}/platform/wasm/index.html.in"
                "${_destination}/index.html" @ONLY)
            add_custom_command(TARGET fluentqt_qml_bridge POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_output_dir}/index.html"
                    "$<TARGET_FILE:fluentqt_qml_bridge>"
                    "$<TARGET_FILE_DIR:fluentqt_qml_bridge>/fluentqt_qml_bridge.wasm"
                    "$<TARGET_FILE_DIR:fluentqt_qml_bridge>/qtloader.js"
                    "$<TARGET_FILE_DIR:fluentqt_qml_bridge>/qtlogo.svg"
                    "${_destination}"
                COMMENT "Stage the QML bridge consumer beside the Gallery")
            set_property(GLOBAL APPEND PROPERTY FLUENT_QT_GALLERY_EXAMPLE_TARGETS
                fluentqt_qml_bridge)
        endif()
    endif()
endfunction()
