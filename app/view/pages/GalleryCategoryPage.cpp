#include "GalleryCategoryPage.h"

#include <QVector>
#include <QSet>

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

    const bool pluginIndex = entry.routeId == QStringLiteral("plugins");
    addSectionHeader(pluginIndex ? QStringLiteral("Plugin modules") : QStringLiteral("Components"));

    // The cards are now data-driven and painted by a single virtualized grid rather
    // than one widget per control — this is what removes the bulk of a category page's
    // build cost. zh_CN: 卡片改为数据驱动、由单个虚拟化网格绘制，而非每个控件一个 widget——
    // 这正是砍掉分类页建页开销大头的关键。
    auto* grid = new GalleryEntryGrid(this);
    QVector<GalleryEntryGrid::Entry> entries;

    // An empty categoryId lists base components; Plugins lists module routes.
    // zh_CN: categoryId 为空时列出基础组件，Plugins 页列出插件模块路由。
    const bool includeAllCategories = entry.categoryId.isEmpty();
    QSet<QString> pluginCategories;
    for (const auto& category : galleryComponentCatalog()) {
        if (category.plugin)
            pluginCategories.insert(category.id);
    }
    for (const GalleryNavigationItem& item : navigationViewModel.items()) {
        if (pluginIndex ? item.kind != GalleryNavigationItem::Kind::CategoryRoute
                        : item.kind != GalleryNavigationItem::Kind::ComponentRoute)
            continue;
        if (!includeAllCategories && item.parentId != entry.categoryId)
            continue;
        if (includeAllCategories && pluginCategories.contains(item.parentId))
            continue;

        m_componentRouteIds.append(item.id);

        QString description;
        if (const GalleryContentEntry* componentEntry = galleryContentEntry(item.id))
            description = componentEntry->description;

        const QString imageResource = galleryControlImageResource(item.title);
        entries.append({item.id, item.title, description, imageResource,
                        imageResource.isEmpty() ? item.iconGlyph : QString()});
    }

    grid->setEntries(entries);
    connect(grid, &GalleryEntryGrid::activated, this, &GalleryContentPage::routeActivated);
    addContentWidget(grid);

    LOG_DEBUG(
        QStringLiteral("GalleryCategoryPage created routeId=%1 categoryId=%2 cards=%3")
            .arg(entry.routeId, includeAllCategories ? QStringLiteral("<all>") : entry.categoryId)
            .arg(entries.size()));
}

} // namespace fluent::gallery
