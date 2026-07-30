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
| M3 — Hosted-widget ownership | In progress | Explicit Python-safe adapters and GC tests for containers that adopt or release child widgets |
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
- [x] Confirm the third slice on the native Linux and Windows Qt 6.2.4 CI
      lanes.
- [x] Audit `Avatar`, `RatingControl`, and `ScrollBar` as a fourth leaf-widget
      slice with no model, overlay, platform-window, or hosted-child ownership
      boundary.
- [x] Add category exports, nested enums, property/signal coverage, manifest
      checks, installed-wheel smoke, and visible acceptance coverage for the
      fourth slice.
- [x] Confirm the fourth slice locally with macOS Qt/PySide6 6.9.3, including
      generated wrapper compilation, all PySide tests, a clean installed wheel,
      35 focused native component tests, and visual snapshot review.
- [x] Confirm the fourth leaf-widget slice on native Linux and Windows
      Qt 6.2.4 CI lanes, including generated-contract checks, the complete
      binding test suite, relocatable wheels, and clean-environment smoke in
      CI run `30553990409`.
- [x] Audit `PipsPager` separately and reproduce the generator leak where its
      animation-only `selectedVisualOffset` and `visibleWindowOffset`
      properties appeared as Python constructor keywords.
- [x] Move the two internal animations to `QVariantAnimation` callbacks so the
      C++ motion stays unchanged while the implementation offsets leave the Qt
      meta-object and generated Python API.
- [x] Add the `PipsPager` category export, enum, property/signal/navigation
      tests, manifest entry, generated privacy contract, wheel smoke, and
      visible acceptance coverage.
- [x] Confirm the fifth slice locally with macOS Qt/PySide6 6.9.3: 17 focused
      native tests, all 15 binding CTests, 29 verifier tests, a clean installed
      wheel, dependency-path checks, and visual snapshot review passed.
- [x] Confirm `PipsPager` on the native Linux and Windows Qt 6.2.4 binding
      lanes and the Qt 5.15 C++ compatibility lane. CI run `30598949551`
      passed generation and compilation, contract checks, the complete binding
      suite, relocatable wheels, clean-venv smoke, and the C++ regression gates.
- [x] Audit and bind `TextEdit` as the next leaf control. Its Python surface
      contains plain-text editing, visible-line layout metrics, styling
      properties/signals, scroll chaining, and a version-stable getter for its
      existing Qt-owned Fluent `ScrollBar`, without exposing the private
      `QTextEdit`.
- [x] Normalize the `verticalScrollBar()` API across generator versions:
      Shiboken 6.2 silently omits the cross-namespace pointer return, so the
      typesystem removes that unstable wrapper and the Python module supplies
      the same getter without changing parentage or ownership.
- [x] Confirm the `TextEdit` slice locally with macOS Qt/PySide6 6.9.3:
      all 19 focused native tests, generated wrapper compilation, all 16
      binding CTests, a newly created clean-environment wheel smoke,
      dependency-path checks, and visible snapshot review passed.
- [x] Confirm the `TextEdit` slice on native Linux and Windows Qt 6.2.4
      binding lanes and the Qt 5.15 C++ compatibility lane. CI run
      `30601608042` passed generation and compilation, generated-contract
      checks, all binding tests, relocatable wheels, clean-environment smoke,
      acceptance snapshots, and the C++ regression gates.
- [x] Audit and bind `CompoundButton` as the next M2 leaf control. It extends
      the already-bound `Button` with one secondary-text property and does not
      cross a model, overlay, platform-window, or hosted-widget boundary.
- [x] Cover all constructor overloads, native `Button` inheritance,
      `secondaryText` and its repeat-safe signal, accessibility-description
      synchronization, mixin isolation, category exports, the API manifest,
      installed-wheel smoke, and visible acceptance rendering.
- [x] Confirm the `CompoundButton` slice locally with macOS Qt/PySide6 6.9.3:
      all 5 focused native tests, generated-wrapper compilation, all 16 binding
      CTests, a newly created clean-environment wheel smoke, dependency-path
      checks, and visible snapshot review passed.
- [x] Confirm the `CompoundButton` slice on native Linux and Windows Qt 6.2.4
      binding lanes and the Qt 5.15 C++ compatibility lane. CI run
      `30603864933` passed generation and compilation, generated-contract
      checks, all binding tests, relocatable wheels, clean-environment smoke,
      acceptance snapshots, and the C++ regression gates.
- [x] Audit and bind `FontIcon` as the next M2 foundation leaf. It stays a
      native theme-aware `QWidget`, accepts stable upstream catalog keys, and
      does not cross an ownership, model, overlay, or platform boundary.
- [x] Cover the default and glyph constructors, all four properties and their
      repeat-safe signals, catalog-key optical-size rendering, mixin isolation,
      foundation/root exports, the API manifest, installed-wheel smoke, and a
      visible acceptance icon. The example deliberately uses
      `ic_fluent_settings_20_regular`; display text such as `Settings` is not
      a catalog key.
- [x] Confirm the `FontIcon` slice locally with macOS Qt/PySide6 6.9.3:
      all 3 focused native tests, generated-wrapper compilation, all 16
      binding CTests, a newly created clean-environment wheel smoke,
      dependency-path checks, and visual snapshot review passed.
- [x] Confirm the `FontIcon` slice on native Linux and Windows Qt 6.2.4
      binding lanes and the Qt 5.15 C++ compatibility lane. CI run
      `30604556341` passed generation and compilation, generated-contract
      checks, all binding tests, relocatable wheels, clean-environment smoke,
      acceptance snapshots, and the C++ regression gates.
- [x] Audit and bind `ColorPicker` as the next M2 leaf control. It remains a
      native `QWidget` with `QColor` and alpha-enabled value semantics and does
      not cross an ownership, model, overlay, or platform-window boundary.
- [x] Publish the intended `color`/`alphaEnabled` properties and signals while
      explicitly hiding the seven spectrum/channel implementation helpers.
      Cover root/category exports, the API manifest, repeat-safe signals,
      installed-wheel smoke, and a dedicated visible acceptance example.
- [x] Confirm the `ColorPicker` slice locally with macOS Qt/PySide6 6.9.3:
      the focused native contract, generated-wrapper compilation, all 17
      binding CTests, a newly created clean-environment wheel smoke,
      dependency-path checks, and visual snapshot review passed.
- [x] Confirm the `ColorPicker` slice on native Linux and Windows Qt 6.2.4
      binding lanes and the Qt 5.15 C++ compatibility lane. CI run
      `30605260392` passed generation and compilation, generated-contract
      checks, all binding tests, relocatable wheels, clean-environment smoke,
      acceptance snapshots, the C++ regression gates, and the final CI Gate.
- [x] Audit and bind `CalendarView` as the next M2 leaf control. It exchanges
      only Qt date, locale, enum, and geometry values and does not cross an
      ownership, model, overlay, or platform-window boundary.
- [x] Cover date ranges and clamping, repeat-safe property signals, nested
      content-level enums, date hit testing, mixin isolation, the new
      `fluentqt.date_time` category, root exports, the API manifest,
      installed-wheel smoke, and a dedicated visible acceptance example.
- [x] Confirm the `CalendarView` slice locally with macOS Qt/PySide6 6.9.3:
      46 automated native tests passed with the interactive VisualCheck
      skipped as designed, generated-wrapper compilation and all 18 binding
      CTests passed, and a newly created clean-environment wheel, dependency
      paths, direct installed-package import, and visual snapshot were checked.
- [x] Confirm the `CalendarView` slice on native Linux and Windows Qt 6.2.4
      binding lanes and the Qt 5.15 C++ compatibility lane. CI run
      `30607530481` passed generation and compilation, generated-contract
      checks, all binding tests, relocatable wheels, clean-environment smoke,
      acceptance snapshots, the C++ regression gates, and the final CI Gate.
- [x] Audit and bind `AnnotatedScrollBar` as the next M2 control. Its Python
      API includes the mutable `AnnotatedScrollBarLabel` value type, range and
      layout properties, label/query methods, static detail text, interaction
      signals, and native two-way synchronization with a borrowed
      `ScrollView`; the control neither hosts nor owns that view.
- [x] Normalize `AnnotatedScrollBarLabel` value equality in the Python category
      module because Shiboken 6.2 does not publish the namespace-level C++
      comparison operators that newer generators expose. Keep the mutable
      value type unhashable and verify the same behavior in build-tree and
      installed-wheel tests.
- [x] Keep the C++ `std::function<QString(int)>` detail provider private until
      a synchronous Python-callable adapter can preserve its semantics on
      Shiboken 6.2+. Generated-contract checks reject that partial provider
      surface and any parent, ownership, or keep-reference bookkeeping on the
      borrowed ScrollView link.
- [x] Confirm the `AnnotatedScrollBar` slice locally with macOS Qt/PySide6
      6.9.3: 11 automated native tests passed with the interactive VisualCheck
      skipped as designed, all 20 binding CTests and 36 verifier tests passed,
      and a new clean virtual environment passed wheel installation,
      dependency-path checks, `pip check`, runtime smoke, and snapshot review.
- [x] Confirm the `AnnotatedScrollBar` slice on native Linux and Windows
      Qt 6.2.4 binding lanes and the Qt 5.15 C++ compatibility lane. CI run
      `30609069504` passed generation and compilation, generated-contract
      checks, all binding tests, relocatable wheels, clean-environment smoke,
      acceptance snapshots, the C++ regression gates, and the final CI Gate.

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

Candidate areas include `ScrollView`, `Accordion`, `StackView`, `DrawerView`,
`TabView`, and other APIs that accept hosted `QWidget` instances.

Current prototype:

- [x] Select `ScrollView` as the first ownership host.
- [x] Hide the runtime-dependent
      `setContentWidget(QWidget*, WidgetOwnership)` overload and publish fixed
      `setOwnedContentWidget()`, `setBorrowedContentWidget()`, and
      `setReparentedContentWidget()` facade methods.
- [x] Verify all three modes across replacement, `None`, host destruction,
      explicit take, original-parent restoration, Python subclass identity,
      and repeated GC/destruction locally.
- [x] Verify that the generated private adapter does not mutate Shiboken
      ownership, parent, or keep-reference tables implicitly.
- [x] Confirm the complete `ScrollView` ownership facade and clean wheel on
      native Linux and Windows Qt 6.2.4 CI lanes.
- [x] Audit `Expander` as the second ownership host and bind `Card`, its public
      base class, without exposing internal header controls.
- [x] Publish fixed owned, borrowed, and reparented `Expander` methods while
      preserving the C++-compatible borrowed default for `setContentWidget()`
      and the `contentWidget=` constructor property.
- [x] Verify `Expander` replacement, `None`, take, host destruction,
      original-parent retention/restoration, Python subclass identity, repeated
      natural GC, generated-code contracts, a clean installed wheel, and the
      visible compatibility showcase locally.
- [x] Confirm the complete `Card`/`Expander` slice and clean wheel on native
      Linux and Windows Qt 6.2.4 CI lanes.
- [x] Audit `InfoBar` as the third ownership host. Its action joins the
      InfoBar Qt parent chain while installed and becomes parentless when
      replaced or cleared.
- [x] Replace the C++ action raw pointer with an observed pointer that clears
      the property, updates layout, and emits `actionWidgetChanged(nullptr)`
      when external code destroys the action.
- [x] Add a Python `InfoBar` facade that intercepts the `actionWidget=`
      constructor keyword, retains Python subclasses, rejects host/ancestor
      cycles, and provides `takeActionWidget()` with Python ownership.
- [x] Confirm the slice locally with macOS Qt/PySide6 6.9.3: 14 focused native
      InfoBar tests, all 16 binding CTests, 33 verifier tests, a clean installed
      wheel, dependency-path checks, and visible snapshot review passed.
- [x] Confirm the `InfoBar` ownership slice on native Linux and Windows
      Qt 6.2.4 binding lanes and the Qt 5.15 C++ compatibility lane in CI run
      `30599841356`, including generated contracts, runtime tests, relocatable
      wheels, clean-environment smoke tests, and acceptance snapshots.
- [x] Audit `Accordion` as the fourth ownership host. It composes already-bound
      `Expander` items and preserves the C++ Borrowed default, while Owned and
      Reparented modes remain explicit per item.
- [x] Remove the public native ownership overloads and publish fixed
      `addOwnedItem()`, `addBorrowedItem()`, `addReparentedItem()` and matching
      insert methods. The Python facade retains item subclasses and original
      parents without using Shiboken parent or keep-reference tables.
- [x] Confirm the `Accordion` slice locally with macOS Qt/PySide6 6.9.3:
      all 6 native Accordion tests, all 24 binding CTests, and all 41 verifier
      tests passed. A new clean virtual environment also passed wheel
      installation, `pip check`, dependency-path checks, runtime smoke, GC
      stress, and visible snapshot review.
- [x] Confirm the `Accordion` ownership slice on native Linux and Windows
      Qt 6.2.4 binding lanes and the Qt 5.15 C++ compatibility lane. CI run
      `30610740405` passed generation and compilation, all 41 generated
      contract checks, all 24 binding CTests, relocatable wheels,
      clean-environment smoke, acceptance snapshots, C++ regression gates,
      source-package verification, and the final CI Gate.
- [x] Audit `StackView` as the fifth ownership host and design its navigation
      boundary without introducing a model/delegate contract. Native
      push/pop/replace/clear transitions, status signals, keyboard back
      navigation, and indexed queries remain available.
- [x] Publish fixed Owned, Borrowed, and Reparented initial/push/bulk-push/
      replace methods. Remove default-policy and direct pointer-current
      wrappers from the native surface, block inherited `QStackedWidget`
      insertion/removal bypasses, and retain page subclasses and restore
      targets until native transitions finish.
- [x] Confirm the `StackView` slice locally with macOS Qt/PySide6 6.9.3:
      22 automated native tests passed and the interactive VisualCheck was
      skipped as designed; all 28 binding CTests and all 44 verifier tests
      passed. A new clean environment also passed wheel installation,
      `pip check`, dependency-path inspection, runtime smoke, GC stress,
      source-package verification, and visible snapshot review.
- [x] Confirm the `StackView` ownership/navigation slice on native Linux and
      Windows Qt 6.2.4 binding lanes and the Qt 5.15 C++ compatibility lane.
      CI run `30613428314` passed generation and compilation, all generated
      contract checks, all 28 binding CTests, Qt runtime-path verification,
      relocatable wheels, clean-environment smoke, acceptance snapshots,
      source-package checks, the C++ regression lanes, and the final CI Gate.

The supported `ScrollView` lifecycle uses normal Python GC and Qt parent
destruction. Hosted `Shiboken.ownedByPython()` flags vary across Shiboken
releases and are not public API. Repeated explicit `Shiboken.delete(host)` can
fast-fail even for a native `QScrollArea` wrapper on PySide6 6.2.4/Windows, so
that low-level debugging operation is not a compatibility gate.

`Expander` follows the same natural-GC contract, but retains its C++ default:
plain `setContentWidget()` is borrowed. The Python facade intercepts the
`contentWidget=` constructor keyword so it cannot bypass the audited ownership
path.

`Accordion` retains the same per-item C++ policies. Its Python facade keeps
hosted item wrappers and Reparented restore targets alive, synchronizes
Shiboken parent bookkeeping before releasing a restore target, and removes
records when Qt destroys an item externally. `takeItem()` is the only operation
that transfers an item back to parentless Python ownership.

`StackView` keeps the C++ Owned default for plain `push()`, `replace()`, and
`setInitialItem()`, while explicit methods fix every other page policy.
Borrowed and Reparented wrappers remain retained while a transition still
references them and are released only after native cleanup. Direct inherited
`QStackedWidget` insertion/removal is not part of the Python contract because
it bypasses the navigation stack. `setCurrentWidget()` remains public through
an index-only adapter so Shiboken cannot infer a new QObject parent from its
pointer argument.

`InfoBar` uses its narrower existing C++ contract: the current action dies with
the host, while replacement, clearing, or `takeActionWidget()` releases it as a
parentless Python-owned widget. The Python facade retains the wrapper; generated
code must not mutate Shiboken ownership, parent, or keep-reference tables.
External action destruction is tested through Qt's supported deferred-delete
path. Direct `Shiboken.delete()` on a still-parented Python subclass can
fast-fail inside PySide6 6.2.4/Windows before Qt completes its destroyed-signal
chain, so that low-level wrapper operation is not a compatibility requirement.

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

1. Treat `Card`/`Expander` as the second completed M3 slice after native
   Linux/Windows Qt 6.2.4 CI run `30552580180`.
2. Treat the `Avatar`/`RatingControl`/`ScrollBar` leaf slice as complete after
   native Linux and Windows Qt 6.2.4 CI run `30553990409`.
3. Treat the `PipsPager` slice as complete after native Linux and Windows
   Qt 6.2.4 plus Qt 5.15 C++ CI run `30598949551`.
4. Treat the `InfoBar` ownership slice as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ CI run `30599841356`.
5. Treat the `TextEdit` leaf slice as complete after native Linux and Windows
   Qt 6.2.4 plus Qt 5.15 C++ CI run `30601608042`.
6. Treat the `CompoundButton` leaf slice as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ CI run `30603864933`.
7. Treat the `FontIcon` foundation leaf as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ CI run `30604556341`.
8. Treat the `ColorPicker` leaf slice as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ CI run `30605260392`.
9. Treat the `CalendarView` leaf slice as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ CI run `30607530481`.
10. Treat the `AnnotatedScrollBar` value-type and borrowed-link slice as
    complete after native Linux and Windows Qt 6.2.4 plus Qt 5.15 C++ CI run
    `30609069504`.
11. Treat the `Accordion` ownership slice as complete after native Linux and
    Windows Qt 6.2.4 plus Qt 5.15 C++ CI run `30610740405`.
12. Treat the `StackView` ownership/navigation slice as complete after native
    Linux/Windows Qt 6.2.4 plus Qt 5.15 C++ CI run `30613428314`.
13. Defer `TabView` and `DrawerView` until their model/navigation or overlay
    boundaries are designed, rather than publishing unstable APIs merely to
    increase binding coverage.
