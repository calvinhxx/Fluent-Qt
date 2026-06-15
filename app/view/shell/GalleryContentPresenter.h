#ifndef GALLERYCONTENTPRESENTER_H
#define GALLERYCONTENTPRESENTER_H

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
     * Re-presenting the route already on screen is a no-op that returns true.
     * zh_CN: 重复呈现已在屏幕上的路由为无操作，返回 true。
     */
    bool presentRoute(const QString& routeId);

    /**
     * @brief Builds every listed route's page up front (one per event-loop tick) and
     * parks it hidden in the content stack, so later navigation is a pure show/hide
     * with no build cost. Meant to run behind a splash screen at startup.
     * zh_CN: 提前建好列表里每个路由的页面（每个事件循环一帧建一个）并隐藏停放在内容栈里，
     * 之后导航就是纯显示/隐藏、无建页开销。设计为启动时在 splash screen 背后运行。
     *
     * Already-built routes are skipped; safe to call repeatedly. Emits prewarmProgress
     * per page and prewarmFinished when the queue drains.
     * zh_CN: 已建过的路由会跳过；可重复调用。每建一个发 prewarmProgress，排空时发 prewarmFinished。
     */
    void prewarmRoutes(const QStringList& routeIds);

signals:
    void routeActivated(const QString& routeId);

    /** @brief Prewarm built `done` of `total` queued pages. */
    void prewarmProgress(int done, int total);
    /** @brief The prewarm queue has drained — all requested pages are built. */
    void prewarmFinished();

private:
    void connectPageNavigation(QWidget* page);
    int ensurePageBuilt(const QString& routeId);
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

    // Startup prewarm queue: routes waiting to be built, drained one per event-loop
    // tick so the splash screen stays responsive while the heavy construction runs.
    // zh_CN: 启动预建队列：等待建页的路由，每个事件循环一帧建一个，让 splash 在重构造期间保持响应。
    QQueue<QString> m_prewarmQueue;
    bool m_prewarmScheduled = false;
    int m_prewarmDone = 0;
    int m_prewarmTotal = 0;
};

} // namespace fluent::gallery

#endif // GALLERYCONTENTPRESENTER_H
