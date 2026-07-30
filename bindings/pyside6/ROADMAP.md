# PySide6 Compatibility Roadmap

[简体中文](ROADMAP.zh-CN.md)

This roadmap expands FluentQt's Python surface in risk-ordered milestones. It
does not treat Python source portability as proof that the native `_fluentqt`
extension is portable: every supported operating system, architecture, Qt
runtime, and CPython ABI requires its own build and validation.

## Compatibility contract

- The C++ library continues to support Qt 5.15+ and Qt 6.2+.
- The optional PySide6 target supports Qt/PySide6/Shiboken6 6.2+ and remains
  disabled by default.
- PySide6, Shiboken6 runtime, Shiboken6 generator, and the C++ Qt SDK must use
  the same version.
- All Python categories re-export one native `_fluentqt` module so theme,
  resources, Qt object identity, and other process-wide state are not
  duplicated.
- Importing `fluentqt` must not create a `QApplication`, initialize resources,
  or mutate the active theme.
- PySide2/Shiboken2 compatibility is not part of this roadmap.

## Status

| Milestone | State | Deliverable |
|---|---|---|
| M0 — Binding foundation | Implemented | Opt-in CMake target, Qt 6.2 generator path, version gates, one native module, startup API, tests |
| M1 — Core widget surface | Implemented | Basic Input, Text Fields, Window, theme/font API, ownership and `nativeEvent` contracts |
| M2 — Low-risk widget coverage | In progress | Add leaf QWidget controls with properties, signals, examples, manifest checks, and wheel smoke coverage |
| M3 — Hosted-widget ownership | Planned | Explicit Python-safe adapters and GC tests for containers that adopt or release child widgets |
| M4 — Models and navigation | Planned | Python models/delegates, virtual dispatch, selection, and navigation lifecycle |
| M5 — Overlays and native windows | Planned | Same-window overlay behavior and native desktop window/backdrop validation |
| M6 — Release-grade Python distribution | Planned | Supported wheel matrix, type stubs, API compatibility policy, signing, and publication |

## M0 — Binding foundation

- [x] `FLUENT_QT_BUILD_PYSIDE6_BINDINGS` is opt-in and Qt 6-only.
- [x] The generator path works across the 6.2+ Shiboken release line without
      requiring a recent Shiboken CMake helper.
- [x] Configure-time checks reject mismatched Qt, PySide6, Shiboken6 runtime,
      and Shiboken6 generator versions.
- [x] The package exposes explicit pre- and post-`QApplication` initialization.
- [x] Generated `Window.nativeEvent()` code is checked at the target-function
      level instead of by broad wrapper-file matching.
- [x] Linux, Windows, and macOS native CI lane definitions build and
      smoke-test wheels.

## M1 — Core widget surface

The implemented core surface contains:

- Basic Input: `Button`, `CheckBox`, `RadioButton`, `Slider`, `ToggleButton`,
  and `ToggleSwitch`.
- Text Fields: `Label`, `LineEdit`, `NumberBox`, and `PasswordBox`.
- Windowing: `Window`, backdrop enums, and backdrop value types.
- Foundation: Light/Dark theme selection, design-language presets, accent
  color, typography roles, font scaling, and build information.

The merge gates for this milestone include Python subclass dispatch, Qt
properties and signals, `Window` child-parent ownership, a safe two-argument
`nativeEvent()` contract, API-manifest checks, and clean-environment wheel
smoke tests.

## M2 — Low-risk widget coverage

Current slice:

- [x] Add `ProgressBar` and `ProgressRing` through the `fluentqt.status_info`
      category.
- [x] Cover range/value properties, component enums, signals, category
      re-exports, API manifest, example usage, and installed-wheel smoke.
- [x] Confirm the slice on the native Linux and Windows Qt 6.2.4 CI lanes.
- [x] Add `RepeatButton`, `HyperlinkButton`, and `Divider` with property,
      signal, category-export, manifest, wheel-smoke, and runnable acceptance
      coverage.
- [x] Confirm the second leaf-widget slice on the native Linux and Windows
      Qt 6.2.4 CI lanes.
- [x] Add `InfoBadge` and the built-in `Shimmer` templates with properties,
      signals, category exports, manifest checks, wheel smoke, and deterministic
      acceptance coverage.
- [x] Keep `ShimmerPainter::Element` collections private until a stable Python
      value-type contract is designed.
- [x] Confirm the third slice locally with macOS Qt/PySide6 6.9.3, including
      native component tests and a clean-wheel runtime check.
- [ ] Confirm the third slice on the native Linux and Windows Qt 6.2.4 CI
      lanes.

Subsequent slices should be selected by API audit. A component belongs in this
milestone only when it:

- is a leaf `QWidget` with no runtime ownership mode;
- does not expose a model/delegate contract;
- does not create a popup or same-window overlay;
- does not require platform-native window behavior; and
- can retain its existing C++ semantics without a Python-only façade.

Each slice must add the generated wrapper inputs, a category re-export,
`api-manifest.json` coverage, property/signal tests, installed-wheel smoke
coverage, and a runnable example when the control has visible behavior.

## M3 — Hosted-widget ownership

Candidate areas include `ScrollView`, `StackView`, `DrawerView`, `TabView`, and
other APIs that accept hosted `QWidget` instances.

Before exposing each API:

- replace runtime-dependent ownership arguments with explicit Python methods
  when static Shiboken ownership rules cannot describe the contract;
- test owned, borrowed, reparented, replaced, taken, and `None` transitions;
- repeat create/adopt/release/delete/`gc.collect()` sequences to detect double
  deletion, premature destruction, and invalid wrappers; and
- preserve Python subclasses while C++ owns the object.

## M4 — Models and navigation

This milestone covers collection and navigation components only after their
Python model boundary is designed. Validation must include:

- `QAbstractItemModel` and delegate lifetime;
- Python virtual overrides and `super()` dispatch;
- selection, reset, row insertion/removal, and persistent-index behavior;
- model replacement and destruction from both Python and C++; and
- keyboard, focus, RTL, and accessibility-relevant navigation behavior.

## M5 — Overlays and native windows

Popup, Flyout, ContentDialog, TeachingTip, dropdown, and other overlay
components require tests for scrim ordering, outside press, Escape, focus
return, top-level resize, and close-policy semantics.

Window, TitleBar, Mica, Acrylic, vibrancy, compositor blur, drag, and resize
must be tested on native desktops. Offscreen rendering is useful for ordinary
widgets but is not an acceptance test for platform window integration.

## M6 — Release-grade Python distribution

- Define the supported CPython/platform/architecture wheel matrix.
- Build each wheel on its native target or a documented equivalent toolchain.
- Generate `.pyi` type stubs and compare public API changes in CI.
- Add dependency, license, repair/audit, clean-install, and import checks.
- Establish versioning and deprecation rules for the Python API.
- Sign and publish wheels only after every required matrix lane passes.

The initial release matrix should stay intentionally smaller than the C++
package matrix. Additional Python versions and ARM64/x64 targets should be
added only when they have native build and runtime coverage.

## Definition of done

A milestone is complete only when:

1. its public API is recorded and documented;
2. generator output compiles with the declared minimum toolchain;
3. properties, signals, enums, Python subclassing, and ownership are tested as
   applicable;
4. a wheel installs and runs in a clean virtual environment;
5. the process loads one matching Qt/PySide6/Shiboken6 runtime set; and
6. required native Linux, Windows, and macOS CI lanes pass.

Local macOS success is useful development evidence, but it does not replace
native Linux or Windows confirmation for a C++ Python extension.

## What “Python compatibility complete” means

The project separates completion into three levels so that a successful import
is not mistaken for complete support:

1. **Core usable**: M0 and M1 are complete. The declared core widgets can be
   constructed, signal-connected, property-driven, subclassed, and run from a
   wheel. The project has reached this level.
2. **Feature complete**: M2 through M5 are complete. Every planned leaf widget,
   hosted widget, model/navigation surface, overlay, and native-window contract
   has a Python API plus applicable lifecycle and interaction tests. Any
   unbound public C++ component has an explicit documented reason.
3. **Release complete**: M6 is complete. The supported CPython, OS, and
   architecture matrix, type stubs, API compatibility/deprecation policy, and
   clean-environment wheels are published.

FluentQt calls Python support complete and release-ready only at the third
level. This excludes PySide2 and Qt 5 Python bindings and does not require
rewriting C++ painting in Python; Python uses the same native FluentQt widgets.

## How to validate the result

- **Automated contracts**: run `ctest --test-dir build/pyside6 -L '^pyside$'
  --output-on-failure` for properties, signals, subclassing, ownership,
  generated code, and the acceptance window.
- **Interactive review**: run `examples/compatibility_showcase.py`; switch
  Light/Dark, Fluent/Material/macOS, and accent colors, drag the Slider, hold
  RepeatButton, and inspect text, dividers, progress controls, and signal
  feedback.
- **Review artifact**: pass `--snapshot <png>` to the showcase. This mode can
  also run with `QT_QPA_PLATFORM=offscreen`.
- **Installation proof**: install the wheel into a fresh virtual environment
  and run `tests/test_wheel_smoke.py` to prove the process is not borrowing the
  source tree or loading a second Qt.
- **Native windows**: validate Window, TitleBar, and backdrop behavior on a
  real desktop. Offscreen snapshots cannot prove drag, resize, Mica, vibrancy,
  or compositor blur.

## Next delivery sequence

1. Pass the third M2 slice through the native Linux and Windows Qt 6.2.4 CI
   lanes.
2. Audit `ScrollView` as the first M3 hosted-widget candidate, then define and
   test its explicit Python ownership adapter before exposing other containers.
