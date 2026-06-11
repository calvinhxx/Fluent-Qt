#include "GalleryNavigationPane.h"

#include <cmath>
#include <QAbstractItemView>
#include <QPropertyAnimation>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include "components/dialogs_flyouts/Popup.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/collections/TreeView.h"
#include "components/scrolling/ScrollBar.h"
#include "utils/Log.h"
#include "GalleryCompactFlyout.h"
#include "GalleryNavigationDelegate.h"
#include "GalleryNavigationMetrics.h"

namespace fluent::gallery {

GalleryNavigationPane::GalleryNavigationPane(const QVector<GalleryNavigationItem>& items, QWidget* parent)
    : QWidget(parent)
    , m_items(items)
{
    setObjectName(QStringLiteral("galleryNavigationPane"));
    setSizePolicy(QSizePolicy::Expanding, isFooterOnly() ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    setMinimumHeight(isFooterOnly() ? kRouteHeight + 9 : 0);
    m_compactVisualAnimation = new QPropertyAnimation(this, "compactVisualProgress", this);
    connect(m_compactVisualAnimation, &QPropertyAnimation::finished,
            this, [this]() {
                setCompactVisualProgress(m_compact ? 1.0 : 0.0);
                updateCompactRowVisibility();
            });
    m_settingsIconRotationAnimation = new QPropertyAnimation(this, "settingsIconRotation", this);
    m_settingsIconRotationAnimation->setObjectName(QStringLiteral("gallerySettingsIconRotationAnimation"));
    connect(m_settingsIconRotationAnimation, &QPropertyAnimation::valueChanged,
            this, [this](const QVariant& value) {
                setSettingsIconRotation(value.toReal());
            });
    connect(m_settingsIconRotationAnimation, &QPropertyAnimation::finished,
            this, [this]() {
                setSettingsIconRotation(0.0);
            });
    rebuild();
}

QStringList GalleryNavigationPane::routeIds() const
{
    QStringList ids;
    for (const GalleryNavigationItem& item : m_items) {
        if (item.kind != GalleryNavigationItem::Kind::SectionHeader)
            ids.append(item.id);
    }
    return ids;
}

QStringList GalleryNavigationPane::visibleTitles() const
{
    QStringList titles;
    for (const GalleryNavigationItem& item : m_items)
        titles.append(item.title);
    return titles;
}

bool GalleryNavigationPane::containsRoute(const QString& routeId) const
{
    return m_routeIndexes.contains(routeId);
}

QModelIndex GalleryNavigationPane::indexForRouteId(const QString& routeId) const
{
    const auto iterator = m_routeIndexes.constFind(routeId);
    return iterator == m_routeIndexes.constEnd() ? QModelIndex() : QModelIndex(iterator.value());
}

void GalleryNavigationPane::setSelectedRouteId(const QString& routeId)
{
    if (m_selectedRouteId == routeId)
        return;
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane selectedRouteChanged object=%1 old=%2 new=%3")
                  .arg(objectName(), m_selectedRouteId, routeId));
    m_selectedRouteId = routeId;
    updateButtonStyles();
    emit selectedRouteIdChanged(m_selectedRouteId);
}

void GalleryNavigationPane::setCompact(bool compact)
{
    if (m_compact == compact)
        return;

    m_compact = compact;
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane compactChanged object=%1 compact=%2")
                  .arg(objectName(), compact ? QStringLiteral("true") : QStringLiteral("false")));
    syncCompactVisualProperties();
    if (compact && m_treeView)
        m_treeView->collapseAll();
    if (!compact)
        closeCompactFlyout();
    updateCompactRowVisibility();
    updateButtonStyles();
    startCompactVisualTransition(compact);
    emit compactChanged(m_compact);
}

void GalleryNavigationPane::setCompactVisualProgress(qreal progress)
{
    const qreal normalized = qBound<qreal>(0.0, progress, 1.0);
    if (qAbs(m_compactVisualProgress - normalized) <= 0.0001)
        return;

    const bool wasFullyCompact = m_compact && m_compactVisualProgress >= 0.999;
    m_compactVisualProgress = normalized;
    syncCompactVisualProperties();
    const bool isFullyCompact = m_compact && m_compactVisualProgress >= 0.999;
    if (wasFullyCompact != isFullyCompact || !m_compact)
        updateCompactRowVisibility();
    if (m_treeView) {
        m_treeView->doItemsLayout();
        if (m_treeView->viewport())
            m_treeView->viewport()->update();
    }
}

void GalleryNavigationPane::setSettingsIconRotation(qreal rotation)
{
    qreal normalized = std::fmod(rotation, kSettingsIconRotationDegrees);
    if (normalized < 0)
        normalized += kSettingsIconRotationDegrees;
    if (qAbs(m_settingsIconRotation - normalized) <= 0.0001)
        return;

    m_settingsIconRotation = normalized;
    if (m_treeView) {
        m_treeView->setProperty("gallerySettingsIconRotation", m_settingsIconRotation);
        if (m_treeView->viewport()) {
            m_treeView->viewport()->setProperty("gallerySettingsIconRotation", m_settingsIconRotation);
            m_treeView->viewport()->update();
        }
    }
}

void GalleryNavigationPane::onThemeUpdated()
{
    if (m_treeView && m_treeView->viewport())
        m_treeView->viewport()->update();
}

void GalleryNavigationPane::rebuild()
{
    m_routeIndexes.clear();

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_treeView = new fluent::collections::TreeView(this);
    m_treeView->setObjectName(isFooterOnly()
                                  ? QStringLiteral("galleryFooterNavigationTreeView")
                                  : QStringLiteral("galleryMainNavigationTreeView"));
    syncCompactVisualProperties();
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane rebuild tree=%1 itemCount=%2 footerOnly=%3")
                  .arg(m_treeView->objectName())
                  .arg(m_items.size())
                  .arg(isFooterOnly() ? QStringLiteral("true") : QStringLiteral("false")));
    m_treeView->setBorderVisible(false);
    m_treeView->setBackgroundVisible(false);
    m_treeView->setHorizontalFluentScrollBarEnabled(false);
    m_treeView->setIndentation(0);
    m_treeView->setIndicatorMotionAnimationEnabled(true);
    m_treeView->setSelectionIndicatorVisible(true);
    fluent::collections::TreeView::SelectionIndicatorStyle indicatorStyle;
    indicatorStyle.inset = kRowLeftInset + 3.0;
    indicatorStyle.width = kSelectionIndicatorWidth;
    indicatorStyle.height = kSelectionIndicatorHeight;
    indicatorStyle.insetRole = IndicatorInsetRole;
    m_treeView->setSelectionIndicatorStyle(indicatorStyle);
    m_treeView->setSelectionMode(fluent::collections::TreeView::TreeSelectionMode::Single);
    m_treeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (auto* scrollBar = m_treeView->verticalFluentScrollBar())
        scrollBar->setThickness(5);

    m_model = new QStandardItemModel(m_treeView);
    QHash<QString, QStandardItem*> categoryItems;
    for (const GalleryNavigationItem& item : m_items)
        appendNavigationItem(m_model, categoryItems, item);

    m_treeView->setModel(m_model);
    m_treeView->setItemDelegate(makeGalleryNavigationDelegate(this, m_treeView));
    if (!isFooterOnly())
        m_treeView->collapseAll();
    updateCompactRowVisibility();

    connect(m_treeView, &fluent::collections::TreeView::itemPressed,
            this, [this](const QModelIndex& index) {
                const QString routeId = index.data(RouteIdRole).toString();
                if (routeId.isEmpty()) {
                    LOG_TRACE(QStringLiteral("GalleryNavigationPane itemPressed tree=%1 state=ignored reason=empty-route")
                                  .arg(m_treeView ? m_treeView->objectName() : objectName()));
                    return;
                }
                const bool hasChildren = m_model && m_model->hasChildren(index);
                LOG_DEBUG(QStringLiteral("GalleryNavigationPane itemPressed tree=%1 routeId=%2 hasChildren=%3")
                              .arg(m_treeView ? m_treeView->objectName() : objectName(),
                                   routeId,
                                   hasChildren ? QStringLiteral("true") : QStringLiteral("false")));
                if (routeId == QStringLiteral("settings"))
                    startSettingsIconRotation();
                setSelectedRouteId(routeId);
                if (m_compact && hasChildren) {
                    showCompactFlyoutForIndex(index);
                } else if (m_treeView && hasChildren) {
                    m_treeView->toggleExpanded(index);
                } else if (m_compact) {
                    closeCompactFlyout();
                }
                emit routeActivated(routeId);
            });

    outerLayout->addWidget(m_treeView);
    updateButtonStyles();
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane modelReady tree=%1 routes=%2 footerOnly=%3")
                  .arg(m_treeView->objectName())
                  .arg(m_routeIndexes.size())
                  .arg(isFooterOnly() ? QStringLiteral("true") : QStringLiteral("false")));
}

void GalleryNavigationPane::updateButtonStyles()
{
    if (!m_treeView || !m_treeView->selectionModel())
        return;

    const QModelIndex index = indexForRouteId(m_selectedRouteId);
    if (!index.isValid()) {
        m_treeView->clearSelection();
        m_treeView->setCurrentIndex(QModelIndex());
        LOG_TRACE(QStringLiteral("GalleryNavigationPane selectionCleared tree=%1 routeId=%2 reason=missing-index")
                      .arg(m_treeView->objectName(), m_selectedRouteId));
        return;
    }

    if (!m_compact) {
        QModelIndex parentIndex = index.parent();
        while (parentIndex.isValid()) {
            m_treeView->expand(parentIndex);
            parentIndex = parentIndex.parent();
        }
    }

    const QModelIndex visualIndex = visualSelectionIndex(index);
    m_treeView->setSelectedItem(visualIndex);
    m_treeView->scrollTo(visualIndex, QAbstractItemView::EnsureVisible);
    LOG_TRACE(QStringLiteral("GalleryNavigationPane selectionApplied tree=%1 routeId=%2")
                  .arg(m_treeView->objectName(), m_selectedRouteId));
}

void GalleryNavigationPane::updateCompactRowVisibility()
{
    if (!m_treeView || !m_model)
        return;

    const bool hideSectionHeaders = m_compact && m_compactVisualProgress >= 0.999;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const QModelIndex index = m_model->index(row, 0);
        const auto kind = static_cast<GalleryNavigationItem::Kind>(index.data(KindRole).toInt());
        m_treeView->setRowHidden(row, QModelIndex(),
                                 hideSectionHeaders && kind == GalleryNavigationItem::Kind::SectionHeader);
    }
}

void GalleryNavigationPane::syncCompactVisualProperties()
{
    if (!m_treeView)
        return;

    m_treeView->setProperty("galleryCompact", m_compact);
    m_treeView->setProperty("galleryCompactVisualProgress", m_compactVisualProgress);
    m_treeView->setProperty("gallerySettingsIconRotation", m_settingsIconRotation);
    if (m_treeView->viewport()) {
        m_treeView->viewport()->setProperty("galleryCompact", m_compact);
        m_treeView->viewport()->setProperty("galleryCompactVisualProgress", m_compactVisualProgress);
        m_treeView->viewport()->setProperty("gallerySettingsIconRotation", m_settingsIconRotation);
    }
}

void GalleryNavigationPane::startCompactVisualTransition(bool compact)
{
    const qreal endValue = compact ? 1.0 : 0.0;
    if (!m_compactVisualAnimation || qAbs(m_compactVisualProgress - endValue) <= 0.0001) {
        setCompactVisualProgress(endValue);
        updateCompactRowVisibility();
        return;
    }

    if (!isVisible() || !window() || !window()->isVisible()) {
        if (m_compactVisualAnimation)
            m_compactVisualAnimation->stop();
        setCompactVisualProgress(endValue);
        updateCompactRowVisibility();
        return;
    }

    const auto animation = themeAnimation();
    m_compactVisualAnimation->stop();
    m_compactVisualAnimation->setDuration(animation.normal);
    m_compactVisualAnimation->setEasingCurve(animation.decelerate);
    m_compactVisualAnimation->setStartValue(m_compactVisualProgress);
    m_compactVisualAnimation->setEndValue(endValue);
    m_compactVisualAnimation->start();
}

void GalleryNavigationPane::startSettingsIconRotation()
{
    if (!m_settingsIconRotationAnimation)
        return;

    if (!isVisible() || !window() || !window()->isVisible()) {
        setSettingsIconRotation(0.0);
        return;
    }

    m_settingsIconRotationAnimation->stop();
    const auto animation = themeAnimation();
    m_settingsIconRotationAnimation->setDuration(animation.slow);
    m_settingsIconRotationAnimation->setEasingCurve(animation.decelerate);
    m_settingsIconRotationAnimation->setStartValue(m_settingsIconRotation);
    m_settingsIconRotationAnimation->setEndValue(m_settingsIconRotation + kSettingsIconRotationDegrees - 0.01);
    m_settingsIconRotationAnimation->start();
}

QModelIndex GalleryNavigationPane::visualSelectionIndex(const QModelIndex& routeIndex) const
{
    if (!m_compact || !routeIndex.isValid())
        return routeIndex;

    const QModelIndex parentIndex = routeIndex.parent();
    return parentIndex.isValid() ? parentIndex : routeIndex;
}

void GalleryNavigationPane::showCompactFlyoutForIndex(const QModelIndex& index)
{
    if (!m_compact || !m_treeView || !m_model || !index.isValid() || !m_model->hasChildren(index))
        return;

    closeCompactFlyout(false);

    const QRect visualRect = m_treeView->visualRect(index);
    if (visualRect.isEmpty())
        return;

    if (!m_compactFlyoutAnchor) {
        m_compactFlyoutAnchor = new QWidget(m_treeView->viewport());
        m_compactFlyoutAnchor->setObjectName(QStringLiteral("galleryCompactNavigationFlyoutAnchor"));
        m_compactFlyoutAnchor->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    m_compactFlyoutAnchor->setGeometry(0,
                                       visualRect.top(),
                                       kCompactPaneWidth,
                                       visualRect.height());
    m_compactFlyoutAnchor->show();

    m_compactFlyout = new fluent::dialogs_flyouts::Popup(this);
    m_compactFlyout->setObjectName(QStringLiteral("galleryCompactNavigationFlyout"));
    m_compactFlyout->setAnimationEnabled(true);
    m_compactFlyout->setClosePolicy(fluent::dialogs_flyouts::Popup::ClosePolicy(
        fluent::dialogs_flyouts::Popup::CloseOnPressOutside |
        fluent::dialogs_flyouts::Popup::CloseOnEscape));

    m_compactFlyoutPanel = new CompactFlyoutPanel(m_compactFlyout);
    m_compactFlyoutPanel->setObjectName(QStringLiteral("galleryCompactNavigationFlyoutPanel"));
    auto* layout = new QVBoxLayout(m_compactFlyoutPanel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    for (int row = 0; row < m_model->rowCount(index); ++row) {
        const QModelIndex childIndex = m_model->index(row, 0, index);
        const QString routeId = childIndex.data(RouteIdRole).toString();
        if (routeId.isEmpty())
            continue;
        auto* itemRow = new CompactFlyoutRow(routeId,
                                             childIndex.data(Qt::DisplayRole).toString(),
                                             routeId == m_selectedRouteId,
                                             m_compactFlyoutPanel);
        itemRow->onActivated = [this](const QString& activatedRouteId) {
            setSelectedRouteId(activatedRouteId);
            emit routeActivated(activatedRouteId);
            QTimer::singleShot(0, this, [this]() {
                closeCompactFlyout();
            });
        };
        layout->addWidget(itemRow);
    }

    const QSize contentSize = m_compactFlyoutPanel->sizeHint();
    QWidget* topLevel = m_treeView->window();
    const QPoint anchorTopLeft = m_compactFlyoutAnchor->mapTo(topLevel, QPoint(0, 0));
    const int safeTop = qMax(kCompactFlyoutWindowMargin,
                             m_treeView->mapTo(topLevel, QPoint(0, 0)).y() + kCompactFlyoutWindowMargin);
    const int safeBottom = topLevel->height() - kCompactFlyoutWindowMargin;
    const int maxVisibleHeight = qMax(kRouteHeight, safeBottom - safeTop);
    const QSize cardSize(contentSize.width() + kCompactFlyoutContentMargins.left() + kCompactFlyoutContentMargins.right(),
                         qMin(contentSize.height() + kCompactFlyoutContentMargins.top() + kCompactFlyoutContentMargins.bottom(),
                              maxVisibleHeight));
    const int preferredTop = anchorTopLeft.y() + kCompactFlyoutVerticalOffset;
    const int cardTop = qBound(safeTop,
                               preferredTop,
                               qMax(safeTop, safeBottom - cardSize.height()));
    const int cardLeft = anchorTopLeft.x() + m_compactFlyoutAnchor->width() + kCompactFlyoutHorizontalOffset;

    const QRect contentRect = QRect(QPoint(0, 0), cardSize).marginsRemoved(kCompactFlyoutContentMargins);
    m_compactFlyoutPanel->resize(QSize(contentRect.width(), contentSize.height()));
    m_compactFlyout->resize(fluent::overlay::outerSizeForVisibleCard(cardSize));
    m_compactFlyoutPanel->setGeometry(fluent::overlay::visibleCardRect(m_compactFlyout->rect())
                                          .marginsRemoved(kCompactFlyoutContentMargins));
    m_compactFlyoutPanel->show();
    m_compactFlyout->setPosition(topLevel, QPoint(cardLeft, cardTop));
    m_compactFlyout->open();

    const QPoint endPos = m_compactFlyout->pos();
    auto* positionAnimation = new QPropertyAnimation(m_compactFlyout, "pos", m_compactFlyout);
    positionAnimation->setDuration(themeAnimation().fast);
    positionAnimation->setEasingCurve(themeAnimation().decelerate);
    positionAnimation->setStartValue(endPos - QPoint(kCompactFlyoutEntranceOffset, 0));
    positionAnimation->setEndValue(endPos);
    m_compactFlyout->move(positionAnimation->startValue().toPoint());
    positionAnimation->start(QAbstractAnimation::DeleteWhenStopped);

    LOG_DEBUG(QStringLiteral("GalleryNavigationPane compactFlyoutOpened parentRouteId=%1 childCount=%2")
                  .arg(index.data(RouteIdRole).toString())
                  .arg(m_model->rowCount(index)));
}

void GalleryNavigationPane::closeCompactFlyout(bool animated)
{
    if (m_compactFlyout) {
        auto* popup = m_compactFlyout;
        m_compactFlyout = nullptr;
        m_compactFlyoutPanel = nullptr;
        if (animated && popup->isVisible()) {
            connect(popup, &fluent::dialogs_flyouts::Popup::closed,
                    popup, &QObject::deleteLater);
            popup->close();
        } else {
            popup->hide();
            popup->deleteLater();
        }
    }
    if (m_compactFlyoutAnchor)
        m_compactFlyoutAnchor->hide();
}

bool GalleryNavigationPane::isFooterOnly() const
{
    return m_items.size() == 1 && m_items.first().kind == GalleryNavigationItem::Kind::FooterRoute;
}

QStandardItem* GalleryNavigationPane::createItem(const GalleryNavigationItem& item)
{
    auto* standardItem = new QStandardItem(item.title);
    standardItem->setEditable(false);
    standardItem->setData(item.id, RouteIdRole);
    standardItem->setData(static_cast<int>(item.kind), KindRole);
    standardItem->setData(item.parentId, ParentRouteIdRole);
    standardItem->setData(item.kind == GalleryNavigationItem::Kind::ComponentRoute ? QString() : item.iconGlyph,
                          IconGlyphRole);
    if (item.kind == GalleryNavigationItem::Kind::ComponentRoute)
        standardItem->setData(kRowLeftInset + kTextStart
                                  - kSelectionIndicatorWidth
                                  - kSelectionIndicatorTextGap,
                              IndicatorInsetRole);
    standardItem->setFlags(item.kind == GalleryNavigationItem::Kind::SectionHeader
                               ? Qt::ItemIsEnabled
                               : Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    return standardItem;
}

void GalleryNavigationPane::appendNavigationItem(QStandardItemModel* model,
                                                 QHash<QString, QStandardItem*>& categoryItems,
                                                 const GalleryNavigationItem& item)
{
    QStandardItem* standardItem = createItem(item);
    if (item.kind == GalleryNavigationItem::Kind::ComponentRoute && categoryItems.contains(item.parentId)) {
        categoryItems.value(item.parentId)->appendRow(standardItem);
    } else {
        model->appendRow(standardItem);
    }

    if (item.kind == GalleryNavigationItem::Kind::CategoryRoute)
        categoryItems.insert(item.id, standardItem);
    if (item.kind != GalleryNavigationItem::Kind::SectionHeader && !item.id.isEmpty())
        m_routeIndexes.insert(item.id, QPersistentModelIndex(standardItem->index()));
}

} // namespace fluent::gallery
