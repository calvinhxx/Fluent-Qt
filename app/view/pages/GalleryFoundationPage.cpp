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
        // Prefer designed topic artwork; component routes without dedicated artwork
        // use their catalog glyph instead of borrowing an unrelated image.
        // zh_CN: 优先使用主题专属图片；没有专属图片的组件路由改用目录字形，
        // 不再借用其他组件的图片。
        const QPixmap icon(galleryControlImageResource(item->title));
        entries.append({item->id,
                        item->title,
                        description,
                        icon,
                        icon.isNull() ? item->iconGlyph : QString()});
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
