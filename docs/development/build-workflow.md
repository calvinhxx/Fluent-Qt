# Build Workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Build, tests, and diagnostics

[Contents](../SUMMARY.md) · [Development index](README.md) · [Testing Workflow →](testing-workflow.md)
<!-- docs-nav:top:end -->

Use the adaptive build wrapper for local repository builds. It selects a
bounded parallel job count from the resources available to the current process
instead of imposing one repository-wide value:

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
