#include "GalleryContentPresenter.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QTimer>
#include <QWidget>

#include "components/navigation/StackContentHost.h"
#include "model/GalleryContentCatalog.h"
#include "model/GalleryNavigationItem.h"
#include "support/logging/Log.h"
#include "view/pages/GalleryContentPage.h"
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

// Time budget for splash-phase prewarm. Building pages freezes the GUI thread, so we only warm
// while the splash hides it; once this elapses we stop and dismiss the splash, leaving the
// un-warmed tail to lazy + shimmer. Bounds startup wait, and adapts to machine speed (Release
// drains the whole queue well inside it). zh_CN: splash 期预热的时间预算。建页会冻结 GUI 线程，故只在
// splash 遮挡时预热；到点即停并消除 splash，剩余尾部交给懒加载+shimmer。限定启动等待，并自适应机器速度
//（Release 远在预算内就能排空整个队列）。
constexpr int kPrewarmBudgetMs = 3000;

} // namespace

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

bool GalleryContentPresenter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Paint
        && m_pendingPage
        && watched == m_pendingPage.data()
        && currentPage() == m_pendingPage.data()) {
        const QString routeId = m_pendingRouteId;
        const bool cold = m_pendingCold;
        const qint64 buildMs = m_pendingBuildMs;
        const qint64 switchMs = m_pendingSwitchMs;
        const qint64 totalMs = m_pendingNavigationTimer.isValid()
            ? m_pendingNavigationTimer.elapsed()
            : 0;
        cancelNavigationWatch();
        LOG_DEBUG(QStringLiteral(
                      "PERF navigationPresented routeId=%1 state=%2 buildMs=%3 switchMs=%4 totalMs=%5")
                      .arg(routeId,
                           cold ? QStringLiteral("cold") : QStringLiteral("warm"))
                      .arg(buildMs)
                      .arg(switchMs)
                      .arg(totalMs));
        emit navigationPresented(routeId, cold, buildMs, switchMs, totalMs);
    }
    return QObject::eventFilter(watched, event);
}

void GalleryContentPresenter::beginNavigationWatch(const QString& routeId,
                                                    bool cold,
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
    if (routeId != QStringLiteral("settings") && !galleryContentEntry(routeId)) {
        LOG_WARN(QStringLiteral("GalleryContentPresenter presentRoute rejected routeId=%1 reason=missing-content-entry")
                     .arg(routeId));
        return false;
    }

    const bool routeResident = m_routeStackIndex.contains(routeId);
    const bool routePending = m_pendingRequestId != 0 && m_pendingRouteId == routeId;
    if (m_currentRouteId == routeId && (routeResident || routePending)) {
        LOG_TRACE(QStringLiteral("GalleryContentPresenter presentRoute skipped routeId=%1 reason=already-current")
                      .arg(routeId));
        return true;
    }

    // Record the target route up front: a deferred lazy build re-checks this to confirm the
    // page it built is still the one the user wants before swapping it in.
    // zh_CN: 先记录目标路由：延迟的懒构建会复查它，确认建好的页面仍是用户想要的才换入。
    m_currentRouteId = routeId;
    const quint64 requestId = ++m_navigationRequestId;

    const int existing = m_routeStackIndex.value(routeId, -1);
    if (existing >= 0) {
        // Warm: the page is already built and resident — a pure show/hide (~1 ms).
        // zh_CN: 已预热：页面已建好常驻——纯显示/隐藏（约 1ms）。
        beginNavigationWatch(routeId, false, requestId);
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
    // Delay the build by kSkeletonRevealMs (not 0): a zero-timer would fire before the just-
    // shown skeleton paints, so the build would block the thread first and the skeleton would
    // never appear. Waiting a frame lets the skeleton render before the build runs.
    // zh_CN: 用 kSkeletonRevealMs 而非 0 延迟构建：零延迟会在刚切上的骨架绘制前触发，导致构建先阻塞线程、
    // 骨架根本不显示。等一帧让骨架先渲染，再执行构建。
    QTimer::singleShot(kSkeletonRevealMs, this, [this, routeId, requestId]() {
        // The user may have navigated on (or to a now-warm page) before this fired; only
        // spend the build if this route is still the one on screen.
        // zh_CN: 触发前用户可能已切走（或切到已预热页）；仅当该路由仍是屏幕上的目标时才花费构建。
        if (m_currentRouteId != routeId || m_navigationRequestId != requestId)
            return;
        qint64 buildMs = 0;
        const int index = ensurePageBuilt(routeId, &buildMs);
        if (index >= 0
            && m_currentRouteId == routeId
            && m_navigationRequestId == requestId) {
            watchNavigationPage(m_contentHost->pageWidget(index), buildMs);
            m_pendingSwitchMs = switchToStackPage(index);
        } else if (index < 0 && m_navigationRequestId == requestId) {
            cancelNavigationWatch();
        }
    });
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
        if (routeId.isEmpty() || m_routeStackIndex.contains(routeId) || m_prewarmQueue.contains(routeId))
            continue;
        m_prewarmQueue.enqueue(routeId);
    }
    if (m_prewarmQueue.isEmpty()) {
        // Nothing to warm — still notify so the splash dismisses. zh_CN: 没有要预热的——仍通知，让 splash 关闭。
        emit prewarmFinished();
        return;
    }
    m_prewarmTotal = m_prewarmQueue.size();
    m_prewarmDone = 0;
    LOG_DEBUG(QStringLiteral("GalleryContentPresenter prewarmRoutes queued=%1 budgetMs=%2")
                  .arg(m_prewarmTotal)
                  .arg(kPrewarmBudgetMs));
    emit prewarmProgress(0, 100);
    m_prewarmBudget.start();
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
    // One page per event-loop tick: the build freezes the thread, but yielding between pages
    // lets the splash spinner keep animating instead of locking up for the whole prewarm.
    // zh_CN: 每帧建一个：建页会冻结线程，但页间让出控制权，让 splash 转圈持续动画而非整段预热期间锁死。
    QTimer::singleShot(0, this, [this]() {
        m_prewarmScheduled = false;
        // Paused after this tick was queued (user grabbed the window): skip the build and wait —
        // setPrewarmPaused(false) re-kicks the pump. zh_CN: 排队后才被暂停（用户抓住了窗口）：跳过本次建页等待，
        // 由 setPrewarmPaused(false) 重启泵。
        if (m_prewarmPaused)
            return;

        // Skip anything already built (e.g. the user reached it first), then warm the next.
        // zh_CN: 跳过已建好的（如用户先到达的），再预热下一个。
        while (!m_prewarmQueue.isEmpty() && m_routeStackIndex.contains(m_prewarmQueue.head()))
            m_prewarmQueue.dequeue();

        if (m_prewarmQueue.isEmpty()) {
            emit prewarmProgress(100, 100);
            emit prewarmFinished();
            return;
        }
        ensurePageBuilt(m_prewarmQueue.dequeue());
        ++m_prewarmDone;
        // Show whichever is further along — pages warmed, or time spent against the budget — so
        // the caption climbs smoothly to ~100% whether the budget caps it (Debug) or the queue
        // drains first (Release), instead of stalling at a low page fraction.
        // zh_CN: 取「已预热页数」与「已用时间/预算」中较大者显示，使百分比平滑爬到约 100%——无论是预算封顶
        //（Debug）还是队列先排空（Release），而不会卡在很低的页数占比上。
        const int pagePct = m_prewarmDone * 100 / m_prewarmTotal;
        const int timePct = static_cast<int>(m_prewarmBudget.elapsed() * 100 / kPrewarmBudgetMs);
        emit prewarmProgress(qBound(0, qMax(pagePct, timePct), 100), 100);

        if (m_prewarmQueue.isEmpty() || m_prewarmBudget.elapsed() >= kPrewarmBudgetMs) {
            // Budget spent (or queue drained): stop warming and let the splash go. The tail
            // builds lazily behind the shimmer skeleton on first visit. Snap the caption to 100%
            // so it reads "ready", not stalled mid-load.
            // zh_CN: 预算用尽（或队列排空）：停止预热并放行 splash。尾部在首次访问时于 shimmer 骨架屏背后懒构建。
            // 把文字补到 100%，读起来是「就绪」而非加载到一半卡住。
            LOG_DEBUG(QStringLiteral("GalleryContentPresenter prewarm stopped warmed=%1 remaining=%2 elapsedMs=%3")
                          .arg(m_prewarmDone)
                          .arg(m_prewarmQueue.size())
                          .arg(m_prewarmBudget.elapsed()));
            m_prewarmQueue.clear();
            emit prewarmProgress(100, 100);
            emit prewarmFinished();
            return;
        }
        scheduleNextPrewarm();
    });
}

int GalleryContentPresenter::ensurePageBuilt(const QString& routeId, qint64* buildMs)
{
    if (buildMs)
        *buildMs = 0;
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
    if (!page) {
        LOG_WARN(QStringLiteral("GalleryContentPresenter ensurePageBuilt rejected routeId=%1 reason=missing-page")
                     .arg(routeId));
        return -1;
    }
    connectPageNavigation(page);
    const int index = m_contentHost->count();
    m_contentHost->insertPage(index, page);
    m_routeStackIndex.insert(routeId, index);
    const qint64 elapsedBuildMs = buildTimer.elapsed();
    if (buildMs)
        *buildMs = elapsedBuildMs;
    LOG_DEBUG(QStringLiteral("PERF buildPage routeId=%1 buildMs=%2 pageType=%3 stackIndex=%4")
                  .arg(routeId)
                  .arg(elapsedBuildMs)
                  .arg(QString::fromLatin1(page->metaObject()->className()))
                  .arg(index));
    return index;
}

void GalleryContentPresenter::connectPageNavigation(QWidget* page)
{
    if (auto* contentPage = dynamic_cast<GalleryContentPage*>(page)) {
        connect(contentPage, &GalleryContentPage::routeActivated,
                this, &GalleryContentPresenter::routeActivated);
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
    const qint64 switchMs = switchTimer.elapsed();
    LOG_DEBUG(QStringLiteral("PERF switchToStackPage from=%1 to=%2 switchMs=%3")
                 .arg(fromIndex)
                 .arg(targetIndex)
                 .arg(switchMs));
    return switchMs;
}

} // namespace fluent::gallery
