#include "GalleryCategoryPage.h"

#include <QGridLayout>
#include <QWidget>

#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "view/widgets/GalleryEntryCard.h"

namespace fluent::gallery {
namespace {
constexpr int kCardColumns = 2;
}

GalleryCategoryPage::GalleryCategoryPage(const GalleryContentEntry& entry,
                                         const GalleryNavigationViewModel& navigationViewModel,
                                         QWidget* parent)
    : GalleryContentPage(entry.routeId, entry.title, entry.description, parent)
{
    setObjectName(QStringLiteral("galleryCategoryPage"));

    addSectionHeader(QStringLiteral("Components"));

    auto* cardsContainer = new QWidget(this);
    cardsContainer->setObjectName(QStringLiteral("galleryCategoryCards"));
    auto* grid = new QGridLayout(cardsContainer);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    for (int column = 0; column < kCardColumns; ++column)
        grid->setColumnStretch(column, 1);

    // An empty categoryId means "no filter": the All controls page lists everything.
    // zh_CN: categoryId 为空表示不过滤：All controls 页列出全部组件。
    const bool includeAllCategories = entry.categoryId.isEmpty();
    int cellIndex = 0;
    for (const GalleryNavigationItem& item : navigationViewModel.items()) {
        if (item.kind != GalleryNavigationItem::Kind::ComponentRoute)
            continue;
        if (!includeAllCategories && item.parentId != entry.categoryId)
            continue;

        m_componentRouteIds.append(item.id);

        QString description;
        if (const GalleryContentEntry* componentEntry = galleryContentEntry(item.id))
            description = componentEntry->description;

        auto* card = new GalleryEntryCard(item.id, item.title, description, cardsContainer);
        connect(card, &GalleryEntryCard::activated,
                this, &GalleryContentPage::routeActivated);
        grid->addWidget(card, cellIndex / kCardColumns, cellIndex % kCardColumns);
        ++cellIndex;
    }

    addContentWidget(cardsContainer);
}

} // namespace fluent::gallery
