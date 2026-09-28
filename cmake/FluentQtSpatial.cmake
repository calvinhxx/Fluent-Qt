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

add_library(FluentQtSpatial STATIC ${FLUENT_QT_SPATIAL_SOURCES})
add_library(FluentQt::Spatial ALIAS FluentQtSpatial)
fluent_qt_configure_cpp_target(FluentQtSpatial)
fluent_qt_enable_project_warnings(FluentQtSpatial)
fluent_qt_enable_sanitizers(FluentQtSpatial)
target_link_libraries(FluentQtSpatial PUBLIC FluentQt::FluentQt)
target_compile_definitions(FluentQtSpatial PUBLIC FLUENT_QT_HAS_SPATIAL=1)
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(FluentQtSpatial PRIVATE Qt6::OpenGLWidgets)
endif()
set_target_properties(FluentQtSpatial PROPERTIES EXPORT_NAME Spatial OUTPUT_NAME FluentQtSpatial)

# Shared native application infrastructure for C++ and Python Gallery. This is
# deliberately not exported by the SDK or linked into the public Spatial target.
if(FLUENT_QT_BUILD_GALLERY OR FLUENT_QT_BUILD_PYSIDE6_BINDINGS OR FLUENT_QT_BUILD_TESTS)
    add_library(FluentQtSpatialRenderer STATIC
        "${PROJECT_SOURCE_DIR}/support/spatial/SpatialTextureHost.cpp"
        "${PROJECT_SOURCE_DIR}/support/spatial/SpatialTextureHost.h")
    fluent_qt_configure_cpp_target(FluentQtSpatialRenderer)
    fluent_qt_enable_project_warnings(FluentQtSpatialRenderer)
    fluent_qt_enable_sanitizers(FluentQtSpatialRenderer)
    set_target_properties(FluentQtSpatialRenderer PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_link_libraries(FluentQtSpatialRenderer PUBLIC FluentQt::Spatial)
    target_include_directories(FluentQtSpatialRenderer PUBLIC "${PROJECT_SOURCE_DIR}")
    if(QT_VERSION_MAJOR EQUAL 6 AND Qt6_VERSION VERSION_GREATER_EQUAL 6.7 AND (WIN32 OR APPLE))
        target_link_libraries(FluentQtSpatialRenderer PRIVATE Qt6::GuiPrivate)
        target_compile_definitions(FluentQtSpatialRenderer PRIVATE FLUENT_QT_HAS_RHI=1)
        target_sources(FluentQtSpatialRenderer PRIVATE
            "${PROJECT_SOURCE_DIR}/support/spatial/shaders/spatial_rhi.qrc")
    endif()
endif()
