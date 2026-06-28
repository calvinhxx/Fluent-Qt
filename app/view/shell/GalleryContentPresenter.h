#ifndef GALLERYCONTENTPRESENTER_H
#define GALLERYCONTENTPRESENTER_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>

class QWidget;

namespace fluent::navigation {
class StackContentHost;
}

namespace fluent::gallery {

class GalleryNavigationViewModel;

/**
 * @brief Owns the right-side content area: turns a route id into a page and
 * swaps it into the navigation view's content host.
 * zh_CN: 负责右侧 content 区：把路由 id 变成页面，并换入导航视图的 content host。
 *
 * GalleryWindow forwards route changes here and never touches page creation or
 * replacement itself. In-page navigation (entry cards) comes back out through
 * routeActivated so the window can route it like any other selection.
 * zh_CN: GalleryWindow 只把路由变化转发到这里，自身不再创建或替换页面。页面内导航
 * （入口卡片）经 routeActivated 信号回流，窗口按普通路由选择处理。
 */
class GalleryContentPresenter : public QObject {
    Q_OBJECT

public:
    GalleryContentPresenter(fluent::navigation::StackContentHost* contentHost,
                            const GalleryNavigationViewModel& navigationViewModel,
                            QObject* parent = nullptr);

    QString currentRouteId() const { return m_currentRouteId; }
    QWidget* currentPage() const;

    /**
     * @brief Shows the page for routeId, replacing the current one.
     * zh_CN: 显示 routeId 对应的页面并替换当前页面。
     *
     * Re-presenting the route already on screen is a no-op that returns true. The first
     * route (startup, behind the splash) builds synchronously so the splash hands directly
     * to real content. Already-built routes switch instantly. A cold route during normal
     * use shows a shimmer skeleton immediately, then builds the real page on the next
     * event-loop tick and swaps it in — so the click never freezes on the build.
     * zh_CN: 重复呈现已在屏幕上的路由为无操作，返回 true。首个路由（启动、splash 背后）同步构建，
     * 使 splash 直接交给真内容。已建路由瞬时切换。正常使用中点开冷路由会立刻显示 shimmer 骨架屏，
     * 下一个事件循环帧再建真页并换入——于是点击不会卡在建页上。
     */
    bool presentRoute(const QString& routeId);

    /**
     * @brief Warms the listed routes' pages up front (one per event-loop tick) behind the
     * splash, capped by a time budget, then emits prewarmFinished.
     * zh_CN: 在 splash 背后提前预热列出的路由页面（每帧一个），受时间预算限制，随后发出 prewarmFinished。
     *
     * Building pages freezes the GUI thread, so we only ever do it while the splash hides the
     * jank. The budget bounds how long startup waits: whatever warmed in time becomes an
     * instant show/hide later; the un-warmed tail builds lazily behind a shimmer skeleton on
     * first visit. We deliberately do NOT warm in the background after the splash — that would
     * stutter the live UI. zh_CN: 建页会冻结 GUI 线程，故只在 splash 遮挡卡顿时做。预算限定启动等待时长：
     * 及时预热到的之后瞬时显隐；没预热到的尾部在首次访问时于 shimmer 骨架屏背后懒构建。刻意不在 splash 之后做
     * 后台预热——那会让活动中的 UI 卡顿。
     */
    void prewarmRoutes(const QStringList& routeIds);

    /**
     * @brief Pauses/resumes splash-phase page warming so it never builds during a window interaction.
     * zh_CN: 暂停/恢复 splash 期建页，使其绝不在窗口交互期间构建。
     *
     * Building a page synchronously freezes the GUI thread for tens-to-hundreds of ms; if that lands
     * mid-drag the window stutters. GalleryWindow pauses warming while the user is moving/resizing the
     * window and resumes shortly after they stop, so dragging during the first-load splash stays smooth
     * without abandoning the warm-ahead benefit. No-op once the queue has drained.
     * zh_CN: 同步建页会冻结 GUI 线程几十~几百毫秒，若落在拖动中途就会卡顿。GalleryWindow 在用户移动/缩放窗口
     * 期间暂停建页、停止片刻后恢复，使首屏 splash 期间拖拽顺滑，又不放弃预热收益。队列排空后为空操作。
     */
    void setPrewarmPaused(bool paused);

signals:
    void routeActivated(const QString& routeId);

    /** @brief Splash-phase prewarm warmed `done` of `total` queued pages; drives the splash caption. */
    void prewarmProgress(int done, int total);
    /** @brief Splash-phase prewarm has finished (budget hit or queue drained); dismiss the splash. */
    void prewarmFinished();

private:
    void connectPageNavigation(QWidget* page);
    int ensurePageBuilt(const QString& routeId);
    void ensureSkeleton();
    void scheduleLazyBuild(const QString& routeId);
    void scheduleNextPrewarm();
    void switchToStackPage(int targetIndex);

    fluent::navigation::StackContentHost* m_contentHost = nullptr;
    const GalleryNavigationViewModel& m_navigationViewModel;
    QString m_currentRouteId;

    // Each route's page is built once and then kept resident in the content host's
    // stack (parented to the host, just shown/hidden). Navigating is a show/hide of an
    // already-parented, already-laid-out widget — no reparenting, so Qt skips the full
    // style re-polish + relayout that made even revisits cost hundreds of ms. The map
    // remembers each route's stack index; indices are stable because pages are only
    // ever appended, never removed.
    // zh_CN: 每个路由的页面只建一次，之后常驻 content host 的栈里（挂在 host 上，仅显示/隐藏）。
    // 导航变成对"已挂父级、已完成布局"的页面做显示/隐藏——不再 reparent，于是 Qt 跳过整棵子树的
    // 样式重新 polish + 重新布局（那正是连重访都要几百毫秒的根因）。此表记住每个路由的栈索引；
    // 页面只追加、从不移除，所以索引稳定。
    QHash<QString, int> m_routeStackIndex;

    // A single shimmer skeleton page, parked in the stack and shown the instant a cold
    // route is requested so navigation feels immediate while the real page builds.
    // zh_CN: 单个 shimmer 骨架页，常驻栈里，请求冷路由时立刻显示，使导航在真页构建期间也即时响应。
    QWidget* m_skeleton = nullptr;
    int m_skeletonIndex = -1;

    // Splash-phase prewarm: routes waiting to warm, drained one per event-loop tick behind the
    // splash until the budget timer elapses. m_prewarmScheduled guards against double-queuing
    // the next tick. zh_CN: splash 期预热：待预热的路由，在 splash 背后每帧建一个，直到预算计时器到点。
    // m_prewarmScheduled 防止重复排入下一帧。
    QQueue<QString> m_prewarmQueue;
    QElapsedTimer m_prewarmBudget;
    bool m_prewarmScheduled = false;
    bool m_prewarmPaused = false;  // Set while the user moves/resizes the window; blocks page builds.

    int m_prewarmTotal = 0;   // Pages queued for splash-phase warm; denominator of the caption.
    int m_prewarmDone = 0;    // Pages warmed so far; numerator of the caption.
};

} // namespace fluent::gallery

#endif // GALLERYCONTENTPRESENTER_H
