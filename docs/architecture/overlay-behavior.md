# Overlay Behavior Contract

> **Status:** Accepted contract

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Window Chrome Architecture](window-chrome.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Typography Resolution →](typography-resolution.md)
<!-- docs-nav:top:end -->

Transient overlays (`Popup`, `Flyout`, `ComboBox` / `MultiSelectComboBox`
dropdowns, `DrawerView`, `Dialog` / `ContentDialog`, `CoachMark`, and
`TeachingTip`) appear within their owning window. They attach to the owning
top-level `QWidget` and retain `Qt::Widget` child semantics, without creating
a separate `Qt::Window` / `Qt::Dialog` / `Qt::Tool`. This follows WinUI Gallery's
ContentDialog / Flyout / TeachingTip model, which binds to the current window's
`XamlRoot`.

Shared helpers live in `src/components/foundation/overlay/`, in the
`fluent::overlay` namespace. The internal `OverlayCoordinator` manages top-level
attachment, host resizing, scrim lifetime, and stacking. `Popup`, `Dialog`, and
`DrawerView` also delegate backdrop settings, parameter normalization, modal
input containment, and outside/Escape dismissal to this coordinator. Each
component retains its placement, animation, ownership, and close-result API.
The coordinator is not an installed header or an application-facing API.

## Startup content cover

`fluent::status_info::SplashScreen` covers a parent widget's content during
startup and follows its size. It does not manage tasks, the title bar, or the
startup sequence. Place it on `Window::contentHost()` to keep window buttons
available, or on a local container chosen by the application.

The default `Presentation::Branded` reveals the full icon with soft background
light, optional brand text, and a bottom progress bar. Applications can select
`Presentation::Simple` for a centered icon, progress ring, and status text.
Gallery startup uses the default presentation; the component sample can switch
between both modes and replay them.

```cpp
#include <FluentQt/FluentQt.h>

// host is the QWidget content area; appIcon is supplied by the application.
auto* splash = new fluent::status_info::SplashScreen(host);
splash->setIcon(appIcon);
splash->setIconSize(QSize(96, 96));
splash->setTitle(QStringLiteral("My App"));
splash->setSubtitle(QStringLiteral("Your workspace, ready to go"));
splash->setText(QStringLiteral("正在加载工作区"));
// Optional: an application-owned icon holder in the same window.
splash->setTransitionTarget(titleBarIcon);
splash->show();

// Deliver worker progress on the GUI thread.
splash->setProgress(3, 5);
// The application calls this when startup has actually finished.
splash->dismiss();
```

```python
splash = fluentqt.SplashScreen(host)
splash.setIcon(app_icon)
splash.setTitle("My App")
splash.setSubtitle("Your workspace, ready to go")
splash.setText("正在加载工作区")
splash.setTransitionTarget(title_bar_icon)
splash.show()
splash.setProgress(3, 5)
splash.dismiss()
```

To select the simple presentation, call
`splash->setPresentation(SplashScreen::Presentation::Simple)` in C++ or
`splash.setPresentation(fluentqt.SplashScreen.Presentation.Simple)` in Python.
Switching modes preserves the current status text and progress.

`setProgress(done, total)` selects determinate progress, normalized from 0 to
100%. A `total <= 0` or `setIndeterminate(true)` selects indeterminate progress
and hides the percentage. Branded uses a progress bar; Simple uses a progress
ring. Status text is retained independently. Reaching 100% does not dismiss
the cover, since a task count does not establish that startup has finished.
Long text is elided visually and remains available in full to accessibility
clients. Small windows reduce the icon size and spacing.

`setTransitionTarget(QWidget*)` borrows an application-owned icon container.
With Full motion, the full icon shrinks and moves into that widget's content
rectangle on dismissal. The target may sit outside the content host in the
same window's title bar. The component does not change the target's ownership,
appearance, or visibility; the application coordinates when the target icon
appears. A missing, invisible, or different-window target uses a fade instead.
Destroying the target clears the pointer, and an icon already in transition
fades from its last valid position. Passing the cover itself or one of its
children is ignored.

`dismiss()` follows the Full/Reduced/Disabled motion policy and emits
`dismissed()` once after hiding. It does not destroy the component by default.
Call `show()` again to reuse it. Calling `hide()` directly cancels dismissal
without a completion signal. For one-shot use, connect `dismissed` to
`deleteLater`. Repeated `dismiss()` calls while hidden or already dismissing
emit no extra signals. Minimizing the window does not cancel a dismissal in
progress, and restoring the window does not reshow a dismissed cover.

Reduced uses a short fade; Disabled completes immediately. Neither moves the
icon or plays a decorative entrance. HighContrast omits background light.
Once the task finishes, `dismiss()` can run immediately without waiting for
the entrance animation.

The Gallery shell controls its own startup timing. Branded with Full motion
stays visible for at least 1400 ms, including loading time. After warm-up it
remains for at least 250 ms, then moves the icon into the title bar along a
700 ms ease-in/out curve. A long load adds only the remaining ready-state
pause, without restarting the full display duration. Simple, Reduced, and
Disabled retain a 120 ms composition wait. First-run onboarding starts its
timer after `dismissed()`. The public component adds no minimum display delay;
the sample's 3600 ms duration is replayable simulated loading.

Within one window, showing a new cover immediately hides any existing cover
on the same host or on nested, overlapping hosts. Non-overlapping containers
can each show a cover. Once the new cover is ready, the old one emits
`replaced()` once, without emitting `dismissed()` or destroying itself.
For one-shot use, connect both `dismissed` and `replaced` to `deleteLater`.
Reusable covers may keep their objects and show them again later. Direct
`hide()` calls and window minimization do not emit `replaced()`.

While visible or dismissing, the cover intercepts mouse and keyboard input
within the host content. It takes content focus and prevents underlying
shortcuts from firing through that focus. After hiding, it restores the
previous focus where possible. The title bar, controls outside the host, and
other windows remain usable. The application still coordinates separate
popups and commands outside the host. Schedule loading in batches or on a
worker thread; blocking the GUI thread also stops animation and window input.

## Geometry

Overlay implementations must distinguish three geometry regions:

- Outer widget: the actual `QWidget::geometry()`, including shadow margins.
- Visible card or panel: the card or drawer region used for placement and
  hit testing.
- Content: the region containing a ListView, viewport, or arbitrary child
  widgets. Inset or clip it to keep rectangular viewport backgrounds inside
  rounded corners.

`setPosition()`, anchor placement, edge placement, and test assertions refer
to the visible card or panel, excluding shadow margins.

Input dropdowns use the private `FieldPopupGeometry_p.h` policy: an 8 logical-pixel
visible gap, logical leading-edge alignment, the roomier side when the preferred
bottom side cannot fit, and containment in the owning window. ComboBox preserves
its explicit `popupOffset` override. ComboBox, MultiSelectComboBox, AutoSuggestBox
(including TagInput) and TreeSelect reuse these calculations; their content
layouts retain responsibility for fitting rows and status controls. Generic
Flyout, dialogs and native command menus retain their separate placement roles.

In the Gallery's 3D shell, anchored overlays use the target's presented bounds
before choosing a side or applying containment. Popup points and attached
tooltips use the same mapping; native menu invocation points are mapped before
Qt applies screen containment. Submenus keep their native coordinates. The
internal presentation mapping stops at native-window boundaries and is removed
when the shell returns to 2D. Dialogs, ContentDialog, and DrawerView remain
centered or edge-aligned in their actual owning window, not in a projected page.

Native menu shadows may overlap between a parent and its submenu. A transient
parent-enter event while the pointer is inside an open submenu window must not
dismiss that submenu. Route that enter and any following misdirected mouse moves
to the submenu, so Qt retains its enter bookkeeping and ancestor close timers.
Use the event's coordinates, including Qt 5's native `QEnterEvent`; a plain Qt 5
synthetic enter without coordinates keeps the default Qt path.
Returning to the parent's own area keeps Qt's normal
selection and dismissal behavior. This applies at each level of a cascade;
keyboard navigation and leaf-action activation retain their Qt semantics.

FluentMenu aligns the first submenu row with the invoking parent row, using
the actual open parent rather than QObject ownership. Visible cards have a
small gap; shadow margins do not accumulate into diagonal cascade offsets.
Placement respects RTL and flips or clamps at the available screen edge.
FluentMenu, CommandBar overflow, and CommandBarFlyout share the menu surface
and shadow tokens. Command scroll viewports and content remain transparent
after content attachment, theme refresh, and reopening.

An embedded `QGraphicsProxyWidget` root is a logical QWidget window, not an
overlay boundary. The private `WidgetPresentationCompat` adapter resolves its
scene viewport; foundation composes that mapping with any outer host transform.
The same placement code is used on Windows, macOS, Linux and WebAssembly.
Chart readouts and other same-window overlays attach to the actual owning
window, so card edges do not clip them. Native menus and tooltips bypass Qt's
automatic child-proxy embedding while retaining their QObject ownership.
Coalesced scene-transform notifications keep open anchors current; hiding the
source or its viewport dismisses the anchored surface. Ordinary non-embedded
2D coordinates are unchanged.

CommandBar overflow and CommandBarFlyout use the same presented owner for
placement, maximum size, invocation validation and focus restoration. An
embedded card is not a separate application window: a flyout can reopen from
that card after attaching to the native owner, while truly foreign-window
invocations remain rejected. Overflow-anchor hit testing uses the inverse
presentation mapping so one press closes the menu without reopening it.
Calling `showAt()` or `showAtPoint()` during the exit animation reopens the
flyout through the base Popup transition. Visibility alone does not mean a
flyout is open; an in-flight close must not swallow the next invocation.

Commands with `QAction::menu()` reuse the attached FluentMenu cascade. Their
shared command-menu controller exempts events inside that cascade from the
parent surface's Escape and light-dismiss filters. Escape returns one level;
activating a leaf closes the owning overflow or command flyout after QAction
handlers finish. Closing or hiding the parent also dismisses the cascade.
Menus stay caller-owned and use the same native/projected popup anchor mapping
as other menu buttons; no second cascade implementation is introduced.

Custom C++ compositor hosts use the Widget-only
`fluent::overlay::presentation` adapter from the directly included
`<FluentQt/components/foundation/overlay/OverlayPresentation.h>`. It publishes
root-local transforms, maps points and opens native menus before containment;
nonfinite or noninvertible transforms clear that root's published transform.
`clearTransform()` removes only that root's value, so mapping can inherit the
nearest published ancestor again. Publish an identity transform to explicitly
shield a subtree from an ancestor's transform; without a published ancestor or
proxy embedding, clearing restores ordinary coordinates. The guarded menu anchor is only
published during synchronous invocation and never owns its source. Property
keys and mapping implementation remain private. This optional
host adapter does not render, create OpenGL contexts or replace native widget
ownership. It is intentionally C++-only; see the
[binding coverage decision](../../bindings/pyside6/ROADMAP.md#implementation-surfaces).

## Light Dismiss

`CommandPalette` presents registered leaf `QAction` objects in a modal,
same-window search surface. It reuses `Popup` placement, scrim and dismissal,
with a Fluent search editor and model-backed results. The background stays
undimmed by default. The default outer width is 464 logical pixels (432 for the visible card).
Height follows the filtered commands up to a scrollable limit; the search row
stays in place while the results shrink. Registration borrows the
actions; updates to their labels, shortcuts, visibility, enabled and checked
states are reflected in the palette. Register submenu leaves explicitly.
The optional group and keywords belong to the palette's search metadata;
applications retain command execution and shortcut registration.

All whitespace-separated query words must match the label, group or keywords,
ignoring case. Hidden actions are omitted, disabled results remain visible and
cannot be selected, and registration order is retained within each group.
Up/Down skip disabled commands, Enter executes the selected command once, and
Escape closes. Query text is retained and selected when reopening; applications
can call `setQuery(QString())` before opening to start fresh. The default search
and empty-result messages can be replaced by the application. The search editor
exposes keyboard instructions through its accessible description; Tab cycles
between search and results. The compact card has no extra header or footer.

The palette closes without an exit animation and restores focus before
triggering the action. Close handlers may remove or disable the action, so
dispatch validates it again. An `EditingCommandRouter` whose actions are
registered in the palette retains the original editor and selection while
the palette's search editor has focus. PySide6 exposes the same native class
and retains wrappers for actions registered with `addCommand()` until removal.
The WebAssembly Gallery compiles the same C++ implementation. See Gallery
**Menus & toolbars → CommandPalette** for shared-menu/toolbar state and editing
context examples.

`CloseOnPressOutside` treats presses outside the visible card or panel as
outside presses; shadow margins are not an interactive region.
`CloseOnEscape` handles Escape in the overlay and its owning top-level window.
Controls inside the overlay receive Escape first, so an editor or chart can
cancel its own interaction before an unhandled key closes the surface.
`NoAutoClose` disables implicit dismissal from outside presses and Escape.

Popup subclasses can override `shouldDismissOnOutsidePress(globalPos)` to
exclude a trigger or related child surface from automatic dismissal. The
coordinator applies this decision before consuming a non-modal press, so a
calendar trigger keeps its normal press/release/click sequence and toggles once.
Returning false does not bypass modal input blocking or change Escape handling.
This C++ composition hook is not exposed as a Python customization point;
PySide6 and WebAssembly use the same native component behavior.

Modal `Popup`, `Dialog`, and `DrawerView` consume outside pointer input before
Qt selects the background receiver. A dismissing press and its matching release
are consumed even with animations disabled or synchronous deletion on close.
Focus stays inside the active modal surface, and background shortcuts in the
owning window are blocked. The topmost active overlay handles input first.
Input in another application window remains independent. Popup passthrough
regions apply only to non-modal dismissal. After a non-modal overlay closes,
the original outside press continues to the background target. Escape is
consumed when closing; `NoAutoClose` keeps a modal surface open and consumes
Escape without triggering background commands.

## Scrim And Stacking

Modal, dimmed, or blurred overlays use a same-window `OverlayScrim`. It sits above
background widgets and below the overlay card or drawer. A modal scrim blocks
background pointer input. The `modal` and `dim` combinations below determine
whether it intercepts input. A blur-only scrim is pointer-transparent when
`modal` is false. Explicitly maintain
the `scrim -> overlay` stacking order with `raiseOverlayStack` when opening an
overlay, resizing the top-level window, or updating a drawer's position.

When closing finishes, the overlay must synchronously hide or destroy its
scrim so a stale scrim cannot keep blocking background widgets. `Dialog` smoke and
`Popup` / `DrawerView` share the `OverlayScrim` implementation, including
optional rounded surfaces and spotlight effects.

`Popup`, `Dialog`, and `DrawerView` expose the same optional backdrop settings.
Derived surfaces such as `Flyout`, `CommandPalette`, and `ContentDialog` inherit
them:

- `setDimOpacity(value)` uses an opacity from 0 to 1 when `dim` is enabled.
  The default -1 uses the theme smoke token; any negative value restores it.
- `setBackdropBlurRadius(radius)` blurs the owning window's content snapshot
  with a radius from 0 to 32 logical pixels. Until explicitly configured,
  modal surfaces use a subtle 4 px blur and non-modal surfaces use 0.
  Changing `modal` updates this automatic radius and emits its change signal.
  An explicit value, including 0 to disable blur, persists across modality
  changes. Setting the radius does not change `modal`, `dim`, dismissal, or
  keyboard focus behavior.

Opening establishes the content → scrim → surface stacking order before capture,
including when a cached surface predates a newly raised 3D compositor.
The snapshot includes widgets below the current surface, including an underlying
overlay when stacked. It excludes the active surface and its scrim without hiding
widgets or disturbing focus. Capture and filtering happen on opening, resize,
theme or radius changes, and an explicit `refreshBackdrop()` call. Content is
frozen between those updates; applications with changing background content
can refresh explicitly. This is content blur, not desktop/compositor blur.
Native vibrancy's transparent areas use the Fluent canvas color; separately
composited native or GPU surfaces depend on their QWidget rendering support.
High Contrast suppresses blur and retains the configured scrim/input policy.

Blur and dim follow the surface's opening and closing progress. The prepared
snapshot is reused throughout the fade; showing the scrim at its already
captured size does not trigger another capture. Reduced or disabled motion
settles both layers together. CommandPalette cancellation uses this exit fade;
executing a command still closes immediately and restores focus before dispatch.

The Gallery Popup, Dialog, and DrawerView pages each include a modal and
backdrop-effects demonstration. Controls inside each surface adjust blur, dim,
modality, and outside-click dismissal immediately. A background counter shows
whether modal input blocking is active; Gallery navigation is covered by the
same barrier. Turning dim off retains blur and modal blocking. The CommandPalette workspace
focuses on eight shared actions across three groups and a modal switch. Its
fixed backdrop uses an 8 px blur and 12% dim; other popups keep their defaults.
C++, PySide6 and WebAssembly use the same native backdrop implementation.

## Rendering And Theme

Overlay surfaces, borders, shadows, and smoke/scrims use custom painting with
Fluent tokens. The area outside the visible card or panel stays transparent;
embedded child backgrounds must not extend beyond rounded corners. Theme
changes repaint the overlay and refresh hosted-content styling without
changing open state, placement, selected values, or content ownership.

Same-window children use `QGraphicsOpacityEffect` or equivalent widget-level
opacity to paint fades into the host's shared backing store. Do not use
`windowOpacity`, which belongs to separate native windows.

## Animation

With animation disabled, opening and closing must settle visibility, progress,
scrims, geometry, and lifecycle signals synchronously. With animation enabled,
opacity-only transitions in Popup/Flyout/Dialog must not change visible card
geometry. DrawerView position animations retain normalized `position` semantics.

## Open State Machine

Overlay components share observable semantics while retaining their own
inheritance hierarchies. `Popup` / `Flyout` / `TeachingTip`, `CoachMark`, and
`Dialog` / `ContentDialog` keep their respective Qt base classes. `DrawerView`,
`ComboBox` / `MultiSelectComboBox` dropdowns, and `SplitButton` / `DropDownButton`
(QMenu) are not merged into a common base class.

### Three meanings of open

| Concept | API | Meaning |
| --- | --- | --- |
| Requested logical state (public `isOpen`) | `isOpen()` / `setIsOpen(bool)` / `isOpenChanged` | The caller's requested state. `open()` / `setIsOpen(true)` sets it to `true` when Opening begins; `close()` / `setIsOpen(false)` sets it to `false` when Closing begins. |
| Completed animation state | `opened()` / `closed()` | The entrance or exit animation has finished. With animation disabled, completion is synchronous with the request. |
| Widget visibility | `QWidget::isVisible()` | An implementation detail. Opening may briefly precede `show()`; Open is visible. Closing remains visible until the exit finishes, then calls `hide()` before emitting `closed()`. |

Public bindings use `isOpen`. Do not infer logical state from `isVisible()`.
`popupProgress` / `animationProgress` describes the transition and does not
replace `isOpen`.

### Phases and signal order

Phases: `Closed → Opening → Open → Closing → Closed`.

Opening follows this sequence. Legacy aliases appear after the current names
below; both signals are emitted:

1. `opening()` (alias: `aboutToShow()`).
2. `isOpenChanged(true)` only when the logical state changes.
3. Widget `show()`, with scrim and geometry in place.
4. `opened()` when the animation finishes, or synchronously within the same
   call when animation is disabled.

Closing:

1. `closing(reason)` (alias: `aboutToHide()`; the parameterless `aboutToHide`
   retains its signature, and `Dialog` retains parameterless `closing()` as
   its main signal).
2. `isOpenChanged(false)` only when the logical state changes.
3. The exit animation, if enabled; during it, `isOpen() == false` and
   `isVisible() == true`.
4. `hide()` and release the scrim, then emit `closed()`.

`Dialog` / `ContentDialog` follows the same order. `QDialog::finished(int)` /
`accepted()` / `rejected()` remains available; these are not overlay phase
signals. `TeachingTip::closing(TeachingTip::CloseReason)` remains a
component-specific signal, with values 0 to 4 aligned with `Popup::CloseReason`.

`CoachMark` retains its existing `open` property, `isOpen()` / `setOpen()`, and
`openChanged(bool)`, without adding synonymous public APIs in 1.7.
`openChanged` fires when the requested logical state changes; `opened` fires
after fade-in, and `closed` fires after fade-out and hiding. Closing during
opening or reopening during closing reverses the current transition. Do not
emit a completion signal for the cancelled direction.

### Reentrancy

| Call | Rule |
| --- | --- |
| `open()` during Opening / Open | No-op; no repeated signals |
| `close()` during Closing / Closed | No-op |
| `close()` during Opening | Cancel the entrance and enter Closing |
| `open()` during Closing | Cancel the exit and enter Opening, reversing from the current progress |
| Destroy the overlay in a phase signal handler | Allowed; implementations must guard with `QPointer` and emit no further signals after destruction |

`opening` / `closing` cannot be cancelled. To prevent dismissal, use
`NoAutoClose` or avoid calling `close()`.

### Close reasons

`Popup::CloseReason` (`Q_ENUM`):

| Value | When it applies |
| --- | --- |
| `Programmatic` | `close()` / `setIsOpen(false)` / `Dialog::done()`, or dismissal with no specified reason |
| `ActionButton` | An explicit action, such as the primary action in TeachingTip / ContentDialog |
| `CloseButton` | The TeachingTip close button |
| `LightDismiss` | `CloseOnPressOutside` hits outside the visible card |
| `TargetDestroyed` | The anchor / target is destroyed |
| `Escape` | `CloseOnEscape`; when light-dismiss is enabled, TeachingTip still reports `LightDismiss` through its own `closing` signal to preserve existing values |

The default reason is `Programmatic`. Repeated `close()` calls do not repeat
`closing`.

Application-level event filters may handle Escape only within the owning
top-level window. Native menus, other top-level windows, and another overlay
in the same window must handle their own key events first. CoachMark must not
close across windows or consume Escape before a menu or overlay above it.

### Independent `modal`, `dim`, and `closePolicy` properties

These properties control separate behaviors:

- `modal`: whether the owning window's background pointer input and shortcuts
  are blocked and keyboard focus is contained. When `true`, the scrim's
  `WA_TransparentForMouseEvents` is false.
- `dim`: whether to paint smoke. When `false`, the scrim remains unpainted but
  may still intercept input according to `modal`.
- `closePolicy` / light-dismiss: `NoAutoClose`, `CloseOnPressOutside`, and
  `CloseOnEscape` control implicit dismissal, not whether a scrim exists.

Combinations:

- Both false: no scrim unless blur is enabled; a blur-only scrim is transparent
  to pointer input.
- `modal` only: an input barrier with the default subtle blur; explicitly
  set the blur radius to 0 for an invisible barrier.
- `dim` only: visible smoke that does not intercept input. Outside presses may
  still dismiss the overlay according to `closePolicy` and reach the background.
- Both true: visible smoke that blocks input.

`Dialog::setSmokeEnabled(true)` is a legacy compatibility wrapper that enables
both `modal` and `dim`. `setSmokeEnabled(false)` disables both;
`isSmokeEnabled()` returns `true` only when both are `true`. This alias does not
change an explicit blur radius or the configured dim opacity. The automatic
blur follows the resulting modal state. Use `setModal` / `setDim` for
independent combinations. `DrawerView` keeps its own `ClosePolicy`
type, separate from `Popup::ClosePolicy`.

### NOTIFY and no-op writes

Bindable properties (`isOpen`, `modal`, `dim`, `closePolicy`,
`animationEnabled`, and equivalent properties already public on each component)
must:

- Provide a `NOTIFY` signal.
- Treat writing the current value as a no-op, without repeating `NOTIFY`.

Theme changes only repaint and refresh hosted content. They must not change
`isOpen`, placement, selected values, or content ownership, or emit open-state
signals as a result.

### Compatibility aliases

Keep legacy names at least until the next major version before considering
removal:

- `aboutToShow` ↔ `opening`
- `aboutToHide` ↔ `closing` (parameterless alias).
- `setIsOpen` / `isOpen` is the public state API; `open()` / `close()` are commands.
- `Dialog::isSmokeEnabled` / `setSmokeEnabled` ↔ the legacy smoke wrapper above.
- TeachingTip retains `CloseReason` and `closing(TeachingTip::CloseReason)`.

### Separate inheritance hierarchies

Do not merge `DrawerView`, `ComboBox`, `MultiSelectComboBox`, `SplitButton`,
`DropDownButton`, and `Dialog` into one overlay base class. For `SplitButton` /
`DropDownButton`, `isOpen` describes QMenu visibility, not a same-window overlay
phase. `ComboBox` and `MultiSelectComboBox` dropdowns continue to compose
`Flyout`. `fluent::overlay::OverlayCoordinator` remains an internal implementation.

## Preserved Differences And Deferred Work

`ComboBox` and `MultiSelectComboBox` dropdowns remain non-modal and undimmed.
The former retains its current index, editable text, and ListView single
selection. The latter retains immediate multi-selection, search, and
select-all over filtered results. `DrawerView` retains edge dragging,
normalized position, content-widget ownership, and its existing public
`ClosePolicy` API.

Keep `Popup::ClosePolicy` and `DrawerView::ClosePolicy` separate, and do not
make `DrawerView` inherit `Popup`. Any consolidation of these public APIs
requires a separate design and implementation task.

Use a separate `Window` for a system dialog that must cross application
boundaries. Do not route `ContentDialog` / `Dialog` through a native top-level
window instead.

<!-- docs-nav:bottom:start -->
---
[← Window Chrome Architecture](window-chrome.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Typography Resolution →](typography-resolution.md)
<!-- docs-nav:bottom:end -->
