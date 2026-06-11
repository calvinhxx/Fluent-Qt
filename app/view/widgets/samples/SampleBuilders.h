#ifndef GALLERYSAMPLEBUILDERS_H
#define GALLERYSAMPLEBUILDERS_H

#include <functional>

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

} // namespace fluent::gallery::samples

#endif // GALLERYSAMPLEBUILDERS_H
