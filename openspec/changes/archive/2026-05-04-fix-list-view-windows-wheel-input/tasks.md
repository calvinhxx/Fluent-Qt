## 1. Reproduce and Characterize Input Paths

- [x] 1.1 Review current `ListView::wheelEvent` NoPhaseDiscrete flow and identify where normal scrolling, boundary detection, cluster accumulation, and bounce entry interact.
- [x] 1.2 Add or adjust helper code in `TestListView.cpp` to create synthetic wheel events for vertical and horizontal ListView instances.
- [x] 1.3 Add failing tests for Windows-style single mouse wheel ticks that must scroll normally and not enter overscroll.
- [x] 1.4 Add failing tests for high-frequency Windows touchpad/RDP NoPhaseDiscrete clusters reaching the bottom boundary without bounce flapping.
- [x] 1.5 Add failing tests for reverse-direction recovery after boundary-tail events.

## 2. Refactor NoPhaseDiscrete Handling

- [x] 2.1 Update `ListView::wheelEvent` so NoPhaseDiscrete events attempt normal `QListView` scrolling before any boundary overscroll logic.
- [x] 2.2 Reset NoPhaseDiscrete cluster state whenever native scrolling changes the target scrollbar value.
- [x] 2.3 Reset cluster state on cluster gap timeout and dominant-direction sign changes.
- [x] 2.4 Consume same-direction NoPhaseDiscrete tail events at an already-hit boundary without extending or repeatedly restarting the bounded bounce.
- [x] 2.5 Ensure reverse-direction NoPhaseDiscrete events after boundary consumption resume normal scrolling immediately.

## 3. Bounce and Scrollbar Synchronization

- [x] 3.1 Keep PhaseBased and NoPhasePixel overscroll behavior unchanged except where required for stale-tail safety.
- [x] 3.2 Ensure bounce-running logic consumes same-direction NoPhaseDiscrete and NoPhasePixel tail events without desynchronizing native and Fluent scrollbars.
- [x] 3.3 Allow reverse-direction NoPhaseDiscrete recovery to clear stale boundary state and move content if the scrollbar can move.
- [x] 3.4 Confirm `syncFluentScrollBar()` and `syncFluentHScrollBar()` still reflect native scrollbar values after wheel handling.

## 4. Horizontal and Keyboard/Mouse Coverage

- [x] 4.1 Add horizontal `LeftToRight` wheel tests using dominant-axis NoPhaseDiscrete input.
- [x] 4.2 Add a test that keyboard selection/navigation still works after wheel input.
- [x] 4.3 Keep existing drag reorder and selection tests passing without behavioral changes.
- [x] 4.4 Add tests for Windows small-angle wheel sensitivity and bounded NoPhaseDiscrete boundary bounce.

## 5. Validation

- [x] 5.1 Build `test_list_view`.
- [x] 5.2 Run `QT_QPA_PLATFORM=offscreen SKIP_VISUAL_TEST=1 ./build/tests/views/collections/test_list_view`.
- [x] 5.3 Run a broader collections build if wheel changes affect shared delegate or scroll behavior.
- [x] 5.4 Run `openspec validate fix-list-view-windows-wheel-input`.
- [ ] 5.5 Manually verify on Windows with a physical mouse wheel and a precision touchpad when hardware is available.
