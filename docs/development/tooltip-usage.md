# Tooltip Usage

All hover help shown by Fluent-QT must use `fluent::status_info::ToolTip`.
Do not present hover help with `QToolTip` or direct call-site uses of
`QWidget::setToolTip()`, because native tooltip styling, placement, animation,
and theme behavior differ across platforms.

Attach hover help with the component helper:

```cpp
fluent::status_info::ToolTip::attach(button, QStringLiteral("Back"));
```

The default placement is above the target and automatically falls back to the
opposite side when screen space is insufficient. Pass `ToolTip::Below`,
`ToolTip::Left`, or `ToolTip::Right` only when the interaction calls for a
specific direction.

Keep `accessibleName` or equivalent accessibility text on icon-only controls;
a tooltip is supplemental help, not the accessible label. Tests for hover help
should send `QEvent::ToolTip` to the target and assert against the attached
`ToolTip`, rather than inspecting a platform-native tooltip window.

中文约定：所有悬停提示统一使用 `fluent::status_info::ToolTip`，调用点不要直接使用
`QToolTip` 或 `QWidget::setToolTip()`。图标按钮仍必须提供无障碍名称；Tooltip 只是补充提示，
不能替代无障碍文本。
