#include "GalleryContentPresenter.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QPixmap>
#include <QTimer>
#include <QWidget>

#include "components/navigation/StackContentHost.h"
#include "model/GalleryContentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "support/logging/Log.h"
#include "view/pages/GalleryContentPage.h"
#include "view/pages/GalleryComponentPage.h"
#include "view/pages/GalleryPageFactory.h"
#include "view/shell/GalleryPageSkeleton.h"
#include "viewmodel/GalleryNavigationViewModel.h"

namespace fluent::gallery {
namespace {

// Delay between showing the skeleton and building a cold page, long enough for the skeleton
// to paint a frame first (so it actually appears) before the build briefly blocks the thread.
// zh_CN: 显示骨架到构建冷页之间的延迟，足够骨架先绘制一帧（确保真的出现）再让构建短暂阻塞线程。
#if defined(QT_DEBUG)
// Debug page construction is deliberately expensive. Treat the skeleton window as a short
// debounce so rapid navigation can replace stale requests before a cold build blocks the UI.
constexpr int kSkeletonRevealMs = 100;
#else
// Release only needs enough time for the skeleton to reach the backing store once.
constexpr int kSkeletonRevealMs = 32;
#endif

} // namespace

GalleryContentPresenter::GalleryContentPresenter(
    fluent::navigation::StackContentHost* contentHost,
    const GalleryNavigationViewModel& navigationViewModel, QObject* parent, int maxResidentRoutes)
    : QObject(parent), m_contentHost(contentHost), m_navigationViewModel(navigationViewModel),
      m_maxResidentRoutes(qMax(0, maxResidentRoutes))
{}

GalleryContentPresenter::~GalleryContentPresenter()
{
    cancelLazyBuild();
    cancelPrewarm();
}

void GalleryContentPresenter::cancelLazyBuild()
{
    delete m_lazyPage.data();
    m_lazyPage.clear();
    m_lazyBuildMs = 0;
}

void GalleryContentPresenter::cancelPrewarm()
{
    m_prewarmPaused = true;
    m_prewarmQueue.clear();
    delete m_prewarmPage.data();
    m_prewarmPage.clear();
    m_prewarmRouteId.clear();
}

void GalleryContentPresenter::setStartupCovered(bool covered)
{
    m_startupCovered = covered;
    if (QWidget* page = currentPage())
        page->setVisible(!covered);
}

QWidget* GalleryContentPresenter::currentPage() const
{
    if (!m_contentHost)
        return nullptr;
    return m_contentHost->pageWidget(m_contentHost->currentIndex());
}

bool GalleryContentPresenter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Paint && m_pendingPage && watched == m_pendingPage.data() &&
        currentPage() == m_pendingPage.data()) {
        const QString routeId = m_pendingRouteId;
        const bool cold = m_pendingCold;
        const qint64 buildMs = m_pendingBuildMs;
        const qint64 switchMs = m_pendingSwitchMs;
        const qint64 totalMs =
            m_pendingNavigationTimer.isValid() ? m_pendingNavigationTimer.elapsed() : 0;
        cancelNavigationWatch();
        LOG_DEBUG(
            QStringLiteral(
                "PERF navigationPresented routeId=%1 state=%2 buildMs=%3 switchMs=%4 totalMs=%5")
                .arg(routeId, cold ? QStringLiteral("cold") : QStringLiteral("warm"))
                .arg(buildMs)
                .arg(switchMs)
                .arg(totalMs));
        emit navigationPresented(routeId, cold, buildMs, switchMs, totalMs);
    }
    return QObject::eventFilter(watched, event);
}

void GalleryContentPresenter::beginNavigationWatch(const QString& routeId, bool cold,
                                                   quint64 requestId)
{
    cancelNavigationWatch();
    m_pendingRequestId = requestId;
    m_pendingRouteId = routeId;
    m_pendingCold = cold;
    m_pendingNavigationTimer.start();
}

void GalleryContentPresenter::watchNavigationPage(QWidget* page, qint64 buildMs)
{
    if (!page || m_pendingRequestId != m_navigationRequestId)
        return;
    if (m_pendingPage && m_pendingPage != page)
        m_pendingPage->removeEventFilter(this);
    m_pendingPage = page;
    m_pendingBuildMs = buildMs;
    page->installEventFilter(this);
}

void GalleryContentPresenter::cancelNavigationWatch()
{
    if (m_pendingPage)
        m_pendingPage->removeEventFilter(this);
    m_pendingRequestId = 0;
    m_pendingRouteId.clear();
    m_pendingPage.clear();
    m_pendingNavigationTimer.invalidate();
    m_pendingBuildMs = 0;
    m_pendingSwitchMs = 0;
    m_pendingCold = false;
}

bool GalleryContentPresenter::presentRoute(const QString& routeId)
{
    if (!m_contentHost) {
        LOG_WARN(QStringLiteral("GalleryContentPresenter presentRoute rejected routeId=%1 "
                                "reason=missing-content-host")
                     .arg(routeId));
        return false;
    }

    const GalleryNavigationItem* item = m_navigationViewModel.itemById(routeId);
    if (!item) {
        LOG_WARN(
            QStringLiteral(
                "GalleryContentPresenter presentRoute rejected routeId=%1 reason=missing-route")
                .arg(routeId));
        return false;
    }
    if (routeId != QStringLiteral("settings") && !galleryContentEntry(routeId)) {
        LOG_WARN(QStringLiteral("GalleryContentPresenter presentRoute rejected routeId=%1 "
                                "reason=missing-content-entry")
                     .arg(routeId));
        return false;
    }

    const bool routeResident = m_routeStackIndex.contains(routeId);
    const bool routePending = m_pendingRequestId != 0 && m_pendingRouteId == routeId;
    if (m_currentRouteId == routeId && (routeResident || routePending)) {
        LOG_TRACE(
            QStringLiteral(
                "GalleryContentPresenter presentRoute skipped routeId=%1 reason=already-current")
                .arg(routeId));
        return true;
    }

    // Record the target route up front: a deferred lazy build re-checks this to confirm the
    // page it built is still the one the user wants before swapping it in.
    // zh_CN: 先记录目标路由：延迟的懒构建会复查它，确认建好的页面仍是用户想要的才换入。
    cancelLazyBuild();
    m_currentRouteId = routeId;
    const quint64 requestId = ++m_navigationRequestId;

    const int existing = m_routeStackIndex.value(routeId, -1);
    if (existing >= 0) {
        // Warm: the page is already built and resident — a pure show/hide (~1 ms).
        // zh_CN: 已预热：页面已建好常驻——纯显示/隐藏（约 1ms）。
        beginNavigationWatch(routeId, false, requestId);
        touchResidentRoute(routeId);
        watchNavigationPage(m_contentHost->pageWidget(existing), 0);
        m_pendingSwitchMs = switchToStackPage(existing);
        return true;
    }

    if (m_routeStackIndex.isEmpty()) {
        // First / startup route, built behind the splash: build synchronously so the splash
        // hands straight to real content with no skeleton flash.
        // zh_CN: 首个/启动路由，在 splash 背后构建：同步建好，使 splash 直接交给真内容，无骨架闪烁。
        beginNavigationWatch(routeId, true, requestId);
        qint64 buildMs = 0;
        const int index = ensurePageBuilt(routeId, &buildMs);
        if (index < 0) {
            cancelNavigationWatch();
            return false;
        }
        watchNavigationPage(m_contentHost->pageWidget(index), buildMs);
        m_pendingSwitchMs = switchToStackPage(index);
        return true;
    }

    // Cold route during normal use: show the shimmer skeleton immediately, return from the
    // input handler, then build the real page after the skeleton has painted one frame.
    // zh_CN: 正常使用中的冷路由先显示 shimmer 骨架并退出输入处理，再在骨架绘制一帧后构建真页。
    beginNavigationWatch(routeId, true, requestId);
    ensureSkeleton();
    switchToStackPage(m_skeletonIndex);
    scheduleLazyBuild(routeId, requestId);
    return true;
}

void GalleryContentPresenter::ensureSkeleton()
{
    if (m_skeletonIndex >= 0)
        return;
    // One reusable skeleton, appended once and then only ever shown/hidden. Appending keeps
    // every route's recorded stack index stable. zh_CN: 单个可复用骨架，只追加一次，之后仅显示/隐藏。
    // 追加保证每个路由记录的栈索引稳定。
    m_skeleton = new GalleryPageSkeleton;
    m_skeletonIndex = m_contentHost->count();
    m_contentHost->insertPage(m_skeletonIndex, m_skeleton);
}

void GalleryContentPresenter::scheduleLazyBuild(const QString& routeId, quint64 requestId)
{
    QTimer::singleShot(kSkeletonRevealMs, this,
                       [this, routeId, requestId] { buildNextLazyPart(routeId, requestId); });
}

void GalleryContentPresenter::buildNextLazyPart(const QString& routeId, quint64 requestId)
{
    if (m_currentRouteId != routeId || m_navigationRequestId != requestId)
        return;
    QElapsedTimer clock;
    clock.start();
    if (!m_lazyPage) {
        GalleryPageFactory factory(m_navigationViewModel);
        m_lazyPage = factory.createPage(routeId, m_contentHost, true);
        if (!m_lazyPage) {
            cancelNavigationWatch();
            return;
        }
        m_lazyPage->hide();
        m_lazyPage->resize(m_contentHost->contentsRect().size());
    } else if (auto* page = qobject_cast<GalleryComponentPage*>(m_lazyPage.data());
               page && page->hasPendingSamples()) {
        page->buildNextSample();
    } else {
        const int index = registerPage(routeId, m_lazyPage);
        m_lazyPage.clear();
        m_lazyBuildMs += clock.elapsed();
        watchNavigationPage(m_contentHost->pageWidget(index), m_lazyBuildMs);
        m_pendingSwitchMs = switchToStackPage(index);
        m_lazyBuildMs = 0;
        return;
    }
    m_lazyBuildMs += clock.elapsed();
    // A single sample factory is the indivisible GUI-thread unit. Let input
    // and Shimmer paint between units; stale navigation cancels the owned page.
    QTimer::singleShot(8, this,
                       [this, routeId, requestId] { buildNextLazyPart(routeId, requestId); });
}

void GalleryContentPresenter::prewarmRoutes(const QStringList& routeIds)
{
    if (!m_contentHost) {
        emit prewarmFinished();
        return;
    }

    // Prepare the reusable skeleton behind the startup splash, after Home is
    // already current. It stays hidden (and its animation timer stays stopped)
    // until a genuinely cold route is requested, removing Shimmer construction
    // and first layout from the user's click path.
    // zh_CN: Home 已成为当前页后，在启动 splash 背后预先准备可复用骨架。它保持隐藏，
    // 动画定时器也不启动，直到真正请求冷路由；由此把 Shimmer 构造和首次布局移出点击路径。
    ensureSkeleton();

    for (const QString& routeId : routeIds) {
        if (routeId.isEmpty() || m_routeStackIndex.contains(routeId) ||
            m_prewarmQueue.contains(routeId))
            continue;
        m_prewarmQueue.enqueue(routeId);
    }
    if (m_prewarmQueue.isEmpty()) {
        // Nothing to warm — still notify so the splash dismisses. zh_CN: 没有要预热的——仍通知，让 splash 关闭。
        const QPointer<GalleryContentPresenter> guard(this);
        emit prewarmProgress(0, 0);
        if (guard)
            emit prewarmFinished();
        return;
    }
    m_prewarmTotal = m_prewarmDone + m_prewarmQueue.size();
    LOG_DEBUG(QStringLiteral("GalleryContentPresenter prewarmRoutes queued=%1")
                  .arg(m_prewarmQueue.size()));
    const QPointer<GalleryContentPresenter> guard(this);
    emit prewarmProgress(m_prewarmDone, m_prewarmTotal);
    if (!guard)
        return;
    if (!m_prewarmTimer.isValid())
        m_prewarmTimer.start();
    scheduleNextPrewarm();
}

void GalleryContentPresenter::setPrewarmPaused(bool paused)
{
    if (m_prewarmPaused == paused)
        return;
    m_prewarmPaused = paused;
    // Resuming re-kicks the pump only if there is still work; a drained queue stays finished so we
    // never re-emit prewarmFinished. zh_CN: 恢复时仅在仍有待预热项时重启泵；已排空的队列保持完成，绝不重复发 prewarmFinished。
    if (!paused && !m_prewarmQueue.isEmpty())
        scheduleNextPrewarm();
}

void GalleryContentPresenter::scheduleNextPrewarm()
{
    if (m_prewarmScheduled || m_prewarmPaused)
        return;
    m_prewarmScheduled = true;
    // One sample factory per turn, followed by a separate first-layout turn. Never process
    // events recursively inside a factory: ownership and navigation stay non-reentrant.
    // zh_CN: 每轮一个示例工厂，首次布局单独一轮；不在工厂内递归处理事件，保证所有权和导航不重入。
    QTimer::singleShot(0, this, [this]() {
        m_prewarmScheduled = false;
        // Paused after this tick was queued (user grabbed the window): skip the build and wait —
        // setPrewarmPaused(false) re-kicks the pump. zh_CN: 排队后才被暂停（用户抓住了窗口）：跳过本次建页等待，
        // 由 setPrewarmPaused(false) 重启泵。
        if (m_prewarmPaused)
            return;

        // Skip anything already built (e.g. the user reached it first), then warm the next.
        // zh_CN: 跳过已建好的（如用户先到达的），再预热下一个。
        while (!m_prewarmQueue.isEmpty() && m_routeStackIndex.contains(m_prewarmQueue.head())) {
            m_prewarmQueue.dequeue();
            ++m_prewarmDone;
            const QPointer<GalleryContentPresenter> guard(this);
            emit prewarmProgress(m_prewarmDone, m_prewarmTotal);
            if (!guard || m_prewarmPaused)
                return;
        }

        if (m_prewarmQueue.isEmpty()) {
            LOG_DEBUG(QStringLiteral(
                          "GalleryContentPresenter prewarm finished ready=%1 total=%2 elapsedMs=%3")
                          .arg(m_prewarmDone)
                          .arg(m_prewarmTotal)
                          .arg(m_prewarmTimer.elapsed()));
            emit prewarmFinished();
            return;
        }
        const QString routeId = m_prewarmQueue.head();
        if (!m_prewarmPage) {
            m_prewarmRouteId = routeId;
            GalleryPageFactory factory(m_navigationViewModel);
            m_prewarmPage = factory.createPage(routeId, m_contentHost, true);
            if (!m_prewarmPage) {
                m_prewarmQueue.dequeue();
                m_prewarmRouteId.clear();
                const QPointer<GalleryContentPresenter> guard(this);
                emit prewarmFailed(routeId);
                if (!guard)
                    return;
            } else {
                m_prewarmPage->hide();
                m_prewarmPage->resize(m_contentHost->contentsRect().size());
            }
            scheduleNextPrewarm();
            return;
        }
        auto* componentPage = qobject_cast<GalleryComponentPage*>(m_prewarmPage.data());
        if (componentPage && componentPage->hasPendingSamples()) {
            componentPage->buildNextSample();
            scheduleNextPrewarm();
            return;
        }
        // Flush delayed layout without showing a hidden demo or retaining a screenshot.
        // zh_CN: 完成延迟布局，不显示隐藏示例，也不保留截图。
        registerPage(routeId, m_prewarmPage);
        m_prewarmPage->resize(m_contentHost->contentsRect().size());
        m_prewarmPage->grab(QRect(0, 0, 1, 1));
        m_prewarmPage.clear();
        m_prewarmRouteId.clear();
        scheduleNextPrewarm();
    });
}

int GalleryContentPresenter::ensurePageBuilt(const QString& routeId, qint64* buildMs)
{
    if (buildMs)
        *buildMs = 0;
    const int existing = m_routeStackIndex.value(routeId, -1);
    if (existing >= 0) {
        touchResidentRoute(routeId);
        return existing;
    }

    const GalleryNavigationItem* item = m_navigationViewModel.itemById(routeId);
    if (!item) {
        LOG_WARN(
            QStringLiteral(
                "GalleryContentPresenter ensurePageBuilt rejected routeId=%1 reason=missing-route")
                .arg(routeId));
        return -1;
    }

    QElapsedTimer buildTimer;
    buildTimer.start();
    GalleryPageFactory pageFactory(m_navigationViewModel);
    QWidget* page = m_prewarmRouteId == routeId ? m_prewarmPage.data() : nullptr;
    if (page) {
        // A programmatic navigation can overtake warm-up; finish and reuse its owned page.
        // zh_CN: 程序化导航可能抢先到达预热页；补完并复用，避免创建第二份。
        if (auto* componentPage = qobject_cast<GalleryComponentPage*>(page)) {
            while (componentPage->hasPendingSamples())
                componentPage->buildNextSample();
        }
        m_prewarmPage.clear();
        m_prewarmRouteId.clear();
    } else {
        page = pageFactory.createPage(routeId, m_contentHost);
    }
    if (!page) {
        LOG_WARN(
            QStringLiteral(
                "GalleryContentPresenter ensurePageBuilt rejected routeId=%1 reason=missing-page")
                .arg(routeId));
        return -1;
    }
    const int residentIndex = registerPage(routeId, page);
    const qint64 elapsedBuildMs = buildTimer.elapsed();
    if (buildMs)
        *buildMs = elapsedBuildMs;
    LOG_DEBUG(QStringLiteral("PERF buildPage routeId=%1 buildMs=%2 pageType=%3 stackIndex=%4")
                  .arg(routeId)
                  .arg(elapsedBuildMs)
                  .arg(QString::fromLatin1(page->metaObject()->className()))
                  .arg(residentIndex));
    return residentIndex;
}

int GalleryContentPresenter::registerPage(const QString& routeId, QWidget* page)
{
    connectPageNavigation(page);
    const int index = m_contentHost->count();
    m_contentHost->insertPage(index, page);
    m_routeStackIndex.insert(routeId, index);
    touchResidentRoute(routeId);
    trimResidentRoutes(routeId);
    return m_routeStackIndex.value(routeId, -1);
}

void GalleryContentPresenter::touchResidentRoute(const QString& routeId)
{
    m_routeRecency.removeAll(routeId);
    m_routeRecency.append(routeId);
}

void GalleryContentPresenter::trimResidentRoutes(const QString& protectedRouteId)
{
    if (!m_contentHost || m_maxResidentRoutes <= 0)
        return;

    while (m_routeStackIndex.size() > m_maxResidentRoutes) {
        QString victimRouteId;
        for (const QString& routeId : m_routeRecency) {
            if (routeId != protectedRouteId && m_routeStackIndex.contains(routeId)) {
                victimRouteId = routeId;
                break;
            }
        }
        if (victimRouteId.isEmpty())
            return;

        const int removedIndex = m_routeStackIndex.value(victimRouteId);
        if (!m_contentHost->releasePage(removedIndex)) {
            LOG_WARN(
                QStringLiteral("GalleryContentPresenter cache eviction failed routeId=%1 index=%2")
                    .arg(victimRouteId)
                    .arg(removedIndex));
            return;
        }
        m_routeStackIndex.remove(victimRouteId);
        m_routeRecency.removeAll(victimRouteId);

        for (auto it = m_routeStackIndex.begin(); it != m_routeStackIndex.end(); ++it) {
            if (it.value() > removedIndex)
                --it.value();
        }
        if (m_skeletonIndex > removedIndex)
            --m_skeletonIndex;

        LOG_DEBUG(
            QStringLiteral("GalleryContentPresenter cache evicted routeId=%1 resident=%2 limit=%3")
                .arg(victimRouteId)
                .arg(m_routeStackIndex.size())
                .arg(m_maxResidentRoutes));
    }
}

void GalleryContentPresenter::connectPageNavigation(QWidget* page)
{
    if (auto* contentPage = dynamic_cast<GalleryContentPage*>(page)) {
        connect(contentPage, &GalleryContentPage::routeActivated, this,
                &GalleryContentPresenter::routeActivated);
    }
}

qint64 GalleryContentPresenter::switchToStackPage(int targetIndex)
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
    if (m_startupCovered) {
        if (QWidget* page = currentPage())
            page->hide();
    }
    const qint64 switchMs = switchTimer.elapsed();
    LOG_DEBUG(QStringLiteral("PERF switchToStackPage from=%1 to=%2 switchMs=%3")
                  .arg(fromIndex)
                  .arg(targetIndex)
                  .arg(switchMs));
    return switchMs;
}

} // namespace fluent::gallery
