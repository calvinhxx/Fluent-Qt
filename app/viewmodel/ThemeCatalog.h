#ifndef THEMECATALOG_H
#define THEMECATALOG_H

#include <QColor>
#include <QString>

#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {

/**
 * @brief Gallery-facing adapter for the library style-theme catalog.
 * zh_CN: 面向 Gallery 设置枚举的库层样式主题 catalog 适配器。
 *
 * The reusable Style & accent implementation lives in
 * fluent::StyleThemeCatalog. This namespace only maps GallerySettings enums to
 * the public library API so existing Gallery settings and tests stay stable.
 * zh_CN: 可复用的 Style 与 accent 实现在 fluent::StyleThemeCatalog 中；此命名空间仅
 * 将 GallerySettings 枚举映射到公共库 API，以保持 Gallery 设置和测试稳定。
 */
namespace ThemeCatalog {

void apply(GallerySettings::StyleTheme theme);

/// Absolute override path when the runtime supports theme files. zh_CN: 运行时支持主题文件时的覆盖文件绝对路径。
QString userThemeFilePath(GallerySettings::StyleTheme theme);

/// Override directory when supported by the runtime. zh_CN: 运行时支持时的主题覆盖目录。
QString themesDirectory();

/// Export an editable theme template without replacing an existing override. zh_CN: 导出可编辑主题模板，且不覆盖现有覆盖文件。
bool exportUserThemeTemplate(GallerySettings::StyleTheme theme, bool overwrite = false);

/// Persist an accent through the selected runtime backend. zh_CN: 通过所选运行时后端持久化强调色。
void setUserAccent(GallerySettings::StyleTheme theme, const QColor& accent);

/// Remove the persisted custom accent override from the style theme. zh_CN: 移除样式主题中已持久化的自定义强调色覆盖。
void clearUserAccent(GallerySettings::StyleTheme theme);

/// Built-in preset accent for a style theme and light/dark mode. zh_CN: 样式主题在明暗模式下的内置预设强调色。
QColor presetAccent(GallerySettings::StyleTheme theme, bool dark);

} // namespace ThemeCatalog

} // namespace fluent::gallery

#endif // THEMECATALOG_H
