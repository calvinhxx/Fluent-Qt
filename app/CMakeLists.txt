file(GLOB_RECURSE FLUENT_QT_GALLERY_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/model/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/model/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/view/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/view/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodel/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/viewmodel/*.cpp
)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${FLUENT_QT_GALLERY_SOURCES})

set(FLUENT_QT_GALLERY_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} PARENT_SCOPE)
set(FLUENT_QT_GALLERY_SOURCES ${FLUENT_QT_GALLERY_SOURCES} PARENT_SCOPE)

add_executable(fluent_qt_gallery
    main.cpp
    ${FLUENT_QT_GALLERY_SOURCES}
)
target_link_libraries(fluent_qt_gallery
    PRIVATE
    fluent_qt_lib
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)
target_include_directories(fluent_qt_gallery PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
