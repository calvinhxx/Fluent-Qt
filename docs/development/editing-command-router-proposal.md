# Editing Command Router API Proposal

## Status

- Capability track: Phase 2
- Proposal state: Accepted on 2026-07-28
- Implementation state: Complete; Capability Phase 2 review gate accepted on
  2026-07-28
- Public API impact: New installed `EditingCommandRouter` type

This document defines the accepted semantic editing-command layer that can
later feed `FluentMenuBar`, application menus, shortcuts, and Capability Phase
3 [command surfaces](command-bar-proposal.md). It intentionally does not make
the Capability Phase 1 private context-menu helper public.

## Goals

- Provide one stable set of editing `QAction` objects per top-level window.
- Route commands only to the currently focused supported Fluent editor.
- Keep native shortcuts, localization, enabled state, and command behavior
  coherent across menus and future command surfaces.
- Support `LineEdit`, `TextEdit`, and the existing `LineEdit` subclasses
  without exposing `TextEdit`'s private `QTextEdit`.
- Keep multiple windows independent and avoid application-global shortcut
  ownership.

## Non-goals

- No `CommandBar` or `CommandBarFlyout` implementation in this phase.
- No public editor-adapter interface or arbitrary third-party editor support.
- No document model, rich-text formatting, search/replace, or clipboard
  history.
- No business policy for whether an application should allow editing.
- No conversion of the private context-menu helper into installed API.

## Accepted Public Surface

The installed header is
`components/textfields/EditingCommandRouter.h`:

```cpp
namespace fluent::textfields {

class EditingCommandRouter final : public QObject {
    Q_OBJECT

public:
    enum class Command {
        Undo,
        Redo,
        Cut,
        Copy,
        Paste,
        Delete,
        SelectAll
    };
    Q_ENUM(Command)

    explicit EditingCommandRouter(
        QWidget* scopeWindow,
        QObject* parent = nullptr);

    QWidget* scopeWindow() const;
    bool hasActiveTarget() const;

    QAction* action(Command command) const;
    QList<QAction*> actions() const;
    bool canExecute(Command command) const;

    bool execute(Command command);
    void refresh();

signals:
    void activeTargetChanged(bool hasActiveTarget);
    void commandCapabilityChanged(
        EditingCommandRouter::Command command,
        bool enabled);
};

} // namespace fluent::textfields
```

No target widget pointer is exposed: applications receive semantic actions and
capability state, not `TextEdit` implementation details.

## Ownership and Lifetime

- The router is scoped to the top-level window resolved from `scopeWindow`.
- The router owns all returned `QAction` objects. Their addresses remain stable
  for the router lifetime; callers borrow them.
- One router is supported per top-level window. A second router for the same
  window is rejected: it retains stable but disabled actions, installs no
  shortcuts, is not attached to the window, and emits a `fluentqt.*` warning.
- Destroying the scope window clears the active target and disables every
  action. A router parented to that window is destroyed normally by Qt.
- Callers may customize action text, icons, and shortcuts. The router owns
  enabled state and trigger routing.

`QAction` is the intentional interoperability boundary: the same action can be
inserted into a Qt menu, `FluentMenu`, `FluentMenuBar`, or a later CommandBar
without duplicating command state.

## Focus and Window Scoping

- Listen to `QApplication::focusChanged` and ignore focus outside the scope
  window.
- A focused `LineEdit` is a direct target. `PasswordBox`, `NumberBox`, and
  `AutoSuggestBox` are handled through that inheritance path.
- When `TextEdit`'s private editor has focus, a private adapter maps it back to
  the owning public `TextEdit`; the private `QTextEdit*` never leaves the
  library implementation.
- Raw application `QLineEdit` and `QTextEdit` instances are not targets in the
  first public version. Supporting them later requires an explicit extension
  contract rather than silent global interception.
- Losing focus to menus or command presentation preserves the last valid
  editor through command activation and restores it afterward unless focus was
  explicitly redirected elsewhere. Ordinary focus transfer to another widget
  clears the target.

The final bullet requires focused interaction tests because a menu temporarily
takes focus while its action still needs to address the editor that opened it.

## Command Capability Contract

| Command | Enabled when |
|---|---|
| Undo | The active target reports an available undo step |
| Redo | The active target reports an available redo step |
| Cut | Editable selected text may be exported |
| Copy | Selected text may be exported |
| Paste | The target is writable and the clipboard has acceptable text |
| Delete | The target is writable and has a deletable selection |
| Select All | The target has content not already fully selected |

Additional rules:

- Read-only targets disable Undo, Redo, Cut, Paste, and Delete while retaining
  Copy and Select All when their selection/content conditions hold.
- Disabled targets expose no executable commands.
- `NumberBox` and `AutoSuggestBox` keep their existing validator and suggestion
  behavior because execution is delegated to the focused editor.
- Capability is revalidated immediately before execution, so a stale enabled
  action cannot mutate a target after focus, selection, read-only state, or
  clipboard content changes.

## PasswordBox Policy

Accepted safe policy:

- `PasswordRevealMode::Hidden` and `PasswordRevealMode::Peek` never advertise
  Cut or Copy, including while the press-and-hold peek button temporarily
  changes the underlying echo mode.
- A context-menu request while press-and-hold Peek is active ends the temporary
  reveal before the menu is built, so the inherited Fluent menu also keeps Cut
  and Copy disabled.
- `PasswordRevealMode::Visible` follows ordinary `LineEdit` capability.
- Paste, Delete, Select All, Undo, and Redo continue to follow writability and
  selection/history state.

This is deliberately stricter than inferring policy from the transient
`QLineEdit::echoMode()` alone.

## Standard Presentation Metadata

- Shortcuts use `QKeySequence::StandardKey` bindings and
  `Qt::WindowShortcut`.
- Default action captions are initialized from Qt's standard editing menu so
  the active platform translation is preserved.
- Every action has a stable object name and non-empty caption; menu surfaces
  use that caption as the accessible command label.
- Icons are not part of the semantic router contract. Capability Phase 3 may
  apply presentation-specific glyphs while reusing the same actions.
- If a language change occurs, a caption is refreshed only when it still
  matches the router's previous default. Caller-customized text is preserved.

## Refresh Sources

The private implementation observes:

- application focus changes;
- target undo/redo, selection, text, enabled, read-only, and destruction
  changes;
- `QClipboard::dataChanged`;
- scope-window language changes.

`refresh()` remains available for an application that changes editor policy
through a path with no observable Qt signal. It does not change focus.

## Rejected or Deferred Alternatives

- A process-global singleton is rejected because it couples independent
  windows and shortcut scopes.
- Publishing `TextEdit`'s private `QTextEdit*` is rejected because it freezes an
  implementation detail into installed API.
- A public editing-target interface is deferred until a real third-party
  extension use case exists.
- Reusing temporary standard-menu actions directly is rejected because those
  actions have menu-scoped ownership and unstable lifetime.
- Making the Capability Phase 1 menu helper public is rejected because
  presentation and command routing have different ownership contracts.

## Acceptance Matrix

The completed `test_editing_command_router` target covers:

- stable action identity and router-owned lifetime;
- hard rejection of a second router in the same top-level window, including
  shortcut and window-action isolation;
- focus entry, focus exit, and focus changes between two supported editors;
- no routing to unsupported widgets or another top-level window;
- Undo/Redo history changes;
- selection-driven Cut, Copy, Delete, and Select All state;
- clipboard-driven Paste state;
- read-only and disabled editors;
- `PasswordBox` Hidden, Peek, and Visible modes, including the Peek
  context-menu path;
- `NumberBox` validation and `AutoSuggestBox` inheritance;
- `TextEdit` routing without a public inner-editor accessor;
- menu focus transfer and focus restoration;
- native shortcuts and caller-overridden action presentation;
- Qt 5.15 and Qt 6 behavior.

## Accepted Decisions

1. Use `fluent::textfields::EditingCommandRouter` as the public name and
   namespace.
2. Use one router per top-level window; no process-global singleton.
3. Keep the stricter PasswordBox rule that Peek never exports text.
4. Keep raw Qt editors unsupported initially, without stealing their native
   editing shortcuts.
5. Use Qt-derived default captions with caller-overridable presentation.

These decisions were accepted on 2026-07-28. The implementation remains
separate from Capability Phase 3 command presentation.

## Completed Implementation Slices

1. Added target-behavior contracts and locked the public API declaration.
2. Implemented private LineEdit/TextEdit adapters and the window-scoped router.
3. Activated lifecycle, focus, clipboard, read-only, password, and
   menu-focus-restoration contracts.
4. Added a Gallery example that reuses the router actions in `FluentMenuBar`.
5. Verified installed headers, default/caller-owned captions, multi-window
   behavior, and Qt 5.15/Qt 6 compatibility.

## Validation Completed on 2026-07-28

- Windows Qt 6.9.3 and Linux Qt 5.15.2 each pass all 15
  `test_editing_command_router` contracts.
- The Gallery builds and its focused action-identity/focus-routing test passes;
  the complete sample-code/public-component audit also accepts the new card.
- Source-subproject and installed-package integration fixtures compile the
  category header and exported public type.
- The Capability Phase 1 `LineEdit`, `TextEdit`, and `PasswordBox` regression
  group discovers 43 tests on both Windows Qt 6.9.3 and Linux Qt 5.15.2:
  39 automated contracts pass and four manual visual tests are intentionally
  skipped by the automated preset.
