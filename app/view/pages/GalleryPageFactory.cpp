#include "GalleryPageFactory.h"

#include <QWidget>

#include "model/GalleryContentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "utils/Log.h"
#include "GalleryCategoryPage.h"
#include "GalleryComponentPage.h"
#include "GalleryFoundationPage.h"
#include "GalleryFoundationTopicPage.h"
#include "GalleryHomePage.h"
#include "PlaceholderPage.h"
#include "SettingsPage.h"

namespace fluent::gallery {

GalleryPageFactory::GalleryPageFactory(const GalleryNavigationViewModel& navigationViewModel)
    : m_navigationViewModel(navigationViewModel)
{
}

QWidget* GalleryPageFactory::createPage(const QString& routeId) const
{
    const GalleryNavigationItem* item = m_navigationViewModel.itemById(routeId);
    if (!item) {
        LOG_WARN(QStringLiteral("GalleryPageFactory createPage rejected routeId=%1 reason=missing-route")
                     .arg(routeId));
        return nullptr;
    }

    // Settings is footer-owned and intentionally excluded from the content catalog.
    if (routeId == QStringLiteral("settings"))
        return new SettingsPage(*item);

    if (const GalleryContentEntry* entry = galleryContentEntry(routeId)) {
        switch (entry->kind) {
        case GalleryPageKind::Home:
            return new GalleryHomePage(*entry, m_navigationViewModel);
        case GalleryPageKind::Category:
            return new GalleryCategoryPage(*entry, m_navigationViewModel);
        case GalleryPageKind::Component:
            return new GalleryComponentPage(*entry, m_navigationViewModel);
        case GalleryPageKind::Foundation:
            return new GalleryFoundationPage(*entry, m_navigationViewModel);
        case GalleryPageKind::FoundationTopic:
            return new GalleryFoundationTopicPage(*entry, m_navigationViewModel);
        case GalleryPageKind::Settings:
        case GalleryPageKind::Fallback:
            break;
        }
    }

    // Known route without content metadata: keep the placeholder fallback for this phase.
    LOG_DEBUG(QStringLiteral("GalleryPageFactory createPage fallback routeId=%1 reason=missing-content-entry")
                  .arg(routeId));
    return new PlaceholderPage(*item);
}

} // namespace fluent::gallery
