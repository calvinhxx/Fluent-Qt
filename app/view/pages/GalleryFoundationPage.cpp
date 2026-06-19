#include "GalleryFoundationPage.h"

#include <QHash>
#include <QVector>

#include "design/Typography.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/widgets/GalleryEntryGrid.h"
#include "utils/Log.h"

namespace fluent::gallery {
namespace {

/**
 * @brief Per-topic glyph for the landing cards (Segoe Fluent Icons).
 * zh_CN: 落地卡片的每个主题字形（Segoe Fluent Icons）。
 */
QString topicGlyph(const QString& routeId)
{
    static const QHash<QString, QString> glyphs{
        {QStringLiteral("foundation-qmlplus"), Typography::Icons::Link},
        {QStringLiteral("foundation-typography"), Typography::Icons::Font},
        {QStringLiteral("foundation-color"), Typography::Icons::Color},
        {QStringLiteral("foundation-iconography"), Typography::Icons::Emoji},
        {QStringLiteral("foundation-geometry"), Typography::Icons::Grid},
    };
    return glyphs.value(routeId, Typography::Icons::Color);
}

} // namespace

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
        entries.append({item->id, item->title, description, QPixmap(), topicGlyph(routeId)});
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
