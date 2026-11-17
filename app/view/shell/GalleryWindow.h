#ifndef GALLERYWINDOW_H
#define GALLERYWINDOW_H

#include <QElapsedTimer>
#include <QPointer>
#include <QStringList>

#include "components/windowing/Window.h"
#include "viewmodel/GalleryNavigationState.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "viewmodel/GallerySettings.h"

class QAction;
class QMoveEvent;
class QHideEvent;
class QResizeEvent;
class QShowEvent;
class QTimer;
class QWidget;

namespace fluent::navigation {
class NavigationView;
}

namespace fluent::textfields {
class EditingCommandRouter;
}

namespace fluent::gallery {

class GalleryCommands;
class GalleryContentPresenter;
class GalleryNavigationPane;
class GalleryContentPage;
class GalleryIntroTour;
class GalleryNavigationItem;
class GallerySearchService;
class GallerySplashScreen;
class GalleryTitleBarController;
class GalleryTopNavigationPane;
class SettingsPage;

/**
 * @brief Application shell hosting the title bar, navigation pane, and content area.
 * zh_CN: 应用主窗口外壳：包含标题栏、导航栏和右侧内容区。
 *
 * Every route change (pane click, search, history back, or in-page card) funnels
 * through selectRoute() -> GalleryNavigationState -> GalleryContentPresenter::presentRoute().
 * zh_CN: 所有路由跳转（导航点击、搜索、后退、页内卡片）统一经由 selectRoute()
 * 流入 GalleryNavigationState 和 GalleryContentPresenter::presentRoute()。
 */
class GalleryWindow : public fluent::windowing::Window {
public:
    explicit GalleryWindow(QWidget* parent = nullptr);

    QString currentRouteId() const;
    QStringList navigationEntryIds() const;
    QStringList visibleNavigationTitles() const;
    bool selectRoute(const QString& routeId);
    /**
     * @brief Navigates using known routes or the completed example index, without loading samples.
     * zh_CN: 使用已知路由或完整示例索引导航，不加载示例。
     *
     * Routes precede examples. Returns false when no indexed destination matches;
     * the title-bar controller uses asynchronous requests for cold example queries.
     * zh_CN: 路由优先于示例；已就绪索引中无匹配时返回 false。标题栏控制器对冷启动示例查询使用异步请求。
     */
    bool navigateToSearchResult(const QString& searchText);
    GalleryContentPage* currentContentPage() const;
    SettingsPage* currentSettingsPage() const;
    /// Opens the searchable list of window commands and their shortcuts.
    /// zh_CN: 打开可搜索的窗口命令及其快捷键列表。
    void showCommandPalette();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    // Pause splash-phase page prewarm during move/resize so synchronous builds do not stutter the drag.
    // zh_CN: 移动或缩放窗口期间暂停启动预热，避免同步建页导致拖拽卡顿。
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    enum class AppWindowWidthState { Expanded, Compact, Minimal };

    void createTitleBarContent();
    void installCommands();
    void focusSearch();
    void buildNavigationShell();
    void buildContentPresenter();
    void installSplashScreen();
    void showInitialRouteContent();
    void prewarmRemainingRoutes();
    void scheduleStartupFinish();
    void finishStartup();
    void maybeStartIntroTour();
    void handleSelectedRouteChanged(const QString& routeId);
    void recordNavigationHistory(const QString& nextRouteId);
    bool navigateBack();
    AppWindowWidthState appWindowWidthState() const;
    void applyNavigationStyle(GallerySettings::NavigationStyle style);
    void setTopNavigationChrome(bool top);
    void toggleNavigationDisplayMode();
    void setNavigationPanesCompact(bool compact);
    // Collapse inline labels immediately on close; reveal them on open only after widening finishes to avoid clipping.
    // zh_CN: 收起时立即切换为图标模式，展开时待宽度动画完成后再显示文字标签，避免滑动过程中被裁剪。
    void applyNavigationPaneDensity();
    void updateNavigationCommands();
    // Pause prewarm immediately and schedule resume once window move/resize goes idle.
    void deferPrewarmDuringInteraction();

    GalleryNavigationViewModel m_navigationViewModel;
    GalleryNavigationState m_navigationState;
    fluent::navigation::NavigationView* m_navigationView = nullptr;
    GalleryNavigationPane* m_mainNavigationPane = nullptr;
    GalleryNavigationPane* m_footerNavigationPane = nullptr;
    GalleryTopNavigationPane* m_topMainNavigationPane = nullptr;
    GalleryTopNavigationPane* m_topFooterNavigationPane = nullptr;
    fluent::textfields::EditingCommandRouter* m_editingCommandRouter = nullptr;
    GalleryContentPresenter* m_contentPresenter = nullptr;
    GallerySearchService* m_searchService = nullptr;
    GalleryTitleBarController* m_titleBar = nullptr;
    GalleryCommands* m_commands = nullptr;
    QAction* m_backCommand = nullptr;
    QAction* m_toggleNavigationCommand = nullptr;
    GalleryIntroTour* m_introTour = nullptr;
    QTimer* m_navigationCompactReleaseTimer = nullptr;
    QTimer* m_prewarmResumeTimer = nullptr; // Idle debounce that resumes prewarm after interaction.
    QStringList m_backRouteStack;
    bool m_isNavigatingHistory = false;

    // Startup splash overlay; self-deletes after its fade-out once the initial route is shown.
    // zh_CN: 启动 splash 覆盖层；首个路由上屏后淡出并自销毁。
    QPointer<GallerySplashScreen> m_splashScreen;
    // Event identity must remain valid during the splash's QWidget destruction.
    QPointer<QObject> m_splashEventSource;
    QElapsedTimer m_startupVisibleTimer;
    QElapsedTimer m_startupReadyTimer;
    QTimer* m_startupFinishTimer = nullptr;
    bool m_startupFinished = false;
    bool m_startupPrewarmStarted = false;
    QStringList m_startupFailedRoutes;
};

} // namespace fluent::gallery

#endif // GALLERYWINDOW_H
