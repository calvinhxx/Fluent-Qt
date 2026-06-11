#ifndef GALLERYCOLLECTIONSSAMPLES_H
#define GALLERYCOLLECTIONSSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Collections category routes; empty when uncovered.
 * zh_CN: Collections 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> collectionsSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYCOLLECTIONSSAMPLES_H
