#include "GalleryFoundationPage.h"

#include <QPixmap>
#include <QVector>

#include "model/GalleryComponentCatalog.h"
#include "model/GalleryContentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "support/logging/Log.h"

namespace fluent::gallery {

GalleryFoundationPage::GalleryFoundationPage(const GalleryContentEntry& entry,
                                             const GalleryNavigationViewModel& navigationViewModel,
                                             QWidget* parent)
    : GalleryContentPage(entry.routeId, entry.title, entry.description, parent)
{
    setObjectName(QStringLiteral("galleryFoundationPage"));

    addSectionHeader(QStringLiteral("Topics"));

    auto* grid = new GalleryEntryGrid(this);
    QVector<GalleryEntryGrid::Entry> entries;
    for (const QString& routeId : entry.relatedRouteIds) {
        const GalleryNavigationItem* item = navigationViewModel.itemById(routeId);
        if (!item)
            continue;
        QString description;
        if (const GalleryContentEntry* topicEntry = galleryContentEntry(routeId))
            description = topicEntry->description;
        // Use the designed Foundation control image (indigo tile + white glyph), same as the
        // All-controls grid — leaving iconGlyph empty so the grid draws the pixmap, not a glyph.
        // zh_CN: 用设计好的 Foundation 控件图（靛蓝底+白色字形），与 All 控件网格一致——iconGlyph 留空，
        // 让网格绘制该图而非字形。
        entries.append({item->id, item->title, description,
                        QPixmap(galleryControlImageResource(item->title)), QString()});
    }

    grid->setEntries(entries);
    connect(grid, &GalleryEntryGrid::activated,
            this, &GalleryContentPage::routeActivated);
    addContentWidget(grid);

    LOG_DEBUG(QStringLiteral("GalleryFoundationPage created routeId=%1 cards=%2")
                  .arg(entry.routeId)
                  .arg(entries.size()));
}

} // namespace fluent::gallery
