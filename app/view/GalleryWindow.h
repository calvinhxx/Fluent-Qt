#ifndef GALLERYWINDOW_H
#define GALLERYWINDOW_H

#include <QStringList>

#include "components/windowing/Window.h"
#include "viewmodel/GalleryNavigationViewModel.h"

namespace fluent::navigation {
class NavigationView;
}

namespace fluent::gallery {

class GalleryNavigationPane;
class PlaceholderPage;
class SettingsPage;

class GalleryWindow : public fluent::windowing::Window {
public:
    explicit GalleryWindow(QWidget* parent = nullptr);

    QString currentRouteId() const { return m_currentRouteId; }
    QStringList navigationEntryIds() const;
    QStringList visibleNavigationTitles() const;
    bool selectRoute(const QString& routeId);
    PlaceholderPage* currentPlaceholderPage() const;
    SettingsPage* currentSettingsPage() const;

private:
    void buildTitleBarContent();
    void buildNavigationShell();
    void syncPaneSelection();
    void createTitleBarContent();

    GalleryNavigationViewModel m_navigationViewModel;
    fluent::navigation::NavigationView* m_navigationView = nullptr;
    GalleryNavigationPane* m_mainNavigationPane = nullptr;
    GalleryNavigationPane* m_footerNavigationPane = nullptr;
    QString m_currentRouteId;
};

} // namespace fluent::gallery

#endif // GALLERYWINDOW_H
