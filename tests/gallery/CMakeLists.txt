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
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/platform/desktop/GalleryPlatform.cpp
    ${FLUENT_QT_GALLERY_RESOURCES}
    ${CMAKE_SOURCE_DIR}/tests/support/VisualGeometryTestUtils.cpp
)
target_include_directories(test_gallery_shell_framework PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(test_gallery_shell_framework
    PRIVATE FluentQtGalleryCore Qt${QT_VERSION_MAJOR}::Network)
target_compile_definitions(test_gallery_shell_framework PRIVATE
    FLUENT_QT_GALLERY_SINGLE_INSTANCE_PROBE_PATH="$<TARGET_FILE:test_gallery_single_instance_probe>")
add_dependencies(test_gallery_shell_framework test_gallery_single_instance_probe)

add_qt_test_module(test_gallery_shell_motion_policy
    TestGalleryShellMotionPolicy.cpp
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/platform/desktop/GalleryPlatform.cpp
    ${FLUENT_QT_GALLERY_RESOURCES}
)
target_include_directories(test_gallery_shell_motion_policy
    PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(test_gallery_shell_motion_policy
    PRIVATE FluentQtGalleryCore Qt${QT_VERSION_MAJOR}::Network)

add_qt_test_module(test_gallery_content_pages
    TestGalleryContentPages.cpp
    TestGallerySampleAccessibility.cpp
    TestGalleryPreview.cpp
    TestGalleryInspectorScenes.cpp
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/platform/desktop/GalleryPlatform.cpp
    ${FLUENT_QT_GALLERY_RESOURCES}
    ${FLUENT_QT_GALLERY_WEB_RESOURCES}
)
target_include_directories(test_gallery_content_pages PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_link_libraries(test_gallery_content_pages
    PRIVATE FluentQtGalleryCore Qt${QT_VERSION_MAJOR}::Network)
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
                "qt;unit;test_gallery_content_pages;gallery_acceptance;high_dpi;ci_full;local_full"
            TIMEOUT 60)
endforeach()

if(WIN32)
    foreach(_gallery_test_target
            test_gallery_single_instance_probe
            test_gallery_single_instance
            test_gallery_shell_framework
            test_gallery_shell_motion_policy
            test_gallery_content_pages)
        target_link_libraries(${_gallery_test_target} PRIVATE user32)
    endforeach()
endif()
