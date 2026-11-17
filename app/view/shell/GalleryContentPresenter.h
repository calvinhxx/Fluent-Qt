#ifndef GALLERYCONTENTPRESENTER_H
#define GALLERYCONTENTPRESENTER_H

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQueue>
#include <QString>
#include <QStringList>

class QEvent;
class QWidget;

namespace fluent::navigation {
class StackContentHost;
}

namespace fluent::gallery {

class GalleryNavigationViewModel;

/**
 * @brief Owns the right-side content area, resolving routes to pages inside the content host.
 * zh_CN: 管理右侧内容区，负责将路由 ID 构建为页面并切入导航视图的内容宿主。
 *
 * GalleryWindow forwards route changes here without creating pages directly; in-page
 * card activations emit routeActivated back to the shell.
 * zh_CN: GalleryWindow 仅将路由变更转发至此，不直接建页；页内卡片点击通过 routeActivated 回传给外壳。
 */
class GalleryContentPresenter : public QObject {
    Q_OBJECT

public:
    GalleryContentPresenter(fluent::navigation::StackContentHost* contentHost,
        const GalleryNavigationViewModel& navigationViewModel,
        QObject* parent = nullptr,
        int maxResidentRoutes = 0);
    ~GalleryContentPresenter() override;

    QString currentRouteId() const { return m_currentRouteId; }
    QWidget* currentPage() const;

    /**
     * @brief Shows the page for routeId, replacing the current one.
     * Returns immediately with true if routeId is already visible. Startup routes build
     * synchronously behind the splash; later cold routes display a shimmer skeleton for
     * one frame before building incrementally.
     * zh_CN: 切换到 routeId 对应的页面。若该路由已在显示则直接返回 true；启动路由在遮罩后同步构建，
     * 常规冷路由先显示骨架屏一帧再分步构建真页。
     */
    bool presentRoute(const QString& routeId);

    /**
     * @brief Prepares every queued page and its first layout before completing startup.
     * zh_CN: 在启动完成前预热排队页面并完成首次布局。
     *
     * Component pages yield between individual sample factories, and progress tracks
     * completed pages while keeping them hidden until ready.
     * zh_CN: 组件页在各示例工厂间让出事件循环；进度按已就绪页统计，构建完成前保持隐藏。
     */
    void prewarmRoutes(const QStringList& routeIds);

    /**
     * @brief Pauses or resumes splash-phase prewarm so builds never run during window move/resize.
     * Does nothing once the prewarm queue is empty.
     * zh_CN: 暂停或恢复启动期预热，避免窗口移动/缩放时同步建页造成卡顿；队列排空后直接返回。
     */
    void setPrewarmPaused(bool paused);
    void cancelPrewarm();
    void setStartupCovered(bool covered);

signals:
    void routeActivated(const QString& routeId);

    /**
     * @brief Reports request-to-first-paint navigation timing for diagnostics and regression tests.
     * zh_CN: 上报从导航请求到目标页首帧绘制的耗时，供诊断与回归测试使用。
     */
    void navigationPresented(const QString& routeId, bool cold, qint64 buildMs, qint64 switchMs, qint64 totalMs);

    /** @brief Splash-phase prewarm warmed `done` of `total` queued pages; drives the splash caption. */
    void prewarmProgress(int done, int total);
    /** @brief The queue is drained; failures are reported separately, never counted as ready. */
    void prewarmFinished();
    void prewarmFailed(const QString& routeId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void beginNavigationWatch(const QString& routeId, bool cold, quint64 requestId);
    void watchNavigationPage(QWidget* page, qint64 buildMs);
    void cancelNavigationWatch();
    void connectPageNavigation(QWidget* page);
    void touchResidentRoute(const QString& routeId);
    void trimResidentRoutes(const QString& protectedRouteId);
    int ensurePageBuilt(const QString& routeId, qint64* buildMs = nullptr);
    int registerPage(const QString& routeId, QWidget* page);
    void ensureSkeleton();
    void scheduleLazyBuild(const QString& routeId, quint64 requestId);
    void buildNextLazyPart(const QString& routeId, quint64 requestId);
    void cancelLazyBuild();
    void scheduleNextPrewarm();
    qint64 switchToStackPage(int targetIndex);

    fluent::navigation::StackContentHost* m_contentHost = nullptr;
    const GalleryNavigationViewModel& m_navigationViewModel;
    QString m_currentRouteId;

    // Keep built pages parented inside the content host stack to avoid reparenting,
    // which would force a full-subtree style re-polish and relayout on revisits.
    // When m_maxResidentRoutes > 0, evict LRU pages and adjust stored stack indices.
    // zh_CN: 已建页面常驻在内容宿主栈中（仅切换显隐），避免重设父控件导致整棵子树重新 polish 与布局；
    // 若平台设置 m_maxResidentRoutes > 0，则按 LRU 淘汰页面并同步修正栈索引。
    QHash<QString, int> m_routeStackIndex;
    QStringList m_routeRecency;
    int m_maxResidentRoutes = 0;

    // Reusable shimmer skeleton shown while a cold route builds incrementally.
    QWidget* m_skeleton = nullptr;
    int m_skeletonIndex = -1;
    QPointer<QWidget> m_lazyPage;
    qint64 m_lazyBuildMs = 0;

    // The active page is owned by the host but not navigable until all samples/layout are ready.
    // zh_CN: 当前构建页由 host 持有，全部示例和布局就绪前不加入可导航缓存。
    QQueue<QString> m_prewarmQueue;
    QElapsedTimer m_prewarmTimer;
    QPointer<QWidget> m_prewarmPage;
    QString m_prewarmRouteId;
    bool m_prewarmScheduled = false;
    bool m_startupCovered = false;
    bool m_prewarmPaused = false; // Set while the user moves/resizes the window; blocks page builds.

    int m_prewarmTotal = 0; // Pages queued for splash-phase warm; denominator of the caption.
    int m_prewarmDone = 0;  // Pages warmed so far; numerator of the caption.

    quint64 m_navigationRequestId = 0;
    quint64 m_pendingRequestId = 0;
    QString m_pendingRouteId;
    QPointer<QWidget> m_pendingPage;
    QElapsedTimer m_pendingNavigationTimer;
    qint64 m_pendingBuildMs = 0;
    qint64 m_pendingSwitchMs = 0;
    bool m_pendingCold = false;
};

} // namespace fluent::gallery

#endif // GALLERYCONTENTPRESENTER_H
