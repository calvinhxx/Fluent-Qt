## Context

VisualCheck tests are currently ordinary GoogleTest cases that build a Qt window, guard on `SKIP_VISUAL_TEST`, and then call `qApp->exec()` for manual review. Automated CTest discovery injects `SKIP_VISUAL_TEST=1`, and the shared Qt test main in `tests/support/QtGTestMain.cpp` initializes `QApplication`, resources, fonts, logging, and the common test environment.

This change adds a second non-interactive path for VisualCheck: when a developer runs a test binary directly with `VISUAL_SNAPSHOT=1`, the VisualCheck case should create a stable window, capture a PNG, write it under the active CMake binary directory's `visual/` folder, and return. Pixel comparison and baseline approval are intentionally deferred.

## Goals / Non-Goals

**Goals:**
- Provide a shared VisualCheck snapshot helper so component tests do not each invent file naming, output paths, event flushing, or capture behavior.
- Keep `SKIP_VISUAL_TEST=1` CTest automation behavior unchanged.
- Make snapshot output repeatable by fixing window size, Fluent theme, and device scale for snapshot runs.
- Document how to run snapshot mode and where generated files are written.

**Non-Goals:**
- Do not add pixel diffing, golden baseline storage, CI failure policy, or image approval workflow.
- Do not require every existing VisualCheck test to be converted in one pass unless the implementation can safely do so mechanically.
- Do not remove the manual `qApp->exec()` VisualCheck path.
- Do not introduce external image comparison dependencies.

## Decisions

1. Snapshot mode is activated by `VISUAL_SNAPSHOT=1` during direct test binary execution.

   `SKIP_VISUAL_TEST=1` remains the automation guard used by CTest and continues to skip VisualCheck cases. If both variables are set, skip behavior wins so existing automation cannot unexpectedly create windows or files.

   Alternative considered: add separate CTest labels or a new CTest preset for snapshot generation first. That can come later, but the first pass should be usable from the same binaries developers already run for VisualCheck.

2. Shared test support owns snapshot environment and capture utilities.

   Extend `tests/support/QtTestEnvironment.*` with helpers such as snapshot-mode detection, build-relative output path creation, fixed window preparation, event flushing, and widget grabbing. The support code should receive the active CMake binary directory from CMake, for example via a compile definition added in `add_qt_test_module`, and write to `<binary-dir>/visual/`.

   Alternative considered: implement capture logic inside each VisualCheck test. That would be faster for one component but would duplicate fragile Qt event and path handling across the suite.

3. Snapshot captures use deterministic defaults with optional explicit variants.

   The helper should set or verify a stable snapshot environment before `QApplication` is created where needed, including `QT_SCALE_FACTOR=1` for `VISUAL_SNAPSHOT=1` unless a caller has explicitly configured Qt scaling. Each snapshot should use a fixed size and a known Fluent theme, defaulting to Light unless the test requests a named variant such as Dark. The output filename should include the test target or suite and variant to avoid collisions.

   Alternative considered: capture the current manual VisualCheck window exactly as displayed. That preserves interactivity but does not meet the repeatability goal because local window size, theme toggles, and device pixel ratio can vary.

4. Snapshot success means a generated image exists, not that it matches a baseline.

   The first phase should assert only that the target directory is created, the PNG is saved successfully, and the file is non-empty. Baseline comparison, tolerance thresholds, and artifact review policy belong in a follow-up change after snapshot generation is reliable.

   Alternative considered: add pixel diff immediately. That would require settling baseline storage, platform variance, and CI behavior before developers can benefit from simple repeatable captures.

## Risks / Trade-offs

- [Risk] Qt device pixel ratio can still vary if a developer overrides Qt scaling environment variables. -> Mitigation: set deterministic defaults early for snapshot mode, document the override behavior, and include actual DPR diagnostics in failure messages or generated metadata if added later.
- [Risk] Existing VisualCheck tests have different setup shapes, so full conversion may be uneven. -> Mitigation: introduce a small helper API and convert representative tests first; keep manual VisualCheck behavior intact while remaining tests migrate.
- [Risk] Snapshot mode can hang if a test still enters `qApp->exec()`. -> Mitigation: require VisualCheck tests to branch on `VISUAL_SNAPSHOT=1` before the manual event loop and add at least one automated test or smoke target proving snapshot execution exits.
- [Risk] Generated images can accumulate in the build tree. -> Mitigation: write under a single `visual/` directory, overwrite deterministic filenames by default, and document that the directory is generated output.

## Migration Plan

1. Add snapshot detection, deterministic environment setup, output path, and capture helpers to shared test support.
2. Update `add_qt_test_module` or shared support definitions so helpers can resolve the active CMake binary directory.
3. Convert one or more representative VisualCheck tests to use the helper, then expand to commonly reviewed component groups as practical.
4. Update README and test workflow notes with `VISUAL_SNAPSHOT=1` usage examples and output path conventions.
5. Validate that normal CTest still skips VisualCheck and direct snapshot runs generate PNG files without blocking.

## Open Questions

- Should snapshot mode eventually generate both Light and Dark variants for every VisualCheck, or should each test opt into additional variants explicitly?
- Should a later baseline comparison change store approved images in the repository or keep baselines external to avoid large binary churn?