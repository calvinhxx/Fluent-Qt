# Qt Component Test Conventions

Apply these rules whenever adding or editing components under `src/components/` or
tests under `tests/components/`, especially `VisualCheck` cases.

For test execution, CTest labels, VisualCheck run modes, and component-directory
synchronization, use [Testing Workflow](testing-workflow.md).

## Inheritance Style

- When tests or demos add repository components under `src/components/`, spell mixin
  inheritance as `public FluentElement, public QMLPlus` inside
  `namespace fluent::<category>`.
- In test helper classes outside component namespaces, qualify foundation mixins
  as `public fluent::FluentElement` and `public fluent::QMLPlus`.

## Component Choice

- Prefer project Fluent components when an equivalent exists.
- Use `fluent::basicinput::Button` instead of `QPushButton`.
- Use `fluent::basicinput::CheckBox` or `ToggleSwitch` instead of `QCheckBox`
  when the control is part of a visible test UI.
- Use project components such as `Label`, `ScrollBar`, `ListView`, `GridView`,
  etc. when the test is demonstrating app UI rather than directly testing a Qt
  primitive.
- Keep native Qt classes for models, signals, low-level event simulation, or
  cases where the test explicitly targets Qt behavior and no custom wrapper
  exists.

## New Component Design

- New visible components should prefer composition of existing custom Fluent
  components before adding raw Qt widgets or duplicating paint/style code.
- Reuse existing custom components for common roles such as text (`Label`),
  commands (`Button` and related basic input controls), selection, scrolling,
  collection views, and status surfaces when their behavior fits.
- Use custom painting for the component-specific visual surface only; keep
  reusable subparts as child components when that gives shared styling, theme
  behavior, accessibility, and interaction states.
- Use native Qt widgets directly only for low-level infrastructure, private
  implementation details, missing project equivalents, or cases where a custom
  component would make the behavior less correct or unnecessarily heavy.

## VisualCheck Layout

- Use `fluent::AnchorLayout` for VisualCheck window layout and control panels.
- Anchor the main component to the window with explicit margins and anchor
  toolbar/control rows above it.
- Avoid `QVBoxLayout`/`QHBoxLayout` for the primary VisualCheck arrangement.
  Small internal helper widgets may still use Qt layouts when that keeps the
  test simple and does not affect the visual layout rule.
- Keep VisualCheck windows resizable unless the component requires a fixed
  viewport.
- When touching an older VisualCheck, migrate visible commands/text/selection
  widgets to project Fluent components and `fluent::AnchorLayout` incrementally. Do not
  perform broad unrelated VisualCheck rewrites just to normalize style.

## Test Structure

- Register component tests with `add_qt_test_module(test_<name> Test<Name>.cpp
  [extra sources...])`.
- Test binaries share Qt setup through `tests/support/QtGTestMain.cpp`; do not
  create `QApplication`, call `Q_INIT_RESOURCE(resources)`, set the app style,
  or load bundled fonts in individual test files.
- Keep `SetUpTestSuite()` only for component-specific metatype registration,
  global test data, or initialization that cannot be shared.
- VisualCheck tests must guard on `SKIP_VISUAL_TEST`, show the test window, and
  block with `qApp->exec()` until the window is closed.
- Do not replace VisualCheck event-loop blocking with `QTest::qWait()`.

## Validation

- Preserve the `SKIP_VISUAL_TEST` guard for interactive VisualCheck tests.
- Ensure non-interactive runs can use offscreen mode when `SKIP_VISUAL_TEST=1`.
- Prefer terminal-native validation commands from
  [Testing Workflow](testing-workflow.md) instead of IDE-specific test launchers.
