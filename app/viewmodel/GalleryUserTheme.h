#ifndef GALLERYUSERTHEME_H
#define GALLERYUSERTHEME_H

#include <QColor>
#include <QString>

namespace fluent::gallery {

/**
 * @brief Gallery-facing adapter for Fluent token and accent customization.
 * zh_CN: 面向 Gallery 的 Fluent token 与强调色定制适配器。
 *
 * Bridges desktop and sandboxed persistence backends while delegating token
 * management to fluent::UserTheme.
 * zh_CN: 桥接桌面与沙箱持久化后端，具体 token 管理委托给 fluent::UserTheme。
 */
namespace GalleryUserTheme {

void apply();

/** @brief Absolute override path when the runtime supports theme files.
 * zh_CN: 运行时支持主题文件时的覆盖文件绝对路径。 */
QString filePath();

/** @brief Override directory when supported by the runtime.
 * zh_CN: 运行时支持时的主题覆盖目录。 */
QString directory();

/** @brief Exports an editable theme template without replacing an existing override.
 * zh_CN: 导出可编辑主题模板，不覆盖已有文件。 */
bool exportTemplate(bool overwrite = false);

/** @brief Persists an accent color through the selected runtime backend.
 * zh_CN: 通过当前运行时后端保存强调色。 */
void setAccent(const QColor& accent);

/** @brief Removes the persisted custom Fluent accent override.
 * zh_CN: 移除已保存的自定义强调色覆盖。 */
void clearAccent();

/** @brief Returns the built-in Fluent accent for light or dark mode.
 * zh_CN: 返回明暗模式下的内置 Fluent 强调色。 */
QColor defaultAccent(bool dark);

} // namespace GalleryUserTheme

} // namespace fluent::gallery

#endif // GALLERYUSERTHEME_H
