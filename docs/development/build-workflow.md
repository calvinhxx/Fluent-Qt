# Build Workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Build, tests, and diagnostics

[Contents](../SUMMARY.md) · [Development index](README.md) · [Testing Workflow →](testing-workflow.md)
<!-- docs-nav:top:end -->

Run commands from the repository root. Reuse an existing configured build
when its toolchain and options still match the task.

## First-use setup

The public [CMake presets](../../CMakePresets.json) require CMake 3.25+; the
library's direct CMake build still supports 3.16+. Install a C++17 toolchain,
Python 3, vcpkg, and a supported desktop Qt Widgets kit (5.15+ or 6.2+). The
Gallery and test dependencies are selected by the preset's vcpkg features;
Qt is discovered from your installed kit.

Enabling `FLUENT_QT_BUILD_QML_BRIDGE` with Qt 6.2–6.5 also requires the matching
Qt Quick private development headers for its embedded keyboard-focus adapter.
Qt 6.6+, Qt 5, and builds without QmlBridge do not need this additional dependency;
see the [QmlBridge contract](../architecture/qml-bridge.md#build-and-consume).

Set `VCPKG_ROOT` to the vcpkg checkout containing
`scripts/buildsystems/vcpkg.cmake`. Select a preset for the actual host and
toolchain: `vcpkg-osx` for macOS arm64, `vcpkg-windows` for Windows x64, or
`vcpkg-linux` for Linux x64. Architecture variants are listed in
`CMakePresets.json`; use `cmake --list-presets` to see those available locally.

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
this repository's memory check. CI and packaging workflows may
continue to use explicit job counts because their runner resources and
reproducibility requirements are known.

<!-- docs-nav:bottom:start -->
---
[Contents](../SUMMARY.md) · [Development index](README.md) · [Testing Workflow →](testing-workflow.md)
<!-- docs-nav:bottom:end -->
