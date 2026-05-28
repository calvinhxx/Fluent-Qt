## 1. ToolTip API and State

- [x] 1.1 Add `animationEnabled` property, getter, and setter to `ToolTip`.
- [x] 1.2 Add the private animation members needed to track the active opacity transition and internal hide completion.
- [x] 1.3 Override `setVisible(bool)` so existing `show()` and `hide()` callers route through the animated visibility path.

## 2. Animation Implementation

- [x] 2.1 Create and configure a `QPropertyAnimation` targeting `ToolTip::windowOpacity`.
- [x] 2.2 Implement animated show by making the tooltip visible, starting from the current opacity, and fading to `1.0`.
- [x] 2.3 Implement animated hide by fading to `0.0` and only calling the base hidden state after the animation finishes.
- [x] 2.4 Handle repeated `show()`/`hide()` calls by stopping the current animation and reversing from the current opacity without changing caller-managed position.
- [x] 2.5 Keep disabled animation synchronous with opacity set to the correct steady-state value.

## 3. Tests

- [x] 3.1 Add unit coverage for `animationEnabled` default and synchronous disabled behavior.
- [x] 3.2 Add unit coverage that animated `hide()` keeps the tooltip visible until fade-out completes.
- [x] 3.3 Add unit coverage that `hide()` during entry reverses cleanly and ends hidden.
- [x] 3.4 Confirm existing text, margins, font, and window flag tests remain valid.

## 4. Verification

- [x] 4.1 Run the status-info ToolTip test target.
- [x] 4.2 Run the OpenSpec validation/status command for `add-tooltip-animation`.
