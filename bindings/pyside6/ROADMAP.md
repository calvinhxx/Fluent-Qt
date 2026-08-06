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
- The reusable `FluentQt` wheel contains only the UILib. The standalone
  pure-Python `FluentQt-Gallery` wheel depends on the exact matching core
  version and imports only public `fluentqt` APIs.
- PySide2/Shiboken2 compatibility is not part of this roadmap.

## Status

| Milestone | State | Deliverable |
|---|---|---|
| M0 — Binding foundation | Complete | Opt-in CMake target, Qt 6.2 generator path, version gates, one native module, startup API, tests |
| M1 — Core widget surface | Complete | Basic Input, Text Fields, Window, theme/font API, ownership and `nativeEvent` contracts |
| M2 — Low-risk widget coverage | Complete | Every planned leaf widget is bound or has an explicit boundary decision, with properties, signals, examples, manifest checks, and wheel smoke coverage |
| M3 — Hosted-widget ownership | Complete | Planned hosted-widget boundaries have fixed-semantics adapters plus ownership and GC tests |
| M4 — Models and navigation | Complete | Planned model/navigation surfaces cover Python models/delegates, virtual dispatch, selection, and lifecycle |
| M4.5 — Foundation authoring API | Validating | `FluentWidget`, `bind()`, `StateGroup`, and `AnchorLayout`/`anchors()` are implemented and pass local Qt 6.9.3 behavior, Gallery, manifest, and stub tests; Qt 6.2 and three-platform wheel CI regression remains |
| M5 — Overlays and native windows | In progress | Local Windows 11 DWM material/layout and pointer-driven move/resize acceptance passed, together with automated XCB/Wayland/Windows/Cocoa checks; only physical KWin/Wayland compositor review remains |
| M6 — Release-grade Python distribution | In progress | Type/API governance, the six-target native wheel matrix, architecture-specific manylinux repair/audit, and x86_64/AArch64 container-CI evidence are complete; the standalone pure-Python Gallery distribution covers all 88 native routes and 199 SampleCards. The current changes still need required-matrix regression, followed by signing and publication |

## Public API coverage ledger

This table is the source of truth for component coverage. A milestone cannot
be marked complete merely because its current checklist is green; every public
component below must either be bound or retain an explicit boundary decision.
That audit is complete for M0 through M4. The manifest currently records 87
required classes/value/support types, 13 enums, 16 functions, and 2 version variables; M4.5, M5, and M6
retain the open boundaries shown below and in their milestone sections.

| Category | Bound now | Remaining boundary |
|---|---|---|
| Basic Input | `Button`, `CheckBox`, `ColorPicker`, `ComboBox`, `CompoundButton`, `DropDownButton`, `HyperlinkButton`, `RadioButton`, `RatingControl`, `RepeatButton`, `Slider`, `SplitButton`, `ToggleButton`, `ToggleSplitButton`, `ToggleSwitch` | — |
| Collections | `DrawerView`, `FlipView`, `FlowView`, `GridView`, `ListView`, `SplitView`, `SplitViewPaneOptions`, `StackView`, `TreeView` | Complete for the current public component and support-type set |
| Date & Time | `CalendarDatePicker`, `CalendarView`, `DatePicker`, `TimePicker` | Complete for the current public component set |
| Dialogs & Flyouts | `CoachMark`, `ContentDialog`, `Dialog`, `Flyout`, `Popup`, `TeachingTip` | Complete for the current public component set |
| Foundation | `FontIcon`, theme/font package API, read-only `Typography`/`Spacing`/`CornerRadius`, typed `ThemeTokens`, `FluentWidget`, `bind()`, `BindingMode`, `StateGroup`, `AnchorLayout`, `AnchorSpec`, `AnchorEdge`, `anchors()`, ownership enum | Raw `FluentElement`/`QMLPlus` multiple-inheritance mixins, mutable registries, and overlay helpers stay implementation-facing; their theme, binding, state, and anchor capabilities are published through safe facades |
| Layout | `Accordion`, `Card`, `Divider`, `Expander` | Complete for the current public component set |
| Menus & Toolbars | `CommandBar`, `CommandBarFlyout`, `FluentMenu`, `FluentMenuBar`, `FluentMenuItem` | Complete for the current public component set; native CI validation passed |
| Navigation | `Breadcrumb`, `BreadcrumbItem`, `NavigationView`, `Pivot`, `PivotItem`, `SelectorBar`, `SelectorBarItem`, `StackContentHost`, `TabView`, `TabViewItem` | Complete for the current public component set |
| Scrolling | `AnnotatedScrollBar`, `AnnotatedScrollBarLabel`, `PipsPager`, `ScrollBar`, `ScrollView`, `ScrollViewZoomAwareWidget` | Complete for the current public component and support-type set |
| Status & Info | `Avatar`, `InfoBadge`, `InfoBar`, `ProgressBar`, `ProgressRing`, `Shimmer`, `Toast`, `ToolTip` | Complete for the current public component set; native CI validation passed |
| Text Fields | `AutoSuggestBox`, `EditingCommandRouter`, `Label`, `LineEdit`, `NumberBox`, `PasswordBox`, `TextEdit` | Complete for the current public component and support-type set |
| Windowing | `Window`, `TitleBar`, and backdrop values | Windows 11 DWM material/layout and pointer-driven system move/resize review are complete; physical KWin/Wayland compositor behavior remains in M5 |

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
- Windowing: `Window`, `TitleBar`, backdrop enums, and backdrop value types.
- Foundation: Light/Dark theme selection, design-language presets, accent
  color, typography roles, font scaling, and build information.

The merge gates for this milestone include Python subclass dispatch, Qt
properties and signals, `Window` child-parent ownership, a safe two-argument
`nativeEvent()` contract, API-manifest checks, and clean-environment wheel
smoke tests.

## M2 — Low-risk widget coverage

Completed scope:

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
- [x] Keep `ShimmerPainter::Element` itself private, then publish custom
      skeleton authoring through the `Shimmer.Shape`/`Shimmer.Element` Python
      value facade and `elements()`/`setElements()`/`clearElements()`.
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
      native CI validation.
- [x] Audit `PipsPager` separately and reproduce the generator leak where its
      animation-only `selectedVisualOffset` and `visibleWindowOffset`
      properties appeared as Python constructor keywords.
- [x] Move the two internal animations to `QVariantAnimation` callbacks and
      exclude their implementation-only properties only while Shiboken parses
      the header. C++ motion and the existing Qt meta-object properties remain
      compatible, while the generated Python API stays clean.
- [x] Add the `PipsPager` category export, enum, property/signal/navigation
      tests, manifest entry, generated privacy contract, wheel smoke, and
      visible acceptance coverage.
- [x] Confirm the fifth slice locally with macOS Qt/PySide6 6.9.3: 17 focused
      native tests, all 15 binding CTests, 29 verifier tests, a clean installed
      wheel, dependency-path checks, and visual snapshot review passed.
- [x] Confirm `PipsPager` on the native Linux and Windows Qt 6.2.4 binding
      lanes and the Qt 5.15 C++ compatibility lane. Native CI validation
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
      binding lanes and the Qt 5.15 C++ compatibility lane. Native CI validation passed generation and compilation, generated-contract
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
      binding lanes and the Qt 5.15 C++ compatibility lane. Native CI validation passed generation and compilation, generated-contract
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
      binding lanes and the Qt 5.15 C++ compatibility lane. Native CI validation passed generation and compilation, generated-contract
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
      binding lanes and the Qt 5.15 C++ compatibility lane. Native CI validation passed generation and compilation, generated-contract
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
      binding lanes and the Qt 5.15 C++ compatibility lane. Native CI validation passed generation and compilation, generated-contract
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
- [x] Keep the raw C++ `std::function<QString(int)>` detail-provider overload
      private and publish `setDetailLabelProvider(callable)` as a facade that
      writes the result synchronously during the native request signal on
      Shiboken 6.2+. Generated-contract checks still reject the raw overload
      and any parent, ownership, or keep-reference bookkeeping on the borrowed
      ScrollView link.
- [x] Confirm the `AnnotatedScrollBar` slice locally with macOS Qt/PySide6
      6.9.3: 11 automated native tests passed with the interactive VisualCheck
      skipped as designed, all 20 binding CTests and 36 verifier tests passed,
      and a new clean virtual environment passed wheel installation,
      dependency-path checks, `pip check`, runtime smoke, and snapshot review.
- [x] Confirm the `AnnotatedScrollBar` slice on native Linux and Windows
      Qt 6.2.4 binding lanes and the Qt 5.15 C++ compatibility lane. Native CI validation passed generation and compilation, generated-contract
      checks, all binding tests, relocatable wheels, clean-environment smoke,
      acceptance snapshots, the C++ regression gates, and the final CI Gate.

M2 is complete for the current public component set. Future leaf components
must receive an API audit before they can extend this milestone. A component
belongs to this class of work only when it:

- is a leaf `QWidget` with no runtime ownership mode;
- does not expose a model/delegate contract;
- does not create a popup or same-window overlay;
- does not require platform-native window behavior; and
- can retain its existing C++ semantics without a Python-only façade.

Each slice must add the generated wrapper inputs, a category re-export,
`api-manifest.json` coverage, property/signal tests, installed-wheel smoke
coverage, and a runnable example when the control has visible behavior.

## M3 — Hosted-widget ownership

The completed scope includes `ScrollView`, `Expander`, `InfoBar`, `Accordion`,
`StackView`, `FlipView`, `SplitView`, `NavigationView`/`StackContentHost`, and
`DrawerView`. `TabView` metadata/navigation remains in M4 because application
pages are caller-managed rather than adopted by the control.

Completed ownership batches:

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
      Qt 6.2.4 binding lanes and the Qt 5.15 C++ compatibility lane in native CI validation, including generated contracts, runtime tests, relocatable
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
      Qt 6.2.4 binding lanes and the Qt 5.15 C++ compatibility lane. Native CI validation passed generation and compilation, all 41 generated
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
      native CI validation passed generation and compilation, all generated
      contract checks, all 28 binding CTests, Qt runtime-path verification,
      relocatable wheels, clean-environment smoke, acceptance snapshots,
      source-package checks, the C++ regression lanes, and the final CI Gate.
- [x] Audit `FlipView` as the sixth ownership host. Preserve the legacy
      host-owned `addPage()` default while adding explicit per-page Owned,
      Borrowed, and Reparented install/release behavior, `takePage()` transfer,
      original-parent restoration, and external-destruction cleanup in C++.
- [x] Publish only fixed-semantics Python add/insert methods, retain page
      subclasses and restore targets in the facade, and keep legacy transfer
      overloads plus the runtime ownership argument private. Generated-code
      checks require adapters to avoid implicit Shiboken parent/reference
      mutation and require `takePage()` to return Python ownership.
- [x] Confirm the `FlipView` slice locally with matched macOS Qt/PySide6 6.9.3:
      30 automated native tests passed and 1 manual VisualCheck was skipped as
      designed; all 39 binding CTests, 119 Python binding tests, and 87 verifier
      tests passed. Subsequent item-view teardown hardening expanded the current
      suite to 43 binding CTests and 123 Python binding tests. A new clean
      environment also passed wheel installation,
      `pip check`, loaded dependency-path inspection, runtime smoke, three GC
      stress cases, source-package integration, and visible snapshot review.
- [x] Native CI validation confirmed the `FlipView` ownership/navigation slice
      on native Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including Qt
      5.15/6.2 C++ regressions, relocatable clean wheels, source-package
      integration, acceptance snapshots, and the final CI Gate. The Windows
      lane also passed 25 no-`close()` model/delegate/selection GC iterations
      for each of the four item-view facades and the complete installed-wheel
      smoke at the previously failing boundary.
- [x] Audit `SplitView` as the seventh ownership host. Preserve the legacy
      host-owned add/insert default and transfer-style C++ removal, while
      adding explicit per-pane release policy, `takePaneAt()` transfer,
      original-parent restoration, and external-destruction cleanup.
- [x] Publish fixed Owned, Borrowed, and Reparented add/insert methods plus the
      mutable `SplitViewPaneOptions` value type. The facade applies recorded
      policies on removal, retains pane subclasses and restore targets, and
      keeps runtime ownership arguments and legacy transfer removals private.
      Its Python initializer preserves positional, keyword, and copy
      construction without adding a C++ constructor, so the public C++ value
      remains aggregate-initializable.
      Generated-code checks require the private adapters to avoid implicit
      Shiboken parent/reference mutation and `takePaneAt()` to return Python
      ownership.
- [x] Confirm the `SplitView` slice locally with matched macOS Qt/PySide6
      6.9.3: 16 automated native tests passed and 1 manual VisualCheck was
      skipped as designed; all 47 binding CTests, 131 Python binding tests,
      and 92 verifier tests passed. A newly created clean environment also
      passed wheel installation, `pip check`, dependency-path inspection,
      runtime smoke, three GC stress cases, source-package integration, and
      visible snapshot review.
- [x] Native CI validation confirmed the `SplitView` ownership slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated contracts,
      all 47 binding CTests, relocatable clean wheels, source-package
      integration, acceptance snapshots, Qt 5.15/6.2 C++ regressions, and the
      final CI Gate.
- [x] Audit `NavigationView` and its C++-owned `StackContentHost` as the eighth
      hosted-widget boundary. Preserve the legacy C++ transfer behavior while
      adding explicit page and header/main/footer chrome release policies,
      take operations, original-parent restoration, duplicate/ancestor
      rejection, and external-destruction cleanup.
- [x] Publish fixed Owned, Borrowed, and Reparented page/chrome methods through
      `fluentqt.navigation`. The internal host receives the same Python facade
      as a directly constructed `StackContentHost`; Python subclasses and
      restore targets remain retained without generator-side parent or
      keep-reference bookkeeping.
- [x] Confirm the slice locally with matched macOS Qt/PySide6 6.9.3: 24
      automated native NavigationView tests passed and 1 manual VisualCheck
      was skipped as designed; all 54 binding CTests, 144 Python binding tests,
      and 100 verifier tests passed. A newly created clean environment also
      passed wheel installation, `pip check`, loaded dependency-path
      inspection, runtime smoke, six isolated GC stress cases, source-package
      integration, and visible snapshot review.
- [x] Native CI validation confirmed the `NavigationView`/`StackContentHost`
      slice on native Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including all
      54 binding CTests, generated contracts, Qt 5.15/6.2 C++ regressions,
      relocatable clean wheels, source-package integration, acceptance
      snapshots, and the final CI Gate.
- [x] Audit `DrawerView` as the ninth hosted-widget boundary and the first M5
      same-window overlay slice. Preserve the C++ Borrowed default, publish
      fixed Owned, Borrowed, and Reparented content methods plus
      `takeContentWidget()`, and cover `CloseFlag`, scrim, outside press,
      Escape, open/close lifecycle, and Python virtual dispatch.
- [x] Confirm the slice locally with matched macOS Qt/PySide6 6.9.3: 22 of 24
      native DrawerView tests passed and 2 desktop/manual tests were skipped as
      designed; all 58 binding CTests, 152 Python binding tests, and 104
      verifier tests passed. A new `.venv-pyside69-drawer-wheel` also passed
      wheel installation, `pip check`, loaded dependency paths, complete smoke,
      three isolated GC stresses, source-package regeneration/build, and
      snapshot review.
- [x] Native CI validation confirmed the `DrawerView` slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all binding tests, relocatable clean wheels on all three
      platforms, acceptance snapshots, source-package integration, Qt
      5.15/6.2 C++ regressions, and the final CI Gate.

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

`FlipView` keeps the C++ Owned default for plain `addPage()` and `insertPage()`.
Explicit methods fix every other policy. `removePage()` applies the recorded
policy, while `takePage()` always returns a parentless Python-owned page. The
facade retains Python subclasses and Reparented restore targets, and both the
C++ host and facade discard records when a page is destroyed externally.

`SplitView` keeps the C++ Owned default for plain `addPane()` and
`insertPane()`. Python exposes fixed Owned, Borrowed, and Reparented entry
points, applies the recorded policy through `removePane()`/`removePaneAt()`,
and reserves `takePaneAt()` for an unconditional parentless transfer. The
facade retains Python subclasses and Reparented restore targets; native and
facade records are cleared when a pane is destroyed externally.

`DrawerView` keeps the C++ Borrowed default for plain `setContentWidget()`.
Python also exposes fixed Owned, Borrowed, and Reparented entry points plus
`takeContentWidget()`. The facade retains hosted wrappers and Reparented
restore targets and clears records on replacement, external destruction, and
explicit transfer. The host itself and its ancestors cannot become content;
changing the same widget's ownership mode requires an explicit take first.

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

This milestone started with navigation components whose public contract has no
model or hosted-page ownership boundary. After defining the Python boundary,
it also completed the model-backed collection set: `ListView`, `GridView`,
`TreeView`, and `FlowView`.
Validation must include:

- `QAbstractItemModel` and delegate lifetime;
- Python virtual overrides and `super()` dispatch;
- selection, reset, row insertion/removal, and persistent-index behavior;
- model replacement and destruction from both Python and C++; and
- keyboard, focus, RTL, and accessibility-relevant navigation behavior.

Completed scope:

- [x] Audit `TabView`: it owns only `TabViewItem` metadata, selection, and
      navigation behavior; application pages remain caller-owned composition.
- [x] Bind `TabView` and mutable `TabViewItem` through
      `fluentqt.navigation`, including QVariant-compatible metadata and stable
      value equality, while keeping the internal `TabStrip` private.
- [x] Cover constructors, metadata mutation, properties, signals, close,
      reorder, keyboard accelerators, RTL, Python virtual override dispatch,
      an external `QStackedWidget` host, API manifest, generated contracts,
      wheel smoke, and a visible acceptance example.
- [x] Confirm the slice locally with matched macOS Qt/PySide6 6.9.3:
      9 automated native TabView tests passed and 2 desktop/manual cases were
      skipped as designed; all 29 binding CTests, 75 Python binding tests, and
      49 verifier tests passed. A fresh virtual environment also passed wheel
      installation, `pip check`, dependency-path inspection, runtime smoke,
      source-package integration build, and visible snapshot review.
- [x] Confirm generated code, tests, source package, and clean wheels in native CI validation: native Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3 binding
      lanes passed generation, compilation, contract checks, binding tests,
      relocatable wheel builds, and clean-environment installation; the Qt 5.15
      and Qt 6.2 C++ integration lanes and final CI Gate also passed.
- [x] Audit `ListView`: models, selection models, and delegates remain
      caller-owned; the view must retain their Python wrappers while installed.
      Custom header/footer QWidget hosting and the section toggle/synchronous
      `std::function` callback remain private pending explicit Python
      contracts.
- [x] Bind `ListView` through `fluentqt.collections`, including a
      Qt 6.2-compatible `SelectionMode` adapter, model/selection retention,
      delegate wrapper retention, internal scrollbar getters implemented by
      explicit binding adapters while preserving their Python method names,
      and the text-only header/footer convenience API.
- [x] Cover Python `QAbstractListModel` insert/remove/reset notifications,
      persistent indexes, custom `QItemSelectionModel`, Python delegate and
      view virtual dispatch, replacement/destruction lifetimes, API manifest,
      generated contracts, wheel smoke, and a visible acceptance example.
- [x] Confirm the complete slice locally with matched macOS Qt/PySide6 6.9.3:
      101 native ListView tests passed and 1 manual VisualCheck was skipped as
      designed; all 30 binding CTests, 81 Python binding tests, and 58 verifier
      tests passed. A fresh virtual environment also passed wheel installation,
      `pip check`, dependency-path inspection, runtime smoke, source-package
      integration build, and visible snapshot review.
- [x] Native CI validation confirmed native Linux/Windows Qt 6.2.4, macOS
      Qt 6.9.3, Qt 5.15/6.2 C++ regressions, relocatable clean wheels on all
      three platforms, and the final CI Gate. This run also exercised the
      compatibility path for Shiboken 6.2 not discovering the cross-namespace
      scrollbar member getters.
- [x] Audit `GridView`: ordinary models, selection models, and delegates remain
      caller-owned. Native drag reordering is explicitly limited to
      `QStandardItemModel`; other `QAbstractItemModel` implementations retain
      display, selection, and notification support without a false reorder
      promise.
- [x] Bind `GridView` through `fluentqt.collections`, reusing the stable
      Qt 6.2 `SelectionMode` converter and exposing native cell metrics,
      selection, scroll behavior, header/placeholder text, reorder signals,
      delegate virtual dispatch, and a borrowed internal-scrollbar adapter.
- [x] Cover model insert/remove/reset and persistent indexes, caller-owned
      model/selection/delegate lifetimes, external destruction, Python delegate
      and view virtual dispatch, keyboard/RTL/accessibility behavior, API
      manifest, generated contracts, installed-wheel smoke, and a visible
      `QStandardItemModel` group-reorder example.
- [x] Harden retained item delegates for Shiboken 6.2 on Windows: validate the
      wrapper before returning it from the Python facade and discard stale
      references even when the Python `destroyed` callback is missed. Unit and
      installed-wheel smoke tests force this compatibility path.
- [x] Confirm the slice locally with matched macOS Qt/PySide6 6.9.3: 56 of 66
      native GridView tests passed and 10 desktop/manual cases were skipped as
      designed; all 31 binding CTests, 88 Python binding tests, and 61 verifier
      tests passed. A newly created clean virtual environment also passed wheel
      installation, `pip check`, dependency-path inspection, runtime smoke,
      source-package integration build, and visible snapshot review.
- [x] Native CI validation confirmed native Linux/Windows Qt 6.2.4, macOS
      Qt 6.9.3, Qt 5.15/6.2 C++ regressions, relocatable clean wheels on all
      three platforms, acceptance snapshots, and the final CI Gate.
- [x] Audit `TreeView`: hierarchical models, selection models, and delegates
      remain caller-owned and their Python wrappers are retained while
      installed. Native drag reordering is limited to `QStandardItemModel`,
      and the implementation-oriented `SelectionIndicatorStyle` remains
      private.
- [x] Bind `TreeView` through `fluentqt.collections`, including the stable
      Qt 6.2 `SelectionMode` adapter, hierarchy expansion, check-state
      selection, indicator visibility/motion scalar APIs, reorder signals,
      Python virtual dispatch, and borrowed internal-scrollbar adapters.
- [x] Cover hierarchical insert/remove/reset notifications and persistent
      indexes, caller-owned model/selection/delegate replacement and external
      destruction, stale delegate wrappers, Python model/delegate/view virtual
      dispatch, keyboard/RTL/accessibility behavior, API manifest, generated
      contracts, installed-wheel smoke, and a visible hierarchy example.
- [x] Confirm the slice locally with matched macOS Qt/PySide6 6.9.3: 92 of 93
      native TreeView tests passed and the manual VisualCheck was skipped as
      designed; all 32 binding CTests, 95 Python binding tests, and 66 verifier
      tests passed. A newly created clean virtual environment also passed wheel
      installation, `pip check`, dependency-path inspection, runtime smoke,
      source-package integration build, and visible snapshot review.
- [x] Native CI validation confirmed generated contracts, native
      Linux/Windows Qt 6.2.4 behavior, macOS Qt 6.9.3, Qt 5.15/6.2 C++
      regressions, source-package integration, relocatable clean wheels and
      acceptance snapshots on all three platforms, and the final CI Gate.
      The Windows Shiboken 6.2 lifecycle case also passed through Qt's
      supported deferred model-destruction path.
- [x] Audit and bind `Breadcrumb` plus mutable `BreadcrumbItem` metadata. The
      Python facade rejects mixed sequences and dispatches text and metadata
      lists through separate native adapters because some Shiboken versions
      otherwise select `QStringList` for value wrappers and silently create
      empty labels.
- [x] Cover metadata/QVariant round trips, stable value equality, properties,
      signals, insertion/removal, overflow geometry, activation, keyboard and
      Python virtual dispatch, API manifest, generated adapter contracts,
      installed-wheel smoke, and a visible full-path/middle-overflow example.
- [x] Confirm the complete slice locally with matched macOS Qt/PySide6 6.9.3:
      10 automated native Breadcrumb tests passed and 1 manual VisualCheck was
      skipped as designed; all 33 binding CTests, 98 Python binding tests, and
      73 verifier tests passed. A newly created virtual environment also passed
      wheel installation, `pip check`, dependency-path inspection, runtime
      smoke, source-package integration build, and visible snapshot review.
- [x] Native CI validation confirmed the `Breadcrumb` slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated contracts,
      binding tests, clean wheel installation, acceptance snapshots,
      Qt 5.15/6.2 C++ integration, source-package integration, and the final
      CI Gate.
- [x] Audit `Pivot` and `SelectorBar`: both own mutable navigation metadata,
      selection, and overflow state, but neither adopts application pages,
      models, delegates, overlays, or caller-owned widgets.
- [x] Bind `Pivot`, `PivotItem`, `SelectorBar`, and `SelectorBarItem` through
      `fluentqt.navigation`, including both text/value item overloads, mutable
      unhashable value semantics, QVariant-compatible data, nested overflow
      enums, and stable root/category/native identities.
- [x] Cover item mutation, duplicate-suppressed selection signals, activation,
      keyboard/Python virtual dispatch, geometry and MoreButton overflow, API
      manifest, generated overload/value/QVariant contracts, installed-wheel
      smoke, and a visible example connected to a caller-owned
      `QStackedWidget`.
- [x] Confirm the complete slice locally with matched macOS Qt/PySide6 6.9.3:
      17 automated native Pivot/SelectorBar tests passed and 2 manual
      VisualChecks were skipped as designed; all 34 binding CTests, 104 Python
      binding tests, and 77 verifier tests passed. A newly created clean
      virtual environment also passed wheel installation, `pip check`, loaded
      dependency-path inspection, runtime smoke, source-package integration
      build, and visible snapshot review.
- [x] Native CI validation confirmed native Linux/Windows Qt 6.2.4, macOS
      Qt 6.9.3, Qt 5.15/6.2 C++ regressions, clean wheels, source-package
      integration, acceptance snapshots, and the final CI Gate. The Windows
      lane also passed the supported deferred-destruction paths for retained
      `ListView` models and delegates.
- [x] Audit `FlipView`, `FlowView`, and `SplitView`: `FlowView` has no hosted
      QWidget boundary and reuses the caller-owned item-model contract;
      `FlipView` has an explicit M3 page contract, and `SplitView` now has an
      explicit M3 pane release and parent-restoration contract.
- [x] Bind `FlowView` through `fluentqt.collections` with stable Qt 6.2
      `SelectionMode` and borrowed-scrollbar adapters, generated-wrapper
      retention for its overridden model/delegate setters, and facade-level
      invalid delegate cleanup.
- [x] Cover variable-size Python model roles, insert/remove/reset and
      persistent indexes, selection, geometry/hit testing, Python delegate
      paint/size virtuals, view virtual dispatch, dependency replacement and
      deferred destruction, API manifest, generated contracts, installed-wheel
      smoke, and a visible adaptive-card example.
- [x] Confirm the slice locally with matched macOS Qt/PySide6 6.9.3: 15
      automated native FlowView tests passed and 1 manual VisualCheck was
      skipped as designed; all 35 binding CTests, 111 Python binding tests, and
      82 verifier tests passed. A newly created clean environment also passed
      wheel installation, `pip check`, dependency-path inspection, runtime
      smoke, source-package integration, and snapshot review.
- [x] Native CI validation confirmed the `FlowView` slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, binding tests, relocatable clean wheels, acceptance snapshots,
      Qt 5.15/6.2 C++ regressions, source-package integration, and the final
      CI Gate. The Windows 6.2.4 lanes also exercised deterministic
      signal/view isolation and installed-wheel FlowView teardown.

## M4.5 — Foundation authoring API

The goal is not to force non-`QObject` C++ mixins into Python multiple
inheritance. It is to let Python authors use the same theme, property-binding,
state, and anchor engines through APIs that respect PySide object identity and
lifetime rules.

- [x] Publish effective theme, design language, typography, color, radius,
      spacing, motion, material, elevation, breakpoint, and backdrop tokens,
      plus a theme-update callback through subclassable `FluentWidget`.
- [x] Publish read-only `Typography.Icons`, `Typography.IconSize`, `Spacing`,
      and `CornerRadius` namespace facades, plus mapping-compatible typed
      `ThemeTokens`, so Python teaching snippets use the same semantic names
      as C++ without exposing mutable registries or private-use glyphs.
- [x] Reuse native `PropertyBinder` through package-level `bind()` and
      `BindingMode`, covering one-way/two-way initial sync, invalid properties,
      and QObject lifetime.
- [x] Reuse the native QMLPlus state engine through the mapping-oriented
      `StateGroup`, covering multi-object changes, default restoration, unknown
      states, destroyed targets, and a Python signal.
- [x] Reuse the native anchor solver through `AnchorLayout`, `AnchorSpec`,
      `AnchorEdge`, and the `anchors()` builder, covering center, edge,
      top-right, and fill semantics.
- [x] Make the Python Gallery QML+ page execute these public APIs directly;
      its displayed snippets no longer simulate the C++ features with signal
      callbacks, `resizeEvent()`, or manual `move()` calls.
- [x] Local Qt/PySide/Shiboken 6.9.3 passes wrapper compilation, Foundation
      behavior tests, generated contracts, Gallery contracts, the 87-class/
      13-enum/16-function manifest, and typing stubs.
- [ ] Regress the slice on Linux/Windows Qt 6.2.4 minimum lanes and the
      Linux/macOS/Windows wheel lanes before marking the milestone complete.

Raw `FluentElement`, `QMLPlus`, mutable `ThemeRegistry`, and overlay-helper
types remain C++ implementation details. That is a binding boundary rather
than a missing feature: the Python facades call the same native engines and do
not create a second theme or layout implementation.

## M5 — Overlays and native windows

Popup, Flyout, ContentDialog, TeachingTip, dropdown, and other overlay
components require tests for scrim ordering, outside press, Escape, focus
return, top-level resize, and close-policy semantics.

Current slices:

- [x] Bind `DrawerView` edge, dimensions, modal/dim behavior, interaction,
      animation, `CloseFlag`, open/close lifecycle, and content ownership APIs.
- [x] Cover same-window attachment, scrim outside press, Escape,
      `NoAutoClose`, Python virtual overrides, Owned/Borrowed/Reparented,
      explicit take, a clean wheel, and a visible snapshot locally.
- [x] Native CI validation confirmed the slice on native Linux/Windows Qt
      6.2.4 and macOS Qt 6.9.3, including clean wheels on all three platforms,
      source-package integration, Qt 5.15/6.2 C++ regressions, and the final CI
      Gate.
- [x] Bind `Popup` open/close state, modal/dim behavior, animations,
      `CloseFlag`, anchor-relative placement, local theme source, and
      light-dismiss passthrough regions behind a dependency-retaining facade.
- [x] Cover same-window attachment, scrim creation, Escape, `NoAutoClose`,
      focus return without stealing a later focus move, Python virtual
      overrides, external QWidget deletion, 25-cycle dependency GC stress,
      generated contracts, installed-wheel smoke, and a visible snapshot
      locally on matched Qt/PySide6 6.9.3.
- [x] Native CI validation confirmed the Popup slice on native Linux/Windows
      Qt 6.2.4 and macOS Qt 6.9.3, including generated contracts, binding
      tests, clean wheels on all three platforms, source-package integration,
      acceptance snapshots, Qt 5.15/6.2 C++ regressions, and the final CI Gate.
- [x] Bind `Flyout` placement, anchor offset, window clamping, inherited
      Popup lifecycle, and caller-owned anchor APIs behind the shared
      dependency-retaining facade.
- [x] Cover Top/Bottom placement, Auto flipping, same-window attachment,
      non-modal/no-scrim defaults, Escape and focus return, Python virtual
      overrides, external anchor destruction, 25-cycle dependency GC stress,
      generated contracts, an installed clean wheel, source-package
      integration, and a visible snapshot locally on matched Qt/PySide6 6.9.3.
- [x] Native CI validation confirmed the Flyout slice on native Linux/Windows
      Qt 6.2.4 and macOS Qt 6.9.3, including generated contracts, binding
      tests, clean wheels on all three platforms, source-package integration,
      acceptance snapshots, Qt 5.15/6.2 C++ regressions, and the final CI Gate.
- [x] Bind `Dialog` and `ContentDialog` with native same-window modality,
      smoke scrim, animation/result lifecycle, command signals, stable result
      constants, caller-owned theme-source retention, and explicit installed
      content adoption/release through `setContent()` and `takeContent()`.
- [x] Harden native `ContentDialog::content()` with `QPointer`, reject unsafe
      static-field generation that crashes Shiboken 6.9 module startup, and
      cover Python subclassing, native result buttons, external destruction,
      host/ancestor rejection, 25-cycle GC stress, generated contracts,
      source packaging, a clean installed wheel, and a visible snapshot on
      matched macOS Qt/PySide6 6.9.3. Locally, all 43 native Dialog tests
      passed (two manual VisualChecks skipped), along with 166 binding tests,
      120 verifier tests, and all 65 PySide CTests.
- [x] Native CI validation confirmed the Dialog/ContentDialog slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all 65 binding CTests, clean wheels on all three platforms,
      source-package integration, acceptance snapshots, Qt 5.15/6.2 C++
      regressions, and the final CI Gate.
- [x] Bind `ComboBox` item/current-value APIs, signals, editable-line-editor
      adoption, caller-owned model retention, model-column/root-index support,
      Python virtual popup dispatch, and the native same-window dropdown.
- [x] Keep the popup's internal `ListView` and delegate implementation-facing,
      reject inherited `setView()`/`setItemDelegate()` calls that would only
      mutate Qt's unused fallback popup, and enforce generated ownership and
      native-fallback contracts for models, editors, and popup overrides.
- [x] On matched macOS Qt/PySide6 6.9.3, pass 40 native ComboBox tests (one
      manual VisualCheck skipped), 170 binding tests, 126 verifier tests, all
      67 PySide CTests, source-package integration, clean-wheel
      installation/runtime isolation, and a visible build-tree/installed-wheel
      snapshot with identical bytes.
- [x] Native CI validation confirmed the ComboBox slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all 67 binding CTests, clean wheels on all three platforms,
      source-package integration, acceptance snapshots, Qt 5.15/6.2 C++
      regressions, and the final CI Gate.
- [x] Bind `DropDownButton`, `SplitButton`, and `ToggleSplitButton` together
      with `FluentMenu` and `FluentMenuItem`. Menus remain caller-owned while
      `setMenu()` retains their Python wrappers until replacement,
      `setMenu(None)`, or host teardown.
- [x] Harden native menu lifecycle state with deletion-safe `QPointer`
      storage, observable `menu`/`isOpen` properties, replacement and external
      destruction signals, RTL secondary hit testing, and strict separation
      between primary, secondary, and toggle activation. Direct native menu
      tests now cover typography notification and QAction triggering.
- [x] On matched macOS Qt/PySide6 6.9.3, pass 25 focused native tests (three
      manual VisualChecks skipped), 174 binding tests, 129 generated-contract
      verifier tests, all 69 PySide CTests, an extracted source-package binding
      rebuild, a clean installed wheel smoke, and build-tree/installed-wheel
      menu-button snapshots with identical bytes.
- [x] Native CI validation confirmed the menu-button slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all 69 binding CTests, clean wheels on all three platforms,
      source-package integration, acceptance snapshots, Qt 5.15/6.2 C++
      regressions, and the final CI Gate. The Windows lane also passed the
      deferred Qt destruction paths for parented `ContentDialog` fixtures.
- [x] Bind `CalendarDatePicker`, `DatePicker`, and `TimePicker` with Qt-native
      `QDate`/`QTime`/locale values, nested field and format enums, repeat-safe
      property signals, Python virtual dispatch, and their existing native
      same-window popup implementations. Keep all internal popup/flyout
      helper classes private, and return the Qt-owned `CalendarView` without
      changing Shiboken ownership, parentage, or retention state.
- [x] On matched macOS Qt/PySide6 6.9.3, pass 55 focused native tests (three
      manual VisualChecks skipped), 178 binding tests, 132 generated-contract
      verifier tests, all 71 PySide CTests, an extracted source-package binding
      rebuild, and a newly created clean-venv wheel smoke. The build-tree and
      installed-wheel picker snapshots have identical SHA-256 values.
- [x] Native CI validation confirmed the date/time picker slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all 71 binding CTests, clean wheels on all three platforms,
      source-package integration, acceptance snapshots, Qt 5.15/6.2 C++
      regressions, and the final CI Gate.
- [x] Bind `AutoSuggestBox` directly on top of the native Fluent `LineEdit`,
      with Python string-list conversion, both nested enums, repeat-safe
      properties, typed text/suggestion/query/open-state signals, Python
      virtual dispatch, keyboard preview/submission, and the existing
      same-window suggestion Flyout. Keep its internal model, popup, and row
      delegate private, and remove the inherited C++ theme-refresh hook from
      the Python text-field hierarchy.
- [x] On matched macOS Qt/PySide6 6.9.3, pass all 15 automated native
      AutoSuggestBox tests (one manual VisualCheck skipped), 181 binding tests,
      137 generated-contract verifier tests, all 73 PySide CTests, an extracted
      source-package binding rebuild/test, and a new clean-venv wheel smoke.
      Build-tree and installed-wheel acceptance snapshots have identical
      SHA-256 values.
- [x] Native CI validation confirmed the AutoSuggestBox slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all 73 binding CTests, clean wheels on all three platforms,
      source-package integration, acceptance snapshots, Qt 5.15/6.2 C++
      regressions, and the final CI Gate.
- [x] Bind `CoachMark` and `TeachingTip` with caller-owned target retention,
      native same-window placement, content-host access, semantic close
      reasons, Python virtual dispatch, and explicit rejection of raw target
      and internal theme-hook bypasses. `TeachingTip` reuses the existing
      Popup dependency-retaining facade without changing QWidget ownership.
- [x] On matched macOS Qt/PySide6 6.9.3, pass all 30 automated native
      CoachMark/TeachingTip tests (two manual VisualChecks skipped), 184
      binding tests, 143 generated-contract verifier tests, and all 75 PySide
      CTests. Also rebuild and test the extracted source package, pass a new
      clean-venv wheel smoke and `pip check`, and produce byte-identical
      build-tree/installed-wheel acceptance snapshots.
- [x] Native CI validation confirmed the CoachMark/TeachingTip slice on
      native Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all 75 binding CTests, clean wheels on all three platforms,
      source-package integration,
      acceptance snapshots, Qt 5.15/6.2 C++ regressions, and the final CI Gate.
- [x] Bind `Toast` and `ToolTip` with all nested enums, native properties and
      signals, direct and managed presentation, keyed update/eviction,
      target-owned tooltip attachment, caller-owned theme-source/QAction
      retention, and explicit removal of animation/theme implementation hooks.
      The Toast facade records `anchor.window()` as the real Python parent
      while preserving the original child anchor for local-theme inheritance.
- [x] On matched macOS Qt/PySide6 6.9.3, pass all 23 automated native
      Toast/ToolTip tests (one manual VisualCheck skipped), 189 binding tests,
      157 generated-contract verifier tests, and all 77 PySide CTests. Also
      rebuild and test the extracted source package, pass a newly created
      clean-venv wheel smoke and `pip check`, and produce byte-identical
      build-tree/installed-wheel status-overlay snapshots.
- [x] Native CI validation confirmed the Status & Info completion slice on
      native Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all 77 binding CTests, clean wheels on all three platforms,
      source-package integration, acceptance snapshots, Qt 5.15/6.2 C++
      regressions, and the final CI Gate.
- [x] Bind `CommandBar`, final `CommandBarFlyout`, and `FluentMenuBar` with
      nested enums, properties, signals, primary/secondary command mutation,
      same-window flyout invocation, and Python-callable `QWidget::addAction`
      overloads that also work with the Qt 6.2 Shiboken generator.
- [x] Keep caller-owned `QAction` objects borrowed while retaining their
      Python wrappers in the command section that actually owns each action;
      synchronize add, insert, move, remove, clear, replacement, external
      deletion, and shared-action lifetimes without transferring QObject
      parentage or Shiboken ownership. Retain flyout invocation widgets until
      replacement, explicit clearing, or host teardown.
- [x] On matched macOS Qt/PySide6 6.9.3, pass 23 focused native tests (four
      manual/interactive checks skipped), 194 binding tests, 169 generated-
      contract verifier tests, and all 79 PySide CTests. Also rebuild and test
      the extracted source package, pass a clean-venv wheel smoke and
      `pip check`, and produce byte-identical build-tree/installed-wheel
      command-surface snapshots (`ba5b29a1f29575198bbc086204235cb268c7d91bf3372d0cd277eaabd2b3767e`).
- [x] Native CI validation confirmed the command-surface slice on native
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, including generated
      contracts, all 79 binding CTests, clean wheels on all three platforms,
      source-package integration, acceptance snapshots, Qt 5.15/6.2 C++
      regressions, and the final CI Gate.
- [x] Bind `TitleBar` and the safe existing `Window` chrome/backdrop surface.
      Keep `Window.titleBar()` Qt-owned, make TitleBar content replacement
      release the old Python child, remove internal theme-refresh hooks, and
      preserve the safe two-argument `Window.nativeEvent()` contract.
- [x] Cover TitleBar properties/signals/subclassing, Window/TitleBar content
      lifecycle, 25-cycle GC stress, generated parent/ownership contracts,
      manifest exports, clean-wheel smoke, and a visible acceptance window.
      Locally, all 199 binding tests, 172 verifier tests, and 82 PySide CTests
      pass in both the working tree and a freshly extracted source package.
- [x] Validate the acceptance window locally with the native Cocoa plugin and
      matched Qt/PySide6 6.9.3. Solid resolved to the opaque backend; Mica and
      Acrylic both resolved to native macOS vibrancy with a composited surface.
      A fresh-venv wheel smoke and `pip check` pass; build-tree and installed-
      wheel offscreen snapshots are byte-identical, and the native JSON report
      records both platform materials before saving a readable Solid snapshot.
- [x] Native CI validation confirmed generated contracts, all 82 binding
      CTests, clean wheels, and native XCB/Windows/Cocoa acceptance reports on
      Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3. It also passed source-package
      integration, Qt 5.15/6.2 C++ regressions, and the final CI Gate. The Qt
      6.2 report path normalizes legacy Shiboken byte strings to UTF-8 text.
- [x] Validate Windows 11 DWM material application and chrome layout on the
      local physical desktop. Qt 6.9.3 applied both Mica and Acrylic through
      `DwmSystemBackdrop`; the native report and readable Solid snapshot
      confirm custom chrome, reserved caption insets, and resize propagation.
- [x] Review pointer-driven system move and border resize on the local Windows
      11 desktop. A real title-bar drag moved the window origin from `(503,209)`
      to `(623,289)`, and a real bottom-right border drag resized it from
      `914x614` to `814x534` while the published chrome frame updated with it.
- [ ] Review KWin/Wayland compositor materials plus system move/resize on a
      physical Linux desktop.

The automated native acceptance rejects offscreen/minimal plugins and verifies
native handles, chrome/content ownership, resize propagation, and resolved
backdrop invariants. The local WSLg Wayland and XCB reports also confirm the
painted fallback without claiming compositor blur. Windows DWM material quality
and pointer-driven system move/resize are now covered; physical KWin blur and
pointer behavior remain the sole M5 desktop acceptance item.

## M6 — Release-grade Python distribution

- [x] Define and validate the supported CPython/platform/architecture matrix
  in `bindings/pyside6/wheel-matrix.json`. The first release covers x64 and
  ARM64 on Linux, macOS, and Windows; x64 means x86_64/AMD64, not 32-bit x86.
- [x] Pass every first-release wheel lane on its native target. Fast CI keeps
  the Python 3.10 + Qt/PySide/Shiboken 6.2.4 minimum lanes on Linux/Windows
  x64 and the existing macOS ARM64 lane. Full CI adds 6.9.3 release lanes for
  Linux x64/ARM64, Windows x64/ARM64, and macOS x64; Linux ARM64 uses Python
  3.12 because the upstream aarch64 Shiboken runtime requires immortal
  singletons, while the other release lanes use Python 3.11. The existing
  macOS ARM64 lane completes the six-target release set.
- [x] Keep pull-request CI proportional with a tested conservative path
  classifier. Library, binding, Gallery, CMake, resource, and binding-toolchain
  changes still run the PySide6 baseline on all three platforms; native-test
  only and unrelated workflow changes skip it. Push, scheduled, and manually
  dispatched validation continue to run the complete required matrix.
- [x] Generate `_fluentqt.pyi` from Shiboken signatures plus facade `.pyi`
  files, validate them against `api-manifest.json`, include them in clean-wheel
  smoke tests, and run a strict installed-wheel mypy consumer check in CI.
- [x] Package exact PySide/Shiboken dependencies, licenses, and notices, then
  run clean-venv install, import, type-check, native dependency, and binary
  architecture checks in every native lane.
- [x] Define the Linux manylinux build, repair, and `auditwheel` audit policy.
  Release lanes rebuild in the matching PyPA policy image, pin `auditwheel`,
  exclude the separately pinned PySide6/Qt/Shiboken runtime, verify relocatable
  RPATHs and wheel metadata, and emit a hashed JSON audit report; see
  `bindings/pyside6/MANYLINUX.md`.
- [x] Complete full CI evidence for x86_64 and AArch64 manylinux container
  builds, repair, `auditwheel` audit, clean installation, and artifact upload;
  the container path is no longer an M6 TODO.
- [x] Establish versioning and deprecation rules for the Python API. The package
  exposes `__version__` and `__api_version__`; manifest schema, SemVer,
  replacement, and later-major removal rules are enforced by the stub gate and
  unit tests; see `bindings/pyside6/API_COMPATIBILITY.md`.
- [x] Ship a standalone pure-Python `FluentQt-Gallery` distribution as a
  public-package integration consumer. It pins the exact matching `FluentQt`
  version, while the reusable UILib wheel excludes Gallery code and artwork.
  A generated contract treats the C++ catalogs as canonical and locks
  12 categories, 88 ordered routes, 67 component pages, and all 199 native
  SampleCards. The routed components plus 20 embedded support types cover all
  87 manifest types. Every card builds its live public-API preview from exact
  executable `preview_source`, while the visible Source code block presents a
  concise, canonical-method-aligned Python teaching snippet. Acceptance gates
  execute the exact preview source, compile and semantically compare the visible
  snippet with the C++ contract, and reject fallback previews, Gallery-internal
  imports, preview-parent leakage, or contract drift. The installed
  app mirrors the current C++ visual shell and page archetypes, packages the
  same Home/control artwork, and locks title-bar, search, navigation, Hero,
  responsive-grid, component-section, SampleCard, and snapshot geometry in
  automated tests. The final Windows all-route acceptance regenerates both
  applications at `1440x900`: 21 route pairs are byte-identical, while the 67
  component pairs differ only inside the intentional Python/C++ `Use` API-text
  rectangle. Across 114,048,000 compared pixels, 113,592,073 are identical and
  all 455,927 changed pixels are confined to that rectangle; there are zero
  changed pixels elsewhere. Snapshot capture clears transient hover state so
  that result no longer depends on the host pointer position.
- [ ] After every required matrix lane passes, sign and publish the native
  `FluentQt` wheels plus one `FluentQt-Gallery` `py3-none-any` wheel.

Qt 6.2.4 remains the binding minimum, not the ARM64 wheel build version. The
official PySide 6.2.4 release has no Linux or Windows ARM64 wheels, Linux ARM64
Qt/PySide 6.9.3 binaries require glibc 2.39, and the Windows ARM64 Python tool
cache starts at CPython 3.11. Therefore the ARM64 release lanes use 6.9.3,
Linux uses `ubuntu-24.04-arm` with CPython 3.12, and the low-version x64 lanes
remain separate. A configure-time reference-ownership probe rejects the unsafe
Linux ARM64 Shiboken 6.9.3 plus pre-3.12 CPython combination.

Native Linux smoke artifacts retain `linux_*` tags. The release workflow now
rebuilds inside the architecture-specific policy image and uploads only the
repaired manylinux wheel plus its audit report; both x86_64 and AArch64
container paths have full CI evidence. The current Foundation-authoring changes
still require a fresh required-matrix run. No wheel is published until that
regression passes and the signing/upload gate is explicitly enabled.

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
2. **Feature complete**: M2 through M5, including M4.5, are complete. Every
   planned leaf widget, hosted widget, model/navigation surface, Foundation
   authoring capability, overlay, and native-window contract has a Python API
   plus applicable lifecycle and interaction tests. Any unbound public C++
   component has an explicit documented reason.
3. **Release complete**: M6 is complete. The supported CPython, OS, and
   architecture matrix, type stubs, API compatibility/deprecation policy, and
   clean-environment wheels are published.

M0 through M4 are complete. Feature completeness now waits on M4.5's Qt 6.2
and three-platform wheel CI regression plus M5's physical Linux KWin/Wayland
compositor review. Release completeness also requires the current M6 matrix
regression, signing, and formal publication.

FluentQt calls Python support complete and release-ready only at the third
level. This excludes PySide2 and Qt 5 Python bindings and does not require
rewriting C++ painting in Python; Python uses the same native FluentQt widgets.

## How to validate the result

- **Automated contracts**: run `ctest --test-dir build/pyside6 -L '^pyside$'
  --output-on-failure` for properties, signals, subclassing, ownership,
  generated code, and the acceptance window.
- **Python Gallery**: install the matching `FluentQt` and `FluentQt-Gallery`
  wheels, then run `python -m fluentqt_gallery` for interactive review. Add
  `--verify-catalog --walk-routes --snapshot
  <png> --report <json>` for deterministic manifest, route, and render evidence.
- **Interactive review**: run `examples/compatibility_showcase.py`; switch
  Light/Dark, Fluent/Material/macOS, and accent colors, drag the Slider, hold
  RepeatButton, and inspect text, dividers, progress controls, and signal
  feedback.
- **Model boundary**: run `examples/list_view_model.py`,
  `examples/flow_view_model.py`, `examples/grid_view_model.py`, and
  `examples/tree_view_model.py`; exercise flat-list notifications,
  variable-size wrapping, grid selection/reordering, and hierarchical
  expansion/selection/reordering while reviewing Python delegate rendering.
- **Hosted pages**: run `examples/flip_view_ownership.py`; navigate between
  pages, remove each ownership mode, and use `takePage()` to verify deletion,
  detachment, original-parent restoration, and explicit transfer. Run
  `examples/navigation_view_ownership.py` to exercise the C++-owned content
  host, header/main/footer chrome policies, and Left/Top responsive layouts.
- **Same-window overlay**: run `examples/drawer_view_ownership.py` to inspect
  the right-side drawer, dim scrim, outside-press close, open/close signals,
  and release behavior for all three content ownership modes. Run
  `examples/popup_overlay.py` to inspect Popup anchoring, focus return,
  Escape/outside dismissal, and passthrough behavior. Run
  `examples/flyout_overlay.py` to inspect Top/Bottom/Left/Right/Auto placement,
  clamping, light dismiss, and anchor lifetime. Run
  `examples/combo_box_dropdown.py` to inspect Python-model rows, editable text,
  keyboard selection, Escape dismissal, and the native same-window dropdown.
  Run `examples/date_time_pickers.py` to inspect Python-provided `QDate`,
  `QTime`, locale, field-format enums, value signals, and all three native
  same-window picker popups. Run `examples/auto_suggest_box.py` to inspect
  Python string-list suggestions, typed reason/query signals, keyboard preview,
  focus retention, and the native same-window suggestion Flyout. Run
  `examples/command_surfaces.py` to inspect FluentMenuBar typography,
  CommandBar primary/secondary commands, a same-window CommandBarFlyout, and
  shared caller-owned QAction behavior.
  Pass `--snapshot <png>` to an example to save a review artifact.
- **Navigation values**: run `examples/breadcrumb_navigation.py` to verify
  Python `BreadcrumbItem` metadata, activation signals, full-path rendering,
  and narrow middle-overflow behavior. Run
  `examples/selector_pivot_navigation.py` to verify caller-owned page
  composition, metadata selection, Pivot filtering, and MoreButton overflow.
- **Review artifact**: pass `--snapshot <png>` to the showcase. This mode can
  also run with `QT_QPA_PLATFORM=offscreen`.
- **Installation proof**: install the wheel into a fresh virtual environment
  and run `tests/test_wheel_smoke.py` to prove the process is not borrowing the
  source tree or loading a second Qt.
- **Native windows**: run `examples/window_chrome.py --verify-native
  --snapshot <png> --report <json>` with Cocoa, Windows, XCB, or Wayland. Add
  `--require-platform-backdrop` where native Mica/Acrylic/vibrancy/compositor
  support is expected. Offscreen snapshots remain layout-only evidence.

## Historical validation record

The entries below describe the contracts exercised while the binding surface
was developed. Their individual workflow-dispatch runs are intentionally
pruned after history consolidation; the retained post-rewrite full CI run is
the current branch-level evidence source.
These entries are point-in-time snapshots: their API counts and “still
remaining” statements are not current status. Use the status table, coverage
ledger, and highest-numbered record for the current state.

1. Treat `Card`/`Expander` as the second completed M3 slice after native
   Linux/Windows Qt 6.2.4 native CI validation.
2. Treat the `Avatar`/`RatingControl`/`ScrollBar` leaf slice as complete after
   native Linux and Windows Qt 6.2.4 native CI validation.
3. Treat the `PipsPager` slice as complete after native Linux and Windows
   Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
4. Treat the `InfoBar` ownership slice as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
5. Treat the `TextEdit` leaf slice as complete after native Linux and Windows
   Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
6. Treat the `CompoundButton` leaf slice as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
7. Treat the `FontIcon` foundation leaf as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
8. Treat the `ColorPicker` leaf slice as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
9. Treat the `CalendarView` leaf slice as complete after native Linux and
   Windows Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
10. Treat the `AnnotatedScrollBar` value-type and borrowed-link slice as
    complete after native Linux and Windows Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
11. Treat the `Accordion` ownership slice as complete after native Linux and
    Windows Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
12. Treat the `StackView` ownership/navigation slice as complete after native
    Linux/Windows Qt 6.2.4 plus Qt 5.15 C++ native CI validation.
13. Treat the `TabView` metadata/navigation slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, and Qt 5.15 C++ native CI validation.
14. Treat the `ListView` model/delegate slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, clean wheels on
    all three platforms, and the final CI Gate passed in native CI validation.
15. Treat the `GridView` model/delegate/reorder slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, clean wheels on
    all three platforms, acceptance snapshots, and the final CI Gate passed in
    native CI validation.
16. Treat the `TreeView` hierarchy/model/delegate/reorder slice as complete
    after native Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++,
    source-package integration, clean wheels on all three platforms,
    acceptance snapshots, and the final CI Gate passed in native CI validation.
17. Treat the `DrawerView` overlay/ownership slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, clean wheels on all
    three platforms, source-package integration, acceptance snapshots, and the
    final CI Gate all passed in native CI validation.
18. Treat the `Breadcrumb` metadata/navigation slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, clean-wheel smoke, acceptance
    snapshots, source-package integration, and the final CI Gate passed in CI
    native CI validation.
19. Treat the `SelectorBar`/`Pivot` metadata-navigation slice as complete after
    native Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, clean wheels,
    source-package integration, acceptance snapshots, Qt 5.15/6.2 C++, and the
    final CI Gate passed in native CI validation.
20. Treat the `FlowView` model/delegate slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, clean wheels, source-package
    integration, acceptance snapshots, Qt 5.15/6.2 C++, and the final CI Gate
    passed in native CI validation.
21. Treat the `FlipView` ownership/navigation slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, clean wheels,
    source-package integration, acceptance snapshots, and the final CI Gate
    passed in native CI validation.
22. Treat the `SplitView` ownership slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, clean wheels,
    source-package integration, acceptance snapshots, and the final CI Gate
    passed together in native CI validation.
23. Treat the `NavigationView`/`StackContentHost` page-and-chrome ownership
    slice as complete after native Linux/Windows Qt 6.2.4, macOS Qt 6.9.3,
    Qt 5.15/6.2 C++, all 54 binding CTests, clean wheels, source-package
    integration, acceptance snapshots, and the final CI Gate passed together
    in native CI validation.
24. Treat the `Popup` same-window overlay and QWidget-dependency slice as
    complete after native Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2
    C++, generated contracts, binding tests, clean wheels on all three
    platforms, source-package integration, acceptance snapshots, and the
    final CI Gate passed together in native CI validation.
25. Treat the `Flyout` placement and caller-owned-anchor slice as complete
    after native Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++,
    generated contracts, binding tests, clean wheels on all three platforms,
    source-package integration, acceptance snapshots, and the final CI Gate
    passed together in native CI validation.
26. Treat the `Dialog`/`ContentDialog` same-window modality, result, and hosted
    content ownership slice as complete after native Linux/Windows Qt 6.2.4,
    macOS Qt 6.9.3, Qt 5.15/6.2 C++, generated contracts, all 65 binding
    CTests, clean wheels on all three platforms, source-package integration,
    acceptance snapshots, and the final CI Gate passed together in native CI validation.
27. Treat the `ComboBox` model/editor ownership and same-window dropdown slice
    as complete after native Linux/Windows Qt 6.2.4, macOS Qt 6.9.3,
    Qt 5.15/6.2 C++, generated contracts, all 67 binding CTests, clean wheels
    on all three platforms, source-package integration, acceptance snapshots,
    and the final CI Gate passed together in native CI validation.
28. Treat the menu-button and Fluent-menu slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, generated
    contracts, all 69 binding CTests, clean wheels on all three platforms,
    source-package integration, acceptance snapshots, and the final CI Gate
    passed together in native CI validation.
29. Treat the native date/time picker and popup-lifecycle slice as complete
    after native Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++,
    generated contracts, all 71 binding CTests, clean wheels on all three
    platforms, source-package integration, acceptance snapshots, and the
    final CI Gate passed together in native CI validation.
30. Treat the AutoSuggestBox string-list/signal and same-window suggestion
    Flyout slice as complete after native Linux/Windows Qt 6.2.4, macOS
    Qt 6.9.3, Qt 5.15/6.2 C++, generated contracts, all 73 binding CTests,
    clean wheels on all three platforms, source-package integration,
    acceptance snapshots, and the final CI Gate passed together in native CI validation.
31. Treat the CoachMark/TeachingTip target-retention, same-window guidance
    surface, content-host, and semantic-close-reason slice as complete after
    native Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, generated
    contracts, all 75 binding CTests, clean wheels on all three platforms,
    source-package integration, acceptance snapshots, and the final CI Gate
    passed together in native CI validation.
32. Treat the Status & Info component set, including Toast/ToolTip overlay
    lifetime and borrowed dependency handling, as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, generated
    contracts, all 77 binding CTests, clean wheels on all three platforms,
    source-package integration, acceptance snapshots, and the final CI Gate
    passed together in native CI validation.
33. Treat the `CommandBar`/`CommandBarFlyout`/`FluentMenuBar` borrowed-action
    and same-window command-surface slice as complete after native
    Linux/Windows Qt 6.2.4, macOS Qt 6.9.3, Qt 5.15/6.2 C++, generated
    contracts, all 79 binding CTests, clean wheels on all three platforms,
    source-package integration, acceptance snapshots, and the final CI Gate
    passed together in native CI validation.
34. Treat the automated `Window`/`TitleBar` API, ownership, backdrop-state, and
    native platform-plugin slice as complete after Linux/Windows Qt 6.2.4,
    macOS Qt 6.9.3, generated contracts, all 82 binding CTests, clean wheels,
    native XCB/Windows/Cocoa reports, source-package integration, Qt 5.15/6.2
    C++ regressions, and the final CI Gate passed in native CI validation. Physical
    Windows 11 DWM and Linux KWin/Wayland visual/interaction review remains.
35. Treat the first M6 typing/API guard slice as complete after the generated
    14-file stub set, the 75-class/11-enum/14-function manifest gate, all 84
    binding CTests, strict installed-wheel mypy checks, clean wheels on native
    Linux/Windows Qt 6.2.4 and macOS Qt 6.9.3, source-package integration,
    Qt 5.15/6.2 C++ regressions, and the final CI Gate passed together in native CI validation. The wider wheel matrix, compatibility policy, signing, and
    publication work remain in M6.
36. Treat the M6 native wheel-matrix and minimum-compatibility slice as complete
    after native CI validation passed before history consolidation with all six
    Qt/PySide/Shiboken 6.9.3 release wheels for x64 and ARM64 on Linux, macOS,
    and Windows; the current Linux ARM64 lane uses Python 3.12 and the other
    release lanes use Python 3.11. The Python 3.10 + 6.2.4 minimum lanes on Linux/Windows
    x64; all binding CTests; strict mypy; clean installs; native-window smoke;
    Qt 5.15/6.2 C++ regressions; sanitizers; CI Gate; and Release ready. The
    run also verifies that CommandBar's queued focus rebuild no longer
    dereferences a destroyed borrowed QAction. Linux manylinux repair/audit,
    Python API versioning rules, signing, and formal publication remain in M6.
37. Synchronize milestone state after the public-API ledger, all 84 local
    PySide CTests, and native CI validation confirm the recorded contracts:
    M0 through M4 are complete. Keep M5 in progress for physical DWM/KWin/
    Wayland review and M6 in progress for manylinux, API versioning, signing,
    and formal publication.
38. Implement the M6 API-governance and manylinux-policy slices. The package
    now publishes full and major/minor version variables, the manifest carries
    a machine-checked deprecation ledger, and Linux release lanes define
    architecture-specific PyPA policy images, pinned repair inputs, external
    PySide6/Qt/Shiboken boundaries, relocatable RPATH checks, and JSON audit
    evidence. Local Windows Qt 6.9.3 and Linux Qt 6.2.4 builds each passed all
    86 PySide CTests, clean-wheel smoke, `pip check`, strict mypy, and runtime
    dependency resolution. Windows 11 DWM applied native Mica and Acrylic;
    WSLg Wayland/XCB passed native chrome with the expected painted fallback.
    M5 remains open for pointer-driven Windows and physical KWin review. M6
    remains open because Docker was unavailable locally, so the new manylinux
    container lanes still need full CI proof before signing and publication.
39. Treat the wheel-installed Python Gallery integration slice as complete.
    A generated contract now makes the C++ Gallery catalogs the source of truth
    for the same 12 categories, 88 ordered routes, 67 component pages, and all
    199 native SampleCards. The Python app covers all 77 manifest types through
    67 routed components and 10 embedded support types; every card constructs a
    live public-API preview and executes the identical displayed Python source,
    while any generic fallback is rejected. Completion also requires the native
    visual shell rather than the earlier catalog-only prototype: the Python app
    now uses the same Home/control artwork and mirrors the title bar, centered
    search, responsive side navigation, Home Hero and grids, component sections,
    reference card, live SampleCards, and Source code expander. Geometry tests
    reject the old raw TreeView/button-list layout, and PNG generation composites
    transparent DWM/Mica pixels over the Fluent fallback canvas. Local Windows
    Qt/PySide 6.9.3 and Linux Qt/PySide 6.2.4 source builds each passed all 89
    PySide CTests; clean wheels passed smoke, `pip check`, strict mypy, 88/88
    installed-wheel route walks, 199/199 native-equivalent samples with zero
    fallbacks, and packaged-asset checks for all 74 control images and 7 Home
    tiles. Windows `windows` and WSLg `wayland` installed-wheel snapshots were
    also reviewed. The Qt 6.2 walker yields between routes to avoid event-loop
    starvation. This evidence does not replace M5's physical KWin/Wayland
    desktop review or M6's remaining container-CI, signing, and publication work.
40. Close the local Windows pointer-acceptance and Gallery-parity gaps.
    Real Windows 11 pointer input moved the custom-chrome window by `120x80`
    pixels and resized its bottom-right border from `914x614` to `814x534`;
    DWM Mica/Acrylic remained native. Final C++ and Python all-route captures
    regenerated 88 snapshots per application at `1440x900`. The current honest
    language-aware result is 21 byte-identical route pairs plus 67 component
    pairs whose changed pixels are confined to the Python/C++ `Use` API text;
    all pixels outside that rectangle are identical. The capture helpers now
    clear transient hover state before rendering.
    Windows Qt/PySide 6.9.3 and Linux Qt/PySide 6.2.4 each pass the 32-test
    Gallery suite, clean installed-wheel smoke, and `pip check`; Linux also
    passes all 201 binding tests plus the focused TreeView (92 pass/1 visual
    skip), ProgressRing (11/1), and ScrollBar (4/1) C++ suites. A native WSLg
    Wayland report confirms custom interactive chrome and honest painted
    Mica/Acrylic fallback. M5 remains open only for physical KWin/Wayland
    compositor and pointer review; M6 still requires container-CI evidence,
    signing, and publication authorization.
41. Correct the Python Gallery startup, API-reference, sample-source, and
    theme-refresh regressions. The splash now covers the whole Gallery client
    area instead of only the right content pane. Component `Use` cards expose
    Python import/type/module values with no C++ headers or CMake target, and
    all 199 live previews are constructed by their displayed Python source;
    ListView source now includes the same avatars, delegate, model ordering,
    and selection state as its preview. Icon-browser theme refresh no longer
    calls the intentionally unbound `ToolTip::onThemeUpdated()` hook. Shared
    non-code copy is language-neutral, so the final 88-route Windows comparison
    has 21 fully identical frames and 67 frames differing only in the `Use`
    text rectangle, with zero changed pixels elsewhere. Windows Qt/PySide
    6.9.3 and Linux 6.2.4 both pass the 33-test Gallery suite; freshly rebuilt
    and force-installed wheels pass the focused source/startup/theme cycle,
    complete wheel smoke, and `pip check` in both environments.
42. Close the Python Gallery lifecycle, teaching-source, settings-latency, and
    Top-to-Left navigation-state gaps. Interactive launches now use a per-user
    `QLockFile`/`QLocalServer` singleton; the lock and activation handshake run
    before the 88-route/199-sample UI graph is imported, queued activations are
    restored once the primary controller is ready, and automated verification
    modes remain intentionally independent. A real Windows second launch exits
    successfully in 1.067 seconds and leaves exactly one Gallery window.
    Superseding the exact-display wording in records 39 and 41, every card now
    keeps an exact executable `preview_source` for the pixel-faithful preview
    and a separate concise public-API `source` for the visible teaching block.
    All 199 exact sources execute, all 199 visible snippets compile and retain
    the canonical C++ API operations, and visible Python totals 2,648 lines
    versus 2,369 C++ lines (1.118x), with a 68-line maximum and no Gallery
    imports or `gallery_parent` leakage. Theme/style/accent changes now refresh
    only shell chrome and the active route, then lazily refresh hidden pages on
    navigation: after all 88 pages are built, measured theme switching falls
    from 1,460.1 ms to 219.0 ms and style switching from 546.5 ms to 28.7 ms.
    Left-pane compact state is released only after the animated pane has fully
    reopened, so a real Windows Top-to-Left cycle restores every main/footer
    navigation item. Final source and force-installed-wheel validation passes
    on Windows Qt/PySide 6.9.3 and Linux 6.2.4, including the Gallery suite,
    wheel smoke, `pip check`, and 88/88 routes with 199/199 previews and zero
    failures. M5 physical KWin/Wayland review and M6 container-CI, signing, and
    publication remain open.
43. Full CI run `30932469204` passed at commit `2d452c7`, including the
    six-target release matrix, both x86_64 and AArch64 manylinux container
    build/repair/`auditwheel`/clean-install/artifact-upload paths, CI Gate, and
    Release ready. This supersedes the provisional “container CI evidence
    remains” wording in records 38–42. M6 now needs required-matrix regression
    for subsequent changes, followed by signing and formal publication
    authorization.
44. Decouple the Python Gallery from the reusable UILib distribution. The
    source package moves to `bindings/pyside6/gallery/src/fluentqt_gallery`;
    the core `FluentQt` wheel no longer contains Gallery modules or artwork.
    A separate `FluentQt-Gallery` `py3-none-any` wheel pins the exact matching
    core version, launches with `python -m fluentqt_gallery`, and has its own
    clean-wheel smoke. Baseline and release CI install both distributions and
    test their composition, while manylinux repair remains scoped to the native
    core extension.
