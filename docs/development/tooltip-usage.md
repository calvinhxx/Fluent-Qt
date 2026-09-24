# Tooltip Usage

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Gallery and site

[← Gallery Control Images](gallery-control-images.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Project site workflow →](site-workflow.md)
<!-- docs-nav:top:end -->

All hover help shown by Fluent-Qt must use `fluent::status_info::ToolTip`.
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

<!-- docs-nav:bottom:start -->
---
[← Gallery Control Images](gallery-control-images.md) · [Contents](../SUMMARY.md) · [Development index](README.md) · [Project site workflow →](site-workflow.md)
<!-- docs-nav:bottom:end -->
