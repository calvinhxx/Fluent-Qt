## MODIFIED Requirements

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

## ADDED Requirements

### Requirement: CalendarView wheel input SHALL remain cross-platform compatible
CalendarView SHALL support Windows mouse wheels, Windows precision touchpads, RDP-forwarded touchpad fallback events, macOS phase-based touchpads, and pixel-delta wheel devices without making one input class regress another. The implementation MUST base behavior on Qt wheel event shape and gesture lifetime, not on operating-system-specific device detection.

#### Scenario: Physical mouse wheel remains responsive
- **WHEN** CalendarView receives separate full-notch NoPhaseDiscrete mouse wheel events with a gap greater than the cluster gap
- **THEN** each event MAY commit one page if it reaches the page threshold
- **AND** each page commit MUST emit `visibleMonthChanged` exactly once

#### Scenario: Small-angle high-resolution wheel accumulates predictably
- **WHEN** CalendarView receives same-direction NoPhaseDiscrete wheel events with sub-notch angle deltas such as `±30` or `±60`
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
