include_guard(GLOBAL)

get_filename_component(_fluent_qt_plugin_extension_source_dir
    "${CMAKE_CURRENT_LIST_DIR}/../examples/plugin_extension" ABSOLUTE)
set_property(GLOBAL PROPERTY FLUENT_QT_PLUGIN_EXTENSION_SOURCE_DIR
    "${_fluent_qt_plugin_extension_source_dir}")

function(fluent_qt_add_plugin_extension_example runtime_output_dir)
    get_property(_source_dir GLOBAL PROPERTY FLUENT_QT_PLUGIN_EXTENSION_SOURCE_DIR)
    get_filename_component(_output_dir "${runtime_output_dir}" ABSOLUTE
        BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")

    if(NOT TARGET FluentQt::Plugins)
        find_package(FluentQt CONFIG REQUIRED COMPONENTS Plugins)
    endif()
    if(EMSCRIPTEN AND NOT TARGET FluentQt::WebAssembly)
        message(FATAL_ERROR
            "The installed FluentQt SDK must enable FLUENT_QT_BUILD_WEBASSEMBLY_SUPPORT for this browser example")
    endif()

    set(_sources
        "${_source_dir}/main.cpp"
        "${_source_dir}/PluginContractProbe.h"
        "${_source_dir}/PluginContractProbe.cpp"
        "${_source_dir}/PluginExtensionApplication.h"
        "${_source_dir}/PluginExtensionWindow.h"
        "${_source_dir}/PluginExtensionWindow.cpp")
    if(EMSCRIPTEN)
        list(APPEND _sources "${_source_dir}/platform/wasm/PluginExtensionApplication.cpp")
    else()
        list(APPEND _sources "${_source_dir}/platform/desktop/PluginExtensionApplication.cpp")
    endif()

    if(COMMAND qt_add_executable)
        qt_add_executable(fluentqt_plugin_extension ${_sources})
    else()
        add_executable(fluentqt_plugin_extension ${_sources})
    endif()
    if(COMMAND fluent_qt_configure_cpp_target)
        fluent_qt_configure_cpp_target(fluentqt_plugin_extension)
    else()
        set_target_properties(fluentqt_plugin_extension PROPERTIES
            AUTOMOC ON
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF)
    endif()
    set_target_properties(fluentqt_plugin_extension PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${_output_dir}")
    target_link_libraries(fluentqt_plugin_extension PRIVATE FluentQt::Plugins)
    target_include_directories(fluentqt_plugin_extension PRIVATE "${_source_dir}")

    if(EMSCRIPTEN)
        target_link_libraries(fluentqt_plugin_extension PRIVATE FluentQt::WebAssembly)
        configure_file("${_source_dir}/platform/wasm/index.html.in"
            "${_output_dir}/index.html" @ONLY)
        if(FLUENT_QT_BUILD_GALLERY)
            set(_destination "${PROJECT_BINARY_DIR}/app/plugin-extension")
            file(MAKE_DIRECTORY "${_destination}")
            configure_file("${_source_dir}/platform/wasm/index.html.in"
                "${_destination}/index.html" @ONLY)
            add_custom_command(TARGET fluentqt_plugin_extension POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory "${_destination}"
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_output_dir}/index.html"
                    "$<TARGET_FILE:fluentqt_plugin_extension>"
                    "$<TARGET_FILE_DIR:fluentqt_plugin_extension>/fluentqt_plugin_extension.wasm"
                    "$<TARGET_FILE_DIR:fluentqt_plugin_extension>/qtloader.js"
                    "$<TARGET_FILE_DIR:fluentqt_plugin_extension>/qtlogo.svg"
                    "${_destination}"
                COMMENT "Stage the optional plugin consumer beside the Gallery")
            set_property(GLOBAL APPEND PROPERTY FLUENT_QT_GALLERY_EXAMPLE_TARGETS
                fluentqt_plugin_extension)
        endif()
    endif()
endfunction()
