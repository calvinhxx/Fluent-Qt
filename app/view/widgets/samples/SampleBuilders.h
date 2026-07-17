#ifndef GALLERYSAMPLEBUILDERS_H
#define GALLERYSAMPLEBUILDERS_H

#include <functional>

#include <QColor>
#include <QPixmap>
#include <QString>

#include "model/GalleryContentCatalog.h"

class QWidget;

namespace fluent::gallery::samples {

/**
 * @brief Assembles a GallerySample record from metadata plus a preview factory.
 * zh_CN: 由元数据和预览工厂组装一个 GallerySample 记录。
 */
GallerySample makeSample(const QString& id,
                         const QString& title,
                         const QString& description,
                         const QString& codeSnippet,
                         std::function<QWidget*(QWidget*)> createPreview);

/**
 * @brief Transparent container with a top-left aligned vertical layout.
 * zh_CN: 顶部左对齐纵向布局的透明容器。
 */
QWidget* verticalGroup(QWidget* parent, int spacing = 12);

/**
 * @brief Transparent container with a left-aligned horizontal layout.
 * zh_CN: 左对齐横向布局的透明容器。
 */
QWidget* horizontalGroup(QWidget* parent, int spacing = 12);

/**
 * @brief Rounded tile with a centered FluentQt Icons glyph.
 * zh_CN: 带居中 FluentQt Icons 字形的圆角图块。
 *
 * Used as item decoration in collection previews so rows carry an icon
 * alongside their text.
 * zh_CN: 用作集合类预览的条目装饰，让行在文本之外带一个图标。
 */
QPixmap glyphPixmap(const QString& glyph, const QColor& background, int size = 28);

/**
 * @brief Circular avatar showing the initials of a person's name.
 * zh_CN: 显示姓名首字母的圆形头像。
 */
QPixmap initialsAvatar(const QString& name, const QColor& background, int size = 28);

/**
 * @brief Smooth diagonal gradient used as a photo stand-in, with optional caption.
 * zh_CN: 平滑对角渐变，用作照片占位，支持可选标题文字。
 */
QPixmap gradientPixmap(const QSize& size,
                       const QColor& from,
                       const QColor& to,
                       const QString& caption = QString());

} // namespace fluent::gallery::samples

#endif // GALLERYSAMPLEBUILDERS_H
