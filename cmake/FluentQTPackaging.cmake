include(GNUInstallDirs)

option(FLUENT_QT_PACKAGE_DEPLOY_QT_RUNTIME
    "Run macdeployqt or windeployqt while installing the packaged Gallery app"
    ON)

if(NOT TARGET fluent_qt_gallery)
    return()
endif()

# Ship the license file as a loose icon inside the Windows install tree. The macOS DMG window
# deliberately keeps it out — a drag-to-install image should contain only the app and the
# Applications alias — but macOS still surfaces the license as a mount-time SLA instead (see the
# CPACK_RESOURCE_FILE_LICENSE in the APPLE branch below).
# zh_CN: 仅在 Windows 安装目录里以散落图标形式附带 license。macOS DMG 窗口故意不放(拖拽安装镜像里
# 只应有 app 和 Applications 别名),但 macOS 改以挂载时 SLA 形式弹出许可(见下方 APPLE 分支的
# CPACK_RESOURCE_FILE_LICENSE)。
if(NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
        DESTINATION ".")
endif()

set(FLUENT_QT_PACKAGE_PLATFORM "generic")
set(FLUENT_QT_DEPLOYQT_EXECUTABLE "")
set(_fluent_qt_deploy_tool_hints "")

if(TARGET Qt${QT_VERSION_MAJOR}::qmake)
    get_target_property(_fluent_qt_qmake_executable
        Qt${QT_VERSION_MAJOR}::qmake
        IMPORTED_LOCATION)
    if(_fluent_qt_qmake_executable)
        get_filename_component(_fluent_qt_qt_bin_dir
            "${_fluent_qt_qmake_executable}"
            DIRECTORY)
        list(APPEND _fluent_qt_deploy_tool_hints "${_fluent_qt_qt_bin_dir}")
    endif()
endif()

foreach(_fluent_qt_config_dir IN ITEMS
    "${Qt${QT_VERSION_MAJOR}_DIR}"
    "${Qt${QT_VERSION_MAJOR}Core_DIR}")
    if(_fluent_qt_config_dir)
        get_filename_component(_fluent_qt_prefix
            "${_fluent_qt_config_dir}/../../.."
            ABSOLUTE)
        list(APPEND _fluent_qt_deploy_tool_hints "${_fluent_qt_prefix}/bin")
    endif()
endforeach()

if(APPLE)
    set(FLUENT_QT_PACKAGE_PLATFORM "macos")
    find_program(_fluent_qt_deployqt_executable
        NAMES macdeployqt
        HINTS ${_fluent_qt_deploy_tool_hints})
elseif(WIN32)
    set(FLUENT_QT_PACKAGE_PLATFORM "windows")
    find_program(_fluent_qt_deployqt_executable
        NAMES windeployqt
        HINTS ${_fluent_qt_deploy_tool_hints})
endif()
if(_fluent_qt_deployqt_executable)
    set(FLUENT_QT_DEPLOYQT_EXECUTABLE "${_fluent_qt_deployqt_executable}")
endif()

# macOS bundle is renamed via OUTPUT_NAME (app/CMakeLists.txt); the Windows exe keeps the
# snake_case target name. zh_CN: macOS bundle 经 OUTPUT_NAME 改名,Windows 可执行仍用 target 名。
if(APPLE)
    set(FLUENT_QT_GALLERY_BUNDLE_DIR "Fluent-QT Gallery.app")
else()
    set(FLUENT_QT_GALLERY_BUNDLE_DIR "fluent_qt_gallery.app")
endif()
set(FLUENT_QT_GALLERY_EXECUTABLE_NAME "fluent_qt_gallery")
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/InstallDeployQtRuntime.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/InstallDeployQtRuntime.cmake"
    @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/InstallDeployQtRuntime.cmake")

set(CPACK_PACKAGE_NAME "Fluent-QT-Gallery")
set(CPACK_PACKAGE_VENDOR "Fluent-QT")
set(CPACK_PACKAGE_CONTACT "Fluent-QT maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "WinUI-style Qt Widgets gallery")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Fluent-QT Gallery")
# Note: CPACK_RESOURCE_FILE_LICENSE is set per-platform in the APPLE/WIN32 branches below, not
# globally, because each generator presents it differently — a mount-time click-through SLA on the
# macOS DragNDrop image, and the installer license page on Windows NSIS.
# zh_CN: CPACK_RESOURCE_FILE_LICENSE 在下方 APPLE/WIN32 分支按平台分别设置,不全局设——因为各
# 生成器呈现方式不同:macOS DragNDrop 是挂载前的点击同意 SLA,Windows NSIS 是安装器许可页。
# Name the artifact after the architecture it actually contains. On macOS we ship one single-arch
# DMG per CPU (arm64 and x86_64 are packaged separately), so the suffix must follow the requested
# CMAKE_OSX_ARCHITECTURES rather than the build host's processor — otherwise an x86_64 image
# cross-built on Apple Silicon would be mislabelled "arm64".
# zh_CN: 产物按其实际包含的架构命名。macOS 上 arm64 与 x86_64 分开打成各自的单架构 DMG,后缀要跟
# CMAKE_OSX_ARCHITECTURES 走,而不是构建主机的处理器,否则在 Apple Silicon 上交叉构建出的 x86_64
# 镜像会被错标成 "arm64"。
set(_fluent_qt_pkg_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _fluent_qt_osx_arch_count)
    if(_fluent_qt_osx_arch_count EQUAL 1)
        set(_fluent_qt_pkg_arch "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(_fluent_qt_pkg_arch "universal")
    endif()
endif()
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${_fluent_qt_pkg_arch}")
set(CPACK_PACKAGE_EXECUTABLES
    "fluent_qt_gallery" "Fluent-QT Gallery")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Fluent-QT Gallery ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
    # Present the license as a click-through SLA shown before the disk image mounts, so the user
    # accepts the terms before reaching the install window. DragNDrop turns an explicit
    # CPACK_RESOURCE_FILE_LICENSE into the SLA; the license stays out of the window itself (the
    # NOT-APPLE guard on install(FILES LICENSE) above keeps the drag-to-install layout clean).
    # zh_CN: 把许可做成挂载前的点击同意 SLA,用户先接受条款再进入安装窗口。DragNDrop 会把显式的
    # CPACK_RESOURCE_FILE_LICENSE 变成 SLA;许可本身不进窗口(上面 install(FILES LICENSE) 的
    # NOT-APPLE 守卫让拖拽安装布局保持干净)。
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
    set(CPACK_DMG_SLA_LANGUAGES "English")
    # Style the mounted DMG as a drag-to-install window. DragNDrop adds the /Applications
    # symlink; the setup AppleScript positions the .app beside it over a HiDPI background.
    # zh_CN: 把挂载后的 DMG 做成"拖入安装"窗口。DragNDrop 会自动加 /Applications 软链；
    # 这段 AppleScript 在 HiDPI 背景图上把 .app 摆到软链旁边。
    set(CPACK_DMG_BACKGROUND_IMAGE "${PROJECT_SOURCE_DIR}/app/assets/dmg-background.tiff")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${PROJECT_SOURCE_DIR}/cmake/dmg-setup.applescript")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_NSIS_DISPLAY_NAME "Fluent-QT Gallery")
    set(CPACK_NSIS_PACKAGE_NAME "Fluent-QT Gallery")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${CMAKE_INSTALL_BINDIR}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME
        "${CMAKE_INSTALL_BINDIR}\\\\fluent_qt_gallery.exe")
    set(CPACK_CREATE_DESKTOP_LINKS "fluent_qt_gallery")
    set(CPACK_NSIS_MENU_LINKS
        "${CMAKE_INSTALL_BINDIR}\\\\fluent_qt_gallery.exe" "Fluent-QT Gallery")
endif()

include(CPack)
