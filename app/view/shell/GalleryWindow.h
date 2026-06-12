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

class GalleryNavigationPane;
class GalleryContentPage;
class GalleryNavigationItem;
class PlaceholderPage;
class SettingsPage;

class GalleryWindow : public fluent::windowing::Window {
public:
    explicit GalleryWindow(QWidget* parent = nullptr);

    QString currentRouteId() const { return m_currentRouteId; }
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

    void buildNavigationShell();
    void createTitleBarContent();
    void showInitialRouteContent();
    bool showRouteContent(const QString& routeId);
    QWidget* createRouteContentPage(const QString& routeId,
                                    const GalleryNavigationItem& fallbackItem) const;
    void connectRouteContentNavigation(QWidget* page);
    void replaceRouteContentPage(const QString& routeId, QWidget* page);
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
    fluent::basicinput::Button* m_backButton = nullptr;
    fluent::basicinput::Button* m_menuButton = nullptr;
    fluent::textfields::AutoSuggestBox* m_searchBox = nullptr;
    QTimer* m_navigationCompactReleaseTimer = nullptr;
    QStringList m_backRouteStack;
    QString m_currentRouteId;
    bool m_isNavigatingHistory = false;
};

} // namespace fluent::gallery

#endif // GALLERYWINDOW_H
