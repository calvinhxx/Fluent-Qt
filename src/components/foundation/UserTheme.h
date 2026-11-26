#ifndef FLUENT_USER_THEME_H
#define FLUENT_USER_THEME_H

#include <QColor>
#include <QJsonObject>
#include <QString>

#include "FluentElement.h"

namespace fluent {

/**
 * @brief Loads Fluent token overrides and accent customization into ThemeRegistry.
 * zh_CN: 将 Fluent token 覆盖与强调色定制加载到 ThemeRegistry。
 */
namespace UserTheme {

/**
 * @brief Installs built-in Fluent tokens plus user JSON overrides into ThemeRegistry.
 * zh_CN: 将内置 Fluent token 与用户 JSON 覆盖加载到 ThemeRegistry。
 */
void apply();

/**
 * @brief Applies sparse in-memory overrides to the current global theme without writing files.
 * zh_CN: 将局部内存配置叠加到当前全局主题，不写入磁盘文件。
 *
 * Accepts light/dark/contrast color objects, radius, and font (family/scale).
 * Invalid keys or values reject the entire update; missing fields remain unchanged.
 * zh_CN: 支持 light/dark/contrast 颜色组、圆角及字体（family/scale）；无效键或值会拒绝整次更新，未指定字段保持不变。
 *
 * @return true if state changed; false for invalid or unchanged overrides.
 * zh_CN: 状态发生变化时返回 true；配置无效或与当前相同时返回 false。
 */
bool applyOverrides(const QJsonObject& overrides);

/**
 * @brief Applies an in-memory accent color override to ThemeRegistry.
 * zh_CN: 对当前 ThemeRegistry 颜色应用内存中的强调色覆盖。
 */
void applyAccentOverride(const QColor& accent);

/**
 * @brief Returns the absolute path of the user-editable Fluent override file.
 * zh_CN: 返回用户可编辑 Fluent 覆盖文件的绝对路径。
 */
QString filePath();

/**
 * @brief Returns the directory containing user-editable Fluent override files.
 * zh_CN: 返回保存用户可编辑 Fluent 覆盖文件的目录路径。
 */
QString directory();

/**
 * @brief Explicitly exports a schema-versioned editable theme template.
 * zh_CN: 显式导出带 schema 版本的可编辑主题模板。
 *
 * Applying a theme never writes to disk. By default this preserves an existing
 * override file; pass @p overwrite as true only when replacement is intended.
 * zh_CN: 应用主题本身不会写入磁盘。默认保留已有的覆盖文件；仅当明确需要替换时传入 overwrite=true。
 */
bool exportTemplate(bool overwrite = false);

/**
 * @brief Persists a custom accent color into the user Fluent override file.
 * zh_CN: 将自定义强调色持久化到用户 Fluent 覆盖文件中。
 */
void setAccent(const QColor& accent);

/**
 * @brief Removes the persisted custom accent override from the user file.
 * zh_CN: 移除用户覆盖文件中已持久化的自定义强调色。
 */
void clearAccent();

/**
 * @brief Returns the built-in Fluent accent color for light or dark mode.
 * zh_CN: 返回深色或浅色模式下的内置 Fluent 强调色。
 */
QColor defaultAccent(bool dark);

} // namespace UserTheme

} // namespace fluent

#endif // FLUENT_USER_THEME_H
