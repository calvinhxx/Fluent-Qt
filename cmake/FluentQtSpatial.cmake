function(fluent_qt_find_spatial_dependencies)
    # OpenGL belongs to this opt-in target, never to the base Widgets library.
    if(QT_VERSION_MAJOR EQUAL 6)
        find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    else()
        include(CheckCXXSourceCompiles)
        include(CMakePushCheckState)
        cmake_push_check_state(RESET)
        set(CMAKE_REQUIRED_LIBRARIES Qt5::Widgets)
        check_cxx_source_compiles(
            "#include <QOpenGLWidget>\nint main() { QOpenGLWidget widget; return 0; }"
            FLUENT_QT_HAS_QOPENGLWIDGET)
        cmake_pop_check_state()
        if(NOT FLUENT_QT_HAS_QOPENGLWIDGET)
            message(FATAL_ERROR "FluentQt Spatial requires Qt 5 Widgets with QOpenGLWidget support. Disable FLUENT_QT_BUILD_SPATIAL for a 2D-only build.")
        endif()
    endif()
endfunction()

function(fluent_qt_spatial_private_headers_available result)
    # Distribution packages may export the private targets before their separate
    # development headers are installed. Resolve both consumers through the same
    # target/include checks before linking: missing directories fail generation.
    cmake_parse_arguments(PARSE_ARGV 1 probe "" "MODULE" "DEPENDENCIES;HEADERS")
    set(${result} FALSE PARENT_SCOPE)
    if(NOT probe_MODULE OR NOT probe_HEADERS)
        return()
    endif()
    set(module_includes)
    foreach(module IN ITEMS ${probe_DEPENDENCIES} ${probe_MODULE})
        if(NOT TARGET Qt${QT_VERSION_MAJOR}::${module}Private)
            return()
        endif()
        get_target_property(private_includes Qt${QT_VERSION_MAJOR}::${module}Private
            INTERFACE_INCLUDE_DIRECTORIES)
        set(module_include_found FALSE)
        foreach(directory IN LISTS private_includes)
            # Recent Qt exports guard these paths with the public module's header
            # property. Resolve that form before testing the installed directories.
            if(directory MATCHES "^\\$<\\$<BOOL:\\$<TARGET_PROPERTY:([^,]+),([^>]+)>>:(.*)>$")
                set(condition_target "${CMAKE_MATCH_1}")
                set(condition_property "${CMAKE_MATCH_2}")
                set(directory "${CMAKE_MATCH_3}")
                if(NOT TARGET "${condition_target}")
                    return()
                endif()
                get_target_property(headers_enabled "${condition_target}" "${condition_property}")
                if(NOT headers_enabled OR directory STREQUAL "")
                    continue()
                endif()
            elseif(directory MATCHES "^\\$<")
                return()
            endif()
            if(NOT IS_DIRECTORY "${directory}")
                return()
            endif()
            set(module_include_found TRUE)
            if(module STREQUAL probe_MODULE)
                list(APPEND module_includes "${directory}")
            endif()
        endforeach()
        if(NOT module_include_found)
            return()
        endif()
    endforeach()
    foreach(header IN LISTS probe_HEADERS)
        set(header_found FALSE)
        foreach(directory IN LISTS module_includes)
            if(EXISTS "${directory}/${header}" OR EXISTS "${directory}/Qt${probe_MODULE}/${header}")
                set(header_found TRUE)
                break()
            endif()
        endforeach()
        if(NOT header_found)
            return()
        endif()
    endforeach()
    set(${result} TRUE PARENT_SCOPE)
endfunction()

function(fluent_qt_spatial_widgets_private_available result)
    fluent_qt_spatial_private_headers_available(available
        MODULE Widgets DEPENDENCIES Core Gui HEADERS private/qwidget_p.h)
    set(${result} ${available} PARENT_SCOPE)
endfunction()

function(fluent_qt_add_spatial_library)
    add_library(FluentQtSpatial STATIC ${FLUENT_QT_SPATIAL_SOURCES})
    add_library(FluentQt::Spatial ALIAS FluentQtSpatial)
    fluent_qt_configure_cpp_target(FluentQtSpatial)
    fluent_qt_enable_project_warnings(FluentQtSpatial)
    fluent_qt_enable_sanitizers(FluentQtSpatial)
    target_link_libraries(FluentQtSpatial PUBLIC FluentQt::Plugins FluentQt::Particles)
    target_compile_definitions(FluentQtSpatial PUBLIC FLUENT_QT_HAS_SPATIAL=1)
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(FluentQtSpatial PRIVATE Qt6::OpenGLWidgets)
    endif()
    set_target_properties(FluentQtSpatial PROPERTIES
        EXPORT_NAME Spatial OUTPUT_NAME FluentQtSpatial
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        AUTOGEN_BUILD_DIR "${PROJECT_BINARY_DIR}/FluentQtSpatial_autogen")

    # Shared native application infrastructure for C++ and Python Gallery. This is
    # deliberately not exported by the SDK or linked into the public Spatial target.
    if(FLUENT_QT_BUILD_GALLERY OR FLUENT_QT_BUILD_PYSIDE6_BINDINGS OR FLUENT_QT_BUILD_TESTS)
        add_library(FluentQtSpatialRenderer STATIC
            ${FLUENT_QT_SPATIAL_RENDERER_SOURCES}
            "${PROJECT_SOURCE_DIR}/support/spatial/SpatialTextureHost.cpp"
            "${PROJECT_SOURCE_DIR}/support/spatial/SpatialTextureHost.h")
        fluent_qt_configure_cpp_target(FluentQtSpatialRenderer)
        fluent_qt_enable_project_warnings(FluentQtSpatialRenderer)
        fluent_qt_enable_sanitizers(FluentQtSpatialRenderer)
        set_target_properties(FluentQtSpatialRenderer PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
            AUTOGEN_BUILD_DIR "${PROJECT_BINARY_DIR}/FluentQtSpatialRenderer_autogen"
            POSITION_INDEPENDENT_CODE ON)
        target_link_libraries(FluentQtSpatialRenderer PUBLIC FluentQt::Spatial)
        target_include_directories(FluentQtSpatialRenderer PUBLIC "${PROJECT_SOURCE_DIR}")
        # Texture widgets paint inside backing-store passes through private widget
        # state. Without private headers they keep Qt's queued update() path.
        fluent_qt_spatial_widgets_private_available(_fluent_qt_widgets_private_available)
        if(_fluent_qt_widgets_private_available)
            target_link_libraries(FluentQtSpatialRenderer PRIVATE Qt${QT_VERSION_MAJOR}::WidgetsPrivate)
            target_compile_definitions(FluentQtSpatialRenderer PRIVATE FLUENT_QT_HAS_WIDGETS_PRIVATE=1)
        endif()
        if(QT_VERSION_MAJOR EQUAL 6 AND Qt6_VERSION VERSION_GREATER_EQUAL 6.7 AND (WIN32 OR APPLE))
            # QRhiWidget is public, but its rendering and shader APIs use the
            # versioned GuiPrivate headers. Keep OpenGL when those are unavailable.
            fluent_qt_spatial_private_headers_available(_fluent_qt_rhi_private_available
                MODULE Gui DEPENDENCIES Core
                HEADERS rhi/qrhi.h rhi/qrhi_platform.h rhi/qshader.h)
            if(_fluent_qt_rhi_private_available)
                target_link_libraries(FluentQtSpatialRenderer PRIVATE Qt6::GuiPrivate)
                target_compile_definitions(FluentQtSpatialRenderer PRIVATE FLUENT_QT_HAS_RHI=1)
                target_sources(FluentQtSpatialRenderer PRIVATE
                    "${PROJECT_SOURCE_DIR}/support/spatial/shaders/spatial_rhi.qrc")
            endif()
        endif()
    endif()
endfunction()
