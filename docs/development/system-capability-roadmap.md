# System Capability Roadmap

## Purpose

This roadmap separates reusable widget behavior from application policy and
operating-system integration. Each capability phase is intended to be
independently reviewable and reversible. A later capability phase does not
begin until the previous phase's API, behavior, tests, and scope have been
reviewed.

Naming note: capability phases below are **not** the same numbering as the
historical UILib contract phases in
[component-contract-baseline.md](component-contract-baseline.md). Prefer
`Capability Phase N` when referring to this document.

The current component inventory already includes application-window surfaces
such as `InfoBar`, `InfoBadge`, `Avatar`, and `Toast`. This roadmap is limited
to reusable cross-component command behavior and in-window notification
lifecycle rather than another set of visually equivalent notification widgets.

## Current Status

| Capability phase | Status |
|---|---|
| 1 Shared text editing context menu | Complete on `release/1.5.x`; Qt 5.15/Qt 6 editing-menu contracts pass |
| 2 Editing command facade and router | Complete; review gate accepted on 2026-07-28 |
| 3 CommandBar / CommandBarFlyout | Complete; final cross-platform and Computer Use review gate accepted on 2026-07-30 |
| 4 Notification accessibility and lifecycle | Complete; Toast lifecycle and InfoBadge accessibility gate accepted on 2026-07-30 |

All capability phases in this roadmap are complete. Further system-level work
requires a new scoped proposal rather than extending this closed roadmap.

## Architecture Boundary

| Layer | Owns | Does not own |
|---|---|---|
| Component library | Editing menus, command presentation, in-window notification visuals, accessibility and lifecycle contracts | Business unread state, routing, notification policy, OS delivery |
| Application | Current editing target, notification store, foreground/background policy, localized visible text, optional platform integration | Reusable component rendering and command semantics |

The core `FluentQt` target remains a cross-platform Qt Widgets library. Private
implementation headers under `*/private/` stay out of
[FluentQtInstallHeaders.cmake](../../cmake/FluentQtInstallHeaders.cmake).

## Capability Phase 1: Shared Text Editing Context Menu

Scope:

- Extract the existing `TextEdit` Fluent context-menu implementation into
  private shared infrastructure under `menus_toolbars/private/`.
- Preserve Qt's standard action ownership, localization, shortcuts, enabled
  state, and Undo/Redo dispatch.
- Apply the shared menu to `LineEdit`; derived inputs (`PasswordBox`,
  `NumberBox`, `AutoSuggestBox`) inherit through `LineEdit` without requiring a
  new public command API.
- Keep the helper private. Add no new public callable API; the existing
  installed `LineEdit` declaration only gains the protected event override
  needed to implement the behavior.

Non-goals for this phase:

- No public editing-command facade or window-scoped actions.
- No CommandBar surfaces.
- No PasswordBox-specific redact policy beyond what Qt's standard menu already
  provides for echo mode.
- No install-package exposure of the private helper.

Acceptance:

- `TextEdit` behavior and object names remain compatible
  (`FluentTextEdit.ContextMenu`).
- `LineEdit` shows a `FluentMenu` for standard editing actions
  (`FluentLineEdit.ContextMenu`).
- Undo and Redo work from both keyboard shortcuts and the Fluent menu.
- Copy, Delete, and Select All retain recognizable glyphs.
- Focused `test_line_edit` and `test_text_edit` targets pass.

Review gate:

- Review the private ownership strategy, action proxying, visual density,
  object naming, and Qt 5.15/Qt 6 behavior before Capability Phase 2.
- Keep an automated contract proving that at least one `LineEdit` subclass
  still opens the Fluent menu (inheritance path), without expanding
  PasswordBox policy.

Validation completed on 2026-07-28:

- Windows Qt 6.9.3 focused `test_line_edit`, `test_text_edit`, and
  `test_password_box` targets built and their non-manual tests passed.
- Linux Qt 5.15.2 built the same three targets; the five context-menu and
  Undo/Redo behavior tests passed, including hidden-PasswordBox capability.

## Capability Phase 2: Editing Command Facade and Router

The accepted, implemented, and reviewed
[Editing Command Router API Proposal](editing-command-router-proposal.md) is
complete. It exposes no internal Qt widget pointers. The Capability Phase 2
review gate was accepted on 2026-07-28, allowing Capability Phase 3 contract
work to begin.

Delivered responsibilities:

- Expose semantic editing commands and capability changes without publishing
  `TextEdit`'s private `QTextEdit`.
- Provide window-scoped actions for Undo, Redo, Cut, Copy, Paste, Delete, and
  Select All.
- Track the focused supported editor without stealing shortcuts from unrelated
  widgets or other windows.
- Allow the same actions to populate `MenuBar`, context menus, and future
  command surfaces.
- Apply explicit reduced-capability rules for `PasswordBox`, read-only
  editors, and suggestion/number inputs.

Acceptance covers focus and menu restoration, clipboard changes, read-only
editors, password fields, multiple windows, native key bindings, caller-owned
presentation, scope destruction, and action lifetime. A duplicate router in
one window is hard-rejected without installing shortcuts, and a PasswordBox
context-menu request ends press-and-hold Peek before presenting disabled
Cut/Copy. The 15 focused contracts pass on Windows Qt 6.9.3 and Linux Qt
5.15.2; source and installed-package consumer fixtures also compile the
exported category API.

## Capability Phase 3: CommandBar and CommandBarFlyout

Add visual command presentation only after Capability Phase 2 establishes
command semantics.

The accepted public API and behavior are defined in
[Command Bar API and Behavior Proposal](command-bar-proposal.md). Capability
Phase 3A provides the installed declarations, shared borrowed-action model,
default-compatible Popup focus setting, logging, package probes, and non-visual
contract tests; its review gate was accepted on 2026-07-28. Capability Phase
3B adds the private action presenters, responsive measurement, composite
keyboard focus, deterministic RTL-aware overflow, and a same-window scrollable
overflow surface. Capability Phase 3C completes the contextual flyout
presentation and interaction contract. Capability Phase 3D adds design-language
rendering, accessibility, Gallery examples, EditingCommandRouter integration,
and package-boundary validation. Automated work and focused Windows desktop
review are complete; the final Capability Phase 3 review gate was accepted on
2026-07-30.

Accepted components:

- `CommandBar` with primary commands, secondary overflow, separators, and
  responsive measurement.
- `CommandBarFlyout` using the same-window
  [overlay behavior](../architecture/overlay-behavior.md) contract.
- Direct composition of Phase 2 editing actions, without a second editing
  facade or automatic context-menu replacement.

The proposal defines action ownership, unsupported action kinds, deterministic
overflow order, keyboard navigation, focus restoration, RTL placement,
touch/pointer behavior, accessibility, and behavior under Fluent, Material, and
Cupertino design languages. It includes one default-preserving protected
`Popup` focus-on-open setter so Transient flyouts can avoid even a temporary
focus transfer without changing Popup object layout or adding an
application-facing property. It also splits implementation into four
independently reviewed slices.

Capability Phase 3A validation completed on 2026-07-28:

- Windows Qt 6.9.3 built `test_command_bar` and
  `test_command_bar_flyout`; all 12 non-visual contracts passed.
- Linux Qt 5.15.2 built the same focused targets; all 12 contracts passed.
- The Windows Popup, Flyout, TeachingTip, MenuBar, and
  EditingCommandRouter regression selection reported 0 failures across 77
  discovered tests; 6 existing headless/manual cases were skipped.
- Source-subproject and installed-package consumers compiled and linked both
  public types through `<FluentQt/MenusToolbars.h>`.
- The install tree contains both public headers and excludes
  `CommandActionModel_p` and `TextEditingMenu_p`.
- At the 3A gate, inline rendering, responsive overflow, command presenters,
  full flyout interaction, accessibility adapters, and Gallery samples were
  explicitly deferred to Capability Phases 3B-3D. The 3B items are now
  delivered below.

Capability Phase 3B validation completed on 2026-07-28:

- Windows Qt 6.9.3 and Linux Qt 5.15.2 each pass all 17 focused
  `test_command_bar` contracts, including 9 inline presentation contracts.
- Coverage includes priority plus logical-tail overflow, normalized separators,
  action state synchronization, collapsed labels, RTL visual order, composite
  keyboard focus, focus repair, same-window popup dismissal, exact activation,
  and deletion safety.
- The related Windows CommandBarFlyout, Popup, Flyout, MenuBar, and
  EditingCommandRouter selection reported 0 failures across 69 discovered
  tests; 5 existing local-desktop/manual cases were skipped.
- The equivalent Linux Qt 5 non-desktop selection passed all 63 tests.
- No public API or installed-header allowlist change was needed for 3B;
  `CommandPresenter_p` remains private.
- Capability Phase 3C and 3D work is delivered below; focused desktop review is
  also complete.

Capability Phases 3C and 3D automated validation completed on 2026-07-28:

- `CommandBarFlyout` now implements Standard and Transient modes,
  collapsed/expanded and always-expanded presentation, deterministic responsive
  overflow, scrolling within host bounds, RTL navigation, focus restoration,
  exact activation, and deletion-safe action updates.
- Private accessibility adapters expose toolbar, command, popup-menu, menu-item,
  accelerator, checked, disabled, focus, and More expansion semantics without
  adding installed API.
- `CommandBar` and `CommandBarFlyout` render under Fluent, Material, and
  Cupertino in Light and Dark themes. Focused rendering and accessibility
  contracts run on Qt 6.9.3 and Qt 5.15.2.
- EditingCommandRouter actions can be reused by inline and flyout command
  surfaces without losing the active editor or selection; cross-window command
  surfaces reject those window-scoped actions.
- The current Windows Qt 6.9.3 label selection discovers 98 CommandBar,
  CommandBarFlyout, EditingCommandRouter, Popup, Flyout, and MenuBar tests:
  92 pass, 6 desktop/manual cases skip as expected, and 0 fail.
- Linux Qt 5.15.2 passes all 17 focused CommandBar contracts and all 14
  automated CommandBarFlyout contracts; the guarded Flyout VisualCheck skips as
  intended.
- The Windows Gallery content-page suite reported zero failures across 44
  tests, with 2 expected visual skips. It includes responsive overflow,
  EditingCommandRouter reuse, show-mode examples, and bundled 72 px artwork.
  The dedicated Linux Qt 5 test configuration intentionally disables Gallery.
- Source-subproject and installed-package fixtures compile and link the category
  API. Installed public headers include both command surfaces, while private
  action-model, presenter, and accessibility headers remain absent.
- Focused Computer Use review exercised the CommandBarFlyout VisualCheck and
  Gallery under Fluent Light/Dark, a Material transition, LTR/RTL,
  Standard/Transient/expanded modes, narrow responsive overflow, pointer and
  keyboard paths, and focus preservation.
- That review exposed stale pre-exposure scroll-viewport widths in both
  CommandBarFlyout secondary rows and CommandBar overflow rows. Immediate plus
  deferred post-open reconciliation now prevents clipped captions, and focused
  geometry contracts cover both paths.

Final Capability Phase 3 closure validation completed on 2026-07-30:

- Linux Qt 6.2.4 passes all 79 focused CommandBar, CommandBarFlyout, Toast,
  InfoBadge, and high-DPI tests. The high-DPI matrix covers 110%, 125%, 150%,
  175%, 200%, and 300%, including Gallery acceptance at 125%, 200%, and 300%.
- Linux Qt 5.15.2 passes all 62 focused Capability Phase 3/4 tests.
- Windows Qt 6.9.3 passes all 19 CommandBar contracts and all 15 automated
  CommandBarFlyout contracts; its guarded VisualCheck remains manual.
- ASan/UBSan passes the borrowed-action window-teardown and both related
  Gallery route teardown paths.
- A final Computer Use pass verifies responsive overflow, pointer-to-keyboard
  navigation, Escape dismissal, Transient focus preservation, Standard
  keyboard focus, the primary/secondary divider, and Light/Dark preview
  rendering in the Gallery.
- Physical touch hardware was not available for this run. The existing
  Qt-widget pointer contract remains unchanged; device-specific touch smoke is
  a release-device check, not an open item in this component roadmap.

## Capability Phase 4: Notification Accessibility and Lifecycle

Delivered contracts:

- Newly presented Toast content emits the best supported Qt accessibility
  announcement. Qt 6.8 and newer use `QAccessibleAnnouncementEvent`; older Qt
  baselines use the compatible alert fallback.
- `InfoBadge` exposes a non-focusable static-text accessible object. Value mode
  supplies a numeric value and fallback name, while explicit accessible names
  remain caller-owned. Value, display, visibility, and parent changes notify
  the accessible hierarchy.
- Toast accepts an optional borrowed `QAction` without taking ownership and
  reports stable `Programmatic`, `TimedOut`, `ActionInvoked`, and `Evicted`
  dismissal reasons.
- Optional hover pause preserves the remaining timeout. The default remains
  disabled so existing pointer-through Toast behavior does not change.
- A non-empty update key refreshes a matching managed Toast in place within
  one host and placement, preserves stack order, and resets its timeout.
  Unkeyed managed Toasts continue to stack up to `maximumVisible()`.
- Reentrant action and close paths remain deletion-safe.

Application-visible text and unread-state policy remain caller-owned.
`setMaximumVisible()` remains process-wide startup configuration that affects
subsequent `showToast` calls only.

Validation completed on 2026-07-30:

- Windows Qt 6.9.3 passes all 15 Toast contracts and all 13 automated
  InfoBadge contracts; the guarded InfoBadge VisualCheck skips as intended.
- Linux Qt 6.2.4 and Qt 5.15.2 pass the focused Capability Phase 4 tests, and
  the related six Gallery route/semantic contracts pass on Linux and Windows.
- Headless environments always verify accessible role, name, and value.
  Accessibility-event delivery assertions run when the platform accessibility
  backend is active rather than forcing a synthetic backend state.
- Computer Use verifies the actionable Toast, timeout reason, in-place
  25%-to-50% update with one active keyed Toast, and InfoBadge value/visibility
  examples in the Gallery.
