if(NOT DEFINED FLUENT_QT_GALLERY_SOURCE_DIR)
    set(FLUENT_QT_GALLERY_SOURCE_DIR ${CMAKE_SOURCE_DIR}/app)
endif()
if(NOT DEFINED FLUENT_QT_GALLERY_SOURCES)
    file(GLOB_RECURSE FLUENT_QT_GALLERY_SOURCES CONFIGURE_DEPENDS
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/model/*.h
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/model/*.cpp
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/view/*.h
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/view/*.cpp
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/viewmodel/*.h
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/viewmodel/*.cpp
    )
endif()
if(NOT DEFINED FLUENT_QT_GALLERY_RESOURCES)
    set(FLUENT_QT_GALLERY_RESOURCES
        ${FLUENT_QT_GALLERY_SOURCE_DIR}/gallery_resources.qrc
    )
endif()

add_executable(test_gallery_single_instance_probe
    GallerySingleInstanceProbe.cpp
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/view/shell/GallerySingleInstance.cpp
    ${FLUENT_QT_GALLERY_SOURCE_DIR}/view/shell/GallerySingleInstance.h
)
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
target_compile_definitions(test_gallery_single_instance PRIVATE
    FLUENT_QT_GALLERY_SINGLE_INSTANCE_PROBE_PATH="$<TARGET_FILE:test_gallery_single_instance_probe>")
add_dependencies(test_gallery_single_instance test_gallery_single_instance_probe)

add_qt_test_module(test_gallery_shell_framework
    TestGalleryShellFramework.cpp
    ${FLUENT_QT_GALLERY_SOURCES}
    ${FLUENT_QT_GALLERY_RESOURCES}
    ${CMAKE_SOURCE_DIR}/tests/support/VisualGeometryTestUtils.cpp
)
target_include_directories(test_gallery_shell_framework PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
target_compile_definitions(test_gallery_shell_framework PRIVATE
    FLUENT_QT_GALLERY_SINGLE_INSTANCE_PROBE_PATH="$<TARGET_FILE:test_gallery_single_instance_probe>")
add_dependencies(test_gallery_shell_framework test_gallery_single_instance_probe)

add_qt_test_module(test_gallery_content_pages
    TestGalleryContentPages.cpp
    ${FLUENT_QT_GALLERY_SOURCES}
    ${FLUENT_QT_GALLERY_RESOURCES}
)
target_include_directories(test_gallery_content_pages PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})

if(WIN32)
    foreach(_gallery_test_target
            test_gallery_single_instance_probe
            test_gallery_single_instance
            test_gallery_shell_framework
            test_gallery_content_pages)
        target_link_libraries(${_gallery_test_target} PRIVATE user32)
    endforeach()
endif()
