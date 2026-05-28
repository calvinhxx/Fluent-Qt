# calendar-view-pager Specification

## Purpose

Defines `CalendarView` pager behavior for discrete wheel navigation, Day/Month/Year page models, transition separation, and CalendarDatePicker compatibility.

## Requirements

### Requirement: CalendarView discrete wheel paging
CalendarView SHALL treat wheel input as discrete calendar page navigation. Sub-threshold wheel deltas MUST be accumulated invisibly and MUST NOT visibly translate, preview, rebound, or commit a calendar page. Wheel handling MUST distinguish phase-based, pixel-delta, and NoPhaseDiscrete input by Qt event signature so Windows and macOS devices remain compatible without hardware-specific detection.

#### Scenario: Small wheel delta does not move or commit a page
- **WHEN** CalendarView receives a wheel event inside its content area with a delta below the page threshold
- **THEN** CalendarView MUST keep the rendered page, `visibleMonth`, and page transition progress unchanged
- **AND** CalendarView MUST accept the event and keep only private accumulation state

#### Scenario: Accumulated wheel delta commits one page
- **WHEN** CalendarView receives multiple same-direction wheel events whose normalized accumulated delta reaches the page threshold
- **THEN** CalendarView MUST start one page transition in that direction
- **AND** CalendarView MUST update the committed page exactly once for that gesture or cluster

#### Scenario: Mouse wheel notch commits one page
- **WHEN** CalendarView receives a NoScrollPhase wheel event representing one full mouse wheel notch in the content area
- **THEN** CalendarView MUST navigate exactly one page in the corresponding direction
- **AND** CalendarView MUST NOT require a settle timer before committing that discrete page change
- **AND** CalendarView MUST mark the active NoPhaseDiscrete cluster as already committed so same-cluster tail packets cannot commit another page

#### Scenario: Under-threshold gesture ends without rebound
- **WHEN** CalendarView receives ScrollEnd after a phase-based gesture whose accumulated delta remains below the page threshold
- **THEN** CalendarView MUST clear the wheel gesture state
- **AND** CalendarView MUST NOT play a rebound animation or emit `visibleMonthChanged`

#### Scenario: Pixel-delta wheel input uses the same discrete page threshold
- **WHEN** CalendarView receives NoScrollPhase wheel input with a non-empty `pixelDelta()` inside its content area
- **THEN** CalendarView MUST accumulate the pixel delta toward the page threshold
- **AND** CalendarView MUST commit at most one page for the active pixel-delta gesture or cluster

#### Scenario: Phase-based touchpad gesture commits at most one page
- **WHEN** CalendarView receives a ScrollBegin, one or more ScrollUpdate events that reach the page threshold, and ScrollEnd
- **THEN** CalendarView MUST navigate at most one page for that phase-based gesture
- **AND** ScrollMomentum packets after the commit MUST NOT navigate another page

### Requirement: CalendarView wheel cluster protection
CalendarView SHALL prevent a single high-frequency or forwarded wheel gesture from producing multiple accidental page changes. NoScrollPhase input MUST be grouped into clusters using an event gap, and each active cluster MUST commit at most one page while navigation animation is in progress or while same-cluster tail packets continue to arrive.

#### Scenario: NoScrollPhase cluster commits at most one page
- **WHEN** CalendarView receives a high-frequency NoScrollPhase sequence from a touchpad fallback or RDP-forwarded gesture
- **THEN** CalendarView MUST navigate at most one page for that cluster
- **AND** same-direction tail events in the cluster MUST be accepted without triggering additional page changes

#### Scenario: Windows touchpad fallback wobble does not double page
- **WHEN** CalendarView receives a Windows-style NoPhaseDiscrete sequence with empty `pixelDelta()` where small opposite or same-direction tail packets arrive shortly after the first committed page
- **THEN** CalendarView MUST keep `visibleMonth` at the single committed target page
- **AND** CalendarView MUST emit `visibleMonthChanged` exactly once for that sequence

#### Scenario: Wheel input during page animation is consumed without queueing
- **WHEN** CalendarView has an active page transition because a wheel, button, or keyboard page navigation just committed
- **AND** CalendarView receives additional wheel input inside the content area
- **THEN** CalendarView MUST accept the wheel input
- **AND** CalendarView MUST NOT queue or commit another page after the active transition finishes

#### Scenario: Fresh cluster may navigate after animation settles
- **WHEN** CalendarView has completed a page navigation animation
- **AND** a later NoScrollPhase wheel event arrives after the cluster gap
- **THEN** CalendarView MUST treat the event as a fresh input cluster
- **AND** CalendarView MAY navigate another page if the fresh input reaches the page threshold

#### Scenario: Momentum tail does not navigate
- **WHEN** CalendarView receives ScrollMomentum after a wheel gesture has committed or ended
- **THEN** CalendarView MUST accept the momentum event
- **AND** CalendarView MUST NOT start another page navigation because of that momentum tail

#### Scenario: Direction change resets stale accumulation
- **WHEN** CalendarView has accumulated wheel input in one direction without committing a page
- **AND** the next wheel event requests the opposite direction
- **THEN** CalendarView MUST reset the stale accumulation before processing the new direction

#### Scenario: Direction change after a committed NoPhaseDiscrete cluster starts a fresh cluster only when it can be intentional
- **WHEN** CalendarView has already committed a page for a NoPhaseDiscrete cluster
- **AND** an opposite-direction event arrives within the cluster gap while page transition state is still active
- **THEN** CalendarView MUST accept the event as cluster tail
- **AND** CalendarView MUST NOT immediately navigate back to the previous page

### Requirement: CalendarView wheel input SHALL remain cross-platform compatible
CalendarView SHALL support Windows mouse wheels, Windows precision touchpads, RDP-forwarded touchpad fallback events, macOS phase-based touchpads, and pixel-delta wheel devices without making one input class regress another. The implementation MUST base behavior on Qt wheel event shape and gesture lifetime, not on operating-system-specific device detection.

#### Scenario: Physical mouse wheel remains responsive
- **WHEN** CalendarView receives separate full-notch NoPhaseDiscrete mouse wheel events with a gap greater than the cluster gap
- **THEN** each event MAY commit one page if it reaches the page threshold
- **AND** each page commit MUST emit `visibleMonthChanged` exactly once

#### Scenario: Small-angle high-resolution wheel accumulates predictably
- **WHEN** CalendarView receives same-direction NoPhaseDiscrete wheel events with sub-notch angle deltas such as `+/-30` or `+/-60`
- **THEN** CalendarView MUST keep accumulating them until the page threshold is reached
- **AND** CalendarView MUST commit no more than one page for the active cluster

#### Scenario: Windows precision touchpad fallback does not overshoot month
- **WHEN** CalendarView receives a fast NoPhaseDiscrete fallback sequence that represents one touchpad scroll gesture
- **THEN** CalendarView MUST navigate no more than one month at Day level
- **AND** CalendarView MUST navigate no more than one year page at Month level or one 12-year page at Year level

#### Scenario: CalendarDatePicker composition remains stable
- **WHEN** CalendarDatePicker hosts CalendarView and forwards user wheel interaction to the popup calendar
- **THEN** CalendarView wheel paging MUST follow the same cross-platform cluster rules
- **AND** CalendarDatePicker MUST still receive date activation normally when the user selects a date

#### Scenario: Keyboard and button navigation are unchanged
- **WHEN** a user navigates CalendarView with PageUp, PageDown, previous button, or next button after wheel input
- **THEN** CalendarView MUST keep the existing page navigation behavior for those inputs
- **AND** stale wheel gesture state MUST NOT block keyboard or button navigation

### Requirement: CalendarView level-specific page models
CalendarView SHALL model Day, Month, and Year levels as separate page types with independent page keys, titles, hit testing, next/previous page math, and activation behavior.

#### Scenario: Day level page key represents one month
- **WHEN** CalendarView is at Day level and navigates one page forward
- **THEN** CalendarView MUST move from the current month page to the next month page
- **AND** the day grid title and cells MUST represent that target month

#### Scenario: Month level page key represents one year
- **WHEN** CalendarView is at Month level and navigates one page forward
- **THEN** CalendarView MUST move from the current year page to the next year page
- **AND** the month grid title and cells MUST represent that target year

#### Scenario: Year level page key represents a 12-year range
- **WHEN** CalendarView is at Year level and navigates one page forward
- **THEN** CalendarView MUST move to the next 12-year range page
- **AND** the title and year cells MUST represent the new range

#### Scenario: Level activation maps to the correct lower-level page
- **WHEN** a user activates a year cell in Year level
- **THEN** CalendarView MUST switch to Month level anchored to that year
- **AND** when the user activates a month cell, CalendarView MUST switch to Day level anchored to that month

### Requirement: CalendarView transition separation
CalendarView SHALL keep page navigation transitions separate from Day/Month/Year level zoom transitions. A page navigation transition MUST only move between pages at the same content level, and a level transition MUST only move between content levels.

#### Scenario: Wheel navigation uses page transition only
- **WHEN** wheel input commits a page change at any content level
- **THEN** CalendarView MUST play the page navigation transition for that level
- **AND** CalendarView MUST NOT start a Day/Month/Year zoom transition

#### Scenario: Title drill-up uses level transition only
- **WHEN** the user activates the title button to move from Day to Month or from Month to Year
- **THEN** CalendarView MUST play the level zoom-out transition
- **AND** CalendarView MUST NOT modify page navigation transition state

#### Scenario: Cell drill-down uses level transition only
- **WHEN** the user activates a year or month cell to drill down
- **THEN** CalendarView MUST play the level zoom-in transition
- **AND** CalendarView MUST NOT treat the action as a wheel or button page navigation

#### Scenario: Starting one transition clears incompatible state
- **WHEN** CalendarView starts a page transition while a level transition is not idle, or starts a level transition while page navigation state is not idle
- **THEN** CalendarView MUST finish or cancel the incompatible transition through an explicit state reset
- **AND** CalendarView MUST leave no stale previous/preview page data behind

### Requirement: CalendarView public behavior compatibility
CalendarView SHALL preserve existing public date selection, date range, first-day-of-week, content-level, and CalendarDatePicker integration behavior while replacing the internal pager architecture.

#### Scenario: CalendarDatePicker continues to compose CalendarView
- **WHEN** CalendarDatePicker opens its calendar popup
- **THEN** it MUST still compose CalendarView, apply date range and first day of week, and receive `dateActivated` when the user picks a date

#### Scenario: Date range clamps page navigation
- **WHEN** CalendarView has min/max dates and the user tries to page before or after the selectable range
- **THEN** CalendarView MUST clamp navigation to the nearest valid page
- **AND** out-of-range day, month, or year cells MUST remain non-activating

#### Scenario: Preview navigation does not invent focus
- **WHEN** CalendarView navigates pages without an existing selected or focused date in the target page
- **THEN** CalendarView MUST NOT draw a misleading focus indicator on the first selectable day by default

#### Scenario: Public signals remain stable
- **WHEN** CalendarView commits a page navigation that changes the public visible month
- **THEN** CalendarView MUST emit `visibleMonthChanged` exactly once
- **AND** CalendarView MUST NOT emit selection or activation signals unless the user activates a date cell

### Requirement: CalendarView pager validation
The project SHALL provide automated tests and VisualCheck coverage for the redesigned CalendarView pager behavior.

#### Scenario: Automated tests cover wheel thresholds and tails
- **WHEN** `test_calendar_view` runs with visual tests skipped
- **THEN** it MUST cover sub-threshold wheel input, threshold commit, NoScrollPhase cluster tail suppression, momentum suppression, and direction-change reset

#### Scenario: Automated tests cover page levels
- **WHEN** `test_calendar_view` runs with visual tests skipped
- **THEN** it MUST cover Day, Month, and Year level page navigation, titles, hit tests, and drill-down/drill-up transitions

#### Scenario: CalendarDatePicker tests still pass
- **WHEN** `test_calendar_date_picker` runs with visual tests skipped
- **THEN** CalendarDatePicker popup behavior MUST remain compatible with the redesigned CalendarView pager

#### Scenario: VisualCheck demonstrates Fluent pager behavior
- **WHEN** the manual CalendarView or CalendarDatePicker VisualCheck is run
- **THEN** it MUST allow visual inspection of wheel navigation, right-side button page navigation, and title/cell level transitions without visible wheel rebound
