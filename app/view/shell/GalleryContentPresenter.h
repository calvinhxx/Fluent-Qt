#ifndef GALLERYCONTENTPRESENTER_H
#define GALLERYCONTENTPRESENTER_H

#include <QObject>
#include <QString>

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

    fluent::navigation::StackContentHost* m_contentHost = nullptr;
    const GalleryNavigationViewModel& m_navigationViewModel;
    QString m_currentRouteId;
};

} // namespace fluent::gallery

#endif // GALLERYCONTENTPRESENTER_H
