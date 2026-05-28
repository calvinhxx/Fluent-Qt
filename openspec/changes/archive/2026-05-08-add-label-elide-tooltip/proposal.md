## Why

`Label` is often used in constrained layouts where long text is elided, but users currently have no built-in way to inspect the full value. Showing the complete text through the existing Fluent `ToolTip` on hover keeps dense UI readable without forcing each caller to implement duplicate hover behavior.

## What Changes

- Add Label support for configuring text elide behavior.
- When Label text is actually elided, hovering the Label shows a Fluent `ToolTip` containing the full, unelided text.
- The tooltip appears only when elision is active and the rendered text is truncated; non-elided labels do not show the tooltip.
- The tooltip updates when Label text, font, size, elide mode, or theme changes affect truncation.
- Existing Label typography and theme APIs remain compatible.

## Capabilities

### New Capabilities

### Modified Capabilities
- `label`: Add elide-aware hover tooltip behavior for truncated Label text.

## Impact

- Affected code: `src/view/textfields/Label.h`, `src/view/textfields/Label.cpp`.
- Affected reusable component: `src/view/status_info/ToolTip.h`, `src/view/status_info/ToolTip.cpp` through reuse only.
- Affected tests: `tests/views/textfields/TestLabel.cpp`.
- Dependencies: no new third-party dependency; implementation should use Qt event handling, `QFontMetrics`, and the existing `view::status_info::ToolTip`.
