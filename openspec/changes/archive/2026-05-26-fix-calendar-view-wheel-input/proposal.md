## Why

CalendarView can accidentally page twice on Windows precision touchpad input because Windows/Qt may deliver touchpad motion as high-frequency `NoScrollPhase` wheel events without `pixelDelta()`, which looks like ordinary discrete wheel input. ListView already fixed the same class of issue by treating Qt wheel event signatures carefully instead of guessing the physical device, and CalendarView should adopt the same cross-platform discipline.

## What Changes

- Tighten CalendarView wheel handling so one Windows NoPhaseDiscrete touchpad or RDP cluster commits at most one calendar page.
- Preserve physical mouse wheel behavior: a full wheel notch should still navigate exactly one page, and small wheel deltas should accumulate predictably.
- Preserve macOS phase-based touchpad behavior: `ScrollBegin`, `ScrollUpdate`, `ScrollMomentum`, and `ScrollEnd` should remain gesture-scoped and should not create extra discrete page commits.
- Treat `NoScrollPhase` + non-empty `pixelDelta()` as a continuous/pixel-capable input path, separate from empty-pixel discrete fallback input.
- Ensure wheel input during page or content-level animations is consumed without queueing another page change.
- Add tests that reproduce Windows touchpad fallback clusters, mouse wheel notches, small-angle deltas, pixel-delta touchpad input, phase-based touchpad input, direction changes, and animation tails.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `calendar-view-pager`: Clarify and tighten CalendarView wheel paging so macOS/Windows touchpads, Windows mouse wheels, NoPhaseDiscrete fallback clusters, and pixel-delta input each navigate predictably without duplicate page commits.

## Impact

- Code: `src/view/date_time/CalendarView.h` and `src/view/date_time/CalendarView.cpp` for private wheel gesture state and `wheelEvent` logic.
- Tests: `tests/views/date_time/TestCalendarView.cpp` for synthetic mouse wheel, touchpad, and Windows fallback wheel event patterns.
- Specs: `openspec/specs/calendar-view-pager/spec.md` receives delta requirements.
- No public CalendarView API changes and no new dependencies.
