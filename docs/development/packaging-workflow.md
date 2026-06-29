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

```bash
cmake --preset vcpkg-osx-release
cmake --build --preset vcpkg-osx-release
cpack --preset vcpkg-osx-dmg
```

For Intel/Rosetta:

```bash
cmake --preset vcpkg-osx-x64-release
cmake --build --preset vcpkg-osx-x64-release
cpack --preset vcpkg-osx-x64-dmg
```

The generated DMG uses the CPack `DragNDrop` generator and contains
`fluent_qt_gallery.app`.

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
