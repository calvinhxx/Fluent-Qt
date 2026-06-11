#ifndef GALLERYDIALOGSFLYOUTSSAMPLES_H
#define GALLERYDIALOGSFLYOUTSSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Dialogs & flyouts category routes; empty when uncovered.
 * zh_CN: Dialogs & flyouts 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> dialogsFlyoutsSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYDIALOGSFLYOUTSSAMPLES_H
