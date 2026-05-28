## Context

CalendarView is currently a single QWidget that owns the header, Day grid, Month grid, Year grid, selection/focus states, wheel gesture state, page transition animation, and level zoom animation. The same `visibleMonth` date is used as the page anchor for all three content levels, even though Day pages represent months, Month pages represent years, and Year pages represent 12-year ranges.

The current wheel path uses a visible preview model (`m_wheelPreviewActive`, `m_previewVisibleMonth`, and a settle timer). Small wheel deltas can move the visual page state and later commit a full page change. That behavior does not match the Fluent Calendar reference, where calendar paging is discrete: small wheel input should not visibly drag a page or rebound, and a page transition should start only after the component decides to navigate.

ScrollView should be treated as the input-behavior reference, not the container architecture. ScrollView's ordinary wheel path lets small deltas remain proportional and natural, and its zoom path normalizes deltas and suppresses gesture tails carefully. CalendarView should adopt those input disciplines while staying a self-painted semantic calendar pager.

## Goals / Non-Goals

**Goals:**
- Replace visible wheel-preview/rebound behavior with invisible wheel accumulation followed by a discrete page transition.
- Prevent high-frequency touchpad, momentum, and RDP wheel tails from producing accidental multi-page jumps.
- Decouple Day, Month, and Year page identity and page math so each level can be reasoned about independently.
- Keep page navigation transitions separate from Day/Month/Year level zoom transitions.
- Preserve CalendarView and CalendarDatePicker public API compatibility where possible.
- Add tests that encode the expected wheel, page, and level behavior before and during implementation.

**Non-Goals:**
- Do not embed CalendarView in ScrollView or convert it to a QScrollArea/scrollbar-based control.
- Do not add a visible bounce/rebound interaction for CalendarView wheel input.
- Do not redesign DatePicker's column flyout as part of this change.
- Do not add range selection, multi-date selection, alternate calendar systems, or agenda content.
- Do not introduce new runtime dependencies.

## Decisions

### Use an internal semantic pager instead of ScrollView composition

CalendarView should remain a custom QWidget and self-paint its pages. A new private pager abstraction should normalize wheel/button input into semantic page navigation events. The pager owns transient navigation state such as source, direction, animation progress, and whether a wheel cluster has already committed a page.

Alternative considered: wrap each content level in ScrollView. This would provide natural pixel scrolling but would be the wrong semantic model: Calendar pages are fixed-size views that navigate by month/year/range, not overflowing content whose viewport should move continuously.

### Treat wheel input as invisible accumulation

Wheel input should update private accumulation state only. The UI must not translate the current page for sub-threshold wheel deltas. When the threshold is reached, or when a physical mouse wheel event represents a full discrete step, CalendarView starts the normal page transition animation and commits exactly one page for that gesture/cluster.

Alternative considered: keep visible preview and animate back on under-threshold input. This was rejected because the Fluent Calendar reference does not show a draggable page or rebound interaction, and because preview state makes `visibleMonth` and rendered content diverge for too long.

### Classify wheel input by event shape

CalendarView should classify wheel input similarly to existing collection controls:
- Phase-based: `phase() != Qt::NoScrollPhase`, usually native touchpad streams.
- No-phase pixel: `NoScrollPhase` with `pixelDelta()`, usually precision touchpad fallback paths.
- No-phase discrete: `NoScrollPhase` without `pixelDelta()`, covering physical mouse wheels, Qt5 default wheel input, and RDP-forwarded touchpad events.

Phase-based streams can use ScrollBegin/ScrollUpdate/ScrollEnd to reset, accumulate, and close gestures. No-phase streams need a time-based cluster gap so one forwarded gesture does not chain through multiple pages. Momentum events should be consumed after a page has been committed and must not trigger further navigation.

Alternative considered: handle only `angleDelta().y()` as one event equals one page. This caused over-sensitive scrolling and does not handle modern touchpads or RDP tails.

### Decouple page keys per content level

CalendarView should model the three levels with separate internal page keys:
- DayPageKey: year and month.
- MonthPageKey: year.
- YearPageKey: start year for the 12-year range.

Each page type should own title text, next/previous page calculation, range clamping, hit testing, and activation mapping. CalendarView can still expose `visibleMonth()` for compatibility, but it should no longer use one QDate as the only source of truth for all page levels.

Alternative considered: continue using `visibleMonth.addYears(...)` for Month and Year pages. This keeps the public API simple but couples unrelated page semantics and makes Year page range bugs difficult to isolate.

### Separate page navigation transitions from level zoom transitions

Page navigation is a vertical slide between two pages at the same level. Level navigation is a zoom transition between Day, Month, and Year levels. These transitions should not share previous/preview page state. Starting one transition should finish or cancel the other through a small, explicit state boundary rather than by reusing flags with different meanings.

Alternative considered: keep one set of transition fields for all motion. This is compact but has already made wheel preview, button navigation, and level transitions interfere with one another.

### Preserve public API while replacing internals

Existing CalendarView properties and signals should remain stable: selected date, visible month, date range, first day of week, content level, frame visibility, and date activation. CalendarDatePicker should continue composing CalendarView and should not need new public APIs for the pager redesign.

If an internal page key cannot be represented cleanly by the existing `visibleMonth()` contract, the implementation should map it conservatively to the currently focused Day-page month or the month implied by selected/default state. Any public API addition should be justified by tests and kept optional.

## Risks / Trade-offs

- [Internal rewrite could regress selection, focus, or range clamping] -> Preserve behavior with focused tests around selected date, focused date, min/max range, adjacent month activation, and CalendarDatePicker reopening.
- [No-phase discrete input cannot perfectly distinguish mouse wheels from RDP touchpad tails] -> Use a conservative cluster model: one page per active cluster while animation is running, then require a fresh cluster before another page.
- [Threshold tuning may feel inert on high-resolution wheels] -> Normalize pixel and angle deltas into a shared unit, add tests for sub-step accumulation, and leave constants local and easy to tune.
- [Decoupling page models may increase code size] -> Keep page types private to CalendarView and avoid new public classes unless future components need them.
- [Visual fidelity can drift while replacing animations] -> Keep existing Fluent token usage and add/refresh VisualCheck cases for wheel navigation, buttons, and level zoom.
