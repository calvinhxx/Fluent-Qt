## Context

`Label` currently wraps `QLabel` with Fluent typography and theme support. It does not own truncation state beyond the inherited `QLabel` text rendering behavior, and it has no hover behavior for full-text discovery. `ToolTip` now exists as a Fluent styled tooltip with opacity animation and can be reused for this interaction.

The implementation needs to distinguish between configured elision and actual truncation. A tooltip should not appear merely because an elide mode is set; it should appear only when the full text does not fit in the current label content area.

## Goals / Non-Goals

**Goals:**
- Add a Label-level elide mode API that can render elided text in constrained widths.
- Show the existing Fluent `ToolTip` on hover when the current Label text is actually truncated.
- Keep the tooltip text equal to the full unelided Label text.
- Update truncation state when text, font, geometry, elide mode, or theme typography changes.
- Preserve existing Label construction, typography, theme refresh, and `QLabel` inheritance behavior.

**Non-Goals:**
- Do not add rich-text parsing or multiline tooltip formatting.
- Do not replace all existing uses of `QLabel::setToolTip()`.
- Do not change `InfoBar` or other callers that already do manual eliding unless they opt into the new Label API later.
- Do not introduce a global tooltip manager.

## Decisions

1. Store the full Label text separately from the rendered elided text.

   Rationale: once `QLabel::setText()` receives an elided string, `QLabel::text()` no longer contains the original content. `Label` needs the original string to decide truncation and to populate `ToolTip`.

   Alternative considered: computing tooltip text from `QLabel::text()` on hover. That loses the original text after truncation and cannot satisfy the requirement.

2. Add a `textElideMode` property with `Qt::ElideNone` as the default.

   Rationale: this matches Qt's existing `Qt::TextElideMode` vocabulary and keeps default Label behavior unchanged. When the mode is `Qt::ElideNone`, `Label` should behave like a normal label and not create a tooltip for elision.

   Alternative considered: automatically eliding all labels when they overflow. That would be a behavioral change for existing labels and could hide content unexpectedly.

3. Recompute elision from the label's current content width.

   Rationale: the same text may be elided or fully visible depending on resize, font, typography, and margins. Recomputing in `resizeEvent`, `setText`, `setFont`, `setFluentTypography`, and `onThemeUpdated` keeps the visible text and tooltip state accurate.

   Alternative considered: compute only in `paintEvent`. That keeps drawing accurate but makes hover state harder to test and can create unnecessary work during every paint.

4. Own a lazy `view::status_info::ToolTip` instance inside `Label`.

   Rationale: creating the tooltip only when needed avoids overhead for normal labels and reuses the Fluent tooltip visual/animation. The tooltip should be top-level or parented so it can appear above the label without activating the window.

   Alternative considered: use Qt's built-in tooltip API. That would not match the custom Fluent `ToolTip` component and would bypass the existing animation behavior.

## Risks / Trade-offs

- [Risk] Eliding the inherited `QLabel` text can surprise code that expects `text()` to return the full string. -> Mitigation: override `text()` in `Label` to return the full stored text and use an internal helper for rendered text updates.
- [Risk] Rich text or mnemonic text could make width calculation inaccurate. -> Mitigation: scope this feature to plain static Label text initially and document tests around plain text.
- [Risk] Tooltip positioning can drift near screen edges. -> Mitigation: use simple label-centered placement first and clamp to available screen geometry during implementation if the existing helpers make it straightforward.
- [Risk] Tooltip lifetime can outlive the label during hover. -> Mitigation: make the tooltip owned by Label and hide/delete it in the Label destructor.
