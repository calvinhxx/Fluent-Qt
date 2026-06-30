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
# Name the artifact after the architecture it actually contains, not the build host's processor:
#   - macOS ships one single-arch DMG per CPU (arm64 / x86_64 packaged separately), so the suffix
#     follows the requested CMAKE_OSX_ARCHITECTURES — otherwise an x86_64 image cross-built on Apple
#     Silicon would be mislabelled "arm64".
#   - Windows cross-builds ARM64 via the VS generator (-A <arch>, CMAKE_VS_PLATFORM_NAME) while
#     CMAKE_SYSTEM_PROCESSOR still reports the x64 host, so the suffix follows the VS platform name
#     (normalized to x64 / arm64 / x86).
# zh_CN: 产物按其实际包含的架构命名,而非构建主机处理器:macOS arm64/x86_64 分别打单架构 DMG,后缀跟
# CMAKE_OSX_ARCHITECTURES(否则 Apple Silicon 上交叉出的 x86_64 会被错标 arm64);Windows 用 VS 生成器
# 交叉构建 ARM64,CMAKE_SYSTEM_PROCESSOR 仍报 x64 主机,故后缀跟 VS 平台名(规范化为 x64/arm64/x86)。
set(_fluent_qt_pkg_arch "${CMAKE_SYSTEM_PROCESSOR}")
if(APPLE AND CMAKE_OSX_ARCHITECTURES)
    list(LENGTH CMAKE_OSX_ARCHITECTURES _fluent_qt_osx_arch_count)
    if(_fluent_qt_osx_arch_count EQUAL 1)
        set(_fluent_qt_pkg_arch "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(_fluent_qt_pkg_arch "universal")
    endif()
elseif(WIN32 AND CMAKE_VS_PLATFORM_NAME)
    if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
        set(_fluent_qt_pkg_arch "arm64")
    elseif(CMAKE_VS_PLATFORM_NAME STREQUAL "x64")
        set(_fluent_qt_pkg_arch "x64")
    elseif(CMAKE_VS_PLATFORM_NAME STREQUAL "Win32")
        set(_fluent_qt_pkg_arch "x86")
    else()
        set(_fluent_qt_pkg_arch "${CMAKE_VS_PLATFORM_NAME}")
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
    # Bundle the MSVC C/C++ runtime (vcruntime140.dll, msvcp140.dll, ...) app-locally so the installed
    # app launches on a clean machine that has no VC++ redistributable installed. CMake locates these
    # from the detected MSVC toolset, which is reliable; windeployqt's --compiler-runtime only copies
    # them inside a Visual Studio developer environment.
    # zh_CN: 把 MSVC C/C++ 运行库（vcruntime140.dll、msvcp140.dll 等）随应用一起装，未装 VC++ 运行库的
    # 干净机器也能启动。CMake 从探测到的 MSVC 工具集定位它们，可靠；windeployqt 的 --compiler-runtime
    # 只在 Visual Studio 开发者环境里才会复制。
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    include(InstallRequiredSystemLibraries)

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_NSIS_DISPLAY_NAME "Fluent-QT Gallery")
    set(CPACK_NSIS_PACKAGE_NAME "Fluent-QT Gallery")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${CMAKE_INSTALL_BINDIR}")
    # Brand the installer + uninstaller wizard with the app icon (the macOS DMG has its background
    # image; this is the Windows equivalent). makensis reads these absolute source paths at package
    # time. Add/Remove Programs and the created shortcuts pick up the icon embedded in the exe.
    # zh_CN: 给安装器/卸载器向导加上应用图标（对应 macOS DMG 的背景图）。makensis 在打包时读取这两个绝对源路径；
    # 控制面板"添加/删除程序"和创建的快捷方式都用 exe 里嵌入的图标。
    set(_fluent_qt_gallery_ico "${PROJECT_SOURCE_DIR}/app/assets/Fluent-QT-Gallery.ico")
    set(CPACK_NSIS_MUI_ICON "${_fluent_qt_gallery_ico}")
    set(CPACK_NSIS_MUI_UNIICON "${_fluent_qt_gallery_ico}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME
        "${CMAKE_INSTALL_BINDIR}\\\\fluent_qt_gallery.exe")
    # Style the wizard like the macOS DMG instead of the raw NSIS grey look: a blue->green branded
    # sidebar on the Welcome/Finish pages, a small header banner on the inner pages, bottom branding
    # text, and a "run now" checkbox on the finish page. The bitmaps are 24-bit BMPs at the NSIS MUI
    # conventional sizes (sidebar 164x314, header 150x57), generated from the shared app-icon.png.
    # zh_CN: 把向导做得像 macOS DMG，而不是 NSIS 原生灰扑扑的样子：欢迎/完成页用蓝绿品牌侧栏，内页加小横幅，
    # 底部品牌文字，完成页加"立即运行"勾选。位图是按 NSIS MUI 约定尺寸（侧栏 164x314、横幅 150x57）的 24 位 BMP。
    set(_fluent_qt_welcome_bmp "${PROJECT_SOURCE_DIR}/app/assets/installer-welcome.bmp")
    set(_fluent_qt_header_bmp "${PROJECT_SOURCE_DIR}/app/assets/installer-header.bmp")
    file(TO_NATIVE_PATH "${_fluent_qt_welcome_bmp}" _fluent_qt_welcome_bmp_native)
    string(REPLACE "\\" "\\\\" _fluent_qt_welcome_bmp_native "${_fluent_qt_welcome_bmp_native}")
    set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${_fluent_qt_welcome_bmp_native}")
    set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${_fluent_qt_welcome_bmp_native}")
    # CPack has no dedicated header-bitmap variable; the template already emits `!define MUI_HEADERIMAGE`
    # and inserts CPACK_NSIS_DEFINES before the page macros, so inject the bitmap define there.
    # No quotes (CPack mangles an embedded quote in CPACK_NSIS_DEFINES into ';') and a NATIVE backslash
    # path (NSIS's internal `File` command rejects forward slashes -> "no files found"). The path has
    # no spaces, so the unquoted form is safe.
    # zh_CN: CPack 没有专门的 header 位图变量；模板已 `!define MUI_HEADERIMAGE` 且在页面宏前插入
    # CPACK_NSIS_DEFINES，故在此注入。不加引号（引号会被 CPack 弄成 ';'），且用反斜杠原生路径
    # （NSIS 内部 File 命令不认正斜杠，会报 "no files found"）；路径无空格，未加引号也安全。
    # CPack writes CPACK_NSIS_DEFINES verbatim, so a native path with single backslashes makes the
    # re-read of CPackConfig.cmake choke on invalid escapes (e.g. "\w"). Double the backslashes so the
    # config parses back to single ones, which is what NSIS's File needs.
    # zh_CN: CPack 逐字写入 CPACK_NSIS_DEFINES，单反斜杠的原生路径会让 CPackConfig.cmake 回读时因非法转义
    # （如 "\w"）报错。把反斜杠翻倍：配置解析回单反斜杠，正是 NSIS File 所需。
    file(TO_NATIVE_PATH "${_fluent_qt_header_bmp}" _fluent_qt_header_bmp_native)
    string(REPLACE "\\" "\\\\" _fluent_qt_header_bmp_native "${_fluent_qt_header_bmp_native}")
    set(CPACK_NSIS_DEFINES "!define MUI_HEADERIMAGE_BITMAP ${_fluent_qt_header_bmp_native}")
    set(CPACK_NSIS_BRANDING_TEXT "Fluent-QT Gallery ${PROJECT_VERSION}")
    # Offer to launch the app straight from the finish page. Give only the exe name — the template
    # prepends "$INSTDIR\<executables-dir>\" (here bin\), so a bin\ prefix would double it.
    # zh_CN: 完成页提供"立即运行"。只给 exe 名——模板会自动加 "$INSTDIR\<可执行目录>\"（这里是 bin\），
    # 带 bin\ 前缀会重复。
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "fluent_qt_gallery.exe")
    set(CPACK_NSIS_MENU_LINKS
        "${CMAKE_INSTALL_BINDIR}\\\\fluent_qt_gallery.exe" "Fluent-QT Gallery")
    # Always create a desktop shortcut. CPACK_CREATE_DESKTOP_LINKS would instead add an *unchecked*
    # checkbox to the NSIS "Install Options" page (and pull in the PATH radio page even with
    # MODIFY_PATH off), so no desktop icon appears unless the user happens to tick it. Create it
    # directly on install and remove it on uninstall; the .lnk inherits the exe's embedded icon.
    # zh_CN: 安装时直接创建桌面快捷方式。用 CPACK_CREATE_DESKTOP_LINKS 只会在 NSIS "Install Options"
    # 页加一个默认未勾选的复选框（即便关了 MODIFY_PATH 也会带出 PATH 单选页），用户不勾就没有桌面图标。
    # 这里安装时直接建、卸载时删除；.lnk 会沿用 exe 内嵌图标。
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "CreateShortCut '$DESKTOP\\\\Fluent-QT Gallery.lnk' '$INSTDIR\\\\${CMAKE_INSTALL_BINDIR}\\\\fluent_qt_gallery.exe'")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "Delete '$DESKTOP\\\\Fluent-QT Gallery.lnk'")
endif()

include(CPack)
