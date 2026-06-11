#ifndef GALLERYSAMPLECATALOG_H
#define GALLERYSAMPLECATALOG_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Resolves the live samples referenced by a component route's content entry.
 * zh_CN: 解析组件路由内容条目引用的实样集合。
 *
 * Each returned sample carries preview/options factories that build live reusable
 * Fluent controls using their public APIs only.
 * zh_CN: 返回的每个实样都带有预览/选项工厂，仅使用公共 API 构建实时 Fluent 控件。
 */
QVector<GallerySample> gallerySamplesForRoute(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYSAMPLECATALOG_H
