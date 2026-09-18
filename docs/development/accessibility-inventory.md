# Accessibility Inventory

> **Status:** Living reference

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › API, policy, and writing

[← Accessibility Contract](accessibility-contract.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:top:end -->

This is the review view of the machine-checked
[accessibility-inventory.json](accessibility-inventory.json). The inventory is
keyed by the canonical Gallery component catalog, so every public visible
component has one classification before release.

## Current baseline

| Classification | Meaning |
| --- | --- |
| Native | The Qt base class supplies the appropriate role, state, actions, and logical children. |
| Augmented | Native semantics are retained while FluentQt manages additional text or events. |
| Adapter | A private accessible interface represents custom-drawn or composite semantics. |
| Gap | No known inventory contract is open. |
| Not applicable | The component is presentation-only; semantics belong to its containing control. |

All inventoried gaps are closed. `covered` means
there is deterministic repository evidence for the component boundary; it is
not a claim of platform assistive-technology certification.

## Cross-cutting motion and contrast contract

The component classifications above do not change when visual motion is
reduced or disabled. `MotionPolicy::Reduced` keeps only short finite
transitions and stops continuous motion; `MotionPolicy::Disabled` settles
finite transitions at their final state and stops continuous motion. Logical
open, selected, expanded, busy, value, focus, and action state therefore remain
observable without depending on an intermediate animation frame. Shared policy
contracts live in `tests/components/TestMotionPolicy.cpp`; focused component and
Gallery-shell contracts cover active-transition convergence and local animation
switches.

`TextEdit` animates only its wrapper height; the native editor remains the value,
selection, and focus surface. Reduced and Disabled modes preserve the same final
line-count geometry without requiring an intermediate frame. Focused contracts
cover focus retention, motion-policy timing, and final geometry in
`tests/components/textfields/TestTextEditMotion.cpp`; they are not a claim of
platform assistive-technology certification. Input-method preedit leaves the
wrapper height stable until composition commits, so candidate exploration does
not repeatedly reflow the surrounding form.

When the initial editor viewport is shorter than a fallback-font caret, height
growth restores the whole caret instead of retaining Qt's temporary scroll
offset. Later resizing preserves an ordinary user scroll position. These
contracts are covered in `tests/components/textfields/TestTextEdit.cpp`.

`FluentElement::HighContrast` resolves a complete third semantic palette, so
controls continue to expose the same roles, state, actions, and logical child
trees while using opaque high-contrast foreground, background, focus, disabled,
and status colors. Native applications currently select this deterministic
theme explicitly; the WebAssembly host additionally maps browser
`forced-colors` state to it. Palette completeness, contrast ratios, local theme
overrides, and legacy theme-envelope compatibility are covered in
`tests/components/TestHighContrastTheme.cpp`. This repository evidence is not
a claim that native builds import or certify every operating-system custom
contrast scheme.

Custom global and per-element theme token overrides preserve the existing accessible
roles and actions. Typography and per-mode color resolution are covered by
`tests/components/TestThemeOverrides.cpp`; caller-defined colors still require
contrast review. See [custom themes](../design-languages/custom-themes.md).

The [WebAssembly runtime](../../platforms/webassembly/Runtime.cpp) keeps Qt's HTML
accessibility proxies readable and focusable without painting a second control
surface through the transparent desktop canvas. A stylesheet scoped to each
application Qt screen makes only identified proxy nodes transparent; it excludes
the screen-reader enable button and leaves the separate keyboard/IME input,
ARIA state, geometry, and event handlers unchanged. This presentation adaptation
does not repair upstream accessibility geometry updates or replace native
screen-reader and IME acceptance.

## Component contracts

Private adapters preserve caller-owned content and the existing public APIs.
The table lists the semantic boundary and its focused regression source.

Toast sizes short messages to their content and wraps complete titles and messages
within the host. Actions sit below the message, wrap long captions, and retain
native Button keyboard semantics. The optional close button keeps its accessible
name, keyboard focus, and 24-pixel target at the top trailing corner. Semantic
icons, announcements, dismissal reasons, and
reduced-motion behavior remain available in both compact and detailed layouts.

| Components | Contract | Tests |
| --- | --- | --- |
| CalendarView | Day, month, and year tables; locale-aware names; paging, selection, range, focus, and no-op events | [CalendarView](../../tests/components/date_time/TestCalendarView.cpp) |
| Breadcrumb, Pivot, SelectorBar, TabView, PipsPager | Ordered logical items, selection, focus, and press actions without per-item widgets; TabView add/close/reorder and all PipsPager pages | [Navigation](../../tests/components/navigation/TestNavigationAccessibility.cpp) |
| ToggleSwitch, RatingControl, NumberBox, ProgressBar, ProgressRing | Toggle and bounded value actions; NumberBox editable text, selection and invalid state; determinate values and observable busy state | [Values](../../tests/components/TestValueAccessibility.cpp) |
| SplitButton, ToggleSplitButton | Separate primary and menu actions; real menu state; Space, Alt+Down, and F4; menu replacement and destruction | [Split actions](../../tests/components/basicinput/TestSplitButtonAccessibility.cpp) |
| Popup, Flyout, TeachingTip, CoachMark | Pane/help-balloon roles, real child trees, target relations, logical open/modal state, dismissal, focus and announcements | [Transient surfaces](../../tests/components/dialogs_flyouts/TestTransientAccessibility.cpp) |
| ColorPicker, DatePicker, TimePicker, AnnotatedScrollBar, AutoSuggestBox | Color/value controls, pending picker values, authored jump links, autocomplete relations and editor focus | [Complex inputs](../../tests/components/TestComplexInputAccessibility.cpp) |
| DropDownButton, DrawerView, ToolTip | Menu actions, drawer content and focus return, tooltip text/target relations and logical visibility | [Auxiliary surfaces](../../tests/components/TestAuxiliarySurfaceAccessibility.cpp) |
| FlipView, SplitView | Ordered pages and navigation; native pane subtrees and keyboard-operable splitter grips with value bounds | [Collections](../../tests/components/TestCollectionSurfaceAccessibility.cpp) |
| HyperlinkButton, InfoBar, Shimmer | Link/visited state, notification severity and dismissal, loading/busy state independent of animation | [Presentation](../../tests/components/TestSemanticPresentationAccessibility.cpp) |
| SplashScreen | Covered-host input and focus restoration, busy state and progress; cached painting preserves live child controls and motion-policy behavior | [SplashScreen](../../tests/components/status_info/TestSplashScreen.cpp) |
| MultiSelectComboBox | Button-menu root, selected labels, expanded state, popup relation and named trigger/search/list | [Multi-selection](../../tests/components/basicinput/TestMultiSelectComboBox.cpp) |
| ComboBox | Explicit fonts reach the field, editor and dropdown; rows grow with text. The popup list takes focus; arrows stage selection, Enter/Return commits once and Escape cancels with focus return. Disabled items and an unselected placeholder cannot activate | [ComboBox](../../tests/components/basicinput/TestComboBox.cpp) |

`ToggleSwitch` keeps a minimum interactive height of 24 logical pixels.
`visualScale` changes its graphics while the accessible rectangle continues to
cover the whole widget; the default scale retains the 40 × 20 Fluent track.

Each family checks effective-change events and no-op silence. New or changed
visible components need an inventory classification and a focused contract
before release. Native screen-reader acceptance remains a separate gate in the
[technical debt roadmap](technical-debt-roadmap.md#td-4-acceptance-boundary).

## Validation

```bash
python3 tools/quality/validate_accessibility_inventory.py --project-root .
ctest --preset vcpkg-osx -R 'AccessibilityInventory|CalendarViewTest\.Contract_Accessibility|NavigationAccessibilityTest\.Contract_Accessibility|ValueAccessibilityTest\.Contract_Accessibility|SplitButtonAccessibilityTest\.Contract_Accessibility|TransientAccessibilityTest\.Contract_Accessibility|ComplexInputAccessibilityTest\.Contract_Accessibility|AuxiliarySurfaceAccessibilityTest\.Contract_Accessibility|CollectionSurfaceAccessibilityTest\.Contract_Accessibility|SemanticPresentationAccessibilityTest\.Contract_Accessibility' --output-on-failure
```

When Python 3.10+ is available, CTest registers
`AccessibilityInventory.Contract_Complete` in the `ci_fast`, `ci_full`,
`contract`, and `local_full` labels. The validator rejects missing, duplicate,
or unknown component IDs, invalid classifications, missing evidence paths, and
open gaps without a next gate.

## Charts

`ChartView` and its eight dedicated chart subclasses expose a Chart role, a caller-owned name/description, and a raw-data
cursor with value-change events and next/previous actions. Arrow keys navigate
original rows, including rows omitted from the visual projection. Attach a Qt
table view to the same `ChartModel` when a tabular alternative is needed.
The outer focus ring is visible for Tab or keyboard interaction; mouse clicks
hide it while retaining focus and the selected data point.
Readouts reuse a passive `Popup` with `Label` children. Pointer exit and focus
loss dismiss the readout while preserving selection and the accessible value;
keyboard navigation shows it again without moving focus into the popup.
Escape resets an explicit Cartesian X range once; otherwise it propagates to
the parent, preserving dialog close policies. Pie and Donut always propagate
Escape because their rendering ignores X ranges.

<!-- docs-nav:bottom:start -->
---
[← Accessibility Contract](accessibility-contract.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:bottom:end -->
