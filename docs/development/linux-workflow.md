# Linux Workflow

Use this workflow for desktop Linux development, CI, testing, and packaging.
WSL2 is one optional local host for that workflow; product code must not depend
on WSL paths, environment variables, or Windows-installed resources.

## Portability Target

- Runtime target: general glibc-based x64 and ARM64 desktop Linux with Qt's X11
  (`xcb`) or Wayland platform plugin and a standards-compliant window
  manager/compositor.
- CI/reference distribution: Ubuntu 22.04 x64 and ARM64. Other distributions
  are expected to build from source with equivalent Qt, compiler, OpenGL,
  fontconfig, and XKB dependencies.
- Binary package target: Debian/Ubuntu-compatible x64 and ARM64 systems through
  per-architecture DEBs; other distributions use the CMake install/export
  interface.
- Qt 6 baseline: distro packages from Ubuntu 22.04, currently Qt 6.2.x, on x64
  and ARM64.
- Qt 5 baseline: official Qt 5.15.2 `gcc_64` on x64. The ARM64 contract starts
  at Qt 6.2 because the official Qt 5 desktop kit is x64-only.
- CMake presets require CMake 3.25 or newer.
- Public presets require `VCPKG_ROOT` and intentionally do not hard-code Qt
  paths. Put machine-specific Qt paths in `CMakeUserPresets.json` or pass them
  on the configure command line.

## Optional WSL2 Layout

When WSL2 is used as the Linux development host, prefer keeping the source
checkout and build tree inside its ext4 filesystem, for example
`/home/<user>/ws/Fluent-QT`.

Building from `/mnt/c` or `/mnt/d` works, but it is much slower because CMake,
AUTOMOC, the compiler, and CTest perform many small file and metadata
operations across the Windows/WSL filesystem boundary.

Windows tools can still open the WSL checkout through:

```text
\\wsl$\Ubuntu-22.04\home\<user>\ws\Fluent-QT
```

This layout guidance affects development performance only. Runtime behavior and
build configuration are the same as on a native Linux installation.

## Optional Apple Silicon Lima Layout

On an ARM64 Mac, Lima provides a lightweight native ARM64 Linux VM with a
WSL2-like command-line workflow. Create an Ubuntu 22.04 instance and confirm the
guest architecture before installing the reference packages:

```bash
limactl start --name=fluent-linux-arm --arch=aarch64 --cpus=6 --memory=8 --disk=80 template:ubuntu-22.04
limactl shell fluent-linux-arm
uname -m  # aarch64
```

Prefer cloning the repository and keeping the build tree in the guest's own
Linux filesystem. The default macOS home mount is convenient for file exchange,
but vcpkg, AUTOMOC, and CTest perform better without crossing the shared-filesystem
boundary. Lima validates native ARM64 build and headless runtime behavior; use a
full X11 or Wayland desktop VM for final visual review.

## Ubuntu Reference Packages

Use the same dependency installer as CI so the local and hosted environments
cannot silently drift. `apt` installs Ubuntu's Qt 6 development kit in addition
to the common desktop dependencies; `aqt` leaves Qt installation to the
official Qt installer and is used for the Qt 5.15.2 route:

```bash
# Ubuntu Qt 6
bash .github/scripts/install-linux-dependencies.sh apt

# Official Qt 5.15.2 or another externally installed Qt kit
bash .github/scripts/install-linux-dependencies.sh aqt
```

If the Ubuntu `cmake` package is older than 3.25, install a newer user-local
CMake or use another supported CMake distribution before running presets.

## Qt 6 Build

x64:

```bash
export VCPKG_ROOT=/home/<user>/vcpkg
cmake --preset vcpkg-linux -DFLUENT_QT_BUILD_GALLERY=OFF -DFLUENT_QT_BUILD_TESTS=ON -DVCPKG_MANIFEST_FEATURES=tests
cmake --build --preset vcpkg-linux --target fluent_qt_ci_fast_tests --parallel
ctest --preset vcpkg-linux -L '^ci_fast$' --output-on-failure
```

ARM64, on a native ARM64 Linux host such as Lima or an ARM64 VM:

```bash
export VCPKG_ROOT=/home/<user>/vcpkg
cmake --preset vcpkg-linux-arm64 -DFLUENT_QT_BUILD_GALLERY=OFF -DFLUENT_QT_BUILD_TESTS=ON -DVCPKG_MANIFEST_FEATURES=tests
cmake --build --preset vcpkg-linux-arm64 --target fluent_qt_ci_fast_tests --parallel
ctest --preset vcpkg-linux-arm64 -L '^ci_fast$' --output-on-failure
```

Run the broader Linux lane locally with:

```bash
cmake --build --preset vcpkg-linux --target fluent_qt_ci_full_tests --parallel
ctest --preset vcpkg-linux -L '^ci_full$' --output-on-failure
```

Use the same commands with `vcpkg-linux-arm64` on ARM64. The x64 and ARM64
presets use separate build directories and vcpkg triplets (`x64-linux` and
`arm64-linux`), so their caches cannot be mixed accidentally.

The `vcpkg-linux` and `vcpkg-linux-arm64` test presets exclude `local_desktop`
tests by default because they require a real desktop and window manager
behavior.

## Qt 5.15.2 Build

Install official Qt 5.15.2 `gcc_64` and include the official ICU archive. The
Ubuntu packages above are still required for the official Qt 5 kit. Then use a
separate build directory so the Qt 5 and Qt 6 CMake caches do not mix:

```bash
export VCPKG_ROOT=/home/<user>/vcpkg
export PATH=/home/<user>/.local/bin:$PATH
QT5_ROOT=/home/<user>/Qt/5.15.2/gcc_64

cmake -S . -B build/vcpkg-linux-qt5 \
  -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -DCMAKE_BUILD_TYPE=Debug \
  -DVCPKG_MANIFEST_FEATURES=tests \
  -DBUILD_TESTING=ON \
  -DFLUENT_QT_BUILD_TESTS=ON \
  -DFLUENT_QT_BUILD_GALLERY=OFF \
  -DCMAKE_PREFIX_PATH=$QT5_ROOT

cmake --build build/vcpkg-linux-qt5 --target fluent_qt_ci_full_tests --parallel
LD_LIBRARY_PATH=$QT5_ROOT/lib:$LD_LIBRARY_PATH \
  ctest --test-dir build/vcpkg-linux-qt5 -L '^ci_full$' -LE '^local_desktop$' --output-on-failure
```

Do not switch between Qt 5 and Qt 6 in the same CMake build directory. If a Qt
major version was discovered incorrectly, remove that build directory or create
a fresh one before reconfiguring.

## Manual Desktop Tests

Use the dedicated preset to list real-desktop tests. Build the specific test
target you want to review, or build `fluent_qt_all_tests`, before listing the
preset:

```bash
cmake --build --preset vcpkg-linux --target fluent_qt_all_tests --parallel
ctest --preset vcpkg-linux-local-desktop -N
```

On ARM64, use `vcpkg-linux-arm64` and
`vcpkg-linux-arm64-local-desktop` respectively.

Automated CTest runs inject `SKIP_VISUAL_TEST=1`, and the Linux desktop preset
is primarily for discovery and grouping. For real desktop behavior, run the
target binary directly without `SKIP_VISUAL_TEST` in an X11 or Wayland desktop
session. WSLg may be used as one optional local display host:

```bash
QT_QPA_PLATFORM=xcb ./build/vcpkg-linux/tests/components/collections/test_list_view \
  --gtest_filter='ListViewTest.SelectedIndicatorSettlesAfterDragReorder'
```

VisualCheck tests remain manual and use the same direct-binary pattern with
`--gtest_filter="*VisualCheck*"`.

## GitHub Actions

The default fast CI matrix runs Ubuntu 22.04 / Qt 6.2.x on x64. Native ARM64
validation runs in the scheduled/manual full matrix, which builds the curated
tests, Gallery, and a DEB smoke package for both architectures, plus the Ubuntu
22.04 x64 / official Qt 5.15.2 `gcc_64` compatibility lane.

The standard release matrix publishes an Ubuntu 22.04 x64 Qt 5 Gallery DEB plus
Ubuntu 22.04 x64 and ARM64 Qt 6 Gallery DEBs through `vcpkg-linux-deb` and
`vcpkg-linux-arm64-deb`. See [Packaging Workflow](packaging-workflow.md).
