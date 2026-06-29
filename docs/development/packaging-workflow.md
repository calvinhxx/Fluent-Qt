# Packaging Workflow

Use CPack for release artifacts. The Gallery app is the packaged runtime
surface; the reusable component library is not installed as a public SDK yet.

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

## macOS DMG

macOS ships **one single-architecture DMG per CPU** — Apple Silicon and Intel are
packaged separately rather than as a fat universal image. Each preset pins
`CMAKE_OSX_ARCHITECTURES` to a single slice, and the install step thins the
(universal) Qt frameworks `macdeployqt` copies down to that slice, so every image
carries only the code its CPU runs (roughly half the size of a universal build).

Apple Silicon (arm64):

```bash
cmake --preset vcpkg-osx-release
cmake --build --preset vcpkg-osx-release
cpack --preset vcpkg-osx-dmg
# → Fluent-QT-Gallery-<version>-Darwin-arm64.dmg
```

Intel (x86_64, cross-builds on Apple Silicon and runs under Rosetta):

```bash
cmake --preset vcpkg-osx-x64-release
cmake --build --preset vcpkg-osx-x64-release
cpack --preset vcpkg-osx-x64-dmg
# → Fluent-QT-Gallery-<version>-Darwin-x86_64.dmg
```

Each DMG uses the CPack `DragNDrop` generator and contains
`Fluent-QT Gallery.app` (drag-to-install layout beside the `/Applications`
alias). The artifact's architecture suffix follows the requested
`CMAKE_OSX_ARCHITECTURES`, not the build host's processor.

## Windows Installer

```powershell
cmake --preset vcpkg-windows-release
cmake --build --preset vcpkg-windows-release
cpack --preset vcpkg-windows-installer
```

The generated installer uses the CPack `NSIS` generator and installs the Gallery
executable under the package `bin` directory.

## Version Contract

The root CMake `project(FluentQT VERSION X.Y.Z ...)` value is the package
version. Keep release tags, generated changelog entries, and CPack artifact
names aligned with that version.
