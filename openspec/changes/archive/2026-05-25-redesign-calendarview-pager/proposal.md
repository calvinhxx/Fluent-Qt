## Why

CalendarView scrolling currently behaves like a custom preview pager: small wheel deltas can start a page preview and later commit a full month/year change, which does not match the Fluent Calendar behavior shown in the reference. The same state also drives Day, Month, and Year pages through a shared `visibleMonth` model, making wheel bugs hard to isolate and regressions likely when tuning animation.

## What Changes

- Redesign CalendarView paging so wheel input is accumulated invisibly and only triggers a page change after a discrete wheel step or an explicit threshold is reached.
- Remove visible wheel-preview and rebound semantics from CalendarView; page transitions should start only when a page change is actually committed.
- Use ScrollView as the input-behavior reference for delta normalization, momentum/tail suppression, and natural small-delta handling, without embedding CalendarView in `ScrollView` or changing it into a pixel-scroll container.
- Decouple Day, Month, and Year page models so each level owns its page key, title, hit testing, next/previous page math, and activation behavior.
- Separate level zoom transitions from page navigation transitions so title drill-up/drill-down and wheel/button page navigation cannot corrupt each other's state.
- Preserve the existing public CalendarView and CalendarDatePicker API unless a compatibility issue is discovered during implementation.

## Capabilities

### New Capabilities
- `calendar-view-pager`: CalendarView page navigation, wheel input handling, and Day/Month/Year page-level behavior.

### Modified Capabilities

## Impact

- Affected components: `src/view/date_time/CalendarView.*`, `CalendarDatePicker.*`, and any DatePicker flyout code that composes or mirrors CalendarView behavior.
- Affected tests: `tests/views/date_time/TestCalendarView.cpp`, `TestCalendarDatePicker.cpp`, and related DatePicker tests where flyout scrolling is covered.
- No new runtime dependencies are expected.
- The change is intended to preserve source compatibility for existing CalendarView consumers while replacing the internal paging architecture.
