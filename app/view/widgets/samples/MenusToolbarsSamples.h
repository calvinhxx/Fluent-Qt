#ifndef GALLERYMENUSTOOLBARSSAMPLES_H
#define GALLERYMENUSTOOLBARSSAMPLES_H

#include <QString>
#include <QVector>

#include "model/GalleryContentCatalog.h"

namespace fluent::gallery {

/**
 * @brief Live samples for the Menus & toolbars category routes; empty when uncovered.
 * zh_CN: Menus & toolbars 分类路由的实样；未覆盖时返回空。
 */
QVector<GallerySample> menusToolbarsSamples(const QString& routeId);

} // namespace fluent::gallery

#endif // GALLERYMENUSTOOLBARSSAMPLES_H
