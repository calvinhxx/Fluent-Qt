## Why

CTest filtering is currently coarse: discovered Qt tests receive `qt`, `unit`, category, target, and component labels, but VisualCheck, platform-specific, slow, animation-heavy, and interactive tests are only identifiable by names or conventions. This makes it harder to run exactly the right slice in CI, local debug loops, and manual visual review.

## What Changes

- Extend test discovery labeling so individual discovered GoogleTest cases can receive semantic labels such as `visual`, `interactive`, `platform_windows`, `platform_macos`, `slow`, and `animation`.
- Ensure VisualCheck tests get a real CTest `visual` label while keeping automated CTest runs protected by `SKIP_VISUAL_TEST=1`.
- Preserve the existing `add_qt_test_module(<target> <source> ...)` registration flow and current base labels: `qt`, `unit`, source-directory labels, target name, and component name.
- Document and validate anchored label-filter workflows for the new labels.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `test-infrastructure`: refine CTest discovery behavior so per-test semantic labels are available in addition to the existing module/category/component labels.

## Impact

- Affected CMake surface: `tests/CMakeLists.txt`, especially `add_qt_test_module` and generated `TEST_INCLUDE_FILES` label scripts.
- Affected tests: representative VisualCheck, animation, platform-specific, slow, and interactive test cases may need naming or metadata conventions that CMake can classify.
- Affected documentation: README and test workflow instructions should describe new labels and anchored `ctest -L` examples.
- No production component API changes are expected.