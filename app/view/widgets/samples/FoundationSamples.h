#ifndef GALLERYFOUNDATIONSAMPLES_H
#define GALLERYFOUNDATIONSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for visible Foundation components; empty when uncovered.
 * zh_CN: 可视 Foundation 组件的实样；未覆盖时返回空。
 */
QVector<GallerySample> foundationSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYFOUNDATIONSAMPLES_H
