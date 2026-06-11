#ifndef GALLERYWINDOWINGSAMPLES_H
#define GALLERYWINDOWINGSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Windowing category routes; empty when uncovered.
 * zh_CN: Windowing 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> windowingSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYWINDOWINGSAMPLES_H
