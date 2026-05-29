# Project Guidelines

## Start Here

- Use [README.md](README.md) for the project overview, supported Qt versions, CMake presets, and local IDE expectations.
- Use [docs/development/README.md](docs/development/README.md) as the index for reusable workflow rules; link to those docs instead of copying them into new agent files.
- Use [docs/architecture/README.md](docs/architecture/README.md) for architecture contracts, especially [docs/architecture/overlay-behavior.md](docs/architecture/overlay-behavior.md) when touching popup, flyout, dropdown, drawer, or other same-window overlay behavior.
- OpenSpec workflow files live under [openspec/](openspec/) and project skills live under [.github/skills/](.github/skills/). For OpenSpec-driven work, follow the matching proposal/apply/archive skill before editing implementation code.

## Build and Test

- The project is C++17 with Qt 5.15+ or Qt 6.2+, vcpkg manifest dependencies, GTest, and spdlog.
- Public CMake presets require `VCPKG_ROOT` and intentionally do not hard-code Qt, Ninja, Visual Studio, or Xcode paths. Put machine-specific paths in ignored `CMakeUserPresets.json` files.
- Common macOS arm64 flow:

```bash
cmake --preset vcpkg-osx
cmake --build --preset vcpkg-osx
ctest --preset vcpkg-osx --output-on-failure
```

- Build focused test targets with `cmake --build --preset vcpkg-osx --target test_<name>`.
- Prefer anchored CTest label filters, for example `ctest --preset vcpkg-osx -L '^test_<name>$' --output-on-failure`; see [docs/development/testing-workflow.md](docs/development/testing-workflow.md).
- VisualCheck tests are interactive by design. Automated CTest runs set `SKIP_VISUAL_TEST=1`; run binaries directly with `--gtest_filter="*VisualCheck*"` for manual review or `VISUAL_SNAPSHOT=1` for snapshots.

## Architecture Map

- [src/design/](src/design/) contains Fluent design tokens for color, spacing, typography, radius, material, elevation, animation, and breakpoints.
- [src/compatibility/](src/compatibility/) contains Qt and platform compatibility helpers. Use `compatibility/QtCompat.h` and `FluentEnterEvent` instead of direct `QEnterEvent` in new `enterEvent` overrides.
- [src/utils/](src/utils/) contains shared logging and debug overlay helpers. Prefer `LOG_TRACE`, `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, and `LOG_ERROR` from [src/utils/Log.h](src/utils/Log.h) over direct `spdlog` calls in components.
- [src/components/foundation/](src/components/foundation/) contains shared component infrastructure such as `fluent::FluentElement`, `fluent::QMLPlus`, `fluent::AnchorLayout`, and `fluent::overlay` contracts.
- [src/components/](src/components/) is grouped by component category; component tests mirror those categories under [tests/components/](tests/components/).

## Component Conventions

- Follow [docs/development/component-api-conventions.md](docs/development/component-api-conventions.md) and [docs/development/component-api-audit.md](docs/development/component-api-audit.md) when adding or changing public component APIs.
- Public non-trivial APIs under `src/` use concise Doxygen comments with English `@brief` plus a `zh_CN:` line. Do not mechanically rewrite untouched comments; see [docs/development/comment-style.md](docs/development/comment-style.md).
- For visible UI in tests and demos, prefer existing project Fluent components over raw Qt widgets when an equivalent exists.
- Within `namespace fluent::<category>`, spell shared mixin inheritance as `public FluentElement, public QMLPlus`; outside component namespaces, qualify them as `fluent::FluentElement` and `fluent::QMLPlus`.
- New visible components should compose existing Fluent components before adding raw Qt widgets or duplicating paint/style code.
- Do not keep empty placeholder directories under `src/components/`. When adding or removing a component directory, update the relevant OpenSpec specs, README overview, tests CMake, and this agent instruction file.

## Testing Conventions

- Register component tests with `add_qt_test_module(test_<name> Test<Name>.cpp [extra sources...])`.
- Individual test files must not duplicate shared Qt setup such as `QApplication`, `Q_INIT_RESOURCE(resources)`, app style, or Segoe font loading; that belongs to [tests/support/QtGTestMain.cpp](tests/support/QtGTestMain.cpp).
- Keep `SetUpTestSuite()` for component-specific metatype registration, global test data, or initialization that cannot be shared.
- VisualCheck tests must guard on `SKIP_VISUAL_TEST`, show the window, and block with `qApp->exec()` until the window closes. Do not replace that contract with `QTest::qWait()`.
- Use `fluent::AnchorLayout` for primary VisualCheck layouts and control panels; detailed rules are in [docs/development/qt-component-test-conventions.md](docs/development/qt-component-test-conventions.md) and [docs/development/visual-review.md](docs/development/visual-review.md).

## Diagnostics and Review

- Use the logging workflow in [docs/development/logging-workflow.md](docs/development/logging-workflow.md). Useful anchors include layout recalculation, state transitions, input handling, popup/animation lifecycle, and test setup failures.
- Runtime logging can be controlled with `SPDLOG_LEVEL=debug|info|warn|error|critical|off` and `SPDLOG_FILE=/path/to/log`.
- For visual or interaction changes, review token color usage, radius, 4 px spacing, typography, Light/Dark behavior, states, text fit, and animation smoothness with [docs/development/visual-review.md](docs/development/visual-review.md).
- If an OpenSpec change is active, validate it with the relevant `openspec validate ... --strict`; telemetry errors from `edge.openspec.dev` are noise when the command exits successfully.
