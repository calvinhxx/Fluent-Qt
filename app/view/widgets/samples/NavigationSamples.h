#ifndef GALLERYNAVIGATIONSAMPLES_H
#define GALLERYNAVIGATIONSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Navigation category routes; empty when uncovered.
 * zh_CN: Navigation 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> navigationSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONSAMPLES_H
