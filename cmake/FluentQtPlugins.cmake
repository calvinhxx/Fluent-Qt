function(fluent_qt_add_plugins_library)
    add_library(FluentQtPlugins STATIC ${FLUENT_QT_PLUGIN_SOURCES})
    add_library(FluentQt::Plugins ALIAS FluentQtPlugins)
    fluent_qt_configure_cpp_target(FluentQtPlugins)
    fluent_qt_enable_project_warnings(FluentQtPlugins)
    fluent_qt_enable_sanitizers(FluentQtPlugins)
    target_link_libraries(FluentQtPlugins PUBLIC FluentQt::FluentQt)
    target_compile_definitions(FluentQtPlugins PUBLIC FLUENT_QT_HAS_PLUGINS=1)
    if(EMSCRIPTEN)
        # Activation rollback catches plugin exceptions, including consumer hooks.
        # Keep JavaScript exception handling scoped to this optional module.
        target_compile_options(FluentQtPlugins PUBLIC -fexceptions)
        target_link_options(FluentQtPlugins INTERFACE -fexceptions)
    endif()
    set_target_properties(FluentQtPlugins PROPERTIES
        EXPORT_NAME Plugins OUTPUT_NAME FluentQtPlugins
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        AUTOGEN_BUILD_DIR "${PROJECT_BINARY_DIR}/FluentQtPlugins_autogen")
endfunction()
