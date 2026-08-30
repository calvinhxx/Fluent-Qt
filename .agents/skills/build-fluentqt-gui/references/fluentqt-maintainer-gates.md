# FluentQt maintainer gates

Use this dispatcher only when the target is FluentQt itself. It connects the
end-to-end GUI workflow to repository-owned gates without copying those gates
into downstream applications or duplicating their detailed guides here.

## Confirm maintainer mode

Confirm the target root contains `src/components/`, `app/`, `tools/quality/`,
and `.agents/skills/build-fluentqt-gui/`. Otherwise use `create`, `integrate`,
or `improve` mode and the target project's own instructions.

Read the target root's `AGENTS.md` first. Select only rows that match the
changed files and behavior; read each named guide completely before editing.
A passing unrelated global suite never replaces the focused owner gate.

## Select gates from the change

| Change | Canonical repository guide | Required local gate or evidence |
| --- | --- | --- |
| C++ component or public API | `docs/development/component-api-conventions.md`, `docs/development/testing-workflow.md` | Focused component build/CTest; `validate_component_api.py --self-test` for installed headers, properties, accessors, signals, catalog mappings, or compatibility readers |
| Visible component or C++ Gallery sample | `docs/development/app-sample-optimization.md`, `docs/development/gallery-preview-workflow.md` | Real Gallery build, `test_gallery_content_pages`, Gallery boundary validator, compiled preview |
| Popup, collection, navigation, window, or other high-risk visual behavior | `docs/development/technical-debt-roadmap.md`, `docs/development/gui-verification-workflow.md` | Visual inventory self-test plus its named deterministic or human-review route |
| PySide6 Gallery or generated Python source | `bindings/pyside6/gallery/README.md`, `docs/development/gallery-preview-workflow.md` | Configured Gallery binding tests, generated snippet check, contract generator, Python acceptance |
| WebAssembly Gallery or shared Gallery code | `docs/development/webassembly-workflow.md` | `wasm` build and the applicable fast/full browser smoke |
| Skill, AI catalog, guidance, or reader docs | `docs/ai/README.md`, `docs/development/documentation-style.md` | AI catalog/asset gates, Skill package inspection, documentation navigation/validation |
| CI classifier, matrix, module, or final gate | `docs/development/testing-workflow.md` | Focused classifier/matrix/workflow unit tests and fail-closed boundary validator |

Use the adaptive wrapper and anchored labels for native focused work:

```bash
python3 tools/dev/fluent_qt_build.py \
  --preset <host-preset> --target <target>
ctest --preset <host-preset> \
  -L '^<owner-label>$' --output-on-failure
```

The repository-wide static selectors are:

```bash
python3 tools/quality/validate_component_api.py \
  --project-root . --self-test
python3 tools/quality/validate_visual_evidence_inventory.py \
  --project-root . --self-test
python3 .github/scripts/validate-gallery-boundary.py
python3 tools/ai/evaluate_ai_catalog.py --project-root .
python3 tools/ai/validate_ai_assets.py --project-root .
python3 tools/docs/generate_navigation.py --project-root . --check
python3 tools/docs/validate_documentation.py --project-root .
```

For a configured build with `FLUENT_QT_BUILD_PYSIDE6_GALLERY=ON`, select the
tests present for that runtime. Record a conditionally absent test instead of
claiming it passed:

```bash
ctest --test-dir <pyside-build> \
  -R '^test_pyside6_(gallery|gallery_python_snippet_catalog|gallery_contract_generator|gallery_acceptance)$' \
  --output-on-failure
```

For shared Gallery or browser-delivery changes, follow the WebAssembly guide;
use `full` for routes, settings, dialogs, menus, shell, or Pages packaging:

```bash
cmake --preset wasm
python3 tools/dev/fluent_qt_build.py \
  --preset wasm --target fluent_qt_gallery
python3 .github/scripts/run-wasm-browser-smoke.py \
  --root build/wasm --mode full --device-scale-factor 2
```

For CI orchestration changes, run:

```bash
python3 .github/scripts/test_classify_ci_changes.py
python3 .github/scripts/test_validate_ci_cpp_matrix.py
python3 .github/scripts/test_validate_ci_workflow_boundaries.py
python3 .github/scripts/validate-ci-workflow-boundaries.py
```

If classification cannot prove a change is documentation-only, keep the build
lanes enabled.

## Use repository GUI evidence

For a high-risk Gallery scenario, tailor the checked-in recipe and run the
final rebuilt binary through the repository verifier:

```bash
python3 tools/dev/fluent_qt_gui_verify.py run \
  --recipe build/gui-verification/recipes/my-change.json \
  --preset <host-preset>
```

Inspect the native-resolution captures, geometry, Inspector results,
`evidence.json`, `review.html`, and immutable review request. A capture without
an approved fingerprint-matching baseline is `human-required`, not a pass.
Never self-approve a baseline or relax a tolerance merely to clear a failure.

Keep the evidence scopes distinct:

- the repository verifier covers a named FluentQt component or Gallery scene;
- Skill contract v4 covers a complete consumer application's product workflow,
  composition, responsiveness, and independent review.

One may cite the other as lower-level evidence, but their schemas and approval
states are not interchangeable.

## Keep platform claims separate

- C++ Gallery evidence applies to its recorded native host, Qt, scale, font,
  theme, and binary.
- PySide6 Gallery evidence applies to the binding and Python presentation path.
- WebAssembly smoke applies to the browser runtime and packaged web assets.
- Native accessibility, IME, compositor, window management, drag-and-drop, and
  physical input require the corresponding real operating-system lane.

Offscreen rendering, injected Qt events, or browser smoke cannot become
cross-platform visual approval. Name every unverified Windows, Linux, macOS,
Python, or WebAssembly boundary explicitly.

## Maintainer acceptance gate

Before finishing, require:

- a recorded delivery mode and lite/full profile;
- a focused owner test for every changed public or visible surface;
- semantic alignment between Gallery UI and displayed source;
- deterministic or explicitly `human-required` evidence for high-risk visuals;
- runtime-specific, never inferred cross-platform claims;
- synchronized generated catalogs, packages, and documentation; and
- a clean `git diff --check` without unrelated user work.
