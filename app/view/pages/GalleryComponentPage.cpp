#include "GalleryComponentPage.h"

#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/widgets/GalleryEntryCard.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"

namespace fluent::gallery {

GalleryComponentPage::GalleryComponentPage(const GalleryContentEntry& entry,
                                           const GalleryNavigationViewModel& navigationViewModel,
                                           QWidget* parent)
    : GalleryContentPage(entry.routeId, entry.title, QString(), parent)
    , m_overviewText(entry.description)
{
    setObjectName(QStringLiteral("galleryComponentPage"));

    addSectionHeader(QStringLiteral("Overview"));
    if (!m_overviewText.isEmpty())
        addBodyText(m_overviewText);

    addSectionHeader(QStringLiteral("Examples"));
    const QVector<GallerySample> samples = gallerySamplesForRoute(entry.routeId);
    for (const GallerySample& sample : samples) {
        auto* card = new GallerySampleCard(sample, this);
        addContentWidget(card);
        m_sampleCards.append(card);
    }

    addSectionHeader(QStringLiteral("API notes"));
    addBodyText(QStringLiteral(
        "These examples use the public %1 API from the Fluent Qt component library. "
        "No Gallery-specific behavior is added to the reusable control.")
                    .arg(entry.title));

    if (!entry.relatedRouteIds.isEmpty()) {
        addSectionHeader(QStringLiteral("Related"));
        for (const QString& relatedRouteId : entry.relatedRouteIds) {
            const GalleryNavigationItem* relatedItem = navigationViewModel.itemById(relatedRouteId);
            if (!relatedItem)
                continue;
            QString relatedDescription;
            if (const GalleryContentEntry* relatedEntry = galleryContentEntry(relatedRouteId))
                relatedDescription = relatedEntry->description;
            auto* card = new GalleryEntryCard(relatedItem->id,
                                              relatedItem->title,
                                              relatedDescription,
                                              this);
            // Categories have no per-control art; render their nav glyph instead.
            // zh_CN: 分类没有控件图片，改用其导航字形图标。
            if (relatedItem->kind != GalleryNavigationItem::Kind::ComponentRoute)
                card->setIconGlyph(relatedItem->iconGlyph);
            connect(card, &GalleryEntryCard::activated,
                    this, &GalleryContentPage::routeActivated);
            addContentWidget(card);
        }
    }
}

} // namespace fluent::gallery
