## Why

Windows `WindowChromeCompat` currently reports successful system move/resize operations in a fallback-style unit test where no native drag operation should be started, causing `WindowChromeCompatFallbackIsSafe` to fail. The same Windows custom chrome path can also leave large black margins after maximizing, making the `Window` look smaller than the maximized screen instead of filling the available work area.

## What Changes

- Tighten Windows system move/resize success semantics so `beginSystemMove()` and `beginSystemResize()` only return `true` when a native operation is actually eligible to start.
- Preserve safe fallback behavior for hidden, non-top-level, unsupported, or otherwise ineligible widgets by returning `false` without crashing or mutating window state.
- Fix Windows maximized custom chrome geometry so the client content fills the maximized work area and does not leave thick black borders around the visible content.
- Preserve DWM-managed caption/frame style, rounded corners, border/shadow, titlebar drag, resize, double-click maximize/restore, and Snap behavior for eligible Windows top-level windows while drawing Windows caption buttons in Qt.
- Ensure platform titlebars use platform-appropriate ownership: macOS reserves the leading native traffic-light region, while Windows owns the full TitleBar in Qt and reserves only the self-drawn caption-button area.
- Reserve self-drawn caption-button space in the custom TitleBar so application actions do not overlap the window controls.
- Add or update targeted tests for Windows fallback safety and maximized custom chrome geometry.
- Keep public API changes small and additive where system titlebar insets must be exposed to the TitleBar layout.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `window-platform-compatibility`: Tighten the WindowChromeCompat contract for Windows native-operation eligibility, fallback-safe return values, and maximized custom chrome frame geometry.
- `fluent-window`: Tighten the Window visual contract so maximized top-level windows fill the usable screen area without black margins while preserving content hosting and titlebar behavior.

## Impact

- Code: `src/compatibility/WindowChromeCompat.cpp`, `src/compatibility/WindowChromeCompat_win.cpp`, and `src/compatibility/WindowChromeCompat.h`.
- Code: `src/view/windowing/TitleBar.*` and `src/view/windowing/Window.*` for system titlebar inset reservation and resync.
- Tests: `tests/views/windowing/TestWindow.cpp` for fallback safety, move/resize return values, and maximized geometry behavior.
- Specs: existing `openspec/specs/window-platform-compatibility/spec.md` and `openspec/specs/fluent-window/spec.md` receive delta requirements.
- No new dependencies.
