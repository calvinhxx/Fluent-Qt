#ifndef GALLERYWINDOW_H
#define GALLERYWINDOW_H

#include <QStringList>

#include "components/windowing/Window.h"
#include "viewmodel/GalleryNavigationState.h"
#include "viewmodel/GalleryNavigationViewModel.h"

class QEvent;
class QTimer;
class QWidget;

namespace fluent::basicinput {
class Button;
}

namespace fluent::textfields {
class AutoSuggestBox;
}

namespace fluent::navigation {
class NavigationView;
}

namespace fluent::gallery {

class GalleryContentPresenter;
class GalleryNavigationPane;
class GalleryContentPage;
class GalleryNavigationItem;
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
    fluent::textfields::AutoSuggestBox* m_searchBox = nullptr;
    QTimer* m_navigationCompactReleaseTimer = nullptr;
    QStringList m_backRouteStack;
    bool m_isNavigatingHistory = false;
};

} // namespace fluent::gallery

#endif // GALLERYWINDOW_H
