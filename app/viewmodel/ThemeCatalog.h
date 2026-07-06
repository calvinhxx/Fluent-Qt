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

/// Absolute path of the user-editable override file for a style theme. zh_CN: 样式主题用户覆盖文件的绝对路径。
QString userThemeFilePath(GallerySettings::StyleTheme theme);

/// Directory holding user-editable style-theme override files. zh_CN: 保存用户可编辑样式主题覆盖文件的目录。
QString themesDirectory();

/// Persist a custom accent into the style theme's override file. zh_CN: 将自定义强调色持久化到样式主题覆盖文件。
void setUserAccent(GallerySettings::StyleTheme theme, const QColor& accent);

/// Remove the persisted custom accent override from the style theme. zh_CN: 移除样式主题中已持久化的自定义强调色覆盖。
void clearUserAccent(GallerySettings::StyleTheme theme);

/// Built-in preset accent for a style theme and light/dark mode. zh_CN: 样式主题在明暗模式下的内置预设强调色。
QColor presetAccent(GallerySettings::StyleTheme theme, bool dark);

} // namespace ThemeCatalog

} // namespace fluent::gallery

#endif // THEMECATALOG_H
