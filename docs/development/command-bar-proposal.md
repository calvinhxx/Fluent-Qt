# Command Bar API and Behavior Proposal

## Status

- Capability track: Phase 3
- Proposal state: Accepted on 2026-07-28
- Implementation state: Capability Phases 3A-3D implemented; automated
  cross-platform validation, sanitizer coverage, high-DPI validation, and the
  final Windows desktop review complete on 2026-07-30
- Public API impact: Installed `CommandBar` and
  `CommandBarFlyout` types, plus one protected `Popup` focus-on-open setter

Capability Phase 2 established `QAction` as the shared semantic-command
boundary. This proposal defines two presentation surfaces on top of that
boundary without introducing a second command model:

- `CommandBar` is an inline, responsive command group with a same-window
  overflow surface.
- `CommandBarFlyout` is an anchored, same-window contextual command surface
  with collapsed and expanded modes.

The decisions at the end of this document were accepted before Capability
Phase 3A began. Implementation has now proceeded through Capability Phase 3D,
including final desktop review through Computer Use. Capability Phase 3 is
closed.

## Local Baseline

The proposal starts from the behavior already present in this repository:

- `FluentMenu` and `FluentMenuBar` already consume ordinary `QAction`
  instances and react to action state changes.
- `Button` and `ToggleButton` provide the reusable visual and accessibility
  foundations for private command presenters.
- `Popup`, `Flyout`, and `OverlayCoordinator` provide top-level attachment,
  anchor tracking, visible-card geometry, light dismiss, and stacking.
- Before Capability Phase 3A, `Popup::open()` took focus unconditionally.
  The accepted default-preserving protected setting now lets a Transient
  `CommandBarFlyout` open without a temporary focus transfer.
- `EditingCommandRouter` owns stable window-scoped editing actions that must be
  reusable without reparenting or copying.
- `FluentMenu` remains a `QMenu`; it must not become the implementation shortcut
  for the new same-window `CommandBarFlyout`.

Consequently, wrapping `QToolBar` or opening a `QMenu` would not satisfy the
Phase 3 overlay and focus contracts. The implementation needs a shared private
action model plus inline and overlay presenters.

## Goals

- Present the same `QAction` in menus, an inline command bar, and a contextual
  command bar flyout.
- Support plain, checkable, exclusive-group, and separator actions without
  duplicating their semantic state.
- Keep every command reachable as the host width changes.
- Make overflow order deterministic in both LTR and RTL layouts.
- Preserve the active editing target while a Phase 2 action is invoked from a
  command surface.
- Provide complete keyboard, pointer, touch, focus-restoration, and
  accessibility contracts.
- Keep Light/Dark and Fluent, Material, and Cupertino presentation differences
  visual only; command identity and behavior remain unchanged.
- Keep the core target dependent only on Qt Widgets.

## Non-goals

- No public `AppBarButton`, `AppBarToggleButton`, `AppBarSeparator`, or custom
  command-element hierarchy in the first version.
- No `QWidgetAction`, arbitrary hosted content, toolbar docking, dragging,
  reordering, or vertical orientation.
- No nested `QAction::menu()` presentation in the first version. Existing
  `FluentMenu` and `FluentMenuBar` remain the supported nested-menu surfaces.
- No WinUI AppBar `Compact` / `Minimal` / `Hidden` display-mode clone and no
  label-reveal expansion of the inline bar.
- No automatic replacement of the Capability Phase 1 editing context menu.
- No public text-editing preset or second editing-command facade. Applications
  compose `EditingCommandRouter` actions directly.
- No new application-facing `Popup` property. The only base-class change is
  the protected, default-preserving focus-on-open setter defined below.
- No sticky flyout policy, hover-only command discovery, or process-global
  command registry.
- No changes to system notification or badge work in later capability phases.

## Official Baseline and Intentional Adaptations

The behavioral reference is the current Microsoft documentation for
[CommandBar](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/command-bar),
[CommandBarFlyout](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/command-bar-flyout),
and
[shared commanding](https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/commanding).

| Reference behavior | Phase 3 decision |
|---|---|
| Primary and secondary command collections | Adopted |
| Shared command semantics across surfaces | Mapped to existing `QAction` |
| Dynamic overflow changes presentation, not collection membership | Adopted |
| Importance order keeps important commands visible | Adopted through `QAction::priority()` plus stable list order |
| Flyout collapsed/expanded and proactive/reactive invocation | Adopted as `Transient` / `Standard` show modes |
| Always-expanded flyout | Adopted |
| Current flyout primary commands show label and icon when both exist | Adopted |
| RTL reverses visual layout | Adopted while preserving logical collection order |
| Bottom, right, or collapsed primary labels tied to bar open state | Adapted to persistent `Right` or `Collapsed`; no separate inline open state |
| Custom command elements and content containers | Deferred |
| Inline AppBar open/closed display modes | Deferred |
| Flyout primary commands may be truncated when too wide | Deliberately changed: commands overflow into the expanded presentation rather than become unreachable |

The last adaptation is a safety and accessibility choice. It preserves the
primary collection and only changes presentation, just like dynamic overflow
in the inline bar.

## Accepted Installed Surface

Both headers live under `src/components/menus_toolbars/`, are exported through
`<FluentQt/MenusToolbars.h>`, and are added to the installed-header allowlist.
Implementation helpers remain under `menus_toolbars/private/`.

`Popup` gains one protected non-virtual setter:

```cpp
protected:
    void setFocusOnOpenEnabled(bool enabled);
```

The private state defaults to enabled. `Popup::open()` consults it immediately
before its existing `setFocus(Qt::PopupFocusReason)` call, so all existing
subclasses retain current behavior. `CommandBarFlyout::setShowMode()` disables
it for Transient and enables it for Standard. Direct inherited `open()` and
`setIsOpen(true)` therefore obey the selected mode too.

The flag lives in private popup/coordinator state rather than the installed
`Popup` object layout. The setter adds no virtual-table slot and avoids an ABI
layout change on the release branch.

### `CommandBar`

```cpp
namespace fluent::menus_toolbars {

class CommandBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT

    Q_PROPERTY(LabelPosition labelPosition
               READ labelPosition
               WRITE setLabelPosition
               NOTIFY labelPositionChanged)
    Q_PROPERTY(bool dynamicOverflowEnabled
               READ isDynamicOverflowEnabled
               WRITE setDynamicOverflowEnabled
               NOTIFY dynamicOverflowEnabledChanged)
    Q_PROPERTY(bool overflowOpen
               READ isOverflowOpen
               WRITE setOverflowOpen
               NOTIFY overflowOpenChanged)
    Q_PROPERTY(bool backgroundVisible
               READ backgroundVisible
               WRITE setBackgroundVisible
               NOTIFY backgroundVisibleChanged)

public:
    enum class LabelPosition {
        Collapsed,
        Right
    };
    Q_ENUM(LabelPosition)

    explicit CommandBar(QWidget* parent = nullptr);
    ~CommandBar() override;

    using QWidget::addAction;
    using QWidget::insertAction;
    using QWidget::removeAction;
    void addAction(QAction* action);
    void insertAction(QAction* before, QAction* action);
    void removeAction(QAction* action);

    bool addPrimaryAction(QAction* action);
    bool insertPrimaryAction(QAction* before, QAction* action);
    bool addSecondaryAction(QAction* action);
    bool insertSecondaryAction(QAction* before, QAction* action);
    bool removeCommandAction(QAction* action);
    void clearPrimaryActions();
    void clearSecondaryActions();

    QList<QAction*> primaryActions() const;
    QList<QAction*> secondaryActions() const;
    QList<QAction*> overflowedPrimaryActions() const;
    bool isPrimaryActionOverflowed(const QAction* action) const;

    LabelPosition labelPosition() const;
    void setLabelPosition(LabelPosition position);

    bool isDynamicOverflowEnabled() const;
    void setDynamicOverflowEnabled(bool enabled);

    bool isOverflowOpen() const;
    bool backgroundVisible() const;
    bool isBackgroundVisible() const;
    void setBackgroundVisible(bool visible);

    void onThemeUpdated() override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setOverflowOpen(bool open);

signals:
    void labelPositionChanged(LabelPosition position);
    void dynamicOverflowEnabledChanged(bool enabled);
    void overflowOpenChanged(bool open);
    void overflowedPrimaryActionsChanged();
    void backgroundVisibleChanged(bool visible);

protected:
    void actionEvent(QActionEvent* event) override;
};

} // namespace fluent::menus_toolbars
```

The same-signature `QWidget::addAction(QAction*)`,
`insertAction(QAction*, QAction*)`, and `removeAction(QAction*)` wrappers are
primary-command shorthands. Their base overload sets remain visible.
`CommandBar` also observes `QActionEvent`, so actions added through a QWidget
base pointer or a Qt convenience overload become primary unless already
registered as secondary by the explicit API. Pointer-shorthand insertion does
not move or reorder an action already registered explicitly; callers use the
section APIs for those operations.
Qt 6.3+ convenience overloads that allocate an action remain inherited when
available, but they are not part of FluentQt's Qt 5.15-compatible surface and
retain Qt's documented widget ownership.

`addPrimaryAction()` and `addSecondaryAction()` provide validated, explicit
entry points and return `false` for null or unsupported actions. Adding an
action already in the same section is a no-op. Adding it to the other section
moves its presentation membership without changing its owner.

For either explicit insert method, null `before` appends; a non-null `before`
must already belong to the destination section or insertion fails without
mutation. Inserting an action already in that section reorders it. An inherited
`QWidget::insertAction()` before a secondary action appends to the primary
section because the inherited method cannot report a section mismatch.
`QWidget::actions()` is only the Qt association list; callers use
`primaryActions()` and `secondaryActions()` for semantic section order.

Initial property values are `LabelPosition::Right`, dynamic overflow enabled,
overflow closed, and background visible.

### `CommandBarFlyout`

```cpp
namespace fluent::menus_toolbars {

class CommandBarFlyout final : public dialogs_flyouts::Flyout {
    Q_OBJECT

    Q_PROPERTY(ShowMode showMode
               READ showMode
               WRITE setShowMode
               NOTIFY showModeChanged)
    Q_PROPERTY(bool expanded
               READ isExpanded
               WRITE setExpanded
               NOTIFY expandedChanged)
    Q_PROPERTY(bool alwaysExpanded
               READ isAlwaysExpanded
               WRITE setAlwaysExpanded
               NOTIFY alwaysExpandedChanged)

public:
    enum class ShowMode {
        Standard,
        Transient
    };
    Q_ENUM(ShowMode)

    explicit CommandBarFlyout(QWidget* parent = nullptr);
    ~CommandBarFlyout() override;

    using QWidget::addAction;
    using QWidget::insertAction;
    using QWidget::removeAction;
    void addAction(QAction* action);
    void insertAction(QAction* before, QAction* action);
    void removeAction(QAction* action);

    bool addPrimaryAction(QAction* action);
    bool insertPrimaryAction(QAction* before, QAction* action);
    bool addSecondaryAction(QAction* action);
    bool insertSecondaryAction(QAction* before, QAction* action);
    bool removeCommandAction(QAction* action);
    void clearPrimaryActions();
    void clearSecondaryActions();

    QList<QAction*> primaryActions() const;
    QList<QAction*> secondaryActions() const;

    ShowMode showMode() const;
    void setShowMode(ShowMode mode);

    bool isExpanded() const;
    bool isAlwaysExpanded() const;
    void setAlwaysExpanded(bool expanded);

    void setAnchor(QWidget* anchor);
    void showAt(QWidget* anchor);
    void showAt(QWidget* anchor, ShowMode mode);
    void showAtPoint(QWidget* relativeTo,
                     const QPoint& localPosition);
    void showAtPoint(QWidget* relativeTo,
                     const QPoint& localPosition,
                     ShowMode mode);

public slots:
    void setExpanded(bool expanded);

signals:
    void showModeChanged(ShowMode mode);
    void expandedChanged(bool expanded);
    void alwaysExpandedChanged(bool expanded);

protected:
    void actionEvent(QActionEvent* event) override;
    QPoint computePosition() const override;
    QWidget* automaticPositionAnchor() const override;

private:
    using dialogs_flyouts::Popup::setPosition;
};

} // namespace fluent::menus_toolbars
```

The same-signature `setAnchor(QWidget*)` and `showAt(QWidget*)` intentionally
hide the non-virtual `Flyout` methods so they can reset the private placement
mode to anchor-based positioning. The two-argument `showAtPoint()` uses the
current `showMode`. Each overload with an explicit mode calls `setShowMode()`
before opening, so the property, notification signal, and later invocations
retain one source of truth. Changing `showMode` while the flyout is already
open affects the next open; it does not retroactively move focus or change
expansion.

`showAtPoint()` positions the visible card near a context-request point and
clamps it to the owning overlay surface. It clears the base anchor and records
the point source; `setAnchor()` clears that point source. Inherited
`Popup::setPosition()` is private on this derived type; mixing a persistent
base position with CommandBarFlyout placement is not supported. Ordinary
callers therefore get repeatable anchor-to-point-to-anchor invocation without
a Popup reset API. Retargeting while already open recomputes and clamps
position without replaying the open animation or changing current
focus/expansion; an explicit mode still becomes the default for the next open.

As with `CommandBar`, the pointer-based `QWidget` action wrappers are
primary-command shorthands while inherited overloads remain available. The
same duplicate, move, insert, ordering, and validation rules apply. If the
protected placement-override design proves
insufficient, implementation returns to review instead of silently widening
`Popup` API.

Initial property values are `ShowMode::Standard`, not expanded, and not always
expanded.

## Action Contract

### Supported action kinds

- Ordinary `QAction` is presented as a command button or overflow row.
- A checkable action is presented as a toggle command. Its checked state and
  any `QActionGroup` exclusivity remain owned by Qt.
- A separator action participates in layout grouping and is never focusable.
- A non-separator action must provide a semantic caption through `text()` or
  `iconText()`.
- An action with `menu() != nullptr` and any `QWidgetAction` are rejected in
  the first version with a `fluentqt.commandbar` warning.

Rejecting
[`QWidgetAction`](https://doc.qt.io/qt-6/qwidgetaction.html) avoids its
per-container widget creation and ownership contract leaking into a surface
that deliberately needs multiple private presenters. Rejecting nested menus
avoids falling back to a top-level `QMenu` inside a same-window flyout.

Primary labels prefer non-empty `iconText()` and otherwise use `text()`;
overflow and secondary rows prefer `text()` and otherwise use `iconText()`.
Visible labels and accessible names remove access markers and any embedded tab
shortcut suffix. Command bars do not create an implicit Alt mnemonic from
`&`; the action's actual `shortcut()` remains authoritative. If a registered
action later loses both captions, its presenter is suppressed until a caption
returns and a warning is logged once for that invalid state. The same
suppression/restoration rule applies if an ordinary registered action later
gains or loses a nested `menu()`.

Overflow and secondary shortcut text uses
`QKeySequence::NativeText`; an embedded tab suffix is display fallback only and
does not create a shortcut binding.

### State mapping

| `QAction` state | Primary presenter | Overflow / secondary presenter |
|---|---|---|
| `text`, `iconText` | Label according to `LabelPosition` | Full label |
| `icon` | 20 px logical icon slot | 16 px logical icon slot |
| `toolTip`, `statusTip` | Tooltip/status forwarding | Description/status forwarding |
| `shortcut` | Remains active through normal Qt action association | Shown as trailing text and remains active |
| `enabled` | Disabled visual and skipped by keyboard activation | Same |
| `visible` | Removed from measurement and focus order | Removed |
| `checkable`, `checked` | Toggle visual | Checkmark/toggle row |
| `separator` | Vertical group divider | Horizontal group divider |

`QAction::changed`, `toggled`, `triggered`, and `destroyed` are the only
semantic synchronization sources. Triggering a presenter calls
`QAction::trigger()` exactly once; it does not copy or reimplement the command.

`LabelPosition::Right` shows icon and text when both exist.
`LabelPosition::Collapsed` hides the visual label only when a usable icon
exists; text-only commands still show text, so the setting cannot create an
invisible command. `CommandBarFlyout` primary commands always use the `Right`
rule, matching current Windows App SDK behavior. Overflow and secondary rows
always show text. Labels are single-line: inline commands overflow as whole
units rather than wrapping, while constrained overlay rows elide as described
below.

## Ownership and Lifetime

- Both surfaces borrow every caller-supplied `QAction`; they never reparent or
  delete it.
- Private button, toggle, separator, overflow-row, and overlay presenters are
  surface-owned and never exposed.
- Removing or clearing an action only removes presentation and Qt association.
- Destroying an action automatically removes it from all sections, recomputes
  overflow, and repairs focus.
- Destroying a surface never explicitly deletes a command action. An action
  survives unless its existing QObject parent already makes it a child of that
  surface; FluentQt never changes that parent.
- The same action may be shown by multiple surfaces simultaneously. Each
  surface owns independent presenter and overflow state.
- Trigger handlers may delete the action, its group, or the command surface;
  presenters use guarded pointers and perform no unchecked post-trigger access.
- One action cannot be in both sections of the same surface. Re-registering it
  in the other section moves section membership.
- A window-scoped action, including an `EditingCommandRouter` action, must only
  be inserted into surfaces owned by that same top-level window.
- `CommandBarFlyout` is bound to its current QWidget parent's top-level before
  first show. A caller may construct it parentless and parent it later, but the
  show target never causes a parentless object to be adopted implicitly. The
  existing overlay coordinator may attach an already parented surface to that
  same owning top-level while it is shown.
- Null targets, a still-parentless flyout, and targets from another top-level
  are rejected without opening or retargeting an already open surface.
  Lifetime remains entirely under ordinary Qt parent or caller ownership.

This follows Qt's existing
[widget action-container rule](https://doc.qt.io/qt-6/qwidget.html#addAction):
adding an existing action to a widget does not transfer ownership.

## Responsive Measurement and Overflow

### Inline `CommandBar`

Dynamic overflow is enabled by default and uses a stable fixed-point layout:

1. Exclude null, destroyed, and invisible actions from measurement.
2. Measure visible primary presenters with the current theme, font, label
   position, layout direction, and high-DPI metrics.
3. If explicit secondary actions produce at least one visible normalized row,
   reserve the More button before fitting primary commands.
4. If primary actions do not fit, reserve the More button and recompute.
5. Move presentation-only, non-separator candidates to overflow until the row
   fits.
6. Normalize separators after every move: no leading, trailing, or consecutive
   visual separators.

With dynamic overflow enabled, `sizeHint()` represents the full unoverflowed
primary row plus More when normalized secondary content exists.
`minimumSizeHint()` represents one minimum More target plus margins whenever
command content can be projected into overflow; an empty/separator-only bar
does not reserve that target.

Candidate order is deterministic:

1. `QAction::LowPriority` overflows before `NormalPriority`.
2. `NormalPriority` overflows before `HighPriority`.
3. Within one priority, actions overflow from the logical tail toward the
   logical start.
4. Expansion restores the exact reverse sequence.

High priority is not an absolute pin: when the host is narrower than every
primary command, all commands may enter overflow so that the More button
remains operable at or above `CommandBar::minimumSizeHint()`. That minimum is
large enough for More plus required margins whenever normalized command
content exists. Geometry forcibly assigned below the minimum can clip and
emits one diagnostic; logical order never changes.

Separators do not enter the priority candidate queue. For each proposed split,
the inline and overflow sections independently project the original logical
order and retain a separator only when that section has a visible
non-separator command on both sides of it. Measurement then repeats with those
projected separators. A separator's own `QAction::priority()` is ignored, and
`overflowedPrimaryActions()` reports only separators retained by the overflow
projection.

The expanded overflow surface presents:

1. overflowed primary actions in original logical order;
2. a synthetic separator when both groups are non-empty;
3. explicit secondary actions in registration order.

The More button exists only when that normalized projection contains at least
one visible non-separator command row; disabled rows still count. Hidden,
captionless, nested-menu-suppressed, and separator-only content does not create
an empty More surface.

Dynamic overflow never mutates `primaryActions()` or `secondaryActions()`.
`overflowedPrimaryActions()` is the presentation snapshot. Repeated
measurement at the same width emits no duplicate change signal.

When dynamic overflow is disabled, primary commands remain inline and the full
row becomes the widget's minimum width. Forced geometry below that minimum may
clip the row; no primary action is silently moved. Normalized explicit
secondary commands still remain available through More.

### `CommandBarFlyout`

Primary actions stay in a horizontal row and secondary actions appear below
when expanded:

- `Transient` opens collapsed and normally shows only primary actions.
- `Standard` opens expanded when secondary or overflowed primary commands
  exist.
- `alwaysExpanded` suppresses the More button and keeps secondary and
  overflowed primary commands visible while the flyout is open.
- A secondary-only flyout always uses the expanded menu-like presentation.
- If the owning window cannot fit the primary row at the minimum target size,
  primary commands selected by the same priority/logical-tail rule use
  presentation-only overflow and become reachable in the expanded section.
  Collection membership is unchanged.

The visible card is clamped to the host overlay surface. A long secondary list
scrolls inside the card; the outer card does not extend beyond its host. A host
surface smaller than one minimum command target plus card margins is physically
unsupported and produces a best-effort clipped surface plus one diagnostic.
Rows wider than the available card elide their visible label on the logical
trailing edge while retaining the full accessible name and tooltip; overlays
do not add horizontal scrolling.

`expanded` reports effective presentation state. `setExpanded(true)` is a
no-op while the flyout is closed or when no secondary or overflowed primary
row exists. With expandable content, setting `alwaysExpanded` true expands
immediately while open and `setExpanded(false)` becomes a no-op. Removing the
last expandable row collapses the presentation while leaving the
`alwaysExpanded` preference unchanged. Closing resets effective `expanded` to
false; the next show mode determines initial expansion, with `alwaysExpanded`
taking precedence. Repeated setters emit no duplicate signals.

## Open, Dismiss, and Focus Contract

### Inline overflow

- `overflowOpen` reports actual presentation state. Setting it true is a no-op
  unless a visible More button currently has content.
- The More button opens a non-modal, non-dim same-window overlay.
- Opening stores the currently focused command. The first enabled overflow row
  receives focus; if all rows are disabled, the overflow surface root receives
  focus so Escape and accessibility remain available.
- Escape or a second More activation closes the overlay and restores focus to
  More.
- Activating a command closes the overlay. If the action deliberately moved
  focus elsewhere, that destination is preserved.
- Outside press uses the shared visible-card hit test, closes the overlay, and
  continues to the background target.
- Resizing, moving, hiding, clipping, or destroying the anchor follows the
  shared overlay contract.
- If an action or geometry change removes the More button, an open overflow
  closes and emits one state change.

### Flyout show modes

`Standard` is the reactive/context-request mode:

- The flyout opens expanded when it has secondary or overflowed primary
  commands.
- It takes focus, keeps keyboard focus within the light-dismiss surface, and
  starts at the first enabled primary command or first enabled secondary
  command. If none is enabled, the surface root receives focus.
- On Escape or command activation, focus returns to the pre-open widget only
  when focus is still inside the flyout and the action did not redirect it.
- An outside press is allowed to choose the new focus destination and is not
  overwritten by restoration.

`Transient` is the proactive mode:

- The flyout opens collapsed without taking focus.
- Pointer activation is available, but Tab order and the current editor focus
  remain unchanged until the user explicitly expands or enters the flyout.
- Expanding through More promotes the surface into keyboard-interactive mode.
- Closing a never-focused transient flyout performs no focus restoration.

The protected `Popup` setting is what makes “without taking focus” literal;
tests must observe no intermediate `QApplication::focusChanged` away from the
pre-open widget.

Both modes are non-modal, non-dim, and use
`CloseOnPressOutside | CloseOnEscape`.

### Editing-router integration

The Phase 2 router currently recognizes menu focus transfer. Phase 3 extends
that private recognition to any supported same-window action container:

- walk from the focused presenter through its QWidget ancestors;
- preserve the edit target when an ancestor contains one of the router's
  actions;
- restore the editor after that action triggers unless focus was explicitly
  redirected.

`CommandBarFlyout` is itself such a container. The inline CommandBar's private
overflow root temporarily associates its rendered actions while open and
removes those associations on close, without changing action ownership. This
keeps the router test generic and avoids a public dependency on either command
surface type.

No CommandBar type is added to the router's public API, and no process-global
registration is introduced.

## Keyboard Contract

`CommandBar` is one composite Tab stop:

- Tab / Shift+Tab enter or leave the whole bar; they do not stop on every
  private presenter.
- On first entry, focus the first enabled visible primary action, then More if
  it is present. Later entry may restore the last still-valid action. With
  neither, the root remains the single Tab stop and arrow/activation keys are
  no-ops.
- Left / Right move to the visually adjacent enabled action.
- Home / End move to the visual start / end.
- Enter and Space trigger the current action.
- The More button participates at the logical trailing edge when present
  (right in LTR, left in RTL).
- Enter, Space, or Down on More opens overflow.

Overflow and expanded secondary lists use:

- Up / Down for the previous / next enabled row;
- Home / End for the first / last enabled row;
- Enter or Space to activate;
- Escape to close the current overflow or flyout surface;
- a trapped Tab cycle while `CommandBarFlyout` is in keyboard-interactive
  mode.

Disabled and invisible actions and separators are skipped. If resize or an
action change invalidates the current item, focus moves to the nearest enabled
visible sibling, then More, then the surface root.

## RTL Contract

- `primaryActions()` and `secondaryActions()` always retain caller-provided
  logical order.
- RTL reverses primary visual layout, More placement, icon/text composition,
  overflow alignment, and horizontal arrow navigation.
- Overflow candidate selection still starts at the logical tail, so changing
  direction does not change which commands are considered most important.
- Overflow and secondary rows align text, icons, and shortcut text using
  logical leading/trailing edges.
- Home and End refer to visual start and visual end.
- Point and anchor placement clamp against the same visible-card bounds in both
  directions.

## Pointer and Touch Contract

- Standard command hit targets are at least 40 by 40 device-independent pixels,
  even when the visible glyph or Cupertino bezel is smaller.
- The More target follows the same minimum.
- Hover never triggers a command and is not required for discovery.
- Press and release must occur in the same enabled target; dragging out
  cancels activation.
- Touch activates on release and uses the same checked, dismiss, and focus
  semantics as pointer input.
- Right-button presses do not trigger primary actions.
- Destructive commands receive no larger implicit semantic role; applications
  remain responsible for confirmation policy.

The 40 px baseline follows Microsoft's current touch-target guidance and the
project's 4 px spacing grid.

## Design-Language Contract

Behavior, measurement order, action identity, and accessibility do not change
between design languages.

| Language | Inline surface | Command target | Overlay |
|---|---|---|---|
| Fluent | Token canvas/layer strip | Subtle rest, Fluent hover/press/focus | Overlay radius, border, elevation |
| Material | Surface-container strip | Neutral state layer; checked uses tonal selection | Elevated borderless surface-container |
| Cupertino | Quiet toolbar surface | Inset bezel/veil; checked uses accent selection | Hairline rounded panel with soft shadow |

Primary icon slots are 20 px and overflow icon slots are 16 px across
languages. The interactive target remains at least 40 px. Theme or design
language changes while open preserve open/expanded state and actions, then
remeasure and repaint atomically.

Visual checks cover Light and Dark for all three languages, long translated
labels, icon-only and text-only commands, high DPI, LTR, and RTL.

## Accessibility Contract

- The inline root exposes a toolbar role and an accessible name set by the
  application when surrounding context does not already label it.
- The flyout exposes a popup/menu surface containing a toolbar-like primary row
  and menu-like secondary rows.
- Presenter names come from access-marker- and shortcut-stripped
  `QAction::text()`.
- Tooltip/status text supplies the description; `QAction::shortcut()` supplies
  accelerator text.
- Disabled, invisible, focused, pressed, checkable, and checked states remain
  queryable through Qt accessibility.
- The More button has a translated internal accessible name and exposes the
  expanded/collapsed state.
- Visual and accessible child order match, including after overflow and in RTL.
- Focus and state accessibility events are sent after the corresponding state
  change.

Private Qt accessibility adapters may be required for the toolbar, popup, and
secondary-row roles. They do not become installed API.

## Diagnostics

A new `fluentqt.commandbar` Qt logging category records actionable diagnostics:

- rejected `QWidgetAction` or nested-menu actions;
- rejected or temporarily suppressed actions without a semantic caption;
- an action registered in both sections before normalization;
- a window-scoped action inserted into a mismatched host when detectable;
- impossible geometry, missing owning top-level, or cross-window show target;
- focus recovery after action destruction.

Normal resize, measurement, hover, and action-state changes do not log.

## Acceptance Matrix

### Public API and packaging

- `CommandBar` inherits `QWidget`, `FluentElement`, and `QMLPlus`.
- `CommandBarFlyout` inherits `Flyout`.
- Popup focus-on-open remains enabled by default; existing subclasses preserve
  their focus behavior and the `Popup` object layout/vtable remains unchanged.
- Property defaults and explicit-mode invocation behavior match this proposal.
- Category and aggregate headers compile for source and installed consumers.
- Public headers are installed; private presenter/model headers are absent.
- New non-trivial public and protected APIs carry concise English and `zh_CN`
  Doxygen contracts.
- Qt 5.15 and supported Qt 6 builds compile without version-only action APIs.

### Action model and lifetime

- Plain, checkable, exclusive-group, and separator actions present correctly.
- Text, icon, tooltip, shortcut, enabled, visible, and checked changes update
  every presenter.
- A trigger reaches the borrowed action exactly once.
- Trigger-time deletion of the action or surface is safe.
- Duplicate registration is a no-op; moving sections is deterministic.
- Explicit insert reorders within a section, rejects a mismatched `before`,
  and inherited Qt action APIs follow their documented primary shorthand.
- Removal, clear, action destruction, and surface destruction preserve
  caller-owned actions while respecting pre-existing QObject parent ownership.
- One action can be shared by `FluentMenuBar`, `CommandBar`, and
  `CommandBarFlyout`.
- Unsupported `QWidgetAction`, nested-menu actions, and captionless commands
  are rejected or safely suppressed according to the action contract.

### Overflow and geometry

- Width contraction and expansion follow priority, logical-tail, and exact
  reverse-order rules.
- More-button reservation reaches a stable layout without oscillation.
- Dynamic overflow does not mutate either public collection.
- Hidden actions do not consume space; disabled actions remain visible.
- Separators do not enter the priority queue and never appear leading,
  trailing, or consecutive in either projection.
- Explicit secondary actions follow overflowed primary actions.
- Dynamic-overflow-disabled minimum width is deterministic.
- Flyout fallback keeps every primary action reachable in a narrow host that
  can fit one minimum command target plus card margins.
- Long lists scroll within the visible card.
- Overflow and secondary rows reconcile against the exposed scroll viewport,
  including a deferred post-open pass, so a stale pre-exposure viewport width
  cannot clip command captions after theme, direction, or geometry changes.
- Repeated anchor-to-point-to-anchor invocation tracks the current source
  without widening `Popup` API.
- A parentless flyout, null target, or cross-window target does not open and
  never causes implicit QObject reparenting.

### Keyboard, focus, and routing

- Composite Tab, Shift+Tab, arrows, Home, End, Enter, Space, Down, and Escape
  behavior matches this proposal in LTR and RTL.
- Focus repairs safely after hide, disable, destruction, or resize overflow.
- Standard flyout traps and restores focus; Transient initially preserves it.
- Transient opening emits no intermediate application focus transfer.
- Closed-state expansion, `alwaysExpanded` precedence, and duplicate-signal
  suppression match the effective-state contract.
- Outside press does not lose the user's newly chosen focus target.
- Editing-router actions retain their editor target and restore focus after
  activation.
- Two top-level windows keep focus and action routing independent.

### Overlay behavior

- Inline overflow and `CommandBarFlyout` remain `Qt::Widget` children of the
  owning top-level; they create no `Qt::Popup`, `Qt::Tool`, or separate window.
- Visible-card geometry drives placement and hit testing; shadow margin is not
  interactive.
- Anchor move/resize, host resize, clipping, destruction, z-order, light
  dismiss, and animation-disabled lifecycle follow the overlay contract.
- Non-modal outside press continues to the background target.

### Pointer, touch, theme, and accessibility

- Every command and More target is at least 40 by 40 logical pixels.
- Press-drag-cancel, touch release, right button, hover, and disabled behavior
  are deterministic.
- Fluent, Material, and Cupertino paint in Light/Dark without opaque fallback
  artifacts.
- Theme switching while open preserves state and recomputes overflow once.
- Accessible roles, names, accelerators, focus, checked state, and expanded
  state are queryable.

### Gallery and boundary checks

- One responsive CommandBar sample demonstrates primary, secondary, separator,
  checkable, priority, resize, and RTL behavior.
- One CommandBarFlyout sample demonstrates Standard, Transient, and
  always-expanded modes.
- One integration sample reuses `EditingCommandRouter` actions without a new
  editing facade.
- Gallery snippets use only installed public API and remain semantically aligned
  with their previews.

### Validation execution

- Register focused `test_command_bar` and `test_command_bar_flyout` modules
  under `tests/components/menus_toolbars/`; shared application setup remains in
  `tests/support/QtGTestMain.cpp`.
- Run non-visual contracts on the current Windows Qt 6 host and the supported
  Linux Qt 5.15 baseline before closing each implementation slice.
- Keep manual interaction tests behind the standard `SKIP_VISUAL_TEST` guard.
  Focused Windows desktop review exercises Light/Dark, design-language changes,
  LTR/RTL, narrow geometry, Standard/Transient focus behavior, pointer, and
  keyboard paths. The final validation also covers the high-DPI matrix and
  sanitizer teardown paths. Physical touch hardware remains an optional
  release-device smoke rather than an open component gate.
- Re-run source-subproject and installed-package fixtures whenever the
  category export or `Popup` protected surface changes.

## Implementation Slices and Review Gates

1. **Capability Phase 3A — API and action model**
   - Add public declarations, the protected default-preserving Popup focus
     setter, private borrowed-action model, logging category, package probes,
     and non-visual lifetime/state tests.
   - Status: implemented and validated on Windows Qt 6.9.3 and Linux Qt
     5.15.2; review gate accepted on 2026-07-28.
   - Review gate: API shape, ownership, unsupported-action policy.
2. **Capability Phase 3B — inline CommandBar**
   - Add private presenters, composite keyboard focus, deterministic
     measurement, and same-window overflow.
   - Status: implemented and validated on Windows Qt 6.9.3 and Linux Qt
     5.15.2; continuation through 3C was authorized.
   - Validation: the target now contains 17 focused `test_command_bar`
     contracts and passes on both baselines. The original 3B gate selection
     reported 0 failures across 69 Windows tests with 5 expected skips; the
     equivalent Linux Qt 5 non-desktop selection passed all 63 tests.
   - Review gate: overflow order, geometry, RTL, keyboard behavior.
3. **Capability Phase 3C — CommandBarFlyout**
   - Add collapsed/expanded presentation, show modes, point/anchor placement,
     light dismiss, and focus restoration.
   - Status: implemented and validated on Windows Qt 6.9.3 and Linux Qt
     5.15.2; continuation through 3D was authorized.
   - Validation: 14 non-visual `test_command_bar_flyout` contracts pass on
     both baselines. Its guarded VisualCheck remains registered and has also
     been exercised through focused Windows desktop review.
   - Review gate: Standard versus Transient behavior and overlay compliance.
4. **Capability Phase 3D — polish and integration**
   - Add design-language branches, accessibility adapters, Gallery samples,
     EditingCommandRouter integration, installed-package validation, and manual
     visual review.
   - Status: implementation, automated validation, and final Windows desktop
     review complete; the Capability Phase 3 gate was accepted on 2026-07-30.
   - Validation: the current Windows Qt 6.9.3 label selection discovers 98
     CommandBar/Flyout/router/overlay/menu tests, with 92 passes, 6 expected
     desktop/manual skips, and 0 failures. Linux Qt 5.15.2 passes all 17
     CommandBar contracts plus 14 automated Flyout contracts, with the guarded
     Flyout VisualCheck skipped as intended. The Windows Gallery content-page
     suite reports zero failures across 44 tests, with 2 expected visual
     skips. Source-subproject and installed-package consumers compile and link
     the public API; the install tree excludes all private command model,
     presenter, and accessibility headers.
   - Focused Computer Use review covered Fluent Light/Dark, a Material
     transition, LTR/RTL, Standard/Transient/expanded flyouts, responsive
     Gallery overflow, and focus preservation. It exposed and drove fixes for
     stale pre-exposure `QScrollArea` viewport widths in both flyout secondary
     rows and inline overflow rows; the corresponding geometry contracts now
     prevent caption clipping.
   - Review gate: cross-platform behavior and final Capability Phase 3
     acceptance.

Each slice remains independently reviewable. Capability Phase 3 has passed its
cross-platform automated, sanitizer, high-DPI, and focused desktop gates and is
accepted as complete.

## Accepted Decisions

1. Keep `QAction` as the only public command item; presenters remain private.
2. Borrow actions without reparenting or deleting them.
3. Treat inherited pointer-based `QWidget` action insertion as a
   primary-command shorthand; explicit section lists remain authoritative.
4. Use `QAction::priority()` plus logical-tail order instead of adding a new
   numeric overflow-priority API.
5. Keep separators outside the priority queue and project them only between
   commands that remain together in a presentation section.
6. Keep flyout commands reachable in narrow hosts through presentation-only
   overflow, deliberately avoiding WinUI's possible primary-command
   truncation.
7. Require same-window overlays and do not reuse `QMenu` for either new
   overflow surface.
8. Use one composite Tab stop, visual arrow navigation, and distinct Standard
   versus Transient focus behavior.
9. Offer persistent `Right` and `Collapsed` label positions on inline
   `CommandBar`, default to `Right`, keep flyout primary labels visible, and
   defer bottom/reveal behavior until an inline open-state contract exists.
10. Reject `QWidgetAction`, nested `QAction::menu()`, and commands without a
    semantic caption in the first version.
11. Do not automatically replace editing context menus and do not add a public
   editing preset; demonstrate direct Phase 2 action composition instead.
12. Add exactly one default-preserving protected
   `Popup::setFocusOnOpenEnabled(bool)` setter so Transient can avoid focus
   transfer; add no application-facing property or Popup layout/vtable change.
13. Keep point/anchor switching in private placement infrastructure; any
   further need to widen `Popup` API returns to review.
14. Defer arbitrary content, docking, vertical bars, sticky mode, and WinUI
   AppBar display modes.
