#include "GalleryContentPresenter.h"

#include <QElapsedTimer>
#include <QTimer>
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
    if (!m_contentHost)
        return nullptr;
    return m_contentHost->pageWidget(m_contentHost->currentIndex());
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

    // Build the page on first visit (or reuse the prewarmed one) and switch the stack
    // to it. With startup prewarm, the build has usually already happened, so this is a
    // pure show/hide. zh_CN: 首次访问才建页（或复用预建好的），然后把栈切过去。有了启动预建，
    // 建页通常已完成，这里就是纯显示/隐藏。
    QElapsedTimer presentTimer;
    presentTimer.start();

    const bool alreadyBuilt = m_routeStackIndex.contains(routeId);
    const int targetIndex = ensurePageBuilt(routeId);
    if (targetIndex < 0)
        return false;

    switchToStackPage(targetIndex);
    m_currentRouteId = routeId;
    // PERF: enable with SPDLOG_LEVEL=debug to profile navigation. After prewarm,
    // alreadyBuilt is true and totalMs is just the show/hide.
    // zh_CN: 用 SPDLOG_LEVEL=debug 开启以分析导航。预建后 alreadyBuilt 为真，totalMs 仅为显示/隐藏。
    LOG_DEBUG(QStringLiteral("PERF presentRoute routeId=%1 totalMs=%2 alreadyBuilt=%3")
                 .arg(routeId)
                 .arg(presentTimer.elapsed())
                 .arg(alreadyBuilt ? QStringLiteral("true") : QStringLiteral("false")));
    return true;
}

int GalleryContentPresenter::ensurePageBuilt(const QString& routeId)
{
    const int existing = m_routeStackIndex.value(routeId, -1);
    if (existing >= 0)
        return existing;

    const GalleryNavigationItem* item = m_navigationViewModel.itemById(routeId);
    if (!item) {
        LOG_WARN(QStringLiteral("GalleryContentPresenter ensurePageBuilt rejected routeId=%1 reason=missing-route")
                     .arg(routeId));
        return -1;
    }

    QElapsedTimer buildTimer;
    buildTimer.start();
    GalleryPageFactory pageFactory(m_navigationViewModel);
    QWidget* page = pageFactory.createPage(routeId);
    if (!page)
        page = new PlaceholderPage(*item);
    connectPageNavigation(page);
    const int index = m_contentHost->count();
    m_contentHost->insertPage(index, page);
    m_routeStackIndex.insert(routeId, index);
    LOG_DEBUG(QStringLiteral("PERF buildPage routeId=%1 buildMs=%2 pageType=%3 stackIndex=%4")
                  .arg(routeId)
                  .arg(buildTimer.elapsed())
                  .arg(QString::fromLatin1(page->metaObject()->className()))
                  .arg(index));
    return index;
}

void GalleryContentPresenter::prewarmRoutes(const QStringList& routeIds)
{
    if (!m_contentHost) {
        emit prewarmFinished();
        return;
    }

    for (const QString& routeId : routeIds) {
        if (routeId.isEmpty() || m_routeStackIndex.contains(routeId) || m_prewarmQueue.contains(routeId))
            continue;
        m_prewarmQueue.enqueue(routeId);
        ++m_prewarmTotal;
    }
    LOG_DEBUG(QStringLiteral("GalleryContentPresenter prewarmRoutes queued=%1 total=%2")
                  .arg(m_prewarmQueue.size())
                  .arg(m_prewarmTotal));
    if (m_prewarmQueue.isEmpty()) {
        // Nothing to build (everything already prewarmed) — still notify so any splash
        // waiting on us dismisses. zh_CN: 没有要建的（都已预建）——仍然通知，让等待的 splash 关闭。
        emit prewarmFinished();
        return;
    }
    scheduleNextPrewarm();
}

void GalleryContentPresenter::scheduleNextPrewarm()
{
    if (m_prewarmScheduled)
        return;
    m_prewarmScheduled = true;
    // One page per event-loop tick: heavy construction yields between pages so a splash
    // screen keeps animating instead of freezing for the whole prewarm.
    // zh_CN: 每个事件循环一帧建一个：页面间让出控制权，让 splash 持续动画而非整段预建期间冻结。
    QTimer::singleShot(0, this, [this]() {
        m_prewarmScheduled = false;
        if (m_prewarmQueue.isEmpty()) {
            emit prewarmFinished();
            return;
        }
        ensurePageBuilt(m_prewarmQueue.dequeue());
        ++m_prewarmDone;
        emit prewarmProgress(m_prewarmDone, m_prewarmTotal);
        if (!m_prewarmQueue.isEmpty())
            scheduleNextPrewarm();
        else
            emit prewarmFinished();
    });
}

void GalleryContentPresenter::connectPageNavigation(QWidget* page)
{
    if (auto* contentPage = dynamic_cast<GalleryContentPage*>(page)) {
        connect(contentPage, &GalleryContentPage::routeActivated,
                this, &GalleryContentPresenter::routeActivated);
    }
}

void GalleryContentPresenter::switchToStackPage(int targetIndex)
{
    // A pure show/hide of an already-built, already-laid-out page: ~1ms. We deliberately
    // do NOT cross-dissolve — that needs a full-page grab() of the outgoing page, which
    // costs ~0.5s when leaving a heavy live-demo page (e.g. Home) and was the last source
    // of click jank once builds moved to startup.
    // zh_CN: 对已建好、已布局的页面做纯显示/隐藏：约 1ms。刻意不做交叉淡化——那需要 grab() 旧页整页，
    // 离开重型 live demo 页（如 Home）时要约 0.5s，是建页移到启动后最后的点击卡顿来源。
    const int fromIndex = m_contentHost->currentIndex();
    QElapsedTimer switchTimer;
    switchTimer.start();
    m_contentHost->setCurrentIndex(targetIndex, 0, false);
    LOG_DEBUG(QStringLiteral("PERF switchToStackPage from=%1 to=%2 switchMs=%3")
                 .arg(fromIndex)
                 .arg(targetIndex)
                 .arg(switchTimer.elapsed()));
}

} // namespace fluent::gallery
