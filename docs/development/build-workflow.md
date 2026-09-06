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
