## 1. vcpkg Manifest And Presets

- [x] 1.1 Add `vcpkg.json` declaring the `gtest` dependency and an explicit baseline strategy.
- [x] 1.2 Add `CMakePresets.json` presets that use `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` without hard-coded developer paths.
- [x] 1.3 Define platform-appropriate vcpkg triplets in presets, starting with the current development platform and adding Windows/Linux presets if practical.
- [x] 1.4 Keep Qt discovery outside vcpkg by preserving the existing `find_package(QT ...)` and `find_package(Qt${QT_VERSION_MAJOR} ...)` flow.
- [x] 1.5 Document the expected configure command or preset usage for developers who need to install/point at vcpkg.

## 2. CMake GoogleTest Migration

- [x] 2.1 Replace root-level `add_subdirectory(third_party/googletest)` usage with `find_package(GTest CONFIG REQUIRED)` inside the `BUILD_TESTING` path.
- [x] 2.2 Remove vendored-GoogleTest-only configuration such as `gtest_force_shared_crt` when it is no longer needed.
- [x] 2.3 Update `tests/CMakeLists.txt` so `add_qt_test_module(...)` links against vcpkg-provided GoogleTest targets.
- [x] 2.4 Preserve `gtest_discover_tests(...)` and its `SKIP_VISUAL_TEST=1` environment property.
- [x] 2.5 Configure the project from a clean build directory using a vcpkg preset and confirm CMake resolves GoogleTest through vcpkg.

## 3. Remove Vendored GoogleTest

- [x] 3.1 Confirm all test targets configure and build without referencing `third_party/googletest`.
- [x] 3.2 Remove the `third_party/googletest` source tree from the repository.
- [x] 3.3 Search CMake, documentation, scripts, and tests for stale `third_party/googletest`, `gtest_force_shared_crt`, or vendored GoogleTest references.
- [x] 3.4 Verify a clean configure still works after the vendored source tree has been removed.

## 4. Shared Qt GoogleTest Entry Point

- [x] 4.1 Add shared test support files under `tests/support/`, including `QtGTestMain.cpp` and a small reusable Qt test environment helper.
- [x] 4.2 Implement `QtGTestMain.cpp` so it initializes GoogleTest, sets offscreen mode when `SKIP_VISUAL_TEST=1`, creates `QApplication`, sets `Fusion`, initializes resources, loads fonts, and runs all tests.
- [x] 4.3 Update `add_qt_test_module(...)` to compile the shared test main/support sources into each Qt widget test target and link `GTest::gtest` instead of `GTest::gtest_main`.
- [x] 4.4 Keep component-specific metatype registration and fixture window setup inside individual test files.
- [x] 4.5 Remove duplicated common setup from representative test fixtures after the shared main covers it.
- [x] 4.6 Continue following `.codex/skills/qt-ut-conventions/SKILL.md` for any touched Qt widget tests and VisualCheck cases.

## 5. Incremental Test Cleanup

- [x] 5.1 Update navigation tests to rely on the shared Qt test initialization while preserving component-specific setup in `TestNavigationView.cpp`.
- [x] 5.2 Update one small non-navigation test target to validate the shared initialization pattern outside the navigation category.
- [x] 5.3 If the pattern is stable, remove redundant common initialization from the remaining affected fixtures by category.
- [x] 5.4 Ensure direct binary execution still runs tests normally and leaves VisualCheck available when `SKIP_VISUAL_TEST` is not set.

## 6. Validation

- [x] 6.1 Build representative targets with the vcpkg preset, including `test_navigation_view`, one text field target, one basic input target, and one collections target.
- [x] 6.2 Run representative binaries with `SKIP_VISUAL_TEST=1` and confirm VisualCheck tests are skipped rather than blocking.
- [x] 6.3 Run `ctest` from the vcpkg build directory with automated VisualCheck skipping.
- [x] 6.4 Run `openspec validate migrate-gtest-to-vcpkg`.
- [x] 6.5 Run `git diff --check` on CMake, test support, test fixture, and OpenSpec files.
- [ ] 6.6 Optionally run one VisualCheck binary manually without `SKIP_VISUAL_TEST=1` to confirm direct interactive execution still works.
