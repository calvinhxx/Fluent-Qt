## 1. Component Skeleton

- [x] 1.1 Add `src/view/navigation/Breadcrumb.h` with `QWidget + FluentElement + view::QMLPlus` inheritance, public enums, item record type, properties, signals, and geometry query helpers.
- [x] 1.2 Add `src/view/navigation/Breadcrumb.cpp` with constructor setup, focus/hover attributes, theme/font refresh, `sizeHint()`, `minimumSizeHint()`, and event override scaffolding.
- [x] 1.3 Confirm the root library build picks up `Breadcrumb.h/.cpp` through existing source discovery.

## 2. Item Model And Public API

- [x] 2.1 Implement `BreadcrumbItem` storage with text, user data, enabled state, and accessible name fields.
- [x] 2.2 Implement `setItems(QStringList)`, rich item collection assignment, append/insert/remove/clear, index validation, and item query APIs.
- [x] 2.3 Implement changed signals for item mutations, breadcrumb size, overflow mode, and auto-truncation configuration without duplicate emissions.
- [x] 2.4 Update accessible control text when the path or active final item changes.

## 3. Layout And Overflow

- [x] 3.1 Implement standard and large metric sets from Figma, including text heights, icon slots, font roles, corner radii, and separator glyph `E974`.
- [x] 3.2 Implement display-record layout for text crumbs, separators, and overflow affordance with stable rectangles and text elision.
- [x] 3.3 Implement `None`, `Beginning`, and `Middle` overflow modes with deterministic hidden item indexes and active-item preservation.
- [x] 3.4 Implement overflow affordance rendering and hit geometry using Segoe Fluent Icons ellipsis glyph `E712`.
- [x] 3.5 Expose item, separator, overflow, and hidden-index query helpers for automated tests.

## 4. Interaction And Visual States

- [x] 4.1 Implement pointer hit testing, hover, pressed, leave, and release behavior for visible items and overflow affordance.
- [x] 4.2 Implement item activation signals and optional WinUI Gallery-style trailing path truncation after activation.
- [x] 4.3 Implement keyboard focus movement with Left/Right/Home/End and activation with Enter/Space while skipping separators and disabled items.
- [x] 4.4 Paint rest, hover, pressed, focused, disabled, Light, and Dark states with `themeColors()` tokens and no QSS/QPalette-driven component styling.
- [x] 4.5 Ensure disabled control and disabled item states block pointer and keyboard activation.

## 5. Tests And VisualCheck

- [x] 5.1 Add `tests/views/navigation/TestBreadcrumb.cpp` with `SetUpTestSuite()` resource/font initialization and VisualCheck guard conventions.
- [x] 5.2 Add `tests/views/navigation/CMakeLists.txt` registering `test_breadcrumb` and add `add_subdirectory(navigation)` to `tests/views/CMakeLists.txt`.
- [x] 5.3 Add automated tests for defaults, item management, metadata preservation, property signals, metric/layout helpers, overflow hidden indexes, pointer activation, keyboard activation, truncation, disabled behavior, theme refresh, and accessibility text.
- [x] 5.4 Add VisualCheck coverage for standard/large sizes, Light/Dark examples, no/beginning/middle overflow, disabled items, focus states, and a WinUI Gallery-inspired folder path sample.

## 6. Validation

- [x] 6.1 Validate OpenSpec with `openspec validate add-breadcrumb --strict`.
- [ ] 6.2 Build incrementally with `cmake --build build --target test_breadcrumb`.
- [ ] 6.3 Run `SKIP_VISUAL_TEST=1 ./build/tests/views/navigation/test_breadcrumb`.
- [x] 6.4 Run `git diff --check` for Breadcrumb source, tests, and OpenSpec files.
