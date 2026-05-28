## Context

`ToolTip` is a top-level frameless `QWidget` using `Qt::ToolTip` and `Qt::WA_ShowWithoutActivating`. Current hover callers, including slider and test helper code, call `show()` and `hide()` directly. That means the animation must be owned by `ToolTip` itself rather than by each caller.

Other components already use Fluent animation tokens through `FluentElement::themeAnimation()`. Top-level windows in this codebase use `windowOpacity` for opacity transitions because `QGraphicsOpacityEffect` is not reliable for top-level windows.

## Goals / Non-Goals

**Goals:**
- Add a minimal show/hide animation that feels clean and quick for hover feedback.
- Keep existing `show()` and `hide()` call sites working without migration.
- Delay the actual hidden state until the hide animation completes.
- Reuse Qt animation primitives and existing Fluent animation tokens.

**Non-Goals:**
- Do not change tooltip placement rules or add target-widget attachment behavior.
- Do not add scale transforms, resize animation, or layout animation.
- Do not replace Qt's native tooltip semantics or introduce a new flyout/popup abstraction.

## Decisions

1. Use `windowOpacity` as the animated property.

   Rationale: `ToolTip` is a top-level tooltip window, and `windowOpacity` is already the project pattern for top-level opacity animation. This avoids `QGraphicsOpacityEffect`, which is not appropriate for top-level windows, and avoids repainting child widgets manually.

   Alternative considered: animating a custom paint opacity. That would only affect the background drawn by `ToolTip::paintEvent()` unless every child render path also participated, so it would be less robust.

2. Override visibility handling so existing callers keep using `show()` and `hide()`.

   Rationale: current call sites invoke `show()`/`hide()` directly. `ToolTip` should intercept visible-state changes, start the appropriate animation, and call the base visibility change only when needed. For hide, it should remain visible during fade-out and become hidden when the animation finishes.

   Alternative considered: adding `showAnimated()` / `hideAnimated()` only. That would require updating every caller and would leave existing behavior inconsistent.

3. Keep the motion opacity-only.

   Rationale: a tooltip is a small hover affordance; opacity-only motion is subtle, readable, and avoids position jitter around cursor targets. It also matches existing popup guidance that avoids scale transforms for content.

   Alternative considered: adding a small position slide. This can look pleasant, but it risks conflicting with caller-managed global positioning and screen-edge adjustments.

4. Provide an animation enable switch for deterministic behavior.

   Rationale: components such as `Dialog` and `Popup` expose animation toggles, and tests often need synchronous visibility changes. `ToolTip` can follow that pattern with an `animationEnabled` property while defaulting it to enabled.

## Risks / Trade-offs

- Hide calls during an enter animation may leave stale animation state -> stop the current animation before starting the opposite transition.
- Repeated `show()` calls after callers move the tooltip may restart the fade unnecessarily -> only animate from the current opacity and preserve the caller's final position.
- Some platforms may ignore or clamp `windowOpacity` for tooltips -> tests should assert state transitions without relying only on per-frame platform rendering.
