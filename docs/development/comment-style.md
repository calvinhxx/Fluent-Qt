# Source Comment Style

Use this guide when adding or materially editing comments under `src/`.

The goal is to make public contracts, design rationale, and non-obvious Qt
behavior easy to understand for both English and Chinese readers. Do not add
comments that only restate the code.

## Scope

- Applies to source files under `src/`.
- Applies most strongly to public headers, reusable component contracts, design
  token definitions, compatibility helpers, logging utilities, and layout or
  rendering invariants.
- Test-only QObject helper models and fixture comments belong under
  `tests/support/` or local test fixtures, not under production `src/` module
  guidance.
- Does not require a mechanical rewrite of existing comments in untouched files.
- Does not require every implementation comment to be bilingual.

## Public API Comments

Use Doxygen block comments for non-trivial public classes, structs, enums,
properties, and methods. Prefer an English `@brief` first, followed by a
Chinese `zh_CN:` line.

```cpp
/**
 * @brief Provides Fluent design tokens for QWidget-based controls.
 * zh_CN: 为 QWidget 控件提供 Fluent 设计 token 访问入口。
 */
class FluentElement {
};
```

For public methods, document behavior, nullable values, side effects, or signal
semantics when they are not obvious from the name.

```cpp
/**
 * @brief Sets the selected date.
 * zh_CN: 设置当前选中日期；传入无效日期时清空选择。
 *
 * @param date Date to select, or an invalid date to clear the selection.
 */
void setSelectedDate(const QDate& date);
```

Keep comments concise. If a getter or setter is self-explanatory and has no
special semantics, it does not need a comment.

## Implementation Comments

Implementation comments in `.cpp` files should explain intent, not mechanics.
Use them for:

- Qt, platform, or font behavior that is easy to misuse.
- Rendering, layout, animation, or event-order invariants.
- Compatibility branches and why they exist.
- Non-obvious ownership, lifetime, or signal-emission decisions.

Implementation comments may be Chinese-only or English-only when they are local
to the implementation. Make them bilingual only when the behavior is part of a
reusable public contract or a common cross-module rule.

Avoid comments like these:

```cpp
// Set color.
m_color = color;

// If enabled is true, enable the widget.
if (enabled) {
    setEnabled(true);
}
```

Prefer comments like this:

```cpp
// Qt reports child widgets as not visible before the dialog is shown, so use
// the authored content state instead of QWidget::isVisible() here.
```

## Module Density

| Area | Comment density | Guidance |
|------|-----------------|----------|
| `src/design/` | Medium-high | Explain token families, measurement units, source assumptions, and semantic usage. |
| `src/compatibility/` | High | Explain Qt version and platform differences that affect call sites. |
| `src/components/foundation/` | High | Document mixin contracts, ownership, lifecycle callbacks, and shared infrastructure. |
| `src/components/**` | Medium | Document public component APIs and subtle layout/rendering behavior; avoid repeating Qt basics. |
| `src/utils/` | Medium-high | Document operational behavior such as log paths, environment variables, debug-only helpers, and lifetime assumptions. |

## Module Summary Comments

Small token or utility headers may use a module-level Doxygen comment when that
is the main public contract:

```cpp
/**
 * @brief Defines Fluent motion durations and easing curves.
 * zh_CN: 定义 Fluent 动效时长和缓动曲线 token。
 */
namespace Animation {
}
```

Avoid large banner separators unless they materially improve scanability in a
dense token table. Prefer short section comments such as `// Durations (ms)` or
`// zh_CN: 动画时长，单位为毫秒。`.

## Glossary

Use these terms consistently in comments and docs:

| English | Chinese | Notes |
|---------|---------|-------|
| component | 组件 | Reusable library widget or infrastructure unit. |
| control | 控件 | User-facing widget/control surface. |
| design token | 设计 token | Semantic design value such as color, spacing, font, radius, motion, or elevation. |
| overlay | 浮层 | Popup-like UI layered above normal content. |
| same-window overlay | 同窗口浮层 | Overlay hosted inside the same top-level window. |
| light-dismiss | light-dismiss / 轻点关闭 | Dismissal caused by outside click, escape, or focus loss depending on the component contract. |
| selection | 选中项 | Selected item or selected value. |
| current item | 当前项 | Current/focused item, distinct from selection when applicable. |
| host | 宿主 | Widget or object that owns or embeds another behavior. |
| content host | 内容宿主 | Container responsible for hosting application-supplied content. |

## Review Checklist

- Public contract comments use English `@brief` plus `zh_CN:` when the contract
  is non-trivial.
- Implementation comments explain why, not what.
- Comments avoid temporary project names, local checkout paths, and stale design
  speculation.
- Comments are updated when the documented contract changes.
- Untouched files are not rewritten just to normalize old comments.
