#ifndef STYLETHEMECATALOG_H
#define STYLETHEMECATALOG_H

#include <QColor>
#include <QString>

#include "components/foundation/FluentElement.h"

namespace fluent {

/**
 * @brief Built-in style theme presets available to applications.
 * zh_CN: 应用可用的内置样式主题预设。
 */
enum class StyleTheme {
    Fluent,
    Material,
    MacOS
};

/**
 * @brief Installs style-theme and accent tokens into the runtime ThemeRegistry.
 * zh_CN: 将样式主题与强调色 token 安装到运行时 ThemeRegistry。
 */
namespace StyleThemeCatalog {

/// Stable lowercase key for a style theme. zh_CN: 样式主题的稳定小写 key。
QString themeKey(StyleTheme theme);

/// Design-language branch used by controls for a style theme. zh_CN: 控件按样式主题使用的设计语言分支。
FluentElement::DesignLanguage designLanguageFor(StyleTheme theme);

/// Install a style-theme preset plus user JSON overrides into ThemeRegistry. zh_CN: 安装样式主题预设与用户 JSON 覆盖。
void apply(StyleTheme theme);

/// Apply an in-memory accent override to current ThemeRegistry colors. zh_CN: 对当前 ThemeRegistry 颜色应用内存态强调色覆盖。
void applyAccentOverride(const QColor& accent);

/// Absolute path of the user-editable override file for a style theme. zh_CN: 样式主题用户覆盖文件的绝对路径。
QString userThemeFilePath(StyleTheme theme);

/// Directory holding user-editable style-theme override files. zh_CN: 保存用户可编辑样式主题覆盖文件的目录。
QString themesDirectory();

/**
 * @brief Explicitly exports a schema-versioned editable theme template.
 * zh_CN: 显式导出带 schema 版本的可编辑主题模板。
 *
 * Applying a theme never writes to disk. By default this function preserves an
 * existing override file; pass overwrite=true only when replacement is intended.
 * zh_CN: 应用主题不会写磁盘。默认保留现有覆盖文件；仅在明确需要替换时传入 overwrite=true。
 */
bool exportUserThemeTemplate(StyleTheme theme, bool overwrite = false);

/// Persist a custom accent into the style theme's override file. zh_CN: 将自定义强调色持久化到样式主题覆盖文件。
void setUserAccent(StyleTheme theme, const QColor& accent);

/// Remove the persisted custom accent override from the style theme. zh_CN: 移除样式主题中已持久化的自定义强调色覆盖。
void clearUserAccent(StyleTheme theme);

/// Built-in preset accent for a style theme and light/dark mode. zh_CN: 样式主题在明暗模式下的内置预设强调色。
QColor presetAccent(StyleTheme theme, bool dark);

} // namespace StyleThemeCatalog

} // namespace fluent

#endif // STYLETHEMECATALOG_H
