#ifndef GALLERYQMLPLUSSAMPLES_H
#define GALLERYQMLPLUSSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Native interactive samples for the QMLPlus runtime.
 * zh_CN: QMLPlus 运行时的原生交互示例。
 */
QVector<GallerySample> qmlPlusSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYQMLPLUSSAMPLES_H
