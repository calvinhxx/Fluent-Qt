# Build Workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Build, tests, and diagnostics

[Contents](../SUMMARY.md) · [Development index](README.md) · [Testing Workflow →](testing-workflow.md)
<!-- docs-nav:top:end -->

Run commands from the repository root. Reuse an existing configured build
when its toolchain and options still match the task.

On MSVC, C++ targets that link `FluentQt::FluentQt` use UTF-8 for source
and execution character sets. This lets consumers read the public headers on
Windows installations with a legacy system code page. A target that supplies
its own character-set flags can set `QT_NO_UTF8_SOURCE` to `ON` and specify
UTF-8 source decoding itself.
When compiling installed headers directly with MSVC and the Qt 5 SDK, pass
`/utf-8` to the compilation that includes them.

## First-use setup

The public [CMake presets](../../CMakePresets.json) require CMake 3.25+; the
library's direct CMake build still supports 3.16+. Install a C++17 toolchain,
Python 3, vcpkg, and a supported desktop Qt Widgets kit (5.15+ or 6.2+). The
Gallery and test dependencies are selected by the preset's vcpkg features;
Qt is discovered from your installed kit.

Desktop C++ Gallery and `FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON` require the
matching Qt Core, Gui, and Widgets private development headers. These SDK
files let Gallery preserve local repainting and support effective DPR below 1;
configuration fails with installation guidance if the interfaces are missing.
Keep `FLUENT_QT_INTERFACE_ZOOM_LIVE=ON`. Debian/Ubuntu packages are
`qtbase5-private-dev` for Qt 5 or `qt6-base-private-dev` for Qt 6, matching the
selected Qt kit. Library-only Widgets and ordinary Python binding builds do
not acquire this requirement. Qt 5.15+/6.2+ remain supported; WebAssembly uses
browser zoom.

Linux builds using an official Qt SDK also need the system development
dependencies exposed by Qt's private GUI interfaces. On Debian/Ubuntu, install
`libxkbcommon-dev` and `libvulkan-dev`; runtime-only XKB/Vulkan packages are not
enough. Keep the Qt private headers from the selected SDK rather than mixing
them with a different distribution Qt version.

Enabling `FLUENT_QT_BUILD_QML_BRIDGE` with Qt 6.2–6.5 also requires the matching
Qt Quick private development headers for its embedded keyboard-focus adapter.
Qt 6.6+, Qt 5, and builds without QmlBridge do not need this additional dependency;
see the [QmlBridge contract](../architecture/qml-bridge.md#build-and-consume).

Set `VCPKG_ROOT` to the vcpkg checkout containing
`scripts/buildsystems/vcpkg.cmake`. Select a preset for the actual host and
toolchain: `vcpkg-osx` for macOS arm64, `vcpkg-windows` for Windows x64, or
`vcpkg-linux` for Linux x64. Architecture variants are listed in
`CMakePresets.json`; use `cmake --list-presets` to see those available locally.

### Debug builds with release Qt

Top-level builds and the standard desktop presets enable
`FLUENT_QT_DEBUG_RELEASE_RUNTIME` by default on Windows, macOS, and Linux.
Project code keeps its Debug optimization and debug-information settings, while
imported Qt and other dependencies prefer Release, RelWithDebInfo, or MinSizeRel.
Single-configuration system kits with `NONE` or unqualified imports are also
supported. A kit that exports only Debug cannot satisfy this policy.

On MSVC, project targets use the matching release CRT (`/MD`). The build copies
the selected vcpkg DLLs next to each executable; Qt loads from its kit. This
Windows deployment path requires CMake 3.21 or newer and the dynamic MSVC
runtime. Other platforms keep their compiler runtime and deployment settings.

To investigate Qt debug-library assertions, Windows debug-heap checks, or
checked STL iterators with a suitable Qt kit, explicitly disable the policy:

```bash
cmake --preset vcpkg-windows -DFLUENT_QT_DEBUG_RELEASE_RUNTIME=OFF
```

Configuring the standard preset without that override restores ON. Keep the
option OFF when embedding FluentQt with `add_subdirectory()` or `FetchContent`;
embedded builds default to OFF so the host selects a consistent configuration
for its whole application.

An installed SDK records its runtime contract. Applications using that SDK
must match it, including their own static libraries. The
[SDK runtime setup](packaging-workflow.md#using-an-installed-sdk) provides one
explicit application-level call; `find_package()` alone does not change the
host's runtime settings.

If Qt is not found automatically, put its installation prefix in an ignored
`CMakeUserPresets.json`. For example, replace the path below with the directory
containing your Qt kit's `lib/cmake`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-osx",
      "inherits": "vcpkg-osx",
      "cacheVariables": {"CMAKE_PREFIX_PATH": "/path/to/Qt/kit"}
    }
  ],
  "buildPresets": [
    {"name": "local-osx", "inherits": "vcpkg-osx", "configurePreset": "local-osx"}
  ],
  "testPresets": [
    {"name": "local-osx", "inherits": "vcpkg-osx", "configurePreset": "local-osx"}
  ]
}
```

Use `local-osx` for configure, build, and CTest with this example; otherwise use
the public preset. Keep Qt, Ninja, compiler, and IDE paths out of shared
presets. Windows requires an environment initialized for its selected MSVC
kit. See [Linux workflow](linux-workflow.md) for distro dependencies and native
desktop setup.

## Configure and build

The adaptive wrapper chooses build parallelism from the current process's CPU
and memory resources:

```bash
cmake --preset vcpkg-osx
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx
```

Pass the same build directory, preset, configuration, target, and native-tool
arguments that would follow `cmake --build`:

```bash
python3 tools/dev/fluent_qt_build.py \
  --preset vcpkg-osx \
  --target fluent_qt_gallery

python3 tools/dev/fluent_qt_build.py \
  build/fluentqt \
  --config Release \
  --target FluentQt
```

PySide generation keeps its complete header dependency set. Shiboken writes to
a temporary directory; only changed generated files replace the previous outputs.
An unrelated header can still trigger generation, but identical wrappers retain
their timestamps and do not force recompilation. Generator failures leave the
last successful outputs in place and fail the build. The generator process uses
a deterministic Qt hash seed, sorts Shiboken's unordered `Extra includes` block,
and places adjacent nonvirtual protected-access helpers before their matching
overrides. Virtual-method order, declarations, bodies, and other includes are preserved.
A generated-header digest makes header-only changes invalidate wrapper objects
in the same Make/Ninja build. Application hash settings remain unchanged. See the
[persistent WSL workflow](linux-workflow.md#repeated-validation-from-a-windows-checkout)
when validating a Windows checkout on Linux repeatedly.

## IDE project tree

The root build adds three module directories: `app`, `uilib`, and `plugin`.
Targets are created from the owning directory's `CMakeLists.txt`, so IDEs can
read the same hierarchy from CMake. After updating the project, rerun CMake;
in Qt Creator, select **Build → Run CMake**.

The main build tree is:

```text
app
  fluent_qt_gallery
  FluentQtGalleryCore
  FluentQtLoggingSupport
uilib
  FluentQt
  fluentqt_hello_world
plugin
  FluentQtPlugins
  plugin_extension
    fluentqt_plugin_extension
  Particles
    FluentQtParticles
  Spatial
    FluentQtSpatial
    FluentQtSpatialRenderer
    fluentqt_hello_world_spatial
  QmlBridge
    FluentQtQmlBridge
    fluentqt_qml_bridge
```

Only enabled targets appear. `FluentQtPlugins` is the lifecycle framework;
`fluentqt_plugin_extension` is an example of writing a plugin. Spatial and
QmlBridge each provide an optional library target and a separate runnable
example. Tests, bindings, and platform directories retain their existing
CMake tree. `app` also owns logging support when C++ tests need it without
building Gallery.

Library implementations remain under `src/`, and example sources remain under
`examples/`. The `examples/hello_world`, `examples/plugin_extension`, and
`examples/qml_bridge` projects remain standalone entry points; the full build
creates their targets under the module that they demonstrate. Target names,
link dependencies, and executable paths are unchanged. IDEs may label source
and generated files differently, but CMake owns the module hierarchy.

## Install the Widgets library with vcpkg

The [fluent-qt overlay port](../../ports/fluent-qt) builds the static C++ Widgets
library against vcpkg's Qt 6. It tracks the release tag in the port manifest.
Gallery, tests, PySide6 and optional FluentQt modules are excluded.

1. For a release with a `fluent-qt-vX.Y.Z-vcpkg.zip` asset, extract it into
   `vcpkg-ports/` in the consuming application. This provides the exact port
   for that release while the official registry PR is under review. For
   v1.12.3, copy the checked-in `ports/fluent-qt/` to `vcpkg-ports/fluent-qt/`.
2. Add `fluent-qt` to the application's `vcpkg.json`. For a new Widgets-only
   application, the following manifest lets the port select the Qt features it
   uses. Existing applications can keep their own Qt features.

   ```json
   {
     "dependencies": [
       "fluent-qt",
       { "name": "qtbase", "default-features": false }
     ]
   }
   ```

3. Add the overlay directory to the application's `vcpkg-configuration.json`.

   ```json
   { "overlay-ports": ["./vcpkg-ports"] }
   ```

4. Link the installed target after defining the application.

   ```cmake
   find_package(FluentQt CONFIG REQUIRED)
   target_link_libraries(my_app PRIVATE FluentQt::FluentQt)
   ```

5. Configure a fresh build directory with the vcpkg toolchain and build.

   ```sh
   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```

The toolchain installs the manifest dependencies during configuration. Use the
same triplet for the application and its dependencies. On Linux and macOS,
Qt's source dependencies use `autoconf`, `autoconf-archive`, `automake`,
`libtool` and `pkg-config` as host tools. On Linux the XCB backend also
requires the X11, XCB and xkbcommon development packages. The
[vcpkg port workflow](../../.github/workflows/vcpkg-port.yml) lists the Ubuntu packages.

## Build the Widgets library with Conan 2

The [Conan recipe](../../packaging/conan/conanfile.py) packages the static Qt 6
Widgets library from a released Fluent-Qt source archive. To create
`fluent-qt/1.12.4` in your local Conan cache, run the following from this
checkout with Conan 2, CMake and a C++17 compiler. Run `conan profile detect`
first if you have no default profile.
Applications can include `<FluentQt/FluentQt.h>` with the SDK's `include`
directory and the Qt headers on the include path. The fix ships in the 1.12.4
source archive. The recipe applies a backport when building 1.12.3.

```sh
conan create packaging/conan --version=1.12.4 --build=missing
```

Conan resolves Qt through the host profile and may build it from source.
To reuse an installed Qt SDK, include the matching compiler and architecture
profile and declare the exact SDK version. For example, save this as
`qt-sdk` for a native Qt 6.9.3 installation:

```ini
include(default)
[platform_requires]
qt/6.9.3
```

Point `CMAKE_PREFIX_PATH` at that SDK's installation prefix, then run
`conan create packaging/conan --version=1.12.4 -pr:h ./qt-sdk -pr:b default --build=missing`.
Both commands build and run the [consumer test](../../packaging/conan/test_package)
on a native build.

In the consuming application's `conanfile.txt`, use:

```ini
[requires]
fluent-qt/1.12.4

[generators]
CMakeDeps
CMakeToolchain
```

Run `conan install` with the same host profile and configure CMake with the
generated toolchain. Link `FluentQt::FluentQt` after
`find_package(FluentQt CONFIG REQUIRED)`. Each new cache needs the local
creation step until the recipe is published on ConanCenter.

## Install the library or create a source package

For a standalone library build, Qt is the only external dependency. From a
fresh build directory, configure without the vcpkg preset:

```bash
cmake -S . -B build/fluentqt \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/kit
python3 tools/dev/fluent_qt_build.py build/fluentqt --config Release --target FluentQt
cmake --install build/fluentqt --config Release \
  --component Development --prefix /path/to/install
```

The development component contains the library, public headers, and CMake
package files. Consumers use `find_package(FluentQt CONFIG REQUIRED)` and link
`FluentQt::FluentQt`. Point `CMAKE_PREFIX_PATH` at the install prefix when it is
outside the system search path.

For a manually configured build, add the SDK's `include` directory and use
`#include <FluentQt/FluentQt.h>`. Configure Qt Widgets, the FluentQt library
and its platform link dependencies in your build system.

Create the reduced library source package for offline or source integration:

```bash
python3 tools/dev/fluent_qt_build.py build/fluentqt --target fluent_qt_source_package
```

Use [packaging workflow](packaging-workflow.md) for desktop Gallery installers
and [WebAssembly workflow](webassembly-workflow.md) for browser artifacts.

## Rebuild bundled fonts

Ordinary CMake builds use the committed resources in `res/` and make no font
network requests. Upstream font-generation inputs are described by
`tools/fonts/source-assets.json`; their licenses and provenance remain under
`third_party/`. They are cached in the ignored `build/font-sources/` directory.

A full clone can restore these inputs from local Git objects without networking:

```bash
python3 tools/fonts/fetch_font_sources.py
```

For a shallow/partial clone or source archive without those objects, explicitly
allow downloads with `python3 tools/fonts/fetch_font_sources.py --download`.
Every cache entry, recovered object, and download must match the pinned size
and SHA-256 before use. Set `FLUENTQT_FONT_SOURCE_CACHE` to use a shared or
prepopulated offline cache; `--cache-dir` overrides it for the fetch command.
Missing inputs fail with preparation instructions rather than using a system
font. A corrupt download never replaces a cache entry.

After installing `tools/fonts/requirements.txt` in a Python environment, verify
that generation preserves the committed runtime resources:

```bash
python3 tools/fonts/generate_typography_assets.py --check
python3 tools/fonts/generate_typography_assets.py --check-web-fallback
```

## Selection policy

The wrapper takes the smaller calculated limit from the CPU and memory checks,
with a minimum of one job. It does not set a fixed maximum for
high-resource hosts.

| Resource | Detection |
|---|---|
| CPU | Logical processors, process affinity where available, and Linux cgroup CPU quota |
| Memory | Current reclaimable/available physical memory and Linux cgroup memory headroom |
| Memory budget | 1.5 GiB per compiler job after reserving the larger of 1 GiB or 10% of currently available memory |

The memory model is a conservative C++/Qt build heuristic, not a hardware
benchmark. Because it reads current headroom at invocation time, the same host
may select fewer jobs while other applications consume memory and more jobs
after that pressure is gone.

Inspect the decision without starting a build:

```bash
python3 tools/dev/fluent_qt_build.py --print-jobs
python3 tools/dev/fluent_qt_build.py --dry-run --preset vcpkg-osx
```

## Overrides

Use an explicit override after measuring a host or when reproducing a fixed CI
lane:

```bash
python3 tools/dev/fluent_qt_build.py \
  --jobs 12 \
  --preset vcpkg-osx
```

`FLUENTQT_BUILD_JOBS` is the project-specific persistent override.
`CMAKE_BUILD_PARALLEL_LEVEL` is also honored when the project variable is not
set. A command-line `--jobs N` has highest priority; `--jobs auto` bypasses both
environment variables for one invocation.

Direct `cmake --build ... --parallel` remains supported. With no numeric value,
CMake delegates the decision to the native build tool, which does not apply
this repository's memory check. Makefile generators then allow an unlimited
number of ready build tasks. The main matrix builds in `ci-cpp.yml`,
`desktop-release-candidate.yml`, and `package-win10-compat-test.yml` use the
adaptive wrapper: an explicit `build_parallel` becomes `--jobs N`, and an empty
value uses `--jobs auto`. This applies to Windows as well as macOS and Linux.
The automatic result depends on the CPU and memory available at invocation;
it need not equal the runner's processor count.

<!-- docs-nav:bottom:start -->
---
[Contents](../SUMMARY.md) · [Development index](README.md) · [Testing Workflow →](testing-workflow.md)
<!-- docs-nav:bottom:end -->
