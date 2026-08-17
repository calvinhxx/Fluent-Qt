#ifndef GALLERYUSERTHEME_H
#define GALLERYUSERTHEME_H

#include <QColor>
#include <QString>

namespace fluent::gallery {

/**
 * @brief Gallery-facing adapter for Fluent token and accent customization.
 * zh_CN: 面向 Gallery 的 Fluent token 与强调色定制适配器。
 *
 * The reusable implementation lives in fluent::UserTheme. This namespace
 * only bridges desktop and sandboxed persistence backends. zh_CN:
 * 可复用实现在 fluent::UserTheme 中；此命名空间仅桥接桌面与沙箱持久化后端。
 */
namespace GalleryUserTheme {

void apply();

/// Absolute override path when the runtime supports theme files. zh_CN:
/// 运行时支持主题文件时的覆盖文件绝对路径。
QString filePath();

/// Override directory when supported by the runtime. zh_CN:
/// 运行时支持时的主题覆盖目录。
QString directory();

/// Export an editable theme template without replacing an existing override.
/// zh_CN: 导出可编辑主题模板，且不覆盖现有覆盖文件。
bool exportTemplate(bool overwrite = false);

/// Persist an accent through the selected runtime backend. zh_CN:
/// 通过所选运行时后端持久化强调色。
void setAccent(const QColor& accent);

/// Remove the persisted custom Fluent accent override. zh_CN:
/// 移除已持久化的 Fluent 自定义强调色覆盖。
void clearAccent();

/// Built-in Fluent accent for light/dark mode. zh_CN:
/// Fluent 在明暗模式下的内置强调色。
QColor defaultAccent(bool dark);

} // namespace GalleryUserTheme

} // namespace fluent::gallery

#endif // GALLERYUSERTHEME_H
