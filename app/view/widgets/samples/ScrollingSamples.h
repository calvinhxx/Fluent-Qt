#ifndef GALLERYSCROLLINGSAMPLES_H
#define GALLERYSCROLLINGSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Scrolling category routes; empty when uncovered.
 * zh_CN: Scrolling 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> scrollingSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYSCROLLINGSAMPLES_H
