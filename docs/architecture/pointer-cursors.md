# Mouse cursors in Widgets

> **Status:** Current guide. The [component audit](pointer-cursor-audit.md) records the pre-fix baseline.

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Architecture](README.md) › Runtime contracts

[← Window Chrome Architecture](window-chrome.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Widget cursor audit →](pointer-cursor-audit.md)
<!-- docs-nav:top:end -->

Choose the system cursor for the action available at the pointer position. In
NumberBox, the editable text, step buttons and optional drag handle each have
their own hit areas. The same rules apply to
C++ and PySide6 widgets and to the Widgets-based WebAssembly Gallery.

## Cursor by hit area

| Hit area | Target behavior |
|---|---|
| Editable or selectable text | Use the editor's text cursor (I-beam while editing). Let the native editor update its cursor on a read-only transition and preserve text selection and copying. |
| Ordinary commands, checkboxes and field actions | Use the standard arrow. Examples include stepping a number, clearing a field and revealing a password. Moving across the icon, padding or edge inside the same button gives the same cursor. |
| Hyperlinks | Use a pointing hand on the active link. The adjacent label or empty space keeps its own cursor. |
| Decorations and disabled actions | A decoration follows the underlying hit area. An unavailable action gives no link, drag or resize cue. A disabled step button inside a field still occupies a button area and shows an arrow. |
| Number scrub handle, splitter, window border | Show the horizontal adjustment, oriented splitter or edge/corner resize cursor respectively. Use the same geometry and enabled-state checks as the operation itself. Native window resizing follows the window system. |
| Pannable image or canvas | Show an open hand where panning is possible and a closed hand while dragging. Fit-to-view content and overlaid command buttons have their own idle behavior. |
| Precision picking and drag-and-drop | A precision picker may use a crosshair. Hide the pointer only during a gesture with a visible position marker. File and item drags use Qt's negotiated copy, move or rejected-drop feedback. |

Some shipped controls use a pointing hand for other actions, including
ToggleSwitch, RatingControl, FlipView navigation and PipsPager. Treat those as
existing component behavior. Review each in its own interaction context
before changing its default. Cursor changes supplement keyboard, focus,
disabled and pressed-state feedback.

## Ownership and restoration

- **Built-in child button.** Set its arrow once where the field creates it.
  This gives it an independent cursor even inside an I-beam editor or on a
  draggable surface. Applications keep control of widgets supplied through
  `setTrailingWidget()`. Decorative children that pass mouse events through
  follow the hit area underneath.
- **Painted hit area.** The widget receiving pointer events owns the temporary
  shape. Use its existing hit test, including the item's command availability.
  For a scrollable item view this is usually the viewport. Update only on a
  change of target, availability or gesture state. Re-evaluate a stationary
  pointer when scrolling, geometry or the model changes what is under it.
- **Temporary interaction.** On the first override, retain the widget's
  previous cursor and whether it was explicitly set (`Qt::WA_SetCursor`).
  A gesture takes precedence over hover. On release, resolve the current hit
  again. On leaving the area, cancellation, hide, disable, deactivation or a
  lost mouse grab, remove the temporary state as appropriate. Restore an
  explicit cursor by value, including custom cursors. Restore inheritance
  with `unsetCursor()` so a later parent change still propagates. A caller
  cursor installed during the gesture takes precedence over the saved one.
  Keep restoration local to the owner and safe to run twice.
- **Application and platform.** Use standard Qt cursor shapes so the platform
  supplies the artwork and size. Leave the application's override stack to
  the application (for example, a wait cursor). Window-edge hit testing and
  native resize sessions remain with window chrome. Local hover requires no
  application-wide event filter, timer, paint or public cursor API.

Qt documents cursor inheritance and the application override stack in
[QWidget](https://doc.qt.io/qt-6/qwidget.html#cursor-prop) and
[QGuiApplication](https://doc.qt.io/qt-6/qguiapplication.html#setOverrideCursor).

## Private cursor adapter

The reusable Qt widget operation lives in
[WidgetCursorCompat_p.h](../../src/compatibility/private/WidgetCursorCompat_p.h), in `::compatibility::detail`.
The component selects the shape from its existing hit test and gesture state.
The adapter owns only the temporary write to one QWidget. Qt already supplies
the cursor vocabulary, including system artwork, high-DPI behavior and user
preferences. A parallel Fluent cursor enum would duplicate it.

The interface binds two operations to one receiver.

```cpp
class ScopedWidgetCursor {
public:
    explicit ScopedWidgetCursor(QWidget* receiver);
    ~ScopedWidgetCursor();
    void apply(Qt::CursorShape shape);
    void clear();
};
```

Use a noncopyable object on the GUI thread. A receiver needing temporary
feedback has one writer. A NumberBox can change from hover to drag using the
same instance. A file view binds to its current viewport; replacing it ends
that instance. Window chrome coordinates edge hover and manual resize on the
same top-level receiver. The overlay has a separate receiver. This avoids
competing snapshots on one widget.

Views allocate the observer on their first active hit and reuse it. An
idle refresh performs no cursor write. Installed widget headers forward-declare
the private type. Fixed field buttons set their cursor once at construction.

| State | Event | Action |
|---|---|---|
| Idle | Clear or cancel | Do nothing. A component can safely refresh its layout while the application owns the cursor. |
| Idle | First apply | Record `WA_SetCursor` and the full explicit `QCursor`, including image and hot spot. Start observing `CursorChange` on this receiver only, then apply the requested shape. |
| Active | Same shape, or transition between hover and drag | Skip a repeated write. Apply a different shape while retaining the original snapshot. |
| Active | External cursor write or unset | Yield to that write, including a write of the current shape. Suppress further writes for the interaction. Its end clears the suppression. |
| Active | End or interruption | Detach the observer before restoration. Restore the saved explicit cursor or resume inheritance. At a normal release over a valid hover region, the component can apply its hover shape first. |
| Any | Receiver destroyed | A guarded pointer makes subsequent cleanup harmless. Destruction of the helper while the receiver lives ends its ownership. |

Distinguish the adapter's own Qt change notifications from external writes.
An application's call to `setOverrideCursor()` is outside the receiver and
has visual precedence. A genuine drop operation remains under Qt's drag
manager. A client-side resize fallback may use the helper only for the part
of the session already owned by window chrome.
Observe only while active. No component-wide mouse tracking, global cursor
stack, per-item QObject or additional repaint is required by the adapter.
Library-owned buttons with a fixed arrow can call `setCursor(Qt::ArrowCursor)`
once at construction. Native text editors, application-provided widgets and
Qt drag-and-drop continue using their existing input paths.

The [cursor regression suite](../../tests/components/TestWidgetCursors.cpp)
exercises inherited and explicit cursors, custom pixmaps and hot spots,
same-shape caller writes, re-entry, destruction and repeated hover requests.
The [PySide6 binding tests](../../bindings/pyside6/tests/test_bindings.py)
also exercise both NumberBox button layouts, stationary hover at a range
limit and a Python cursor override during a native scrub gesture.

## Component integration

- LineEdit, NumberBox, PasswordBox, AutoSuggestBox and TagInput set an arrow
  on their built-in actions. At a numeric limit, Qt can resolve the disabled
  step button's cursor on the parent NumberBox. The editor uses the button's
  hit area to keep the arrow there, including when scrubbing is toggled under
  a stationary pointer. The scrub handle uses a horizontal adjustment cursor.
- Dialog tracks a drag from the accepted left press. ColorPicker releases
  its hidden cursor when a pick finishes or loses the mouse grab.
- SplitView and ImageViewer scope resize and pan feedback to their own
  surfaces. FileListView updates the viewport cursor when a row command changes.
  FlipView and PipsPager recheck painted navigation targets on page changes.
- Window chrome combines edge hover and manual resize before mirroring the
  cursor onto the native host. Application-level busy cursors remain under
  Qt's override stack.

## Acceptance

| Exercise | What to verify |
|---|---|
| Gallery → Text fields → NumberBox → **Quantity and precision** | In Split, Compact and Inline, move from text to each step button and back. Check the whole button and a disabled button at a range limit. Hidden buttons leave no active hit area. Stepping, holding to repeat and selecting text still work. |
| NumberBox → **Drag to adjust a value** | Cross the handle boundary, drag out and release, then repeat with Escape, focus loss and scrubbing switched off. Check the cursor and value separately. Exercise the editor default, a caller-set cursor and an inherited cursor whose parent changes. |
| Other fields | Hit-test the visible clear, reveal and query buttons. The editor on either side retains text selection. Exercise read-only and disabled states and a caller-supplied trailing widget with its own cursor. |
| Painted targets and window chrome | Move between active and inactive targets, scroll a row under a stationary pointer, interrupt a drag and open a second window. Verify restoration, including an active application wait cursor. |

Automated checks should hit the actual child or painted coordinates, inspect
the effective cursor and exercise the original action. Use the Windows Qt
6.2.4 Gallery for the reported pointer transition. Check representative
macOS, Linux and WebAssembly runs separately, including the browser cursor
over the Gallery canvas. An offscreen widget property records inheritance;
native pointer appearance needs an on-screen check.

<!-- docs-nav:bottom:start -->
---
[← Window Chrome Architecture](window-chrome.md) · [Contents](../SUMMARY.md) · [Architecture index](README.md) · [Widget cursor audit →](pointer-cursor-audit.md)
<!-- docs-nav:bottom:end -->
