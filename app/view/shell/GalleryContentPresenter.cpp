#include "GalleryContentPresenter.h"

#include <QWidget>

#include "components/navigation/StackContentHost.h"
#include "model/GalleryNavigationItem.h"
#include "utils/Log.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/GalleryPageFactory.h"
#include "view/pages/PlaceholderPage.h"
#include "viewmodel/GalleryNavigationViewModel.h"

namespace fluent::gallery {

GalleryContentPresenter::GalleryContentPresenter(
    fluent::navigation::StackContentHost* contentHost,
    const GalleryNavigationViewModel& navigationViewModel,
    QObject* parent)
    : QObject(parent)
    , m_contentHost(contentHost)
    , m_navigationViewModel(navigationViewModel)
{
}

QWidget* GalleryContentPresenter::currentPage() const
{
    return m_contentHost ? m_contentHost->pageWidget(0) : nullptr;
}

bool GalleryContentPresenter::presentRoute(const QString& routeId)
{
    if (!m_contentHost) {
        LOG_WARN(QStringLiteral("GalleryContentPresenter presentRoute rejected routeId=%1 reason=missing-content-host")
                     .arg(routeId));
        return false;
    }

    const GalleryNavigationItem* item = m_navigationViewModel.itemById(routeId);
    if (!item) {
        LOG_WARN(QStringLiteral("GalleryContentPresenter presentRoute rejected routeId=%1 reason=missing-route")
                     .arg(routeId));
        return false;
    }

    if (m_currentRouteId == routeId && m_contentHost->count() > 0) {
        LOG_TRACE(QStringLiteral("GalleryContentPresenter presentRoute skipped routeId=%1 reason=already-current")
                      .arg(routeId));
        return true;
    }

    GalleryPageFactory pageFactory(m_navigationViewModel);
    QWidget* page = pageFactory.createPage(routeId);
    if (!page)
        page = new PlaceholderPage(*item);
    connectPageNavigation(page);

    m_currentRouteId = routeId;
    LOG_DEBUG(QStringLiteral("GalleryContentPresenter presentRoute routeId=%1 pageType=%2 title=%3")
                  .arg(routeId,
                       QString::fromLatin1(page->metaObject()->className()),
                       item->title));

    replaceCurrentPage(routeId, page);
    return true;
}

void GalleryContentPresenter::connectPageNavigation(QWidget* page)
{
    if (auto* contentPage = dynamic_cast<GalleryContentPage*>(page)) {
        connect(contentPage, &GalleryContentPage::routeActivated,
                this, &GalleryContentPresenter::routeActivated);
    }
}

void GalleryContentPresenter::replaceCurrentPage(const QString& routeId, QWidget* page)
{
    if (m_contentHost->count() == 0) {
        m_contentHost->insertPage(0, page);
        m_contentHost->setCurrentIndex(0, 0, false);
        LOG_TRACE(QStringLiteral("GalleryContentPresenter contentHost pageInserted routeId=%1")
                      .arg(routeId));
        return;
    }

    QWidget* previousPage = m_contentHost->replacePage(0, page);
    // Defer deletion: an in-page card (e.g. a Home featured card) may trigger this
    // navigation from inside its own mouseReleaseEvent, so the previous page is still
    // dispatching the event. Deleting it now is a use-after-free; deleteLater frees it
    // once control returns to the event loop.
    // zh_CN: 延迟删除：页面内卡片可能在自身 mouseReleaseEvent 中触发此次导航，旧页面仍在
    // 派发事件，立即删除会造成 use-after-free；deleteLater 等回到事件循环后再释放。
    if (previousPage)
        previousPage->deleteLater();
    m_contentHost->setCurrentIndex(0, 0, false);
    LOG_TRACE(QStringLiteral("GalleryContentPresenter contentHost pageReplaced routeId=%1")
                  .arg(routeId));
}

} // namespace fluent::gallery
