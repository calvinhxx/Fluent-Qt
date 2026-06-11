#ifndef GALLERYSTATUSINFOSAMPLES_H
#define GALLERYSTATUSINFOSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Status & info category routes; empty when uncovered.
 * zh_CN: Status & info 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> statusInfoSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYSTATUSINFOSAMPLES_H
