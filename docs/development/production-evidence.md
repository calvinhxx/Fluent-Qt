# Production Evidence Baselines

> **Status:** Living collection of dated measurements

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Baselines and historical records

[← Component Contract Baseline](component-contract-baseline.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [FluentQt 1.7 delivery record →](release-1.7-roadmap.md)
<!-- docs-nav:top:end -->

This file records small, reproducible production-oriented baselines. These are
trend measurements, not performance promises or cross-platform guarantees.
Structural component contracts remain the regression gate; wall-clock and
process-memory values are supporting evidence.

## Release consumer startup baseline

Recorded on 2026-08-25 on a MacBook Air (Apple M2, 8 cores, 24 GB) running
macOS 15.7.3 arm64. FluentQt and a freshly generated C++ Workbench starter were
built in Release mode with Qt 6.9.3 and AppleClang 17. The starter consumed a
clean `cmake --install` prefix rather than the FluentQt source or build tree.
Its 8,748,032-byte executable has SHA-256
`ac12d79356525d2789eed03223e56813900fc6c3f393dbe9cdaa5d046911d02c`.

The command below creates the real application and window, calls `show()`, and
then exits through the starter's deterministic smoke path. The offscreen Qt
platform keeps the measurement repeatable and does not claim native first-paint
latency.

```bash
env QT_QPA_PLATFORM=offscreen \
  /usr/bin/time -l \
  build-release/r5_cpp_workbench --smoke-test
```

Five fresh processes each completed in 0.12 seconds. Median maximum resident
set size was 40,812,544 bytes (38.9 MiB); median macOS peak memory footprint was
12,142,016 bytes (11.6 MiB). The same generated project passed its application
test, offscreen UI smoke, and Inspector quality report (zero findings).

## Gallery cold-build baseline

Recorded on 2026-08-28 on the same MacBook Air (Apple M2, 8 logical cores,
24 GB) using macOS arm64, Qt 6.9.3, AppleClang 17, CMake 3.26.4, the
`vcpkg-osx` Debug preset, and Unix Makefiles. The independent
`build/benchmark-vcpkg-osx` directory did not reuse project object files;
vcpkg dependencies were restored from the existing binary cache.

Configuration completed in 5.07 seconds wall time. The cold build covered the
shared Gallery core, the desktop Gallery, and both Gallery shell/content test
executables:

```bash
/usr/bin/time -p python3 tools/dev/fluent_qt_build.py \
  build/benchmark-vcpkg-osx \
  --target FluentQtGalleryCore fluent_qt_gallery \
           test_gallery_shell_framework test_gallery_content_pages
```

The adaptive wrapper selected five jobs from an eight-core CPU limit and a
five-job memory limit (9.8 GiB currently available, 1.0 GiB reserved, 1.5 GiB
per job). The cold build completed in 77.20 seconds wall time, with 298.53
seconds user CPU and 33.67 seconds system CPU. An immediate no-change run
completed in 4.36 seconds wall time; the wrapper selected six jobs after
available memory rose to 10.0 GiB.

The object-library boundary is the structural result behind this measurement:
the 59 Gallery model/view/viewmodel translation units in the measured tree were
compiled once instead of once for each of the application and two test
executables, removing 118 duplicate compile actions for that source set. The
subsequent TreeView sample split leaves 60 smaller translation units compiled
once, rather than 180 compile actions under the former target layout. This is
one local point-in-time baseline, not a universal build-time improvement claim;
the checked build graph and tests remain the regression gates.

## DataGrid large-model baseline

Recorded on 2026-08-25 from commit
`36577cfaa711f6dbfc2c34d253bc6dce06211d42` using macOS 15.7.3 arm64,
Qt 6.9.3, CMake 3.26.4, and the `vcpkg-osx` Debug preset. The worktree had
uncommitted AI/onboarding changes, but no changes under `src/` or the DataGrid
test target.

The focused contracts use a deterministic 100,000-row by 20-column model and
verify that initial show, end-of-model scroll, resize, retained objects, and
editors remain bounded by the viewport rather than total model size.

Build:

```bash
cmake --build --preset vcpkg-osx --target test_data_grid --parallel
```

macOS measurement:

```bash
env SKIP_VISUAL_TEST=1 QT_QPA_PLATFORM=offscreen \
  /usr/bin/time -l \
  build/vcpkg-osx/tests/components/collections/test_data_grid \
  '--gtest_filter=DataGridTest.Contract_LargeModel*:DataGridTest.Contract_CellWidgetsAndEditorsDoNotScaleWithModelSize'
```

One fresh process passed all three contracts in 245 ms of test time and 330 ms
wall time. Maximum resident set size was 64,585,728 bytes (61.6 MiB). The
individual checks took 131 ms for initial show, 49 ms for scroll/resize, and
64 ms for object/editor scaling.

The same process repeated the three tests five times with per-iteration totals
of 250, 168, 157, 158, and 161 ms. The median was 161 ms and the process maximum
resident set size was 79,462,400 bytes (75.8 MiB). The first iteration includes
Qt and test warm-up; the repeated-process memory value is intentionally not
presented as control-only memory.

## WebAssembly first-load baseline

The public Gallery build record returned HTTPS 200 on 2026-08-25 and reported
FluentQt 1.7.1 at commit `2169e7586a32d0e22e952a104e2ff76f137b6aef`,
Qt 6.9.3, Emscripten 3.1.70, `wasm_singlethread`, and full validation. This
ties the measurement to a versioned deployment.

Five fresh headless Chromium 145 processes loaded the public Gallery at
1280x800 and DPR 2 with the browser cache disabled. Timing starts before the
top-level navigation and stops when the application publishes
`data-fluent-qt-loaded="true"`; it is not the longer full-route smoke. Runs took
4.269, 5.299, 5.847, 3.216, and 4.710 seconds, for a 4.710-second median on the
recording network. The WebAssembly response took a median 3.317 seconds and
transferred 11,836,857 bytes (11.29 MiB encoded, 25,754,182 bytes decoded).
The JavaScript response transferred 67,339 bytes.

These public-network values are a dated trend baseline, not a latency target.
The existing browser smoke remains the functional regression gate.

## Platform manual follow-up

macOS VoiceOver acceptance remains a platform-specific manual check for each
release build. It is not a performance baseline or an automated release gate.
Record the release build, macOS version, tested workflow, and dated result here
when the check is run.

<!-- docs-nav:bottom:start -->
---
[← Component Contract Baseline](component-contract-baseline.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [FluentQt 1.7 delivery record →](release-1.7-roadmap.md)
<!-- docs-nav:bottom:end -->
