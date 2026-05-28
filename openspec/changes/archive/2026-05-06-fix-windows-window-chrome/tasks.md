## 1. Native Operation Safety

- [x] 1.1 Add a shared eligibility check in `WindowChromeCompat` for system move/resize requests.
- [x] 1.2 Ensure hidden, non-top-level, handle-less, or custom-chrome-ineligible widgets return `false` without creating a native handle for the operation.
- [x] 1.3 Update Windows move/resize hooks so they do not call `winId()` or send `WM_NCLBUTTONDOWN` before eligibility has passed.
- [x] 1.4 Preserve valid Windows top-level custom chrome move/resize behavior for titlebar drag and resize borders.

## 2. Windows DWM Client-Area Geometry

- [x] 2.1 Apply expanded-client-area titlebar flags for Windows custom chrome.
- [x] 2.2 Preserve DWM-managed caption/frame styles while expanding the client area into the titlebar.
- [x] 2.3 Stop exposing native Windows trailing caption-button insets and reserve the self-drawn caption-button host instead.
- [x] 2.4 Ensure maximized/restored windows fill the usable work area without large black margins.

## 3. Window Integration

- [x] 3.1 Verify `Window::changeEvent`, `resizeEvent`, and maximize/restore slots resync chrome options after state changes.
- [x] 3.2 Keep `src/view/windowing/` free of Windows SDK headers and platform macros.
- [x] 3.3 Keep titlebar hit-test, drag exclusions, resize border width, and double-click restore behavior working after the geometry fix.
- [x] 3.4 Reserve self-drawn Windows caption-button space so custom TitleBar actions do not overlap window controls.

## 4. Tests and Verification

- [x] 4.1 Update `WindowChromeCompatFallbackIsSafe` or add targeted tests so hidden QWidget move/resize returns `false` on Windows.
- [x] 4.2 Add tests for empty resize edges and ineligible widgets returning `false`.
- [x] 4.3 Add a Windows-only maximized Window geometry regression test with tolerance for DPI and taskbar work-area differences.
- [x] 4.4 Run the windowing unit tests on Windows and confirm `WindowTest.WindowChromeCompatFallbackIsSafe` passes.
- [x] 4.5 Add Windows regression tests for DWM caption preservation and native titlebar double-click maximize/restore.
- [x] 4.6 Add a Windows regression test that custom TitleBar content shares the native caption row instead of rendering as a second titlebar row.
- [x] 4.7 Add a Windows regression test for WinUI-style titlebar ownership: custom titlebar content with trailing self-drawn caption buttons reserved.
- [x] 4.8 Ensure Windows self-drawn caption buttons drive minimize, maximize/restore, and close window slots.
- [x] 4.9 Ensure Windows caption-button hit-test areas remain client exclusions instead of native caption-button regions.
- [x] 4.10 Manually verify the Window VisualCheck on Windows: normal titlebar layout, maximize/restore, titlebar double-click, and maximized geometry without black margins.
	- Verified on Windows desktop on 2026-05-06.
