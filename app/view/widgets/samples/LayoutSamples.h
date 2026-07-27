#ifndef GALLERYLAYOUTSAMPLES_H
#define GALLERYLAYOUTSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Layout category routes; empty when uncovered.
 * zh_CN: Layout 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> layoutSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYLAYOUTSAMPLES_H
