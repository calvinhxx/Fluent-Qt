#ifndef THEMECATALOG_H
#define THEMECATALOG_H

#include <QColor>
#include <QString>

#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {

/**
 * @brief Installs a brand style-theme preset into the runtime ThemeRegistry.
 * zh_CN: 把品牌样式主题预设安装进运行时 ThemeRegistry。
 *
 * apply() resets the registry to the built-in Fluent defaults, layers the requested brand preset
 * (Material 3 / macOS: accent, surfaces, text, semantic colors, corner radius) on top, then merges
 * any user overrides from <AppLocalData>/themes/<key>.json so the look can be customized without
 * recompiling. It does NOT repaint — the caller (GallerySettings) drives refresh timing so startup
 * and live changes stay coordinated. The first time a brand is applied its preset is also written out
 * as an editable JSON template (if absent), giving users a ready starting point.
 * zh_CN: apply() 先把注册表重置为内置 Fluent 默认,叠加所选品牌预设(Material 3 / macOS:强调色、表面、
 * 文字、语义色、圆角),再合并 <AppLocalData>/themes/<key>.json 的用户覆盖,使外观无需重编即可定制。它不
 * 触发重绘——由调用方(GallerySettings)统一刷新时机。某品牌首次应用时,其预设还会(若不存在)导出为可编辑
 * JSON 模板,给用户现成起点。
 */
namespace ThemeCatalog {

void apply(GallerySettings::StyleTheme theme);

/// Absolute path of the user-editable override file for a style theme. zh_CN: 某样式主题用户可编辑覆盖文件的绝对路径。
QString userThemeFilePath(GallerySettings::StyleTheme theme);

/// Absolute path of the directory holding the user-editable theme override files; created if absent.
/// zh_CN: 存放用户可编辑主题覆盖文件的目录绝对路径;不存在则创建。
QString themesDirectory();

/// Persist a custom accent into a style theme's override file (both Light and Dark accentDefault),
/// merging with any existing overrides. Does NOT repaint — the caller re-applies + refreshes.
/// zh_CN: 把自定义强调色写入某样式主题的覆盖文件(明暗两态的 accentDefault),与既有覆盖合并。不触发重绘——由调用方重新 apply + 刷新。
void setUserAccent(GallerySettings::StyleTheme theme, const QColor& accent);

/// Remove any user accent override from a style theme's file, reverting to the preset accent.
/// zh_CN: 移除某样式主题文件中的用户强调色覆盖,恢复为预设强调色。
void clearUserAccent(GallerySettings::StyleTheme theme);

/// The built-in preset accent for a style theme + mode (independent of user overrides).
/// zh_CN: 某样式主题 + 模式的内置预设强调色(不含用户覆盖)。
QColor presetAccent(GallerySettings::StyleTheme theme, bool dark);

} // namespace ThemeCatalog

} // namespace fluent::gallery

#endif // THEMECATALOG_H
