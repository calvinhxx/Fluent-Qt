## Context

Qt widget tests are registered through `add_qt_test_module(...)` in `tests/CMakeLists.txt`. The helper builds the target, calls `gtest_discover_tests`, injects `SKIP_VISUAL_TEST=1`, and writes a CMake include file that applies shared labels to all discovered test cases for that target.

Today those shared labels are useful for broad filtering (`qt`, `unit`, directory/category labels, target name, and component name), but they do not distinguish VisualCheck, manual/interactive, platform-specific, slow, or animation-heavy cases. VisualCheck tests are currently discoverable mainly by test name and are skipped during automation by environment guard rather than by a CTest label.

## Goals / Non-Goals

**Goals:**

- Keep `add_qt_test_module(<target> <source> ...)` as the normal test registration API.
- Preserve all existing base labels on discovered tests.
- Add semantic per-test labels for `visual`, `interactive`, `platform_windows`, `platform_macos`, `slow`, and `animation`.
- Make VisualCheck selectable with `ctest -L '^visual$'` while still skipped in automated CTest runs through `SKIP_VISUAL_TEST=1`.
- Document anchored label filters for common local and CI workflows.

**Non-Goals:**

- Rewriting component test CMake files to list every test case manually.
- Changing GoogleTest test names or forcing immediate renames across all tests unless needed for a representative label rule.
- Removing the existing `unit` label from VisualCheck tests in this change.
- Replacing VisualCheck with an automated screenshot comparison framework.

## Decisions

### Extend the generated label include script

Continue using `gtest_discover_tests` and `TEST_INCLUDE_FILES`, but change the generated label script from a single `set_tests_properties(${target_TESTS} ...)` call to per-test processing. For each discovered CTest test name, start from the existing base label set and append semantic labels when the name matches project conventions.

Alternatives considered:

- Passing all labels through `gtest_discover_tests(PROPERTIES LABELS ...)`: rejected because it can only apply one shared label set and cannot distinguish individual test cases.
- Requiring every component `CMakeLists.txt` to supply explicit per-test labels: rejected because it would make small component test files noisy and easy to forget.
- Parsing C++ source directly for label comments: rejected for the first pass because discovered test names are already available at CMake time and VisualCheck naming is established.

### Use conservative naming-derived semantic rules first

Derive labels from discovered test names using conservative token patterns:

- `VisualCheck` -> `visual` and `interactive`.
- `Interactive` -> `interactive`.
- `Animation`, `Animated`, or `Animations` -> `animation`.
- `Slow` -> `slow`.
- `Windows`, `Win32`, or `WinUIWindows` style platform tokens -> `platform_windows`.
- `MacOS`, `Darwin`, or `Cocoa` style platform tokens -> `platform_macos`.

Rules should treat separators such as `.`, `/`, `_`, and camel-case boundaries carefully enough to avoid accidental substring matches. If a future test cannot be named clearly, the helper can grow an explicit per-target override map later without changing the base workflow.

### Keep automation behavior independent from labels

The new `visual` and `interactive` labels are selection metadata; they do not replace the existing `SKIP_VISUAL_TEST=1` guard. Automated CTest discovery and runs must continue to set that environment value so VisualCheck tests skip even when someone runs all labels.

### Validate labels through CTest metadata

Implementation should verify both CMake discovery and label filtering. Useful checks include `ctest -N -V` or filtered dry runs/output checks for `^visual$`, `^interactive$`, `^animation$`, and a representative category/target label. Focused validation should confirm VisualCheck tests still skip under CTest and remain runnable directly from the test binary.

## Risks / Trade-offs

- [Risk] Name-derived labels can miss tests whose names do not follow conventions. -> Mitigation: document naming rules and add representative tests or metadata checks for each supported semantic label.
- [Risk] Regexes that are too broad can label unrelated tests. -> Mitigation: use token/camel-case-aware patterns and validate discovered CTest label output.
- [Risk] Platform labels may be hard to infer for tests that use compile-time guards without platform words in the name. -> Mitigation: start with explicit naming conventions and allow a future override mechanism if real cases require it.
- [Risk] `visual` tests still carry `unit` because base labels are preserved. -> Mitigation: document that semantic labels are additive in this change; exclusion filters can use `-LE '^visual$|^interactive$'` when needed.