#ifndef FLUENT_USER_THEME_H
#define FLUENT_USER_THEME_H

#include <QColor>
#include <QJsonObject>
#include <QString>

#include "components/foundation/FluentElement.h"

namespace fluent {

/**
 * @brief Loads Fluent token overrides and accent customization into
 * ThemeRegistry. zh_CN: 将 Fluent token 覆盖与强调色定制加载到 ThemeRegistry。
 */
namespace UserTheme {

/// Install built-in Fluent tokens plus user JSON overrides into ThemeRegistry.
/// zh_CN: 安装内置 Fluent token 与用户 JSON 覆盖。
void apply();

/**
 * @brief Applies sparse in-memory overrides to the current global theme without writing files.
 * zh_CN: 将稀疏内存配置叠加到当前全局主题，不写入文件。
 * @return true if state changed; false for invalid or unchanged overrides.
 * zh_CN: 状态改变返回 true；无效或未改变的配置返回 false。
 * Accepts light/dark/contrast color objects, radius and font (family/scale).
 * Invalid keys or values reject the complete update. Missing fields remain unchanged.
 * zh_CN: 支持三套颜色、圆角和字体族/缩放；无效字段拒绝整次更新，缺省字段保持不变。
 */
bool applyOverrides(const QJsonObject& overrides);

/// Apply an in-memory accent override to current ThemeRegistry colors. zh_CN:
/// 对当前 ThemeRegistry 颜色应用内存态强调色覆盖。
void applyAccentOverride(const QColor& accent);

/// Absolute path of the user-editable Fluent override file. zh_CN: Fluent
/// 用户覆盖文件的绝对路径。
QString filePath();

/// Directory holding user-editable Fluent token override files. zh_CN:
/// 保存用户可编辑 Fluent token 覆盖文件的目录。
QString directory();

/**
 * @brief Explicitly exports a schema-versioned editable theme template.
 * zh_CN: 显式导出带 schema 版本的可编辑主题模板。
 *
 * Applying a theme never writes to disk. By default this function preserves an
 * existing override file; pass overwrite=true only when replacement is
 * intended. zh_CN:
 * 应用主题不会写磁盘。默认保留现有覆盖文件；仅在明确需要替换时传入
 * overwrite=true。
 */
bool exportTemplate(bool overwrite = false);

/// Persist a custom accent into the Fluent override file. zh_CN:
/// 将自定义强调色持久化到 Fluent 覆盖文件。
void setAccent(const QColor& accent);

/// Remove the persisted custom accent override. zh_CN:
/// 移除已持久化的自定义强调色覆盖。
void clearAccent();

/// Built-in Fluent accent for light/dark mode. zh_CN: Fluent
/// 在明暗模式下的内置强调色。
QColor defaultAccent(bool dark);

} // namespace UserTheme

} // namespace fluent

#endif // FLUENT_USER_THEME_H
