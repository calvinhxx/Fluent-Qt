#ifndef GALLERYBASICINPUTSAMPLES_H
#define GALLERYBASICINPUTSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Basic input category routes; empty when uncovered.
 * zh_CN: Basic input 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> basicInputSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYBASICINPUTSAMPLES_H
