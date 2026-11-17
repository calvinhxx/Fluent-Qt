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

GalleryContentPresenter::GalleryContentPresenter(fluent::navigation::StackContentHost* contentHost,
    const GalleryNavigationViewModel& navigationViewModel,
    QObject* parent,
    int maxResidentRoutes)
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
    if (event->type() == QEvent::Paint && m_pendingPage && watched == m_pendingPage.data()
        && currentPage() == m_pendingPage.data()) {
        const QString routeId = m_pendingRouteId;
        const bool cold = m_pendingCold;
        const qint64 buildMs = m_pendingBuildMs;
        const qint64 switchMs = m_pendingSwitchMs;
        const qint64 totalMs = m_pendingNavigationTimer.isValid() ? m_pendingNavigationTimer.elapsed() : 0;
        cancelNavigationWatch();
        LOG_DEBUG(QStringLiteral("PERF navigationPresented routeId=%1 state=%2 buildMs=%3 switchMs=%4 totalMs=%5")
                .arg(routeId, cold ? QStringLiteral("cold") : QStringLiteral("warm"))
                .arg(buildMs)
                .arg(switchMs)
                .arg(totalMs));
        emit navigationPresented(routeId, cold, buildMs, switchMs, totalMs);
    }
    return QObject::eventFilter(watched, event);
}

void GalleryContentPresenter::beginNavigationWatch(const QString& routeId, bool cold, quint64 requestId)
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
        LOG_WARN(QStringLiteral(
            "GalleryContentPresenter presentRoute rejected routeId=%1 "
            "reason=missing-content-host")
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
        LOG_WARN(QStringLiteral(
            "GalleryContentPresenter presentRoute rejected routeId=%1 "
            "reason=missing-content-entry")
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
    cancelLazyBuild();
    m_currentRouteId = routeId;
    const quint64 requestId = ++m_navigationRequestId;

    const int existing = m_routeStackIndex.value(routeId, -1);
    if (existing >= 0) {
        beginNavigationWatch(routeId, false, requestId);
        touchResidentRoute(routeId);
        watchNavigationPage(m_contentHost->pageWidget(existing), 0);
        m_pendingSwitchMs = switchToStackPage(existing);
        return true;
    }

    if (m_routeStackIndex.isEmpty()) {
        // Build the initial startup route synchronously so the splash hands off directly to real content.
        // zh_CN: 启动首路由在遮罩后同步构建，确保退场时直接切换到真实内容而无骨架闪烁。
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

    // Cold route: show the shimmer skeleton first and start incremental construction after one frame.
    // zh_CN: 冷路由先展示骨架屏，待其完成首帧绘制后再分步构建真页。
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
    // Append once so previously recorded page stack indices never shift.
    m_skeleton = new GalleryPageSkeleton;
    m_skeletonIndex = m_contentHost->count();
    m_contentHost->insertPage(m_skeletonIndex, m_skeleton);
}

void GalleryContentPresenter::scheduleLazyBuild(const QString& routeId, quint64 requestId)
{
    QTimer::singleShot(kSkeletonRevealMs, this, [this, routeId, requestId] { buildNextLazyPart(routeId, requestId); });
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
    } else if (auto* page = qobject_cast<GalleryComponentPage*>(m_lazyPage.data()); page && page->hasPendingSamples()) {
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
    QTimer::singleShot(8, this, [this, routeId, requestId] { buildNextLazyPart(routeId, requestId); });
}

void GalleryContentPresenter::prewarmRoutes(const QStringList& routeIds)
{
    if (!m_contentHost) {
        emit prewarmFinished();
        return;
    }

    // Pre-create the hidden skeleton behind the splash so the first cold route never stalls on Shimmer setup.
    ensureSkeleton();

    for (const QString& routeId : routeIds) {
        if (routeId.isEmpty() || m_routeStackIndex.contains(routeId) || m_prewarmQueue.contains(routeId))
            continue;
        m_prewarmQueue.enqueue(routeId);
    }
    if (m_prewarmQueue.isEmpty()) {
        // Notify even when empty so the splash screen can dismiss.
        const QPointer<GalleryContentPresenter> guard(this);
        emit prewarmProgress(0, 0);
        if (guard)
            emit prewarmFinished();
        return;
    }
    m_prewarmTotal = m_prewarmDone + m_prewarmQueue.size();
    LOG_DEBUG(QStringLiteral("GalleryContentPresenter prewarmRoutes queued=%1").arg(m_prewarmQueue.size()));
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
    // Resume only when work remains so an empty queue never re-emits prewarmFinished.
    if (!paused && !m_prewarmQueue.isEmpty())
        scheduleNextPrewarm();
}

void GalleryContentPresenter::scheduleNextPrewarm()
{
    if (m_prewarmScheduled || m_prewarmPaused)
        return;
    m_prewarmScheduled = true;
    // Build one sample per turn without nested event loops so ownership and navigation stay non-reentrant.
    // zh_CN: 每轮事件循环只构建一个示例，且不递归处理事件，保证页面所有权与导航状态不重入。
    QTimer::singleShot(0, this, [this]() {
        m_prewarmScheduled = false;
        if (m_prewarmPaused)
            return;

        // Skip routes already built (e.g. if the user navigated to one before prewarm reached it).
        while (!m_prewarmQueue.isEmpty() && m_routeStackIndex.contains(m_prewarmQueue.head())) {
            m_prewarmQueue.dequeue();
            ++m_prewarmDone;
            const QPointer<GalleryContentPresenter> guard(this);
            emit prewarmProgress(m_prewarmDone, m_prewarmTotal);
            if (!guard || m_prewarmPaused)
                return;
        }

        if (m_prewarmQueue.isEmpty()) {
            LOG_DEBUG(QStringLiteral("GalleryContentPresenter prewarm finished ready=%1 total=%2 elapsedMs=%3")
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
        LOG_WARN(QStringLiteral("GalleryContentPresenter ensurePageBuilt rejected routeId=%1 reason=missing-route")
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
        LOG_WARN(QStringLiteral("GalleryContentPresenter ensurePageBuilt rejected routeId=%1 reason=missing-page")
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
            LOG_WARN(QStringLiteral("GalleryContentPresenter cache eviction failed routeId=%1 index=%2")
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

        LOG_DEBUG(QStringLiteral("GalleryContentPresenter cache evicted routeId=%1 resident=%2 limit=%3")
                .arg(victimRouteId)
                .arg(m_routeStackIndex.size())
                .arg(m_maxResidentRoutes));
    }
}

void GalleryContentPresenter::connectPageNavigation(QWidget* page)
{
    if (auto* contentPage = dynamic_cast<GalleryContentPage*>(page)) {
        connect(contentPage, &GalleryContentPage::routeActivated, this, &GalleryContentPresenter::routeActivated);
    }
}

qint64 GalleryContentPresenter::switchToStackPage(int targetIndex)
{
    // Switch directly without cross-fade; grabbing heavy live-demo pages like Home takes ~500 ms.
    // zh_CN: 直接切换不做交叉淡化，避免离屏抓取首页等重型实时示例页面带来约 0.5 秒卡顿。
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
