#ifndef GALLERYNAVIGATIONPANE_H
#define GALLERYNAVIGATIONPANE_H

#include <QHash>
#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "model/GalleryNavigationItem.h"

class QStandardItem;
class QStandardItemModel;
class QPropertyAnimation;
class QEvent;
class QPaintEvent;

namespace fluent::collections {
class TreeView;
}

namespace fluent::dialogs_flyouts {
class Popup;
}

namespace fluent::status_info {
class ToolTip;
}

namespace fluent::gallery {

class GalleryNavigationPane : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString selectedRouteId READ selectedRouteId WRITE setSelectedRouteId NOTIFY selectedRouteIdChanged)
    Q_PROPERTY(bool compact READ isCompact WRITE setCompact NOTIFY compactChanged)
    Q_PROPERTY(qreal compactVisualProgress READ compactVisualProgress WRITE setCompactVisualProgress)
    Q_PROPERTY(qreal settingsIconRotation READ settingsIconRotation WRITE setSettingsIconRotation)

public:
    explicit GalleryNavigationPane(const QVector<GalleryNavigationItem>& items, QWidget* parent = nullptr);

    QString selectedRouteId() const { return m_selectedRouteId; }
    QStringList routeIds() const;
    QStringList visibleTitles() const;
    bool containsRoute(const QString& routeId) const;
    QModelIndex indexForRouteId(const QString& routeId) const;
    bool isCompact() const { return m_compact; }
    bool isSurfaceVisible() const { return m_surfaceVisible; }
    qreal compactVisualProgress() const { return m_compactVisualProgress; }
    qreal settingsIconRotation() const { return m_settingsIconRotation; }
    void setSelectedRouteId(const QString& routeId);
    void setCompact(bool compact);
    void setSurfaceVisible(bool visible);
    void setCompactVisualProgress(qreal progress);
    void setSettingsIconRotation(qreal rotation);
    void onThemeUpdated() override;

signals:
    void routeActivated(const QString& routeId);
    void selectedRouteIdChanged(const QString& routeId);
    void compactChanged(bool compact);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void rebuild();
    void updateButtonStyles();
    void updateCompactRowVisibility();
    void syncCompactVisualProperties();
    void updateDividerPalette();
    void startCompactVisualTransition(bool compact);
    void startSettingsIconRotation();
    void activateRouteIndex(const QModelIndex& index, bool pointerActivation);
    QModelIndex visualSelectionIndex(const QModelIndex& routeIndex) const;
    void showCompactFlyoutForIndex(const QModelIndex& index);
    void closeCompactFlyout(bool animated = true);
    void showCompactToolTip(const QModelIndex& index);
    void hideCompactToolTip();
    bool isFooterOnly() const;
    QStandardItem* createItem(const GalleryNavigationItem& item);
    void appendNavigationItem(QStandardItemModel* model,
                              QHash<QString, QStandardItem*>& categoryItems,
                              const GalleryNavigationItem& item);

    QVector<GalleryNavigationItem> m_items;
    QStandardItemModel* m_model = nullptr;
    fluent::collections::TreeView* m_treeView = nullptr;
    QWidget* m_footerDivider = nullptr;
    fluent::dialogs_flyouts::Popup* m_compactFlyout = nullptr;
    fluent::status_info::ToolTip* m_compactToolTip = nullptr;
    QPersistentModelIndex m_compactToolTipIndex;
    QWidget* m_compactFlyoutPanel = nullptr;
    QWidget* m_compactFlyoutAnchor = nullptr;
    QPropertyAnimation* m_compactVisualAnimation = nullptr;
    QPropertyAnimation* m_settingsIconRotationAnimation = nullptr;
    QHash<QString, QPersistentModelIndex> m_routeIndexes;
    QString m_selectedRouteId;
    qreal m_compactVisualProgress = 0.0;
    qreal m_settingsIconRotation = 0.0;
    bool m_compact = false;
    bool m_surfaceVisible = false;
};

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONPANE_H
