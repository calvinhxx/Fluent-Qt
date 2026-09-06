# Project Guidelines

## Start with the task

Use [README.md](README.md) for supported versions and entry points,
[docs/README.md](docs/README.md) for the reader tree, and
[docs/development/README.md](docs/development/README.md) to find the workflow
for the change. Read the relevant sections and owning code first; follow
dependencies as needed. Keep workflow details in their canonical guides.

| Task | Read |
|---|---|
| Local setup and builds | [Build workflow](docs/development/build-workflow.md); [Linux workflow](docs/development/linux-workflow.md) for Linux |
| C++ or test changes | [Testing workflow](docs/development/testing-workflow.md), including the local static gate; [test conventions](docs/development/qt-component-test-conventions.md) when authoring tests |
| Public API or baseline change | [API conventions](docs/development/component-api-conventions.md), [compatibility policy](docs/development/compatibility-policy.md), [API audit](docs/development/component-api-audit.md) |
| Runtime ownership or overlays | [Architecture](docs/architecture/README.md); [overlay behavior](docs/architecture/overlay-behavior.md) for popups, flyouts, dropdowns, and drawers |
| Visible component change | [Accessibility inventory](docs/development/accessibility-inventory.md), [visual review](docs/development/visual-review.md) |
| Cross-cutting maintenance or high-risk visual work | [Technical debt roadmap](docs/development/technical-debt-roadmap.md) and its [visual evidence inventory](docs/development/visual-evidence-inventory.json) |
| Gallery samples or card images | [Sample/source alignment](docs/development/app-sample-optimization.md), [preview workflow](docs/development/gallery-preview-workflow.md), or [card images](docs/development/gallery-control-images.md) |
| FluentQt application creation, integration, or GUI improvement | Canonical [build-fluentqt-gui Skill](.agents/skills/build-fluentqt-gui/SKILL.md); [AI tools index](docs/ai/README.md) for discovery and catalogs |
| Documentation or Skill maintenance | [Documentation style](docs/development/documentation-style.md), [AI tools index](docs/ai/README.md); a docs-only change does not require GUI design work |
| CI, packaging, or release | [CI workflow](docs/development/ci-workflow.md), [packaging](docs/development/packaging-workflow.md), [release governance](docs/development/release-governance.md) |
| Diagnostics | [Logging workflow](docs/development/logging-workflow.md) |

## Boundaries to preserve

- The project uses C++17 and Qt Widgets 5.15+ or 6.2+. `FluentQt` itself has no
  spdlog dependency. Library diagnostics use Qt logging with a `fluentqt.*`
  category; Gallery/tests use [support/logging/Log.h](support/logging/Log.h).
- [src/design/](src/design/) owns tokens; [src/compatibility/](src/compatibility/)
  owns Qt/platform helpers; [src/utils/](src/utils/) owns diagnostics.
  [src/components/](src/components/) is grouped by category with mirrored tests
  under [tests/components/](tests/components/). Shared infrastructure belongs in
  `foundation/`, composition surfaces in `layout/`, and model/view surfaces in
  `collections/`. Preserve caller-owned collection models.
- Use `compatibility/QtCompat.h` and `FluentEnterEvent` in new `enterEvent`
  overrides. Inside `namespace fluent::<category>`, inherit shared mixins as
  `public FluentElement, public QMLPlus`; qualify them with `fluent::` outside.
- [FluentQtInstallHeaders.cmake](cmake/FluentQtInstallHeaders.cmake) is the
  installed-header allowlist. Update it with public headers; keep private
  implementation headers out. Application examples use `<FluentQt/FluentQt.h>`.
- Fluent is the only visual contract. Branding uses Light/Dark semantic tokens
  and accent customization, not a second design-language enum or geometry
  branch. Compose existing Fluent components before duplicating widgets or
  paint/style code, including visible tests and demos.
- Non-trivial public APIs use concise English Doxygen `@brief` and a `zh_CN:`
  line. Follow [comment style](docs/development/comment-style.md) without
  mechanically rewriting untouched comments.
- Update the accessibility inventory for new or materially changed visible
  components. Keep Gallery previews and displayed source semantically aligned.
- Do not keep empty component directories. A category addition/removal updates
  the README overview, tests CMake, and this map.
- Register tests with `add_qt_test_module`. Shared QApplication, resources,
  style, and fonts belong in [QtGTestMain.cpp](tests/support/QtGTestMain.cpp);
  `SetUpTestSuite()` is for component-specific setup.

## Validate the changed surface

Use the current host preset and the adaptive build wrapper. For a configured
macOS arm64 checkout, replace `test_<name>` with the owning target:

```bash
python3 tools/dev/fluent_qt_build.py --preset vcpkg-osx --target test_<name>
ctest --preset vcpkg-osx -L '^test_<name>$' --output-on-failure
```

Public presets require CMake 3.25+ and `VCPKG_ROOT`; the library CMake baseline
remains 3.16. Keep machine paths in ignored `CMakeUserPresets.json`. Setup,
parallelism overrides, and platform presets belong to the build guide.

Run focused checks and required owner gates, then broaden only for changed
dependencies or unresolved risks. Before committing or pushing C++ changes,
run the read-only [local static gate](docs/development/testing-workflow.md#local-static-gate).

VisualCheck uses `SKIP_VISUAL_TEST` for automated skips and otherwise shows the
window and blocks with `qApp->exec()`; use the shared helper for
`VISUAL_SNAPSHOT=1`. Do not substitute `QTest::qWait()`. Use `fluent::AnchorLayout`
for primary VisualCheck layouts. A skipped test, registered scenario, or
offscreen result is not native visual approval. Close maintenance phases only
with the roadmap's checked-in exit condition and evidence.

After adding, removing, or reordering reader-facing Markdown, update
`docs/navigation.json` and run:

```bash
python3 tools/docs/generate_navigation.py --project-root .
python3 tools/docs/validate_documentation.py --project-root .
```

Never hand-edit generated navigation or catalogs. Report what was verified
and any remaining platform or visual-review boundary separately.
