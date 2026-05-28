## 1. Baseline Audit

- [x] 1.1 Review current `Popup`, `Flyout`, `ComboBox::ComboBoxPopup`, and `DrawerView` implementations for top-level attachment, window flags, transparency attributes, shadow margins, masks, scrims, event filters, z-order, and animation paths.
- [x] 1.2 Map existing overlay tests in `TestPopup.cpp`, `TestFlyout.cpp`, `TestComboBox.cpp`, and `TestDrawerView.cpp` to the new `overlay-behavior` requirements.
- [x] 1.3 Identify existing behavior that intentionally differs per component, such as DrawerView edge drag semantics and ComboBox selection behavior, and record those as preserved behavior.

## 2. Shared Overlay Internals

- [x] 2.1 Add private overlay utility helpers for resolving the owning top-level widget and safely tracking original parent/top-level lifetime.
- [x] 2.2 Add shared visible-card geometry helpers that separate outer widget geometry, shadow margin, visible card rect, and content rect.
- [x] 2.3 Add shared rounded mask/path helpers for card or panel clipping where existing component-specific code can reuse them safely.
- [x] 2.4 Add shared scrim/z-order helpers for same-window modal and dim overlays.
- [x] 2.5 Add shared close-policy/event-filter helpers for outside press, Escape, modal blocking, and non-modal event propagation.
- [x] 2.6 Keep shared helpers private or internal; do not expose a new public overlay base API unless implementation proves it is necessary.

## 3. Popup And Flyout Alignment

- [x] 3.1 Refactor `Popup` to use the shared top-level resolution, visible-card geometry, scrim, z-order, close-policy, and animation-disabled lifecycle rules.
- [x] 3.2 Preserve `Popup` public API and lifecycle signal order while adding or tightening tests for same-window flags, transparent outside-card areas, modal scrim blocking, non-modal outside press, Escape, and parent destruction safety.
- [x] 3.3 Refactor `Flyout` placement and clamp calculations to use visible-card geometry rather than raw shadow-inclusive widget geometry.
- [x] 3.4 Add or tighten `Flyout` tests for anchor destruction, Auto placement, clamp behavior, z-order after top-level resize, and animation-disabled open/close.

## 4. ComboBox Dropdown Alignment

- [x] 4.1 Align `ComboBox::ComboBoxPopup` with shared Flyout visible-card and shadow-margin geometry while preserving current index, editable text, and ListView selection behavior.
- [x] 4.2 Ensure dropdown rounded card corners are not leaked by embedded ListView viewport backgrounds in both light and dark themes.
- [x] 4.3 Add or tighten ComboBox tests for same-window dropdown flags, below/above placement, right-edge clamping, light-dismiss state synchronization, and Escape closure.
- [x] 4.4 Keep ComboBox dropdown non-modal and non-dimming unless a later explicit proposal changes that behavior.

## 5. DrawerView Alignment

- [x] 5.1 Align `DrawerView` same-window top-level attachment, parent destruction safety, scrim lifecycle, and z-order with the shared overlay rules.
- [x] 5.2 Preserve DrawerView edge placement, drag gestures, normalized position, hosted content ownership, and public ClosePolicy API.
- [x] 5.3 Ensure DrawerView panel mask, shadow, dim opacity, and rounded outer corners use shared geometry/mask helpers where practical.
- [x] 5.4 Add or tighten DrawerView tests for modal scrim blocking, non-modal event propagation, stale scrim cleanup, top-level resize z-order, and animation-disabled lifecycle.

## 6. VisualCheck And Documentation

- [x] 6.1 Update affected VisualCheck cases following `.codex/skills/qt-ut-conventions/SKILL.md`, preserving `SKIP_VISUAL_TEST` guards and using project Fluent components for visible demo controls.
- [x] 6.2 Make overlay VisualChecks expose rounded corners, transparent outside-card areas, shadow, dim scrim, nested content, z-order, light-dismiss, and light/dark theme switching.
- [x] 6.3 Document the overlay behavior contract in project docs or README, including same-window overlay defaults, visible-card geometry, light-dismiss policy, scrim behavior, z-order, and animation-disabled expectations.
- [x] 6.4 Record any deferred public API consolidation, such as unifying ClosePolicy enum types or making DrawerView derive from Popup, as explicit follow-up recommendations.

## 7. Validation

- [x] 7.1 Build focused targets with `cmake --build --preset vcpkg-osx --target test_popup test_flyout test_combo_box test_drawer_view`.
- [x] 7.2 Run focused test binaries with `SKIP_VISUAL_TEST=1` for Popup, Flyout, ComboBox, and DrawerView.
- [x] 7.3 Run `ctest --preset vcpkg-osx -R "Popup|Flyout|ComboBox|DrawerView" --output-on-failure`.
- [x] 7.4 Run `openspec validate standardize-overlay-behavior --strict`.
- [x] 7.5 If platform-specific behavior cannot be fully automated, document the remaining manual VisualCheck steps for macOS and Windows.
