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
  For an install-free local toolchain, extract the official ZIP under
  `build/nsis-toolchain/nsis-<version>`; the project wrapper discovers its
  top-level `makensis.exe` automatically. The `build` directory remains ignored.
- Linux DEB packaging requires `dpkg-shlibdeps` (provided by `dpkg-dev`; it is
  installed transitively by Ubuntu's `build-essential`).

The packaging install step runs the Qt deployment tool by default. Set
`FLUENT_QT_PACKAGE_DEPLOY_QT_RUNTIME=OFF` only for local smoke packages that are
allowed to depend on the developer machine's Qt installation.

Gallery packages intentionally include only the `GalleryRuntime` component. This
keeps DMG/NSIS artifacts focused on the showcase application while normal
`cmake --install` or `cmake --install --component Development` remains available
for SDK-style library installs.

Gallery application identity is defined once in `app/GalleryMetadata.cmake`.
CMake applies it to the native metadata templates under `app/platform/windows`,
`app/platform/macos`, and `app/platform/linux`; do not duplicate the application
ID or display name in those templates.

## Linux DEB

Linux packages are built natively per architecture. x64:

```bash
cmake --preset vcpkg-linux-release
cmake --build --preset vcpkg-linux-release
cpack --preset vcpkg-linux-deb
# -> Fluent-Qt-Gallery-<version>-Linux-x86_64.deb
```

ARM64:

```bash
cmake --preset vcpkg-linux-arm64-release
cmake --build --preset vcpkg-linux-arm64-release
cpack --preset vcpkg-linux-arm64-deb
# -> Fluent-Qt-Gallery-<version>-Linux-arm64.deb
```

The package installs the Gallery executable under `/usr/bin`, a freedesktop
entry under `/usr/share/applications`, and the application icon in the hicolor
icon theme. CPack runs `dpkg-shlibdeps` so the package declares the Qt and system
libraries required by the binary built on the Ubuntu 22.04 baseline. The Qt
runtime is intentionally consumed from distribution packages rather than
duplicated inside the DEB. Because `dpkg-shlibdeps` cannot see plugins loaded
dynamically by `QApplication`, Qt 6 packages also declare `qt6-qpa-plugins` as
an explicit dependency and recommend `qt6-wayland` for native Wayland sessions.
Do not use the ARM64 preset as a cross-compilation preset on x64; it is intended
for a native ARM64 Linux host or runner.

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

The executable embeds the app icon and version metadata via
`app/platform/windows/app.rc.in`
(compiled into a `.rc` at configure time), so Explorer, the taskbar, Alt-Tab and
the installer-created shortcuts all show the Fluent-Qt Gallery icon. The icon
source of truth is `app/assets/Fluent-Qt-Gallery.ico`, the Windows counterpart to
the macOS `app/assets/Fluent-Qt-Gallery.icns`. Both are derived from the shared
`app/assets/app-icon.png` master.

### Installer branding

The NSIS wizard keeps CPack's reliable installation behavior but applies a
project-owned Modern UI presentation layer:

- Installer/uninstaller window icon: `app/assets/Fluent-Qt-Gallery.ico`.
- Welcome/Finish sidebar (164x314) and inner-page header banner (150x57): light
  Fluent artwork in `app/assets/installer-welcome.bmp` and
  `app/assets/installer-header.bmp`, generated from `app-icon.png` by
  `cmake/GenerateNsisBranding.ps1`. Version text must not be baked into images.
- Segoe UI typography, concise page copy, centered version branding, a GitHub
  link, and a finish-page "launch Fluent-Qt Gallery" option.
- The unused Install Options page and interactive Start Menu folder page are
  skipped. Current-user Start Menu and desktop shortcuts are still created.

Branding assets are wired in `cmake/FluentQTPackaging.cmake`; the small,
template-independent page customization is applied by
`cmake/PatchNsisScript.ps1` immediately before `makensis` runs.

### Architectures

Windows is packaged per-architecture, mirroring the macOS arm64/x64 split. The
artifact name carries the architecture (`...-Windows-x64.exe`,
`...-Windows-arm64.exe`):

```powershell
# x64 (default)
cmake --preset vcpkg-windows-release
cmake --build --preset vcpkg-windows-release
cpack --preset vcpkg-windows-installer

# ARM64 cross-build from an x64 host via the Visual Studio generator
cmake --preset vcpkg-windows-arm64-release -D "CMAKE_PREFIX_PATH=C:/Qt/6.9.3/msvc2022_arm64" -D "QT_HOST_PATH=C:/Qt/6.9.3/msvc2022_64"
cmake --build --preset vcpkg-windows-arm64-release
cpack --preset vcpkg-windows-arm64-installer
```

The ARM64 presets work on native Windows ARM64 and as an x64-hosted cross-build.
Both routes require the **`msvc2022_arm64` Qt kit**. The cross-build additionally
requires a matching x64 host Qt kit supplied through `QT_HOST_PATH`; the native
route does not. Public presets intentionally do not hard-code machine paths;
pass them on the configure command line or put them in an ignored
`CMakeUserPresets.json`.

For an x64-hosted ARM64 package, the host `windeployqt` must receive the target
Qt kit's `qtpaths` wrapper. Otherwise it deploys x64 Qt DLLs beside the ARM64
Gallery executable and Windows rejects the process with
`STATUS_INVALID_IMAGE_FORMAT` (`0xc000007b`). CI verifies every packaged EXE and
DLL is ARM64 and that the app-local ARM64 MSVC runtime is present before it
uploads the installer.

CI runs the ARM64 binaries on the native `windows-11-arm` runner before release,
while the full CI and release workflows keep the established x64-hosted
cross-build for the installer artifact. A full CI run uploads
`fluent-qt-gallery-windows-arm64-installer`, which can be installed directly in
a Windows 11 ARM VM for visual review. This separates runtime compatibility
from deterministic packaging.

> 32-bit x86 is intentionally not packaged: Qt 6 ships no 32-bit Windows binaries,
> so an `x86-windows` build would require a self-compiled 32-bit Qt.

## Desktop Compatibility Test Artifacts

The `Desktop Compat Test Packages` GitHub Actions workflow builds installable
artifacts from release, test, and agent branches and can also be started with
`workflow_dispatch`. It publishes four independent, 14-day test artifacts:

- Windows 10-compatible x64 installer built with Qt 6.2.4.
- macOS Apple Silicon DMG built with Qt 6.2.4.
- Ubuntu 22.04 x64 DEB built against the distribution Qt 5.15 baseline.
- Ubuntu 22.04 x64 DEB built against the distribution Qt 6.2 baseline.

Each lane runs its platform/package-focused tests before CPack and uploads a
SHA-256 file beside the installer. These artifacts are for manual installation
validation; tagged release assets continue to be produced by `release.yml`.

## Version Contract

The root CMake `project(FluentQT VERSION X.Y.Z ...)` value is the package
version. Keep release tags, generated changelog entries, and CPack artifact
names aligned with that version.
