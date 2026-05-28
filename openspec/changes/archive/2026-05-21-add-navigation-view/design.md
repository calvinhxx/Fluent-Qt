## Context

`NavigationView` should be a shell layout primitive, not a full WinUI item control. The application already has reusable controls such as `ListView`, `Button`, `Label`, and `StackContentHost`. Keeping item models, selection, nested expansion, settings/search/back commands, and page selection at the application layer makes the component easier to reason about and avoids duplicating list behavior inside `NavigationView`.

## Goals / Non-Goals

**Goals:**

- Provide a Fluent `NavigationView` shell container under `src/view/navigation/`.
- Support `Auto`, `Left`, `LeftCompact`, `LeftMinimal`, and `Top` display modes.
- Provide three chrome slots: header, main, and footer.
- Lay chrome slots vertically in side modes and horizontally in top mode.
- Use `StackContentHost` as the content surface.
- Expose deterministic chrome/content geometry for tests and application integration.

**Non-Goals:**

- Do not own navigation items, item models, item selection, item invocation, nested expansion, or overflow.
- Do not provide built-in settings/search/back/pane-toggle commands.
- Do not own routing, URL state, or page selection policy.
- Do not implement a native title bar or platform chrome.
- Do not introduce a delegate framework or new third-party dependencies.

## Decisions

### 1. NavigationView is a shell coordinator

`NavigationView` inherits `QWidget`, `FluentElement`, and `view::QMLPlus`. It owns a `StackContentHost` and three optional chrome widgets:

```text
NavigationView
├─ headerChromeWidget
├─ mainChromeWidget
├─ footerChromeWidget
└─ StackContentHost
```

The chrome widgets can be `ListView`, `Button` rows, custom panels, or any QWidget chosen by the application.

### 2. Side mode is vertical chrome + content

Side modes arrange chrome as:

```text
┌──────────────────────────────────────┐
│ NavigationView                        │
│ ┌──────────────┐ ┌─────────────────┐ │
│ │ Header       │ │ StackContentHost │ │
│ │ Main         │ │                 │ │
│ │ Footer       │ │                 │ │
│ └──────────────┘ └─────────────────┘ │
└──────────────────────────────────────┘
```

Header uses its preferred height, footer uses its preferred height, and main receives the remaining chrome height. `Left` reserves the expanded pane width. `LeftCompact` reserves the compact rail when closed and the expanded pane width when opened. `LeftMinimal` reserves no chrome width when closed and overlays the expanded chrome when opened.

### 3. Top mode is horizontal chrome + content

Top mode arranges chrome as:

```text
┌──────────────────────────────────────┐
│ Header | Main | stretch | Footer      │
├──────────────────────────────────────┤
│ StackContentHost                      │
└──────────────────────────────────────┘
```

Header and footer use preferred widths. Main uses preferred width when available. The unused space between main and footer acts as stretch.

### 4. Content belongs to StackContentHost

`NavigationView` exposes `contentHost()` and assigns it the current content geometry. Applications insert pages and choose current pages directly through `StackContentHost`. This keeps page routing and navigation item selection outside the shell component.

### 5. Painting is limited to shell surfaces

`NavigationView` paints only the base canvas, content background, chrome background, and divider line. Chrome widgets paint themselves. This keeps visual responsibility local to the widgets supplied by application code.

## Risks / Trade-offs

- [Risk] Applications must compose their own chrome widgets. -> Mitigation: this is intentional and keeps the component flexible; tests and VisualCheck show the expected composition.
- [Risk] Existing full NavigationView APIs are removed. -> Mitigation: this change is still active and unarchived, so the public surface can be corrected before finalizing.
- [Risk] Top overflow and nested item behavior are not built in. -> Mitigation: use existing list/menu components or application-level logic where those behaviors are actually needed.
