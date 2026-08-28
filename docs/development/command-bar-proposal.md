# Command Bar API and Behavior Contract

> **Status:** Accepted contract, shipped in FluentQt 1.5.2

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Accepted component contracts

[← Editing Command Router API Contract](editing-command-router-proposal.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:top:end -->

`CommandBar` and `CommandBarFlyout` present caller-owned `QAction` objects
without introducing a second command model:

- `CommandBar` is an inline, responsive primary/secondary command group.
- `CommandBarFlyout` is a same-window contextual surface with collapsed and
  expanded presentation.

The same action may appear in menus, a command bar, and a command flyout while
retaining one semantic identity, enabled state, shortcut, and trigger path.

## Public boundary

Exact declarations live in the installed
[`CommandBar.h`](../../src/components/menus_toolbars/CommandBar.h) and
[`CommandBarFlyout.h`](../../src/components/menus_toolbars/CommandBarFlyout.h)
headers.

| Surface | Public contract |
|---|---|
| `CommandBar` | `QWidget + FluentElement + QMLPlus`; ordered primary and secondary actions; label position; dynamic overflow; overflow open state; optional background |
| `CommandBarFlyout` | `Flyout`; ordered primary and secondary actions; Standard/Transient show mode; expanded and always-expanded state; widget-anchor or local-point invocation |
| Shared action API | Add, insert, remove, clear, and inspect borrowed primary/secondary `QAction` objects |
| Popup integration | One protected default-preserving focus-on-open switch used by Transient flyouts; no new application-facing Popup property |

Inherited `QWidget::addAction()`, `insertAction()`, and `removeAction()`
remain available as primary-command shorthand. Explicit primary and secondary
methods are authoritative when section choice matters.

## Action model and lifetime

- Supported actions are ordinary commands, checkable commands, exclusive action
  groups, and separators.
- The surfaces borrow actions. They do not reparent, copy, or delete them.
- One action can belong to only one semantic section of a surface. Duplicate
  insertion is a no-op; adding it to the other section moves it deterministically.
- Removing or clearing membership does not destroy the action or change its
  pre-existing QObject parent.
- Text, icon, tooltip, shortcut, enabled, visible, checked, and destroyed
  changes update every presenter.
- Triggering a presenter invokes the borrowed action exactly once, including
  when the action or surface is deleted during the trigger.
- `QWidgetAction`, nested `QAction::menu()`, and actions without a semantic
  caption are rejected in the first public contract.

`EditingCommandRouter` actions can be inserted directly. Command surfaces do
not add another editing preset or command registry.

## Inline CommandBar

Primary actions appear inline while secondary actions are available from the
More surface. When dynamic overflow is enabled:

1. measure visible primary presenters with the current font and label mode;
2. reserve the More target when secondary or overflow content exists;
3. move lower-priority actions first, using `QAction::priority()`;
4. break equal-priority ties from the logical tail; and
5. restore actions in the exact reverse order as width returns.

Overflow changes presentation only. It never mutates public primary or
secondary membership.

- Hidden actions consume no space; disabled actions remain visible.
- Separators do not participate in priority and are never leading, trailing, or
  consecutive in a projected section.
- Overflowed primary actions precede explicit secondary actions.
- `overflowedPrimaryActions()` exposes the current presentation snapshot.
- Disabling dynamic overflow keeps primary actions inline and exposes a
  deterministic minimum width instead of silently making commands unreachable.
- Long overflow content scrolls inside the visible card.
- `LabelPosition::Right` and `Collapsed` are persistent visual choices;
  label state does not change command semantics.

RTL reverses visual order and navigation direction while preserving logical
collection order and the same priority/tail algorithm.

## CommandBarFlyout

`CommandBarFlyout` uses the same borrowed primary/secondary action contract.

| State | Behavior |
|---|---|
| Standard | Opens as a focus-taking command surface and participates in focus restoration |
| Transient | Opens without an intermediate focus transfer so the invoking editor or target stays active |
| Collapsed | Shows the primary row and expansion affordance when secondary commands exist |
| Expanded | Shows primary and secondary rows |
| Always expanded | Keeps the effective expanded state true and suppresses collapse |

`showAt()` opens from a widget anchor. `showAtPoint()` interprets the point in
the supplied widget's local coordinates. Both require a valid target in the
same top-level window as the flyout.

- A parentless flyout, null target, or cross-window target does not open.
- Switching between anchor and point invocation replaces the previous source;
  stale tracking connections cannot reposition a later invocation.
- Narrow hosts retain at least one reachable primary command and move the rest
  into expanded presentation.
- Placement clamps the visible card, not its shadow margin, to the owning
  window.
- Changing show mode while closed changes the next open. Changing it while open
  updates effective focus and expansion behavior without a duplicate lifecycle
  transition.

## Keyboard and focus

- The inline bar is one Tab stop. Arrow keys move through visible commands and
  the More target; Home/End move to the first/last eligible item.
- Enter and Space trigger the focused command. Down opens available overflow.
- Overflow and flyout rows use visual arrow order in LTR and RTL.
- Escape closes the active transient surface.
- Focus repairs to the nearest eligible command after hide, disable,
  destruction, or responsive overflow.
- Standard flyouts trap and restore focus according to the overlay contract.
  Transient flyouts initially preserve the current target.
- Invoking a routed editing action keeps the editor target valid through menu
  or flyout focus transfer and restores it unless the application deliberately
  moved focus elsewhere.
- Separate top-level windows keep action routing, focus recovery, and overlays
  independent.

## Overlay behavior

Inline overflow and command flyouts are `Qt::Widget` children of the owning
top-level window. They do not create `Qt::Popup`, `Qt::Tool`, or another
native window.

- Visible-card geometry drives placement and hit testing.
- Anchor move, resize, host resize, clipping, destruction, stacking, animation,
  light dismiss, and focus return follow the
  [overlay contract](../architecture/overlay-behavior.md).
- Outside press closes a non-modal surface and continues to the newly chosen
  background target.
- Theme, layout direction, font, label mode, and available geometry changes
  invalidate measurement without changing semantic action membership.

## Pointer, visual, and accessibility contract

- Command and More targets are at least 40 × 40 logical pixels.
- Pointer and touch activation occurs on eligible release; drag-out cancels.
  Right-button and disabled input do not trigger commands.
- Light/Dark changes use Fluent semantic tokens without changing action state.
- Toolbar, popup, command, separator, checked, disabled, expanded, accelerator,
  and focus semantics are exposed through Qt accessibility.
- A command's accessible name comes from its semantic caption, not an
  implementation object name or icon alone.
- Gallery snippets use installed public APIs and teach the same operations as
  their live previews.

## Diagnostics

The `fluentqt.commandbar` logging category reports actionable contract
violations:

- unsupported, captionless, or mismatched actions;
- the same action registered in both sections before normalization;
- a detectable cross-window action or show target;
- impossible geometry or a missing owning top-level; and
- focus recovery after action destruction.

Ordinary resize, hover, measurement, and action-state updates remain quiet.

## Verification

The maintained `test_command_bar` and `test_command_bar_flyout` targets
cover:

- public defaults, installed headers, Qt 5/Qt 6 compatibility, and private
  implementation boundaries;
- action identity, ownership, live state updates, trigger-time destruction, and
  unsupported inputs;
- responsive priority overflow, separators, narrow hosts, scrolling, and RTL;
- keyboard, focus repair, Standard/Transient behavior, and multi-window
  isolation;
- same-window placement, anchor lifetime, outside press, theme changes, and
  teardown; and
- accessibility plus source-aligned Gallery integration.

Interactive VisualCheck review remains responsible for Light/Dark, normal and
narrow widths, RTL, focus, pointer, and open flyout presentation.

## Deferred behavior

The first public contract intentionally excludes:

- arbitrary hosted `QWidgetAction` content and nested action menus;
- docking, dragging, reordering, and vertical command bars;
- a public command-element hierarchy separate from `QAction`;
- automatic replacement of editing context menus;
- sticky flyouts and hover-only command discovery; and
- WinUI AppBar display modes or label-reveal behavior that would require a
  separate inline open-state contract.

Any addition must preserve borrowed action identity, same-window overlays, and
the existing public defaults.

<!-- docs-nav:bottom:start -->
---
[← Editing Command Router API Contract](editing-command-router-proposal.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:bottom:end -->
