include(GNUInstallDirs)
include("${PROJECT_SOURCE_DIR}/app/GalleryMetadata.cmake")

option(FLUENT_QT_PACKAGE_DEPLOY_QT_RUNTIME
    "Run macdeployqt or windeployqt while installing the packaged Gallery app"
    ON)

if(NOT TARGET ${FLUENT_QT_GALLERY_EXECUTABLE_NAME})
    return()
endif()

# Ship the license in the platform-appropriate install location. The macOS DMG
# deliberately keeps it out of the drag-to-install window and presents it as a
# mount-time SLA instead.
if(WIN32)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
        DESTINATION "."
        COMPONENT GalleryRuntime)
elseif(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DOCDIR}"
        COMPONENT GalleryRuntime)
endif()

set(CPACK_COMPONENTS_ALL GalleryRuntime)

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
# snake_case target name.
if(APPLE)
    set(FLUENT_QT_GALLERY_BUNDLE_DIR "${FLUENT_QT_GALLERY_DISPLAY_NAME}.app")
else()
    set(FLUENT_QT_GALLERY_BUNDLE_DIR "${FLUENT_QT_GALLERY_EXECUTABLE_NAME}.app")
endif()
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/InstallDeployQtRuntime.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/InstallDeployQtRuntime.cmake"
    @ONLY)
install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/InstallDeployQtRuntime.cmake"
    COMPONENT GalleryRuntime)

set(CPACK_PACKAGE_NAME "${FLUENT_QT_GALLERY_PACKAGE_BASENAME}")
set(CPACK_PACKAGE_VENDOR "${FLUENT_QT_GALLERY_ORGANIZATION_NAME}")
set(CPACK_PACKAGE_CONTACT "Fluent-Qt maintainers")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "WinUI-style Qt Widgets gallery")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${FLUENT_QT_GALLERY_DISPLAY_NAME}")
# Note: CPACK_RESOURCE_FILE_LICENSE is set per-platform in the APPLE/WIN32 branches below, not
# globally, because each generator presents it differently: a mount-time click-through SLA on the
# macOS DragNDrop image, and the installer license page on Windows NSIS.
# Name the artifact after the architecture it actually contains, not the build host's processor:
#   - macOS ships one single-arch DMG per CPU (arm64 / x86_64 packaged separately), so the suffix
#     follows the requested CMAKE_OSX_ARCHITECTURES; otherwise an x86_64 image cross-built on Apple
#     Silicon would be mislabelled "arm64".
#   - Windows cross-builds ARM64 via the VS generator (-A <arch>, CMAKE_VS_PLATFORM_NAME) while
#     CMAKE_SYSTEM_PROCESSOR still reports the x64 host, so the suffix follows the VS platform name
#     (normalized to x64 / arm64 / x86).
#   - Native Linux ARM64 hosts commonly report aarch64, while Debian and the public support matrix
#     call the architecture arm64. Normalize that spelling without renaming existing x86_64 assets.
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
elseif(UNIX AND NOT APPLE)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _fluent_qt_linux_processor)
    if(_fluent_qt_linux_processor MATCHES "^(aarch64|arm64)$")
        set(_fluent_qt_pkg_arch "arm64")
    endif()
endif()
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${_fluent_qt_pkg_arch}")
set(CPACK_PACKAGE_EXECUTABLES
    "${FLUENT_QT_GALLERY_EXECUTABLE_NAME}" "${FLUENT_QT_GALLERY_DISPLAY_NAME}")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${FLUENT_QT_GALLERY_DISPLAY_NAME} ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT "UDZO")
    # Present the license as a click-through SLA shown before the disk image mounts, so the user
    # accepts the terms before reaching the install window. DragNDrop turns an explicit
    # CPACK_RESOURCE_FILE_LICENSE into the SLA; the platform-specific license
    # install rules above keep the drag-to-install window clean.
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_DMG_SLA_USE_RESOURCE_FILE_LICENSE ON)
    set(CPACK_DMG_SLA_LANGUAGES "English")
    # Style the mounted DMG as a drag-to-install window. DragNDrop adds the /Applications
    # symlink; the setup AppleScript positions the .app beside it over a HiDPI background.
    set(CPACK_DMG_BACKGROUND_IMAGE "${PROJECT_SOURCE_DIR}/app/assets/dmg-background.tiff")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${PROJECT_SOURCE_DIR}/cmake/dmg-setup.applescript")
elseif(WIN32)
    # Bundle the MSVC C/C++ runtime (vcruntime140.dll, msvcp140.dll, ...) app-locally so the installed
    # app launches on a clean machine that has no VC++ redistributable installed. CMake locates these
    # from the detected MSVC toolset, which is reliable; windeployqt's --compiler-runtime only copies
    # them inside a Visual Studio developer environment.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
    include(InstallRequiredSystemLibraries)

    set(CPACK_GENERATOR "NSIS")
    # CPack 4.2's bundled NSIS template is machine-wide by default. Route the
    # makensis call through a tiny project-owned wrapper so the generated script
    # becomes a current-user installer without carrying a full NSIS template.
    set(CPACK_NSIS_EXECUTABLE
        "${CMAKE_CURRENT_LIST_DIR}/nsis-per-user-wrapper.cmd")
    set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA\\\\Programs")
    set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
    set(CPACK_NSIS_DISPLAY_NAME "${FLUENT_QT_GALLERY_DISPLAY_NAME}")
    set(CPACK_NSIS_PACKAGE_NAME "${FLUENT_QT_GALLERY_DISPLAY_NAME}")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "${CMAKE_INSTALL_BINDIR}")
    set(CPACK_NSIS_MANIFEST_DPI_AWARE ON)
    set(CPACK_NSIS_WELCOME_TITLE "Install ${FLUENT_QT_GALLERY_DISPLAY_NAME}")
    set(CPACK_NSIS_FINISH_TITLE "${FLUENT_QT_GALLERY_DISPLAY_NAME} is ready")
    # Brand the installer + uninstaller wizard with the app icon (the macOS DMG has its background
    # image; this is the Windows equivalent). makensis reads these absolute source paths at package
    # time. Add/Remove Programs and the created shortcuts pick up the icon embedded in the exe.
    set(_fluent_qt_gallery_ico
        "${PROJECT_SOURCE_DIR}/app/assets/${FLUENT_QT_GALLERY_ICON_BASENAME}.ico")
    set(CPACK_NSIS_MUI_ICON "${_fluent_qt_gallery_ico}")
    set(CPACK_NSIS_MUI_UNIICON "${_fluent_qt_gallery_ico}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME
        "${CMAKE_INSTALL_BINDIR}\\\\${FLUENT_QT_GALLERY_EXECUTABLE_NAME}.exe")
    # Use light Fluent artwork instead of the stock NSIS wizard graphics. Text and versions stay in
    # the generated script rather than being baked into these 24-bit bitmaps, so every package keeps
    # correct and accessible copy. Generate them with cmake/GenerateNsisBranding.ps1.
    set(_fluent_qt_welcome_bmp "${PROJECT_SOURCE_DIR}/app/assets/installer-welcome.bmp")
    set(_fluent_qt_header_bmp "${PROJECT_SOURCE_DIR}/app/assets/installer-header.bmp")
    file(TO_NATIVE_PATH "${_fluent_qt_welcome_bmp}" _fluent_qt_welcome_bmp_native)
    string(REPLACE "\\" "\\\\" _fluent_qt_welcome_bmp_native "${_fluent_qt_welcome_bmp_native}")
    file(TO_NATIVE_PATH "${_fluent_qt_header_bmp}" _fluent_qt_header_bmp_native)
    string(REPLACE "\\" "\\\\" _fluent_qt_header_bmp_native "${_fluent_qt_header_bmp_native}")
    set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${_fluent_qt_welcome_bmp_native}")
    set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${_fluent_qt_welcome_bmp_native}")
    set(CPACK_NSIS_MUI_HEADERIMAGE "${_fluent_qt_header_bmp_native}")
    set(CPACK_NSIS_BRANDING_TEXT "${FLUENT_QT_GALLERY_DISPLAY_NAME} ${PROJECT_VERSION}")
    set(CPACK_NSIS_BRANDING_TEXT_TRIM_POSITION "CENTER")
    # Offer to launch the app straight from the finish page. Give only the exe name; the template
    # prepends "$INSTDIR\<executables-dir>\" (here bin\), so a bin\ prefix would double it.
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${FLUENT_QT_GALLERY_EXECUTABLE_NAME}.exe")
    set(CPACK_NSIS_MENU_LINKS
        "${CMAKE_INSTALL_BINDIR}\\\\${FLUENT_QT_GALLERY_EXECUTABLE_NAME}.exe"
        "${FLUENT_QT_GALLERY_DISPLAY_NAME}")
    # Always create a desktop shortcut. CPACK_CREATE_DESKTOP_LINKS would instead add an *unchecked*
    # checkbox to the NSIS "Install Options" page (and pull in the PATH radio page even with
    # MODIFY_PATH off), so no desktop icon appears unless the user happens to tick it. Create it
    # directly on install and remove it on uninstall; the .lnk inherits the exe's embedded icon.
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "CreateShortCut '$DESKTOP\\\\${FLUENT_QT_GALLERY_DISPLAY_NAME}.lnk' '$INSTDIR\\\\${CMAKE_INSTALL_BINDIR}\\\\${FLUENT_QT_GALLERY_EXECUTABLE_NAME}.exe'")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "Delete '$DESKTOP\\\\${FLUENT_QT_GALLERY_DISPLAY_NAME}.lnk'")
elseif(UNIX)
    set(FLUENT_QT_PACKAGE_PLATFORM "linux")
    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
    set(CPACK_DEBIAN_PACKAGE_NAME "${FLUENT_QT_GALLERY_LINUX_PACKAGE_NAME}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Fluent-Qt maintainers")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "fonts-noto-color-emoji")
    set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/calvinhxx/Fluent-Qt")
endif()

include(CPack)
