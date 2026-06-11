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

add_qt_test_module(test_gallery_shell_framework
    TestGalleryShellFramework.cpp
    ${FLUENT_QT_GALLERY_SOURCES}
    ${CMAKE_SOURCE_DIR}/tests/support/VisualGeometryTestUtils.cpp
)
target_include_directories(test_gallery_shell_framework PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})

add_qt_test_module(test_gallery_content_pages
    TestGalleryContentPages.cpp
    ${FLUENT_QT_GALLERY_SOURCES}
)
target_include_directories(test_gallery_content_pages PRIVATE ${FLUENT_QT_GALLERY_SOURCE_DIR})
