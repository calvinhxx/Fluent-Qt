#include "GalleryCategoryPage.h"

#include <QPixmap>
#include <QVector>

#include "model/GalleryComponentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "support/logging/Log.h"

namespace fluent::gallery {

GalleryCategoryPage::GalleryCategoryPage(const GalleryContentEntry& entry,
                                         const GalleryNavigationViewModel& navigationViewModel,
                                         QWidget* parent)
    : GalleryContentPage(entry.routeId, entry.title, entry.description, parent)
{
    setObjectName(QStringLiteral("galleryCategoryPage"));

    addSectionHeader(QStringLiteral("Components"));

    // The cards are now data-driven and painted by a single virtualized grid rather
    // than one widget per control — this is what removes the bulk of a category page's
    // build cost. zh_CN: 卡片改为数据驱动、由单个虚拟化网格绘制，而非每个控件一个 widget——
    // 这正是砍掉分类页建页开销大头的关键。
    auto* grid = new GalleryEntryGrid(this);
    QVector<GalleryEntryGrid::Entry> entries;

    // An empty categoryId means "no filter": the All controls page lists everything.
    // zh_CN: categoryId 为空表示不过滤：All controls 页列出全部组件。
    const bool includeAllCategories = entry.categoryId.isEmpty();
    for (const GalleryNavigationItem& item : navigationViewModel.items()) {
        if (item.kind != GalleryNavigationItem::Kind::ComponentRoute)
            continue;
        if (!includeAllCategories && item.parentId != entry.categoryId)
            continue;

        m_componentRouteIds.append(item.id);

        QString description;
        if (const GalleryContentEntry* componentEntry = galleryContentEntry(item.id))
            description = componentEntry->description;

        entries.append({item.id, item.title, description,
                        QPixmap(galleryControlImageResource(item.title))});
    }

    grid->setEntries(entries);
    connect(grid, &GalleryEntryGrid::activated,
            this, &GalleryContentPage::routeActivated);
    addContentWidget(grid);

    LOG_DEBUG(QStringLiteral("GalleryCategoryPage created routeId=%1 categoryId=%2 cards=%3")
                  .arg(entry.routeId,
                       includeAllCategories ? QStringLiteral("<all>") : entry.categoryId)
                  .arg(entries.size()));
}

} // namespace fluent::gallery
