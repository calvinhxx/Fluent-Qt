#include "GalleryPageFactory.h"

#include <QWidget>

#include "model/GalleryContentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "support/logging/Log.h"
#include "GalleryCategoryPage.h"
#include "GalleryComponentPage.h"
#include "GalleryFoundationPage.h"
#include "GalleryFoundationTopicPage.h"
#include "GalleryHomePage.h"
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
            break;
        }
    }

    // Every navigable documentation route must have a concrete page. Missing
    // metadata is a catalog error rather than a user-facing placeholder state.
    // zh_CN: 每个可导航文档路由都必须有真实页面；缺失元数据属于目录错误，
    // 不再以面向用户的占位页掩盖。
    LOG_WARN(QStringLiteral("GalleryPageFactory createPage rejected routeId=%1 reason=missing-content-entry")
                 .arg(routeId));
    return nullptr;
}

} // namespace fluent::gallery
