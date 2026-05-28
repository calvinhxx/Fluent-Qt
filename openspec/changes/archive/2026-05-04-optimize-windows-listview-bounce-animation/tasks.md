## 1. Bounce Lifecycle

- [x] 1.1 Review the current `ListView::wheelEvent`, `startBounceBack`, and NoPhaseDiscrete boundary-tail paths to identify where timer or animation restarts can occur.
- [x] 1.2 Add minimal private state/helpers needed to track the active NoPhaseDiscrete boundary rebound direction and whether a rebound is armed or running.
- [x] 1.3 Reset the rebound lifecycle state on bounce completion, reverse-direction recovery, cluster reset, flow-relevant cleanup, and destruction-safe timer/animation shutdown.

## 2. Animation Behavior

- [x] 2.1 Refactor NoPhaseDiscrete boundary-tail handling so the first boundary event sets one bounded overscroll value and starts or arms one bounce-back sequence.
- [x] 2.2 Consume same-direction NoPhaseDiscrete tail events without restarting the bounce timer, restarting the running animation, or increasing active overscroll beyond the bounded feedback value.
- [x] 2.3 Ensure bounce-back starts from the current active-axis overscroll value and animates continuously back to `0.0`.
- [x] 2.4 Preserve reverse-direction NoPhaseDiscrete recovery so input that can move back into content clears stale boundary state and scrolls normally.
- [x] 2.5 Keep PhaseBased and NoPhasePixel behavior compatible with the existing macOS and pixel-delta paths.

## 3. Tests

- [x] 3.1 Add or update `TestListView` coverage for a NoPhaseDiscrete boundary event starting a single bounded rebound while the scrollbar remains pinned.
- [x] 3.2 Add coverage showing repeated same-direction NoPhaseDiscrete tail events do not restart, extend, or destabilize an active bounce.
- [x] 3.3 Add coverage showing reverse-direction NoPhaseDiscrete input during or after boundary feedback resumes normal scrolling.
- [x] 3.4 Keep existing wheel, keyboard selection, horizontal flow, and macOS PhaseBased regression tests passing.

## 4. Verification

- [x] 4.1 Run `test_list_view` with `QT_QPA_PLATFORM=offscreen SKIP_VISUAL_TEST=1`.
- [ ] 4.2 When available, manually verify on Windows with mouse wheel, precision touchpad, and RDP-forwarded touchpad gestures.
  - Not available in this non-interactive run; synthetic Windows-style NoPhaseDiscrete coverage passed.
- [x] 4.3 Confirm no public `ListView` API, model/delegate behavior, selection behavior, drag reorder behavior, or `ScrollBar` API changed.
