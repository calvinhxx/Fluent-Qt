#ifndef GALLERYCATEGORYPAGE_H
#define GALLERYCATEGORYPAGE_H

#include <QStringList>

#include "model/GalleryContentCatalog.h"
#include "GalleryContentPage.h"

namespace fluent::gallery {

class GalleryNavigationViewModel;

/**
 * @brief Category overview page listing component cards for a component category.
 * zh_CN: 列出某组件分类下各组件卡片的分类概览页。
 */
class GalleryCategoryPage : public GalleryContentPage {
    Q_OBJECT

public:
    GalleryCategoryPage(const GalleryContentEntry& entry,
                        const GalleryNavigationViewModel& navigationViewModel,
                        QWidget* parent = nullptr);

    QStringList componentRouteIds() const { return m_componentRouteIds; }

private:
    QStringList m_componentRouteIds;
};

} // namespace fluent::gallery

#endif // GALLERYCATEGORYPAGE_H
