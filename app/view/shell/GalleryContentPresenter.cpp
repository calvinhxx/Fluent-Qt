#include "GalleryContentPresenter.h"

#include <QAbstractAnimation>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPixmap>
#include <QPropertyAnimation>
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

    // Reuse a previously built page when we have one — building the page (sample cards, live
    // previews, code blocks) is the expensive synchronous work that made navigation janky.
    // zh_CN: 已建过的页面直接复用——建页（sample 卡片、live 预览、代码块）是同步重活，正是导航卡顿来源。
    QWidget* page = cachedPage(routeId);
    if (page) {
        LOG_DEBUG(QStringLiteral("GalleryContentPresenter presentRoute reuse routeId=%1").arg(routeId));
    } else {
        GalleryPageFactory pageFactory(m_navigationViewModel);
        page = pageFactory.createPage(routeId);
        if (!page)
            page = new PlaceholderPage(*item);
        connectPageNavigation(page);
        m_pageCache.insert(routeId, page);
        LOG_DEBUG(QStringLiteral("GalleryContentPresenter presentRoute build routeId=%1 pageType=%2 title=%3")
                      .arg(routeId,
                           QString::fromLatin1(page->metaObject()->className()),
                           item->title));
    }

    m_currentRouteId = routeId;
    replaceCurrentPage(routeId, page);
    return true;
}

QWidget* GalleryContentPresenter::cachedPage(const QString& routeId) const
{
    return m_pageCache.value(routeId).data();
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

    // Snapshot the outgoing page (still on screen, fully laid out) before swapping. We fade this
    // flat pixmap out over the new page rather than animating the live page — a cross-dissolve
    // that costs one grab, not a per-frame rasterization of a heavy page, so it stays smooth.
    // zh_CN: 换页前先把旧页（仍在屏幕、已完成布局）抓成快照。之后让这张静态图在新页之上淡出，
    // 而不是逐帧渲染重型的活动页面——只付一次 grab 成本的交叉淡化，保持流畅。
    QWidget* outgoing = m_contentHost->pageWidget(0);
    QPixmap snapshot;
    if (outgoing && outgoing->isVisible() && !outgoing->size().isEmpty())
        snapshot = outgoing->grab();

    QWidget* previousPage = m_contentHost->replacePage(0, page);
    // Keep the previous page alive for reuse instead of deleting it (rebuilding pages per
    // navigation is the jank). replacePage already detached it from the host (parent cleared)
    // — even though an in-page card may have triggered this from its own mouseReleaseEvent, we
    // only reparent (never delete) the still-dispatching page, so there is no use-after-free.
    // zh_CN: 旧页面保留复用而非删除（每次导航重建才是卡顿根因）。replacePage 已把它从 host 摘下
    // （清空 parent）——即便是页面内卡片在自身 mouseReleaseEvent 里触发的导航，我们也只重设父级、
    // 绝不删除仍在派发事件的页面，因此不会 use-after-free。
    if (previousPage)
        stashPage(previousPage);
    m_contentHost->setCurrentIndex(0, 0, false);

    if (!snapshot.isNull())
        startContentCrossfade(snapshot);

    LOG_TRACE(QStringLiteral("GalleryContentPresenter contentHost pageReplaced routeId=%1")
                  .arg(routeId));
}

void GalleryContentPresenter::startContentCrossfade(const QPixmap& snapshot)
{
    // Retire any still-fading overlay so fast back-to-back navigation doesn't stack snapshots.
    // zh_CN: 撤掉上一张还在淡出的快照，避免快速连续导航时叠加多张。
    if (m_transitionOverlay)
        m_transitionOverlay->deleteLater();

    auto* overlay = new QLabel(m_contentHost);
    overlay->setObjectName(QStringLiteral("galleryContentTransitionOverlay"));
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    overlay->setScaledContents(false);
    overlay->setPixmap(snapshot);
    overlay->setGeometry(m_contentHost->rect());
    overlay->show();
    overlay->raise();
    m_transitionOverlay = overlay;

    auto* opacity = new QGraphicsOpacityEffect(overlay);
    opacity->setOpacity(1.0);
    overlay->setGraphicsEffect(opacity);

    const auto motion = m_contentHost->themeAnimation();
    auto* fade = new QPropertyAnimation(opacity, "opacity", overlay);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setDuration(motion.normal);
    fade->setEasingCurve(motion.decelerate);
    QPointer<QWidget> guard = overlay;
    QObject::connect(fade, &QPropertyAnimation::finished, overlay, [this, guard]() {
        if (m_transitionOverlay == guard)
            m_transitionOverlay = nullptr;
        if (guard)
            guard->deleteLater();
    });
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void GalleryContentPresenter::stashPage(QWidget* page)
{
    if (!page)
        return;
    if (!m_pageStash) {
        m_pageStash = new QWidget(m_contentHost);
        m_pageStash->setObjectName(QStringLiteral("galleryPageStash"));
        m_pageStash->hide();
    }
    page->setParent(m_pageStash);
    page->hide();
}

} // namespace fluent::gallery
