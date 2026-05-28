## Why

The project currently vendors GoogleTest under `third_party/googletest` and wires it with `add_subdirectory`, which makes a test-only dependency part of the source tree and couples the build to the vendored copy. Moving GoogleTest to vcpkg manifest mode makes the dependency explicit, easier to update, and cleaner for long-term test infrastructure work.

## What Changes

- Add vcpkg manifest files that declare GoogleTest as a test/build dependency while leaving Qt discovery in the existing local Qt toolchain.
- Add CMake presets that pin the vcpkg toolchain file and platform triplets for repeatable local and CI configuration.
- Replace `add_subdirectory(third_party/googletest)` with `find_package(GTest CONFIG REQUIRED)`.
- Keep existing `add_qt_test_module(...)` test target ergonomics while linking against vcpkg-provided `GTest::gtest` / `GTest::gtest_main` as appropriate during the migration.
- Remove the vendored `third_party/googletest` source tree after all test targets build and run through the vcpkg package.
- Add a Qt-aware GoogleTest entry point so `QApplication`, resources, fonts, style, and offscreen automation defaults are initialized centrally rather than repeated in every fixture.
- Update tests incrementally to use the centralized Qt test initialization without changing test behavior or VisualCheck semantics.

## Capabilities

### New Capabilities
- `test-infrastructure`: Covers test dependency management, vcpkg-based GoogleTest integration, CMake presets, Qt-aware GoogleTest entry point, and centralized Qt test initialization behavior.

### Modified Capabilities
无。

## Impact

- Build configuration: root `CMakeLists.txt`, `tests/CMakeLists.txt`, and CMake preset/manifest files.
- Dependency layout: `third_party/googletest` is removed from the repository after CMake consumes GoogleTest through vcpkg.
- Test runtime: test binaries continue to run through CTest and direct binary execution, but Qt application setup moves into shared test support code.
- Developer workflow: contributors configure with the provided CMake presets or an equivalent `CMAKE_TOOLCHAIN_FILE` pointing at vcpkg.
- CI workflow: any CI/build documentation must install or cache vcpkg packages before configuring tests.
