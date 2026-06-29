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

# Build a proper macOS .app bundle. A bundle carries a CFBundleIdentifier, which is what macOS
# (LaunchServices, the Accessibility/automation layer, screen-capture filtering) keys on to identify
# an application — a bare Mach-O executable has none, so tools that target apps by identity can't see
# it. The bundle also lets us declare NSHighResolutionCapable for crisp Retina rendering.
# zh_CN: 在 macOS 上打成正经的 .app 包。包带 CFBundleIdentifier，这是 macOS（LaunchServices、辅助功能/自动化层、
# 截屏过滤）识别一个「应用」的依据——裸 Mach-O 可执行文件没有它，按应用身份定位的工具就看不到它。包里还能声明
# NSHighResolutionCapable 以保证 Retina 清晰渲染。
if(APPLE)
    # Ship a Finder/Dock/DMG icon. The .icns is copied into Contents/Resources and named by
    # CFBundleIconFile (set via MACOSX_BUNDLE_ICON_FILE, substituted into Info.plist.in). Without
    # this the bundle has no icon file and LaunchServices falls back to the generic app placeholder.
    # zh_CN: 给 Finder/Dock/DMG 一个图标。.icns 拷进 Contents/Resources，由 CFBundleIconFile
    # （经 MACOSX_BUNDLE_ICON_FILE 写入 Info.plist.in）命名；不设置则 bundle 没有图标文件，
    # LaunchServices 会回退到通用占位图标。
    set(FLUENT_QT_GALLERY_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/assets/Fluent-QT-Gallery.icns")
    target_sources(fluent_qt_gallery PRIVATE "${FLUENT_QT_GALLERY_ICNS}")
    set_source_files_properties("${FLUENT_QT_GALLERY_ICNS}" PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    set_target_properties(fluent_qt_gallery PROPERTIES
        MACOSX_BUNDLE TRUE
        # OUTPUT_NAME drives the on-disk bundle + executable name. The CMake target stays the
        # snake_case fluent_qt_gallery, but the user-facing bundle becomes "Fluent-QT Gallery.app"
        # (the snake_case name looked rough in the Dock / DMG). zh_CN: OUTPUT_NAME 决定磁盘上的
        # bundle 与可执行名;CMake target 仍叫 fluent_qt_gallery,但对用户呈现为 Fluent-QT Gallery.app。
        OUTPUT_NAME "Fluent-QT Gallery"
        MACOSX_BUNDLE_BUNDLE_NAME "Fluent-QT Gallery"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.fluentqt.gallery"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "Fluent-QT-Gallery"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        # macdeployqt copies Qt into Contents/Frameworks and rewrites the binary to @rpath/Qt*.
        # The installed binary needs this rpath or dyld reports "no LC_RPATH's found" and the app
        # won't launch. zh_CN: 安装后的二进制需要这条 rpath,否则 dyld 报 "no LC_RPATH's found",应用无法启动。
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
endif()

include(GNUInstallDirs)
install(TARGETS fluent_qt_gallery
    BUNDLE DESTINATION "."
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
