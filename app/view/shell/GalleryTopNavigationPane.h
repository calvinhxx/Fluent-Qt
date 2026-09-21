#ifndef GALLERYTOPNAVIGATIONPANE_H
#define GALLERYTOPNAVIGATIONPANE_H

#include <QHash>
#include <QString>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "model/GalleryNavigationItem.h"

namespace fluent::basicinput {
class Button;
}

namespace fluent::dialogs_flyouts {
class Popup;
}

namespace fluent::gallery {

class GalleryTopNavigationPane : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString selectedRouteId READ selectedRouteId WRITE setSelectedRouteId NOTIFY
                   selectedRouteIdChanged)

public:
    explicit GalleryTopNavigationPane(const QVector<GalleryNavigationItem>& items,
                                      QWidget* parent = nullptr);

    QString selectedRouteId() const { return m_selectedRouteId; }
    void setSelectedRouteId(const QString& routeId);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void routeActivated(const QString& routeId);
    void selectedRouteIdChanged(const QString& routeId);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateButtonStyles();
    void updateButtonLayout();
    QString visualRouteId() const;
    void showChildFlyout(const QString& routeId, fluent::basicinput::Button* anchor);
    void closeChildFlyout(bool animated = true);
    void startSettingsIconRotation(fluent::basicinput::Button* button);

    QHash<QString, fluent::basicinput::Button*> m_buttons;
    QVector<GalleryNavigationItem> m_items;
    fluent::basicinput::Button* m_moreButton = nullptr;
    QWidget* m_groupDivider = nullptr;
    QHash<QString, QVector<GalleryNavigationItem>> m_childItems;
    QHash<QString, QString> m_parentRoutes;
    QString m_selectedRouteId;
    fluent::dialogs_flyouts::Popup* m_childFlyout = nullptr;
    QWidget* m_childFlyoutPanel = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYTOPNAVIGATIONPANE_H
