#ifndef GALLERYCONTENTPRESENTER_H
#define GALLERYCONTENTPRESENTER_H

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QPixmap;
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

signals:
    void routeActivated(const QString& routeId);

private:
    void connectPageNavigation(QWidget* page);
    void replaceCurrentPage(const QString& routeId, QWidget* page);
    QWidget* cachedPage(const QString& routeId) const;
    void stashPage(QWidget* page);
    void startContentCrossfade(const QPixmap& snapshot);

    fluent::navigation::StackContentHost* m_contentHost = nullptr;
    const GalleryNavigationViewModel& m_navigationViewModel;
    QString m_currentRouteId;

    // Pages are built once and reused: navigating to a visited route swaps the existing widget
    // back in (state intact) instead of reconstructing it, which is what made nav feel janky.
    // Off-screen pages live under m_pageStash so they stay alive and themed but hidden.
    // zh_CN: 页面只建一次并复用：切到访问过的路由会把已有 widget 换回来（状态保留），而不是重建——
    // 这正是导航卡顿的根因。离屏页面挂在 m_pageStash 下，保持存活与主题更新但隐藏。
    QHash<QString, QPointer<QWidget>> m_pageCache;
    QWidget* m_pageStash = nullptr;

    // A fading snapshot of the outgoing page, layered over the freshly-swapped-in page so the
    // content cross-dissolves. Held so rapid navigation can retire a stale overlay.
    // zh_CN: 旧页面的淡出快照，叠在刚换入的新页之上做内容交叉淡化。保留引用，便于快速连续导航时
    // 撤掉上一张尚未淡完的快照。
    QPointer<QWidget> m_transitionOverlay;
};

} // namespace fluent::gallery

#endif // GALLERYCONTENTPRESENTER_H
