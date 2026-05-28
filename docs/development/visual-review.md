# Visual Review

Use this workflow to run Fluent component `VisualCheck` tests for interactive UI
review after visual, theme, painting, or layout changes.

## When to Use

- Validating rendering after `paintEvent()` changes.
- Checking Light/Dark theme behavior.
- Reviewing spacing, typography, rounded corners, shadows, and interaction
  states.
- Debugging component-specific visual regressions.

## Find the Test Binary

Default build output uses the `vcpkg-osx` preset:

```bash
./build/vcpkg-osx/tests/views/<category>/test_<snake_case_name>
```

Examples:

```bash
./build/vcpkg-osx/tests/views/basicinput/test_button
./build/vcpkg-osx/tests/views/basicinput/test_combo_box
./build/vcpkg-osx/tests/views/collections/test_list_view
./build/vcpkg-osx/tests/views/collections/test_tree_view
./build/vcpkg-osx/tests/views/dialogs_flyouts/test_popup
./build/vcpkg-osx/tests/views/navigation/test_navigation_view
./build/vcpkg-osx/tests/views/date_time/test_calendar_date_picker
```

If another preset is used, replace `vcpkg-osx` in the path with that preset name.

## Build Before Review

Build the focused test target when known:

```bash
cmake --build --preset vcpkg-osx --target test_<name>
```

Build the full preset only when broad dependencies changed:

```bash
cmake --build --preset vcpkg-osx
```

## Run VisualCheck

Run only VisualCheck tests. The window closes when the reviewer closes it.

```bash
./build/vcpkg-osx/tests/views/<category>/test_<name> --gtest_filter="*VisualCheck*"
```

Do not set `SKIP_VISUAL_TEST` for manual review. Automated CTest runs set
`SKIP_VISUAL_TEST=1` to skip interactive cases.

## Review Checklist

- Colors match semantic tokens in `ThemeColors.h`.
- Control-level corners use `CornerRadius::Control`; overlay surfaces use
  `CornerRadius::Overlay`.
- Spacing follows the 4 px grid and component-specific layout metrics.
- Typography uses the project font tokens and stays legible in Light/Dark modes.
- Rest, hover, pressed, focused, selected, and disabled states are visible and
  coherent.
- Text fits its container across the intended window sizes.
- Animated transitions are readable without interrupting input flow.
- Overlay components respect [Overlay Behavior](../architecture/overlay-behavior.md).

## After Review

Run the focused automated validation without interactive VisualCheck windows:

```bash
ctest --preset vcpkg-osx -L '^test_<name>$' --output-on-failure
```
