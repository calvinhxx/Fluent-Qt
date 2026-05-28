## Why

`ListView` currently exposes Fluent selection visuals, but the selected indicator does not have the richer direction-aware motion shown in modern WinUI-style navigation and tab patterns. Vertical and horizontal list flows need different indicator placement and transition behavior so ListView can support both side navigation and top tab-like surfaces without each caller reimplementing selection animation.

## What Changes

- Add direction-aware selected indicator motion for vertical `ListView` flow:
  - moving selection downward and upward SHALL produce distinct indicator motion using the previous and next selected item geometry.
  - the indicator remains on the leading side of the selected item, matching the current vertical list visual language.
- Add horizontal-flow selected indicator behavior:
  - the indicator SHALL render along the bottom edge of the selected item.
  - moving selection left-to-right and right-to-left SHALL produce distinct direction-aware motion using the previous and next selected item geometry.
- Keep the behavior inside `ListView` so callers can use the same selection API (`setSelectedIndex`, pointer selection, keyboard/model selection) without custom indicator painting.
- Preserve current list item, scroll, drag-reorder, section, theme, and selection semantics.
- Add automated geometry/animation tests plus a VisualCheck scenario demonstrating vertical and horizontal indicator motion.

## Capabilities

### New Capabilities
- `listview-indicator-motion`: Defines selected indicator placement and direction-aware transition behavior for vertical and horizontal `ListView` flows.

### Modified Capabilities
- None.

## Impact

- Affected code:
  - `src/view/collections/ListView.h`
  - `src/view/collections/ListView.cpp`
  - `tests/views/collections/TestListView.cpp`
- Public API impact:
  - Prefer no mandatory API break.
  - Add query/configuration helpers only if needed for testing or future caller control.
- Dependencies:
  - No new third-party dependencies.
- Validation:
  - Build and run `test_list_view` with `SKIP_VISUAL_TEST=1`.
  - Keep VisualCheck manual for confirming the GIF-like motion quality in both list directions.
