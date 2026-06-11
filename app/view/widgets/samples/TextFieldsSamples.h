#ifndef GALLERYTEXTFIELDSSAMPLES_H
#define GALLERYTEXTFIELDSSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Text fields category routes; empty when uncovered.
 * zh_CN: Text fields 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> textFieldsSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYTEXTFIELDSSAMPLES_H
