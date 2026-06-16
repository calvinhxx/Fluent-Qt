#ifndef GALLERYWINDOW_H
#define GALLERYWINDOW_H

#include <QPointer>
#include <QStringList>

#include "components/windowing/Window.h"
#include "viewmodel/GalleryNavigationState.h"
#include "viewmodel/GalleryNavigationViewModel.h"

class QEvent;
class QLabel;
class QTimer;
class QWidget;

namespace fluent::basicinput {
class Button;
}

namespace fluent::textfields {
class AutoSuggestBox;
class Label;
}

namespace fluent::status_info {
class ToolTip;
}

namespace fluent::navigation {
class NavigationView;
}

namespace fluent::gallery {

class GalleryContentPresenter;
class GalleryNavigationPane;
class GalleryContentPage;
class GalleryNavigationItem;
class GallerySplashScreen;
class PlaceholderPage;
class SettingsPage;

/**
 * @brief Application shell: title bar, left navigation pane, right content area.
 * zh_CN: 应用外壳：标题栏、左侧导航栏、右侧 content 区。
 *
 * Construction is three symmetric build steps (title bar, navigation shell,
 * content presenter) plus the initial route. After that every route change —
 * pane click, search, back, in-page card — funnels through selectRoute:
 * zh_CN: 构造分三个对称的构建步骤（标题栏、导航壳、content 呈现器）加初始路由。
 * 之后所有路由变化——导航点击、搜索、后退、页面内卡片——都汇入 selectRoute：
 *
 *   selectRoute -> GalleryNavigationState (selection)
 *               -> handleSelectedRouteChanged -> history
 *               -> GalleryContentPresenter::presentRoute (page swap)
 */
class GalleryWindow : public fluent::windowing::Window {
public:
    explicit GalleryWindow(QWidget* parent = nullptr);

    QString currentRouteId() const;
    QStringList navigationEntryIds() const;
    QStringList visibleNavigationTitles() const;
    bool selectRoute(const QString& routeId);
    /**
     * @brief Navigates to the route whose title best matches the search text.
     * zh_CN: 跳转到标题与搜索文本最匹配的路由。
     *
     * Exact (case-insensitive) title matches win; otherwise the first title
     * containing the text is used. Returns false when nothing matches.
     * zh_CN: 优先大小写不敏感的精确匹配，其次取第一个包含该文本的标题；无匹配返回 false。
     */
    bool navigateToSearchResult(const QString& searchText);
    GalleryContentPage* currentContentPage() const;
    PlaceholderPage* currentPlaceholderPage() const;
    SettingsPage* currentSettingsPage() const;

private:
    bool eventFilter(QObject* watched, QEvent* event) override;

    void createTitleBarContent();
    void buildNavigationShell();
    void buildContentPresenter();
    void installSplashScreen();
    void setTitleBarChromeVisible(bool visible, bool animated = false);
    // Re-flows the title-bar content for the current width / nav display mode: hides the
    // app title+icon in the minimal layout and sizes/centers the search box into the free
    // span so it never overlaps the buttons or the native caption controls.
    // zh_CN: 按当前宽度/导航模式重排标题栏内容：最小布局下隐藏应用标题+图标，并把搜索框尺寸/居中到
    // 空闲区间，使其不与按钮或原生标题栏控件重叠。
    void updateTitleBarLayout();
    void showTitleBarToolTip(fluent::basicinput::Button* button);
    void hideTitleBarToolTip();
    void showInitialRouteContent();
    void prewarmAllRoutes();
    void handleSelectedRouteChanged(const QString& routeId);
    void recordNavigationHistory(const QString& nextRouteId);
    bool navigateBack();
    void toggleNavigationDisplayMode();
    void setNavigationPanesCompact(bool compact);
    void updateNavigationCommands();

    GalleryNavigationViewModel m_navigationViewModel;
    GalleryNavigationState m_navigationState;
    fluent::navigation::NavigationView* m_navigationView = nullptr;
    GalleryNavigationPane* m_mainNavigationPane = nullptr;
    GalleryNavigationPane* m_footerNavigationPane = nullptr;
    GalleryContentPresenter* m_contentPresenter = nullptr;
    fluent::basicinput::Button* m_backButton = nullptr;
    fluent::basicinput::Button* m_menuButton = nullptr;
    QLabel* m_titleBarAppIcon = nullptr;
    fluent::textfields::Label* m_titleBarTitle = nullptr;
    fluent::textfields::AutoSuggestBox* m_searchBox = nullptr;
    fluent::status_info::ToolTip* m_titleBarToolTip = nullptr;
    bool m_titleBarChromeVisible = true;
    QTimer* m_navigationCompactReleaseTimer = nullptr;
    QStringList m_backRouteStack;
    bool m_isNavigatingHistory = false;

    // Startup splash overlay; self-deletes after its fade-out on prewarmFinished.
    // zh_CN: 启动 splash 覆盖层；在 prewarmFinished 后淡出并自销毁。
    QPointer<GallerySplashScreen> m_splashScreen;
};

} // namespace fluent::gallery

#endif // GALLERYWINDOW_H
