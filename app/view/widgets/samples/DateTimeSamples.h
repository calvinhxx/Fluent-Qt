#ifndef GALLERYDATETIMESAMPLES_H
#define GALLERYDATETIMESAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Date & time category routes; empty when uncovered.
 * zh_CN: Date & time 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> dateTimeSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYDATETIMESAMPLES_H
