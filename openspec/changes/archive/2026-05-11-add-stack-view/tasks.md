## 1. Component Skeleton And API

- [x] 1.1 Add `src/view/collections/StackView.h` and `.cpp` with `QStackedWidget + FluentElement + view::QMLPlus` inheritance, constructor defaults, size hints, theme hook, and focus policy.
- [x] 1.2 Define public enums for `StackViewItemStatus`, transition operation/type, item ownership, and expose Qt properties for `depth`, `currentItem`, `initialItem`, `busy`, `orientation`, `transitionAnimationEnabled`, and `transitionDuration`.
- [x] 1.3 Implement stack query APIs: `itemAt`, `indexOf`, `contains`, `itemStatus`, `canPop`, `currentItem`, `initialItem`, and signal declarations required by the spec.

## 2. Stack Operations And Lifecycle

- [x] 2.1 Implement `setInitialItem`, `push`, multi-item push, `replace`, `pop`, `popToRoot`, `popToItem`, `goBack`, and `clear` with depth/current signal emission.
- [x] 2.2 Maintain an internal stack entry model using guarded widget pointers, item ownership metadata, and lifecycle status.
- [x] 2.3 Implement item status transitions for Activating, Active, Deactivating, and Inactive, including status signals for both animated and non-animated paths.
- [x] 2.4 Handle external widget destruction or removal safely by pruning stale stack entries and updating current/depth state.
- [x] 2.5 Implement owned vs non-owned page removal behavior for pop, replace, clear, and destructor cleanup.

## 3. Transition Animation And Layout

- [x] 3.1 Implement synchronous transition completion path for disabled animation or zero duration.
- [x] 3.2 Implement push, pop, and replace transitions with slide/fade animation using Qt animation groups and Fluent animation tokens.
- [x] 3.3 Support horizontal and vertical transition geometry, resize handling during transitions, and correct final visibility for inactive pages.
- [x] 3.4 Implement `busy` guards so stack operations and keyboard return are rejected or ignored during an active transition.
- [x] 3.5 Refresh transition duration/easing from `onThemeUpdated()` without changing stack content.

## 4. Input And Integration Behavior

- [x] 4.1 Implement keyboard return handling for Backspace and available platform back key combinations when `canPop()` and not busy.
- [x] 4.2 Ensure direct `QStackedWidget` current-index changes synchronize visible current state without fabricating push/pop history.
- [x] 4.3 Provide safe adoption of an existing widget as an initial stack item or stack-managed page.

## 5. Tests And VisualCheck

- [x] 5.1 Add `tests/views/collections/TestStackView.cpp` covering default properties, inheritance, size hints, and basic push/pop/replace/clear behavior.
- [x] 5.2 Add tests for popToRoot, popToItem, goBack, busy rejection, disabled animation, current/depth signals, and item status signal order.
- [x] 5.3 Add lifecycle and ownership tests for owned pages, non-owned pages, external widget destruction, and clear/destructor cleanup.
- [x] 5.4 Add animation/layout tests for horizontal and vertical transitions, resize during transition, final page visibility, and theme update safety.
- [x] 5.5 Register `test_stack_view` in `tests/views/collections/CMakeLists.txt`.
- [x] 5.6 Add a VisualCheck showing push, pop, replace, horizontal/vertical transitions, keyboard/back navigation, and Light/Dark theme switching guarded by `SKIP_VISUAL_TEST`.

## 6. Verification

- [x] 6.1 Build `test_stack_view`.
- [x] 6.2 Run `SKIP_VISUAL_TEST=1 ./build/tests/views/collections/test_stack_view`.
- [x] 6.3 Run affected existing collection tests, at minimum `test_flip_view`, to catch regressions in shared collection patterns.
- [x] 6.4 Run `openspec validate add-stack-view --strict` and confirm the change is apply-ready.