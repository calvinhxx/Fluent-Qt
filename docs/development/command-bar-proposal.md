# Command Bar API and Behavior Contract

> **Status:** Current contract; the initial command surfaces shipped in FluentQt 1.5.2

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
  groups, separators, and actions with an attached `FluentMenu` hierarchy.
- The surfaces borrow actions and attached menus. They do not reparent, copy,
  or delete them. Python callers keep menus alive with a Qt parent or a strong
  Python reference, just as with `QAction.setMenu()`.
- One action can belong to only one semantic section of a surface. Duplicate
  insertion is a no-op; adding it to the other section moves it deterministically.
- Removing or clearing membership does not destroy the action or change its
  pre-existing QObject parent.
- Text, icon, tooltip, shortcut, enabled, visible, checked, and destroyed
  changes update every presenter.
- Activating a leaf invokes the borrowed action exactly once, including when
  the action or surface is deleted during the trigger. Opening a branch does
  not trigger its action or toggle its checked state.
- `QWidgetAction` and actions without a semantic caption remain unsupported.

`EditingCommandRouter` actions can be inserted directly. Command surfaces do
not add another editing preset or command registry.

## Nested menus

Attach a `FluentMenu` with the existing `QAction::setMenu()` API, or register
its `menuAction()` directly. Build subsequent levels with `FluentMenu::addMenu()`.
Both primary and secondary commands support the same hierarchy, including
primary commands moved into overflow. No separate command-tree API is needed.

```cpp
auto* view = new FluentMenu("View", window);
auto* sort = new FluentMenu("Sort by", view);
auto* order = new FluentMenu("Order", sort);
view->addMenu(sort);
sort->addMenu(order);
order->addAction("Ascending");
order->addAction("Descending");
bar->addPrimaryAction(view->menuAction());
flyout->addSecondaryAction(view->menuAction());
```

The Python equivalent uses the same native objects:

```python
view = fluentqt.FluentMenu("View", window)
sort = fluentqt.FluentMenu("Sort by", view)
order = fluentqt.FluentMenu("Order", sort)
view.addMenu(sort)
sort.addMenu(order)
order.addAction("Ascending")
order.addAction("Descending")
bar.addPrimaryAction(view.menuAction())
flyout.addSecondaryAction(view.menuAction())
```

- Click, Enter, or Space opens a branch. Down opens an inline branch; the
  forward arrow (Right in LTR, Left in RTL) opens an overflow/secondary branch.
- Primary menus open below their command; overflow menus open beside their row.
  FluentMenu retains its normal screen-edge placement and nested hover behavior.
- Escape closes one menu level. From the root menu, Escape or the back arrow
  returns focus to the originating command, leaving its parent surface open.
- Activating a leaf closes the menu cascade and any owning overflow or flyout.
  An inline bar stays visible. An application-directed focus change is preserved.
- Existing QAction state, exclusive groups, and lazy `aboutToShow` population
  remain native Qt behavior. Replacing/removing the originating command or
  hiding its presenter dismisses the borrowed menu. Destroying a menu removes
  the disclosure affordance without destroying the command.
- Opening is asynchronous through the shared popup placement adapter. C++,
  PySide6, and WebAssembly use the same native implementation; WebAssembly
  does not require a blocking menu event loop.

The Gallery cards `command-bar-submenus` and `command-bar-flyout-submenus`
demonstrate View → Sort by → Order with shared exclusive choices. They follow
the ordinary command examples as separate cards; the default examples use
plain actions. A command exposes a submenu only when its QAction has a menu.

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
- Enter and Space activate the focused command or open its attached menu. Down
  opens a primary command menu before considering available overflow.
- Overflow and flyout rows use visual arrow order in LTR and RTL.
- Pointer activation keeps logical focus without a focus outline. Keyboard
  navigation shows the outline, including when the focused command stays the same.
- Escape closes the innermost active menu or transient surface.
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
top-level window. Attached FluentMenu cascades use the existing Qt popup path
and shared anchor mapping. Their events do not dismiss the parent while the
pointer or keyboard is inside the cascade. Closing the parent dismisses its
menus without changing their ownership.

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
- Menu-bearing commands expose `hasPopup`, expanded/collapsed state, and the
  accessible Show Menu action; inline branches have the menu-button role.
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

The maintained `test_command_bar`, `test_command_bar_flyout`, and
`test_command_submenus` targets
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

C++ and Python Gallery tests execute the two nested-menu cards and verify the
selected leaf. The full WebAssembly browser smoke also drives primary,
overflow, and flyout secondary branches with real mouse and keyboard input,
including delayed hover closing and returning one level with Escape.

Interactive VisualCheck review remains responsible for Light/Dark, normal and
narrow widths, RTL, focus, pointer, and open flyout presentation.

## Deferred behavior

This contract excludes:

- arbitrary hosted `QWidgetAction` content;
- docking, dragging, reordering, and vertical command bars;
- a public command-element hierarchy separate from `QAction`;
- automatic replacement of editing context menus;
- sticky flyouts and hover-only command discovery; and
- WinUI AppBar display modes or label-reveal behavior that would require a
  separate inline open-state contract.

Any addition must preserve borrowed action identity, the shared overlay
placement paths, and the existing public defaults.

<!-- docs-nav:bottom:start -->
---
[← Editing Command Router API Contract](editing-command-router-proposal.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:bottom:end -->
