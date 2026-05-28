## 1. Shared Snapshot Infrastructure

- [x] 1.1 Extend `tests/support/QtTestEnvironment` with `VISUAL_SNAPSHOT=1` detection while preserving `SKIP_VISUAL_TEST=1` as the higher-priority skip path.
- [x] 1.2 Configure deterministic snapshot environment before `QApplication` creation, including a stable Qt scale factor for snapshot runs when the caller has not explicitly configured scaling.
- [x] 1.3 Pass the active CMake binary directory to Qt test support from `add_qt_test_module` so snapshot files can be written under `<binary-dir>/visual/`.
- [x] 1.4 Add a shared snapshot capture helper that applies a fixed window size, sets a known Fluent theme, processes pending Qt events, grabs the window, creates output directories, saves PNG files, and verifies the result is non-empty.
- [x] 1.5 Use deterministic snapshot filenames that include test identity and optional variant names, overwriting the same path on repeated runs.

## 2. VisualCheck Adoption

- [x] 2.1 Update representative VisualCheck tests to branch to snapshot capture when `VISUAL_SNAPSHOT=1` is set, returning without entering `qApp->exec()`.
- [x] 2.2 Keep existing manual VisualCheck behavior unchanged when neither `SKIP_VISUAL_TEST` nor `VISUAL_SNAPSHOT` is set.
- [x] 2.3 Ensure VisualCheck tests still skip without generating files when `SKIP_VISUAL_TEST=1`, including when `VISUAL_SNAPSHOT=1` is also present.
- [x] 2.4 Migrate additional standard single-window VisualCheck tests to the shared helper where the change is mechanical and low risk.

## 3. Tests and Documentation

- [x] 3.1 Add automated coverage for snapshot-mode helper behavior, including output path construction, skip precedence, deterministic filename generation, and non-empty PNG save verification.
- [x] 3.2 Add at least one direct binary snapshot smoke validation command to the implementation notes or test workflow so developers can verify `VISUAL_SNAPSHOT=1` exits without hanging.
- [x] 3.3 Update README and/or project test workflow documentation with `VISUAL_SNAPSHOT=1` usage examples, output location, and the explicit absence of pixel diffing in this phase.

## 4. Validation

- [x] 4.1 Run focused build and test validation for the changed Qt test support and migrated VisualCheck test targets.
- [x] 4.2 Run a direct `VISUAL_SNAPSHOT=1` VisualCheck command and confirm PNG output appears under the build `visual/` directory.
- [x] 4.3 Run a CTest path with `SKIP_VISUAL_TEST=1` to confirm VisualCheck cases still skip and do not create snapshot output.
- [x] 4.4 Run `openspec validate add-visualcheck-snapshot-mode --strict`.