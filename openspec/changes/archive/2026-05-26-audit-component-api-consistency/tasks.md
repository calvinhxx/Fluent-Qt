## 1. Audit Baseline

- [x] 1.1 Inventory public component headers under `src/view/**` and map each component to its focused test file and OpenSpec spec when present.
- [x] 1.2 Build a consistency checklist covering inheritance, ownership, Qt properties, getter/setter/signal naming, repeated setter behavior, nullable values, open state, selection/current/activated naming, tests, and VisualCheck expectations.
- [x] 1.3 Create a durable audit report document with finding severity, category, component path, rationale, recommended action, and follow-up marker.
- [x] 1.4 Mark intentional deviations separately when existing specs or architecture justify them.

## 2. Public API Review

- [x] 2.1 Review button-like entry components, including DatePicker, TimePicker, CalendarDatePicker, DropDownButton, SplitButton, and ToggleSplitButton, for base-class and open-state consistency.
- [x] 2.2 Review collection components, including ListView, GridView, FlowView, TreeView, DrawerView, FlipView, SplitView, and StackView, for selection, current item, model/delegate ownership, scrolling, and reorder API consistency.
- [x] 2.3 Review navigation components, including Breadcrumb, NavigationView, Pivot, SelectorBar, StackContentHost, and TabView, for caller-owned composition boundaries and page/selection naming consistency.
- [x] 2.4 Review popup and overlay components, including Popup, Flyout, Dialog, ContentDialog, and TeachingTip, for open/close state, light-dismiss, and ownership naming consistency.
- [x] 2.5 Review date/time, text field, status/info, scrolling, and windowing components for nullable value, valueChanged, open state, and theme update consistency.
- [x] 2.6 Compare audit findings against relevant OpenSpec specs and record code/spec/test mismatches.

## 3. Low-Risk Consistency Fixes

- [x] 3.1 Apply compatible documentation or header-comment fixes for clear conventions without removing public APIs.
- [x] 3.2 Add compatibility aliases only when a naming inconsistency can be corrected without breaking current callers.
- [x] 3.3 Add missing repeated-setter no-op handling only where tests or implementation show duplicate signal emission.
- [x] 3.4 Add or correct inheritance assertions in focused tests where the public contract depends on a Qt or project base class.
- [x] 3.5 Add nullable-value tests for audited components that use invalid `QDate`, `QTime`, or equivalent empty values.
- [x] 3.6 Avoid broad visual rewrites; if a VisualCheck is touched, preserve `SKIP_VISUAL_TEST` and follow `.codex/skills/qt-ut-conventions/SKILL.md`.

## 4. Documentation

- [x] 4.1 Add a component API convention checklist in a durable repository skill location.
- [x] 4.2 Add the detailed audit report with completed findings, intentional deviations, applied fixes, and deferred follow-up proposals.
- [x] 4.3 Update `readme.md` to link to the convention checklist and audit report.
- [x] 4.4 Document any deferred breaking migrations as explicit follow-up change names or recommendations.

## 5. Tests And Validation

- [x] 5.1 Build focused targets for every component test modified by this change with `cmake --build --preset vcpkg-osx --target <target>`.
- [x] 5.2 Run each modified focused test binary with `SKIP_VISUAL_TEST=1`.
- [x] 5.3 Run a focused `ctest --preset vcpkg-osx -R "<affected component filters>" --output-on-failure` sweep for touched categories.
- [x] 5.4 Run `openspec validate audit-component-api-consistency --strict`.
- [x] 5.5 If broad docs-only audit findings are produced without code changes for a category, record why no automated test was needed.
