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

# Build as a GUI-subsystem app in non-Debug configs so launching the exe standalone does not pop a
# console window. Debug keeps the console subsystem so spdlog output stays visible while developing.
# Qt's WinMain shim comes from Qt6::EntryPoint (linked transitively via Qt Widgets), so main() is unchanged.
# zh_CN: 非 Debug 配置链成 GUI 子系统，独立启动 exe 时不弹控制台；Debug 保留控制台以便开发时看 spdlog 日志。
# WinMain 入口由随 Qt Widgets 传递链接的 Qt6::EntryPoint 提供，main() 无需改动。
if(WIN32)
    set_target_properties(fluent_qt_gallery PROPERTIES
        WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>
    )
endif()
