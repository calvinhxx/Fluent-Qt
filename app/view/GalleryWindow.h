#ifndef GALLERYWINDOW_H
#define GALLERYWINDOW_H

#include <QStringList>

#include "components/windowing/Window.h"
#include "viewmodel/GalleryNavigationState.h"
#include "viewmodel/GalleryNavigationViewModel.h"

class QEvent;
class QTimer;

namespace fluent::basicinput {
class Button;
}

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
    bool eventFilter(QObject* watched, QEvent* event) override;

    void buildNavigationShell();
    bool applyRoute(const QString& routeId);
    void createTitleBarContent();
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
    QTimer* m_navigationCompactReleaseTimer = nullptr;
    QStringList m_backRouteStack;
    QString m_currentRouteId;
    bool m_isNavigatingHistory = false;
};

} // namespace fluent::gallery

#endif // GALLERYWINDOW_H
