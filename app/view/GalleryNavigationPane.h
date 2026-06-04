#ifndef GALLERYNAVIGATIONPANE_H
#define GALLERYNAVIGATIONPANE_H

#include <QHash>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "model/GalleryNavigationItem.h"

namespace fluent::basicinput {
class Button;
}

namespace fluent::gallery {

class GalleryNavigationPane : public QWidget, public FluentElement {
    Q_OBJECT

public:
    explicit GalleryNavigationPane(const QVector<GalleryNavigationItem>& items, QWidget* parent = nullptr);

    QString selectedRouteId() const { return m_selectedRouteId; }
    QStringList routeIds() const;
    QStringList visibleTitles() const;
    bool containsRoute(const QString& routeId) const;
    void setSelectedRouteId(const QString& routeId);
    void onThemeUpdated() override;

signals:
    void routeActivated(const QString& routeId);

private:
    void rebuild();
    void updateButtonStyles();
    QWidget* createSectionHeader(const GalleryNavigationItem& item);
    fluent::basicinput::Button* createRouteButton(const GalleryNavigationItem& item);

    QVector<GalleryNavigationItem> m_items;
    QHash<QString, fluent::basicinput::Button*> m_buttons;
    QString m_selectedRouteId;
};

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONPANE_H
