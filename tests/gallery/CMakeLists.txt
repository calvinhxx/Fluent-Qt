if(NOT DEFINED FLUENT_QT_GALLERY_SOURCE_DIR)
    set(FLUENT_QT_GALLERY_SOURCE_DIR ${CMAKE_SOURCE_DIR}/app)
endif()
if(NOT TARGET FluentQtGalleryCore)
    message(FATAL_ERROR
        "Gallery tests require the shared FluentQtGalleryCore target")
endif()
if(NOT DEFINED FLUENT_QT_GALLERY_RESOURCES)
    set(FLUENT_QT_GALLERY_RESOURCES
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/gallery_resources.qrc
    )
endif()
if(NOT DEFINED FLUENT_QT_GALLERY_WEB_RESOURCES)
    set(FLUENT_QT_GALLERY_WEB_RESOURCES
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/gallery_web_resources.qrc
    )
endif()

# Link the resource objects into every test executable so static-resource
# initialization is retained, while compiling the large QRC only once.
add_library(FluentQtGalleryTestPlatform OBJECT
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/platform/desktop/GalleryPlatform.cpp
    ${FLUENT_QT_GALLERY_RESOURCES}
)
fluent_qt_configure_cpp_target(FluentQtGalleryTestPlatform)
fluent_qt_enable_sanitizers(FluentQtGalleryTestPlatform)
target_include_directories(FluentQtGalleryTestPlatform
    PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(FluentQtGalleryTestPlatform
    PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

add_executable(test_gallery_single_instance_probe
    GallerySingleInstanceProbe.cpp
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/view/shell/GallerySingleInstance.cpp
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/view/shell/GallerySingleInstance.h
)
fluent_qt_configure_cpp_target(test_gallery_single_instance_probe)
target_link_libraries(test_gallery_single_instance_probe
    PRIVATE Qt${QT_VERSION_MAJOR}::Network)
target_include_directories(test_gallery_single_instance_probe
    PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})

add_qt_test_module(test_gallery_single_instance
    TestGallerySingleInstance.cpp
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/view/shell/GallerySingleInstance.cpp
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/view/shell/GallerySingleInstance.h
)
target_include_directories(test_gallery_single_instance PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(test_gallery_single_instance PRIVATE Qt${QT_VERSION_MAJOR}::Network)
target_compile_definitions(test_gallery_single_instance PRIVATE
    FLUENT_QT_GALLERY_SINGLE_INSTANCE_PROBE_PATH="$<TARGET_FILE:test_gallery_single_instance_probe>")
add_dependencies(test_gallery_single_instance test_gallery_single_instance_probe)

add_qt_test_module(test_gallery_shell_framework
    TestGalleryShellFramework.cpp
    TestGallerySpatialFallback.cpp
    TestUpdateChecker.cpp
    ${CMAKE_SOURCE_DIR}/tests/support/VisualGeometryTestUtils.cpp
)
target_include_directories(test_gallery_shell_framework PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(test_gallery_shell_framework
    PRIVATE FluentQtGalleryCore FluentQtGalleryTestPlatform Qt${QT_VERSION_MAJOR}::Network)
target_compile_definitions(test_gallery_shell_framework PRIVATE
    FLUENT_QT_GALLERY_SINGLE_INSTANCE_PROBE_PATH="$<TARGET_FILE:test_gallery_single_instance_probe>")
add_dependencies(test_gallery_shell_framework test_gallery_single_instance_probe)

add_qt_test_module(test_gallery_shell_motion_policy
    TestGalleryShellMotionPolicy.cpp
)
target_include_directories(test_gallery_shell_motion_policy
    PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(test_gallery_shell_motion_policy
    PRIVATE FluentQtGalleryCore FluentQtGalleryTestPlatform Qt${QT_VERSION_MAJOR}::Network)

if(TARGET FluentQtSpatial)
add_qt_test_module(test_gallery_spatial
    TestGallerySpatial.cpp
    TestGallerySpatialBackdrop.cpp
    TestGalleryParticleCompositor.cpp
)
target_include_directories(test_gallery_spatial PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(test_gallery_spatial
    PRIVATE FluentQtGalleryCore FluentQtGalleryTestPlatform FluentQtSpatialRenderer Qt${QT_VERSION_MAJOR}::Network)
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(test_gallery_spatial PRIVATE Qt6::OpenGLWidgets)
endif()
if(APPLE)
    add_test(NAME GallerySpatialTest.NativeGalleryAssemblyPreservesMaterial
        COMMAND $<TARGET_FILE:test_gallery_spatial>
            --gtest_filter=GallerySpatialTest.GalleryAssemblyPreservesMaterialAndNativeSurface)
    set_tests_properties(
        GallerySpatialTest.NativeGalleryAssemblyPreservesMaterial
        GallerySpatialTest.NativeOpenGLPreviewsRevealWithoutHover
        GallerySpatialTest.NativeOpenGLPreviewsSurviveClippingAndExternalUpdates
        GallerySpatialTest.MacStartupPrewarmKeepsNativeWindowSurface
        GallerySpatialTest.MacTrafficLightsRemainCenteredAcrossSpatialNavigation
        PROPERTIES
            ENVIRONMENT "SKIP_VISUAL_TEST=1;QT_QPA_PLATFORM=cocoa"
            LABELS "qt;unit;gallery;gallery_spatial;test_gallery_spatial;platform_macos;local_desktop;native_window"
            RUN_SERIAL TRUE
            TIMEOUT 30)
endif()
endif()

add_qt_test_module(test_gallery_content_pages
    TestGalleryContentPages.cpp
    TestGalleryParticleBackdrop.cpp
    TestGallerySampleAccessibility.cpp
    TestGalleryPreview.cpp
    TestGalleryInspectorScenes.cpp
    ${CMAKE_SOURCE_DIR}/tests/support/VisualGeometryTestUtils.cpp
    ${FLUENT_QT_GALLERY_WEB_RESOURCES}
)
target_include_directories(test_gallery_content_pages PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(test_gallery_content_pages
    PRIVATE FluentQtGalleryCore FluentQtGalleryTestPlatform Qt${QT_VERSION_MAJOR}::Network)
target_compile_definitions(test_gallery_content_pages PRIVATE
    FLUENT_QT_GALLERY_VERSION="${PROJECT_VERSION}"
    FLUENT_QT_APPLICATION_SCENES_PATH="${CMAKE_SOURCE_DIR}/docs/ai/evals/application-scenes.json")

add_qt_test_module(test_gallery_preview_cli
    TestGalleryPreviewCli.cpp
)
target_compile_definitions(test_gallery_preview_cli PRIVATE
    FLUENT_QT_GALLERY_PREVIEW_PATH="$<TARGET_FILE:fluent_qt_gallery>")
add_dependencies(test_gallery_preview_cli fluent_qt_gallery)

foreach(_gallery_acceptance_scale 1.25 2 3)
    string(REPLACE "." "" _gallery_acceptance_scale_id
        "${_gallery_acceptance_scale}")
    add_test(
        NAME "GalleryAcceptanceScaleTest.Scale${_gallery_acceptance_scale_id}"
        COMMAND $<TARGET_FILE:test_gallery_content_pages>
            --gtest_filter=GalleryContentPagesTest.GalleryAcceptanceMatrixHonorsProcessScale)
    set_tests_properties(
        "GalleryAcceptanceScaleTest.Scale${_gallery_acceptance_scale_id}"
        PROPERTIES
            ENVIRONMENT
                "SKIP_VISUAL_TEST=1;QT_QPA_PLATFORM=offscreen;QT_FONT_DPI=96;QT_SCALE_FACTOR=${_gallery_acceptance_scale}"
            LABELS
                "qt;unit;test_gallery_content_pages;gallery;gallery_acceptance;high_dpi;ci_full;local_full"
            TIMEOUT 60)
endforeach()

if(WIN32)
    foreach(_gallery_test_target
            test_gallery_single_instance_probe
            test_gallery_single_instance
            test_gallery_shell_framework
            test_gallery_shell_motion_policy
            test_gallery_spatial
            test_gallery_content_pages)
        if(TARGET ${_gallery_test_target})
            target_link_libraries(${_gallery_test_target} PRIVATE user32)
        endif()
    endforeach()
endif()

if(TARGET FluentQtSpatial)
    foreach(_spatial_gallery_test test_gallery_shell_framework test_gallery_shell_motion_policy test_gallery_spatial test_gallery_content_pages)
        target_link_libraries(${_spatial_gallery_test} PRIVATE FluentQt::Spatial)
    endforeach()
endif()
