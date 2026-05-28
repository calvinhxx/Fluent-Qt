## Context

`WindowChromeCompat` centralizes platform chrome behavior for `view::windowing::Window`. The current Windows implementation handles `WM_NCHITTEST` and sends `WM_NCLBUTTONDOWN` from `beginPlatformSystemMove()` / `beginPlatformSystemResize()`, but it does not first prove that the widget is an eligible visible top-level custom chrome window. Calling `winId()` on an ordinary hidden `QWidget` can create a native handle, so the Windows path can report success in fallback-safety tests even though no user-visible system move/resize should begin.

The maximized black-border issue is in the same boundary: Windows custom chrome needs the client area expanded into the titlebar while keeping DWM in charge of the caption/frame styles needed for rounded corners, frame border, shadow, animations, Snap, and maximize geometry. `Qt::ExpandedClientAreaHint` alone can still leave QWidget content below a separate native titlebar row, while a fully frameless HWND removes too much DWM behavior.

## Goals / Non-Goals

**Goals:**

- Make `beginSystemMove()` and `beginSystemResize()` return `false` for hidden, non-top-level, unsupported, or custom-chrome-ineligible widgets.
- Preserve native Windows move, resize, titlebar hit-test, double-click restore, Snap, and DWM border/shadow/rounded-corner behavior for visible top-level custom chrome windows.
- Fix Windows maximized custom chrome geometry so the Fluent content fills the usable monitor work area without large black margins.
- Draw Windows caption buttons in Qt and reserve their TitleBar area so custom TitleBar actions do not overlap the window controls.
- Keep Windows SDK details inside `src/compatibility/`; `src/view/windowing/` remains platform-neutral.
- Cover the regression with targeted unit tests and, where native window-manager behavior is difficult to assert, a manual VisualCheck checklist.

**Non-Goals:**

- No broad public `Window` or `WindowChromeCompat` API redesign; small additive inset accessors are acceptable if needed by TitleBar layout.
- No new third-party frameless-window library.
- No macOS behavior change.
- No macOS custom replacement for native traffic-light buttons.

## Decisions

### Decision 1: Add a shared eligibility gate before any native move/resize request

`WindowChromeCompat::beginSystemMove()` and `beginSystemResize()` should validate eligibility before calling the platform hook or Qt `QWindow::startSystemMove()` / `startSystemResize()` fallback. The gate should check the invariant that the operation is user-visible and meaningful:

- `m_window` is non-null.
- `m_window->isWindow()` is true.
- `m_window->isVisible()` is true.
- `m_window->windowHandle()` already exists or can be obtained without creating a hidden test-only native window.
- Windows custom chrome is enabled when using the Windows native non-client path.
- Resize requests include at least one edge.

This prevents `winId()` from becoming an accidental success signal in tests. On Windows, the platform hook should also avoid calling `winId()` until after the common gate passes.

Alternative considered: update the test to expect Windows success. That would encode the current accidental behavior and still allow hidden widgets to receive native non-client commands, so it does not address the safety contract.

### Decision 2: Pass chrome options into Windows native operation hooks or gate them in common code

The current Windows hook signatures receive only `QWidget*` plus the pointer/edges. The fix can either extend the detail hooks to accept `WindowChromeOptions` or keep the signatures and gate in `WindowChromeCompat.cpp` before dispatch. Prefer the smaller change that keeps public headers stable:

- Add a private/common helper in `WindowChromeCompat.cpp` for move and resize eligibility.
- If Windows needs `options.useCustomWindowChrome` inside `WindowChromeCompat_win.cpp`, extend only the internal `detail::beginPlatformSystemMove/Resize` declarations and implementations.

Either approach must keep `WindowChromeCompat.h` public API unchanged unless implementation constraints make a private test seam necessary.

### Decision 3: Use a DWM custom frame while self-drawing Windows caption buttons

Windows should use Qt's expanded-client titlebar flags plus a small Win32 non-client override instead of a fully frameless HWND. The native style should keep `WS_CAPTION` and `WS_THICKFRAME` so DWM continues to own rounded corners, border/shadow, animations, Snap, and maximized work-area behavior. The visible minimize, maximize/restore, and close buttons are Qt controls owned by `Window`; the compatibility layer handles `WM_NCHITTEST` for custom draggable/resizable regions and may fall back to native non-client messages only after the common eligibility gate has passed.

Implementation direction:

- Set `Qt::ExpandedClientAreaHint` and `Qt::NoTitleBarBackgroundHint` for Windows custom chrome windows before show.
- Do not set `Qt::FramelessWindowHint` for the Windows path.
- Ensure the HWND preserves `WS_CAPTION`, `WS_THICKFRAME`, `WS_SYSMENU`, and min/max boxes after handle creation.
- Handle `WM_NCCALCSIZE` for custom chrome windows so QWidget client content starts in the native caption row instead of below a second system titlebar.
- Disable QWidget's automatic safe-area contents-margin behavior for the top-level Window, because the TitleBar reserves native system UI regions explicitly.
- Adopt WinUI-style ownership on Windows: the TitleBar surface, app icon/title area, actions, and caption buttons are all Qt content, while the HWND keeps DWM caption/frame styles only for system behavior.
- Avoid Qt native caption-button flags and do not clip the `_q_titlebar` helper; visible right-side controls come from `Window`'s self-drawn button host.
- Return custom hit-test results before DWM can claim the right caption-button area so Qt caption buttons receive normal client mouse events.
- Expose no native trailing titlebar inset on Windows; `Window` reserves the self-drawn caption-button host width through the existing TitleBar trailing reservation.
- Treat the self-drawn button area as a drag exclusion so clicks stay in Qt client space.

This keeps native frame math inside Qt/Windows DWM and lets `Window` continue to treat geometry as normal Qt layout.

Alternative considered: preserve native Windows caption buttons and clip Qt's titlebar helper to the right edge. That retains native button rendering, but it couples app layout to helper-HWND timing and safe-area quirks. The chosen path draws Windows buttons in Qt while retaining DWM styles for system behavior.

### Decision 4: Resync chrome geometry on state changes after native frame updates

`Window::changeEvent()` and `resizeEvent()` already call `updateChromeOptions()`. The implementation should verify this still happens after maximize/restore and that options reflect the current titlebar geometry, resize border width, drag exclusion rects, and safe-area-derived system button insets.

The preferred path is keeping macOS native leading insets in the compatibility layer and deriving Windows trailing reservation from the self-drawn caption-button host in `Window`, without adding platform macros or native headers to `src/view/windowing/`.

### Decision 5: Test the safe-return contract directly and native geometry with bounded assertions

Automated tests should cover:

- Hidden `QWidget` with `WindowChromeCompat` returns `false` for move and resize on Windows and non-Windows.
- Empty resize edges return `false`.
- Visible top-level Window keeps hit-test classification unchanged.
- Windows custom chrome preserves `WS_CAPTION`/`WS_THICKFRAME`, uses expanded client area flags, and reserves trailing self-drawn caption-button space.
- Windows custom chrome exposes visible self-drawn caption buttons whose click area remains client space rather than native caption hit-test space.
- Windows titlebar double-click maximizes and restores through the native path.
- A Windows-only maximized test verifies that a shown `Window` reaches maximized state and that its content host geometry approximately fills the window client area without large margins.

Native work-area exactness can vary with taskbar position, DPI, and CI window-manager permissions. Use tolerance-based assertions for geometry and keep a manual VisualCheck note for the full black-border reproduction.

## Risks / Trade-offs

- **[Risk] Eligibility checks become too strict and disable legitimate titlebar drags.** -> Mitigation: only require visibility/top-level/native-handle conditions before starting explicit move/resize; keep `WM_NCHITTEST` path active for normal OS-driven interactions.
- **[Risk] Self-drawn caption-button reservation gets out of sync with TitleBar height or window state.** -> Mitigation: create the buttons inside `Window`, reserve their host width through TitleBar, and resync on titlebar height, show, resize, and window-state changes.
- **[Risk] Returning handled native events too broadly can break Qt painting or resize.** -> Mitigation: handle only custom chrome Windows and only the specific messages/conditions needed for hit-test and maximized frame correction.
- **[Risk] CI cannot reliably assert full native maximize behavior.** -> Mitigation: keep the safety regression as a deterministic unit test and cover black-border behavior with bounded geometry assertions plus VisualCheck.
