# Packaging Workflow

Use CPack for release artifacts. The Gallery app is the packaged runtime
surface; the reusable component library is installed through the separate
`Development` install component and exported as `FluentQt::FluentQt`.

## Prerequisites

- Configure with a Release packaging preset.
- Keep `VCPKG_ROOT` set for vcpkg manifest dependencies.
- Ensure the Qt deployment tool from the selected Qt kit is available:
  - macOS: `macdeployqt`.
  - Windows: `windeployqt`.
- Windows NSIS packaging requires NSIS to be installed and available to CPack.

The packaging install step runs the Qt deployment tool by default. Set
`FLUENT_QT_PACKAGE_DEPLOY_QT_RUNTIME=OFF` only for local smoke packages that are
allowed to depend on the developer machine's Qt installation.

Gallery packages intentionally include only the `GalleryRuntime` component. This
keeps DMG/NSIS artifacts focused on the showcase application while normal
`cmake --install` or `cmake --install --component Development` remains available
for SDK-style library installs.

## macOS DMG

macOS ships **one single-architecture DMG per CPU**: Apple Silicon and Intel are
packaged separately rather than as a fat universal image. Each preset pins
`CMAKE_OSX_ARCHITECTURES` to a single slice, and the install step thins the
(universal) Qt frameworks `macdeployqt` copies down to that slice, so every image
carries only the code its CPU runs (roughly half the size of a universal build).

Apple Silicon (arm64):

```bash
cmake --preset vcpkg-osx-release
cmake --build --preset vcpkg-osx-release
cpack --preset vcpkg-osx-dmg
# -> Fluent-Qt-Gallery-<version>-Darwin-arm64.dmg
```

Intel (x86_64, cross-builds on Apple Silicon and runs under Rosetta):

```bash
cmake --preset vcpkg-osx-x64-release
cmake --build --preset vcpkg-osx-x64-release
cpack --preset vcpkg-osx-x64-dmg
# -> Fluent-Qt-Gallery-<version>-Darwin-x86_64.dmg
```

Each DMG uses the CPack `DragNDrop` generator and contains
`Fluent-Qt Gallery.app` (drag-to-install layout beside the `/Applications`
alias). The artifact's architecture suffix follows the requested
`CMAKE_OSX_ARCHITECTURES`, not the build host's processor.

## Windows Installer

```powershell
cmake --preset vcpkg-windows-release
cmake --build --preset vcpkg-windows-release
cpack --preset vcpkg-windows-installer
```

The generated installer uses the CPack `NSIS` generator and installs the Gallery
executable under the package `bin` directory. It always creates Start Menu and
desktop shortcuts, and offers a "run now" checkbox on the finish page.

### Elevation model

The Windows installer is a per-user installer. It installs by default under
`%LOCALAPPDATA%\Programs\Fluent-Qt Gallery`, writes uninstall metadata under the
current user registry hive, and creates Start Menu/Desktop shortcuts for the
current user. It must not require an administrator token for a normal install.

CPack 4.2's bundled NSIS template is machine-wide by default, so
`cmake/FluentQTPackaging.cmake` routes `makensis` through
`cmake/nsis-per-user-wrapper.cmd`. The wrapper patches the generated NSIS script
just before compilation, keeping the per-user policy centralized in the
packaging layer without maintaining a full copied NSIS template.

The executable embeds the app icon and version metadata via `app/app.rc.in`
(compiled into a `.rc` at configure time), so Explorer, the taskbar, Alt-Tab and
the installer-created shortcuts all show the Fluent-Qt Gallery icon. The icon
source of truth is `app/assets/Fluent-Qt-Gallery.ico`, the Windows counterpart to
the macOS `app/assets/Fluent-Qt-Gallery.icns`. Both are derived from the shared
`app/assets/app-icon.png` master.

### Installer branding

The NSIS wizard is styled to match the polish of the macOS DMG rather than the raw
grey NSIS look:

- Installer/uninstaller window icon: `app/assets/Fluent-Qt-Gallery.ico`.
- Welcome/Finish sidebar (164x314) and inner-page header banner (150x57): a
  blue-green branded gradient in `app/assets/installer-welcome.bmp` and
  `app/assets/installer-header.bmp`, generated from `app-icon.png`.
- Bottom branding text and a finish-page "run Fluent-Qt Gallery" option.

All branding is wired in `cmake/FluentQTPackaging.cmake`.

### Architectures

Windows is packaged per-architecture, mirroring the macOS arm64/x64 split. The
artifact name carries the architecture (`...-Windows-x64.exe`,
`...-Windows-arm64.exe`):

```powershell
# x64 (default)
cmake --preset vcpkg-windows-release
cmake --build --preset vcpkg-windows-release
cpack --preset vcpkg-windows-installer

# ARM64 (cross-built from an x64 host via the Visual Studio generator)
cmake --preset vcpkg-windows-arm64-release -D "CMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_arm64" -D "QT_HOST_PATH=C:/Qt/6.9.3/msvc2022_64"
cmake --build --preset vcpkg-windows-arm64-release
cpack --preset vcpkg-windows-arm64-installer
```

The ARM64 presets require the **`msvc2022_arm64` Qt kit** installed (via the Qt
Maintenance Tool) and a matching x64 host Qt kit. Public presets intentionally
do not hard-code those machine paths; pass `CMAKE_PREFIX_PATH` and
`QT_HOST_PATH` on the configure command line or put them in an ignored
`CMakeUserPresets.json`.

> 32-bit x86 is intentionally not packaged: Qt 6 ships no 32-bit Windows binaries,
> so an `x86-windows` build would require a self-compiled 32-bit Qt.

## Version Contract

The root CMake `project(FluentQT VERSION X.Y.Z ...)` value is the package
version. Keep release tags, generated changelog entries, and CPack artifact
names aligned with that version.
