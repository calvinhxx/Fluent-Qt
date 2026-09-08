# 自定义主题与组件局部覆盖

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Fluent design](README.md) › Design references

[← Fluent (Windows) — Design Reference](fluent.md) · [Contents](../SUMMARY.md) · [Fluent design index](README.md) · [Fluent Design Kit Source →](figma-sources.md)
<!-- docs-nav:top:end -->

FluentQt 支持在 Fluent 视觉体系内定制颜色、圆角、字体族和字号缩放。使用全局配置统一应用外观；只需要调整某个组件时，设置该组件的局部覆盖。Light、Dark 和 HighContrast 仍是主题模式，不需要新增主题枚举。

## 全局定制

`UserTheme::applyOverrides()` 将配置叠加到当前全局主题，不读写文件。未指定字段保持当前值；配置发生变化时自动刷新组件。返回 `true` 表示状态改变，`false` 表示配置无效或没有变化。

```cpp
#include <FluentQt/FluentQt.h>
#include <QJsonObject>

fluent::UserTheme::applyOverrides({
    {"light", QJsonObject{{"accentDefault", "#006B5E"}}},
    {"dark", QJsonObject{{"accentDefault", "#60DCC8"}}},
    {"radius", QJsonObject{{"control", 6}, {"overlay", 10}}},
    {"font", QJsonObject{{"scale", 1.15}}}
});
```

```python
import fluentqt

fluentqt.apply_theme_overrides({
    "light": {"accentDefault": "#006B5E"},
    "dark": {"accentDefault": "#60DCC8"},
    "radius": {"control": 6, "overlay": 10},
    "font": {"scale": 1.15},
})
```

仅指定 `accentDefault` 时，会派生对应的次级强调色和强调色文字。需要精确控制这些状态时，在同一配置中显式指定。未提供 `contrast` 时保留当前高对比度调色板。

C++ 也可以继续使用 `ThemeRegistry::extendedSnapshot()` 和 `applyExtendedSnapshot()` 提交强类型的完整配置。`ThemeRegistry::resetToDefaults()` / Python 的 `reset_theme_tokens()` 恢复全局内置值，不清除组件局部覆盖。

## 单个组件覆盖

继承 `FluentElement` 的组件可以调用 `setThemeOverrides()`。它替换该组件之前的整份覆盖配置；缺省字段跟随最新全局主题，不会固定为设置时的快照。

```cpp
auto* button = new fluent::basicinput::Button("Local action", parent);
button->setThemeOverrides({
    {"light", QJsonObject{{"textPrimary", "#7A2454"}}},
    {"dark", QJsonObject{{"textPrimary", "#FFABD8"}}},
    {"radius", QJsonObject{{"control", 12}}},
    {"font", QJsonObject{{"scale", 1.25}}}
});

// Clear local token overrides and follow the current global theme again.
button->clearThemeOverrides();
```

```python
button = fluentqt.Button("Local action")
fluentqt.set_widget_theme_overrides(button, {
    "light": {"textPrimary": "#7A2454"},
    "dark": {"textPrimary": "#FFABD8"},
    "radius": {"control": 12},
    "font": {"scale": 1.25},
})
saved = fluentqt.widget_theme_overrides(button)
fluentqt.set_widget_theme_overrides(button, {})  # Restore global tokens.
```

局部覆盖只作用于当前 `FluentElement`，不递归应用到子控件、同级组件或另行组合的弹层。某个自绘代理若读取宿主的 token，则会使用宿主覆盖；独立组件仍使用自己的配置。需要统一整个应用时使用全局配置；需要调整组合控件的某个部分时使用该控件公开的对应接口。普通 `QWidget` 不支持这套 token 覆盖，Python 设置函数对它返回 `False`。

读取 `themeOverrides()` / `widget_theme_overrides()` 得到的是已写入的局部配置副本。重复写入相同配置不会触发刷新；修改单个字段时可先读取配置，再修改并整体写回。颜色、圆角、字体分别覆盖，设置字号不会使颜色退出主题管理。

## 字体优先级

在支持显式字体契约的组件（如 Button、ToggleSwitch、ComboBox、DatePicker 和 TimePicker）中，优先级为：

1. 调用方显式 `setFont()`。
2. 当前组件的 `font` token 覆盖。
3. 全局字体配置与当前 `fontRole`。

`setFontRole(component->fontRole())` 清除显式字体，恢复按角色解析；若组件仍有局部 `font` 覆盖，会继续使用该覆盖。`clearThemeOverrides()` 只清除 token 配置，不清除独立的 `setFont()` 设置。完整规则见 [Typography resolution](../architecture/typography-resolution.md)。

局部 `font.scale` 是相对于内置角色字号的绝对倍率，不与全局倍率相乘。任意精确字号使用 `QFont::setPixelSize()` 或 `setPointSizeF()`。组件需要实际使用相应 token 才会受其影响；这套接口不把全部 QWidget 属性或编译期尺寸变成可配置字段。

例如 Body 内置字号为 14px，全局倍率为 1.5 时是 21px；局部设为 2.0 后是 28px。只覆盖 `font.family` 时仍跟随全局倍率，并保留 BodyStrong、Title 等角色的字重。自定义字体族不沿用内置字体的 `styleName`，由 Qt 按字重匹配该字体族可用的字形；最终效果取决于字体是否提供对应字重。

## 配置字段与文件

| 字段 | 值与约束 |
| --- | --- |
| `light` / `dark` / `contrast` | 颜色对象，如 `textPrimary`、`controlDefault`、`accentDefault`、`bgCanvas`、`strokeDefault`；完整字段见导出的模板 |
| `radius` | `none`、`control`、`overlay`，整数 0–64 |
| `font.family` | 字体族字符串；空字符串恢复内置字体族 |
| `font.scale` | 数值 0.5–4.0 |

颜色使用字符串，支持 `#RRGGBB`、`#RRGGBBAA` 和 QColor 颜色名。八位十六进制中的透明度在末尾。运行时接口传入上表对应的配置对象，不要传文件的 `schemaVersion` / `theme` / `overrides` 外层。未知字段、错误类型和越界数值会拒绝整次运行时更新，不修改已有状态。

需要文件配置时，C++ 可用 `UserTheme::exportTemplate()` 显式导出可编辑模板，使用 `UserTheme::filePath()` 查找路径。编辑 `fluent.json` 后调用 `UserTheme::apply()`（Python：`apply_user_theme()`）；它先恢复内置主题，再加载文件，因此会替换此前的全局内存配置。调用 `apply()` 本身不会创建或改写文件。文件加载保留既有容错规则；它与运行时接口的严格校验行为不同。

## 验证边界

`TestThemeOverrides.cpp` 覆盖全局原子更新、局部隔离、主题切换、全局配置更新、恢复默认、错误输入和字体优先级。Python 绑定有对应回归测试。局部覆盖不会自动保证自定义配色的对比度；修改交互状态颜色后，需要检查文字、焦点和禁用状态。

<!-- docs-nav:bottom:start -->
---
[← Fluent (Windows) — Design Reference](fluent.md) · [Contents](../SUMMARY.md) · [Fluent design index](README.md) · [Fluent Design Kit Source →](figma-sources.md)
<!-- docs-nav:bottom:end -->
