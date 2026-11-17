#include "GalleryNavigationPane.h"
#include "view/widgets/GalleryFeedbackButton.h"

#include <cmath>
#include <QAbstractItemView>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QTimer>
#include <QVariant>
#include <QVBoxLayout>

#include "components/dialogs_flyouts/Popup.h"
#include "view/support/GalleryMotion.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/collections/TreeView.h"
#include "components/layout/Divider.h"
#include "components/scrolling/ScrollBar.h"
#include "components/status_info/ToolTip.h"
#include "components/windowing/WindowBackdrop.h"
#include "support/logging/Log.h"
#include "GalleryCompactFlyout.h"
#include "GalleryNavigationDelegate.h"
#include "GalleryNavigationMetrics.h"
#include "view/support/GalleryStyleSupport.h"
#include "view/support/GalleryDepth.h"
#include "view/widgets/GallerySpatialSupportBadge.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {

GalleryNavigationPane::GalleryNavigationPane(const QVector<GalleryNavigationItem>& items, QWidget* parent)
    : QWidget(parent), m_items(items)
{
    setObjectName(QStringLiteral("galleryNavigationPane"));
    setSizePolicy(QSizePolicy::Expanding, isFooterOnly() ? QSizePolicy::Fixed : QSizePolicy::Expanding);
    setMinimumHeight(isFooterOnly() ? kRouteHeight + 9 : 0);
    m_compactVisualAnimation = new QPropertyAnimation(this, "compactVisualProgress", this);
    m_compactVisualAnimation->setObjectName(QStringLiteral("galleryNavigationCompactVisualAnimation"));
    connect(m_compactVisualAnimation, &QPropertyAnimation::finished, this, [this]() {
        setCompactVisualProgress(m_compact ? 1.0 : 0.0);
        updateCompactRowVisibility();
    });
    m_settingsIconRotationAnimation = new QPropertyAnimation(this, "settingsIconRotation", this);
    m_settingsIconRotationAnimation->setObjectName(QStringLiteral("gallerySettingsIconRotationAnimation"));
    connect(m_settingsIconRotationAnimation, &QPropertyAnimation::valueChanged, this, [this](const QVariant& value) {
        setSettingsIconRotation(value.toReal());
    });
    connect(m_settingsIconRotationAnimation, &QPropertyAnimation::finished, this, [this]() {
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
    if (m_footerActions) {
        m_footerActions->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_footerActions->setSpacing(compact ? 0 : 4);
        m_footerActions->setContentsMargins(0, 0, compact ? 0 : 8, 0);
        setMinimumHeight(compact ? 73 : kRouteHeight + 9);
        updateGeometry();
    }
    hideCompactToolTip();
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

void GalleryNavigationPane::setSurfaceVisible(bool visible)
{
    if (m_surfaceVisible == visible)
        return;

    m_surfaceVisible = visible;
    setAttribute(Qt::WA_NoSystemBackground, m_surfaceVisible);
    setAttribute(Qt::WA_TranslucentBackground, m_surfaceVisible);
    if (m_treeView) {
        m_treeView->setBackgroundVisible(false);
        m_treeView->setProperty("fluentPreserveParentSurface", m_surfaceVisible);
        if (m_treeView->viewport()) {
            m_treeView->viewport()->setProperty("fluentPreserveParentSurface", m_surfaceVisible);
            m_treeView->viewport()->setAttribute(Qt::WA_NoSystemBackground, m_surfaceVisible);
            m_treeView->viewport()->update();
        }
    }
    // Re-push the footer divider with the new surface state so it stops erasing through the flyout
    // card (and resumes erasing on the inline Mica pane). zh_CN: 用新的 surface 状态重设页脚分隔线，
    // 使其在浮层卡片上不再擦穿、回到内嵌 Mica 窗格时恢复擦除。
    updateDividerPalette();
    update();
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
    updateDividerPalette();
    update();
    if (m_treeView && m_treeView->viewport())
        m_treeView->viewport()->update();
}

void GalleryNavigationPane::paintEvent(QPaintEvent* event)
{
    // Inline pane chrome paints windowChromeBackdropFill under Normal mode so non-row areas
    // do not turn transparent over an un-composited top-level; floating drawers paint their own card.
    // zh_CN: 内联窗格在 Normal 模式下填充窗体背景色，避免空白区域在透明顶层窗口下漏出桌面；浮动抽屉保持透明以透出卡片。
    if (!m_surfaceVisible && !usesPaintedWindowBackdrop(this)) {
        const QColor fill = windowing::windowChromeBackdropFill(*this, window(),
            window() && window()->isActiveWindow());
        if (fill.isValid()) {
            QPainter painter(this);
            painter.fillRect(rect(), fill);
        }
    }
    QWidget::paintEvent(event);
}

bool GalleryNavigationPane::event(QEvent* event)
{
    if (event->type() == depth::changeEvent()) {
        update();
        if (m_treeView && m_treeView->viewport())
            m_treeView->viewport()->update();
    }
    // Track window activation so the inline pane tint updates in lockstep with the title bar.
    if (event->type() == QEvent::WindowActivate || event->type() == QEvent::WindowDeactivate)
        update();
    // Switch to transparent surface mode while floating inside NavigationView's overlay drawer.
    // zh_CN: 处于浮层抽屉时切换为透明模式，让下方的浮动卡片面板完整透出。
    if (event->type() == QEvent::DynamicPropertyChange) {
        const auto* propertyEvent = static_cast<QDynamicPropertyChangeEvent*>(event);
        if (propertyEvent->propertyName() == "fluentNavPaneFloating")
            setSurfaceVisible(property("fluentNavPaneFloating").toBool());
    }
    return QWidget::event(event);
}

bool GalleryNavigationPane::eventFilter(QObject* watched, QEvent* event)
{
    if (m_treeView && watched == m_treeView && event->type() == QEvent::KeyPress) {
        const auto* keyEvent = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers() & ~Qt::KeypadModifier;
        const bool movesCurrentItem = modifiers == Qt::NoModifier
            && (keyEvent->key() == Qt::Key_Up || keyEvent->key() == Qt::Key_Down || keyEvent->key() == Qt::Key_Home
                || keyEvent->key() == Qt::Key_End || keyEvent->key() == Qt::Key_PageUp
                || keyEvent->key() == Qt::Key_PageDown);
        if (movesCurrentItem) {
            // The event filter runs before QTreeView updates its current index. Activate on the
            // next event-loop turn so keyboard selection follows the same route path as a click.
            // zh_CN: eventFilter 先于 QTreeView 更新 currentIndex，故下一轮事件循环再激活，
            // 让键盘选择与鼠标点击进入同一路由流程。
            QTimer::singleShot(0, this, [this]() {
                if (m_treeView)
                    activateRouteIndex(m_treeView->currentIndex(), false);
            });
        }
    }

    if (m_treeView && watched == m_treeView->viewport()) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Paint)
            updateSpatialBadgeGeometry();
        switch (event->type()) {
        case QEvent::ToolTip: {
            const auto* helpEvent = static_cast<QHelpEvent*>(event);
            const QModelIndex index = m_treeView->indexAt(helpEvent->pos());
            if (m_compact && m_compactVisualProgress >= 0.999 && index.isValid())
                showCompactToolTip(index);
            else
                hideCompactToolTip();
            // Model rows carry tooltip text so Qt starts its usual hover timer. The Fluent
            // bubble owns presentation in compact mode; expanded mode intentionally has none.
            return true;
        }
        case QEvent::MouseMove:
            if (m_compactToolTip && m_compactToolTip->isVisible()) {
                const auto* mouseEvent = static_cast<QMouseEvent*>(event);
                if (m_treeView->indexAt(mouseEvent->pos()) != m_compactToolTipIndex)
                    hideCompactToolTip();
            }
            break;
        case QEvent::Leave:
        case QEvent::Hide:
        case QEvent::MouseButtonPress:
        case QEvent::Wheel:
        case QEvent::Resize:
            hideCompactToolTip();
            break;
        default:
            break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void GalleryNavigationPane::rebuild()
{
    m_routeIndexes.clear();

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    if (isFooterOnly()) {
        m_footerDivider = new fluent::layout::Divider(this);
        m_footerDivider->setLeadingInset(16);
        m_footerDivider->setTrailingInset(16);
        m_footerDivider->setObjectName(QStringLiteral("galleryFooterNavigationDivider"));
        m_footerDivider->setFixedHeight(1);
        m_footerDivider->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        outerLayout->addWidget(m_footerDivider);
        updateDividerPalette();
    }

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
    m_treeView->setProperty("fluentPreserveParentSurface", m_surfaceVisible);
    if (m_treeView->viewport()) {
        m_treeView->viewport()->setProperty("fluentPreserveParentSurface", m_surfaceVisible);
        m_treeView->viewport()->setAttribute(Qt::WA_NoSystemBackground, m_surfaceVisible);
        m_treeView->viewport()->installEventFilter(this);
    }
    m_treeView->setHorizontalFluentScrollBarEnabled(false);
    // Disable elastic overscroll so wheel scrolling stops cleanly at the pane edge.
    m_treeView->setOverscrollEnabled(false);
    m_treeView->setIndentation(0);
    m_treeView->setIndicatorMotionAnimationEnabled(true);
    m_treeView->setSelectionIndicatorVisible(true);
    fluent::collections::TreeView::SelectionIndicatorStyle indicatorStyle;
    indicatorStyle.inset = kRowLeftInset + 3.0;
    indicatorStyle.width = kSelectionIndicatorWidth;
    indicatorStyle.height = kSelectionIndicatorHeight;
    indicatorStyle.insetRole = IndicatorInsetRole;
    m_treeView->setSelectionIndicatorStyle(indicatorStyle);
    m_treeView->setSelectionMode(fluent::collections::TreeView::SelectionMode::Single);
    m_treeView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    if (auto* scrollBar = m_treeView->verticalFluentScrollBar())
        scrollBar->setThickness(5);

    m_model = new QStandardItemModel(m_treeView);
    QHash<QString, QStandardItem*> categoryItems;
    for (const GalleryNavigationItem& item : m_items)
        appendNavigationItem(m_model, categoryItems, item);

    m_treeView->setModel(m_model);
    m_treeView->setItemDelegate(makeGalleryNavigationDelegate(this, m_treeView));
    if (indexForRouteId(QStringLiteral("spatial")).isValid()) {
        m_spatialBadge = new GallerySpatialSupportBadge(m_treeView->viewport());
        m_spatialBadge->setObjectName(QStringLiteral("galleryNavigationSpatialSupportBadge"));
        m_spatialBadge->hide();
        m_model->setData(indexForRouteId(QStringLiteral("spatial")), m_spatialBadge->width() + qRound(kTextRightGap),
            AccessoryWidthRole);
        const auto updateSupportDescription = [this] {
            m_model->setData(indexForRouteId(QStringLiteral("spatial")), m_spatialBadge->accessibleDescription(),
                Qt::AccessibleDescriptionRole);
        };
        connect(&GallerySettings::instance(), &GallerySettings::spatialAvailabilityChanged, this,
            updateSupportDescription);
        updateSupportDescription();
        connect(m_treeView->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &GalleryNavigationPane::updateSpatialBadgeGeometry);
        connect(m_treeView, &QTreeView::expanded, this, &GalleryNavigationPane::updateSpatialBadgeGeometry);
        connect(m_treeView, &QTreeView::collapsed, this, &GalleryNavigationPane::updateSpatialBadgeGeometry);
    }
    m_treeView->installEventFilter(this);
    if (!isFooterOnly())
        m_treeView->collapseAll();
    updateCompactRowVisibility();

    connect(m_treeView, &fluent::collections::TreeView::itemPressed, this, [this](const QModelIndex& index) {
        activateRouteIndex(index, true);
    });

    if (isFooterOnly()) {
        m_treeView->setFixedHeight(kRouteHeight + 8);
        m_footerActions = new QBoxLayout(QBoxLayout::LeftToRight);
        m_footerActions->setContentsMargins(0, 0, 8, 0);
        m_footerActions->setSpacing(4);
        m_footerActions->addWidget(m_treeView, 1);
        m_footerActions->addWidget(new GalleryFeedbackButton(this), 0, Qt::AlignCenter);
        outerLayout->addLayout(m_footerActions);
    } else {
        outerLayout->addWidget(m_treeView);
    }
    updateButtonStyles();
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane modelReady tree=%1 routes=%2 footerOnly=%3")
            .arg(m_treeView->objectName())
            .arg(m_routeIndexes.size())
            .arg(isFooterOnly() ? QStringLiteral("true") : QStringLiteral("false")));
}

void GalleryNavigationPane::updateSpatialBadgeGeometry()
{
    if (!m_spatialBadge)
        return;
    const QRect row = m_treeView->visualRect(indexForRouteId(QStringLiteral("spatial")));
    const auto* viewport = m_treeView->viewport();
    const int right = viewport->width()
        - qRound(kRowRightInset + kChevronRightInset + kChevronAreaWidth + kTextRightGap);
    const QRect badgeRect(right - m_spatialBadge->width(), row.top() + (row.height() - m_spatialBadge->height()) / 2,
        m_spatialBadge->width(), m_spatialBadge->height());
    const bool visible = !m_compact && m_compactVisualProgress < 0.01 && !row.isEmpty()
        && viewport->rect().intersects(badgeRect);
    if (m_spatialBadge->geometry() != badgeRect)
        m_spatialBadge->setGeometry(badgeRect);
    if (m_spatialBadge->isHidden() == visible)
        m_spatialBadge->setVisible(visible);
}

void GalleryNavigationPane::activateRouteIndex(const QModelIndex& index, bool pointerActivation)
{
    const QString routeId = index.data(RouteIdRole).toString();
    if (routeId.isEmpty()) {
        LOG_TRACE(QStringLiteral("GalleryNavigationPane routeActivation tree=%1 state=ignored reason=empty-route")
                .arg(m_treeView ? m_treeView->objectName() : objectName()));
        return;
    }

    if (!pointerActivation && routeId == m_selectedRouteId)
        return;

    const bool hasChildren = m_model && m_model->hasChildren(index);
    LOG_DEBUG(QStringLiteral("GalleryNavigationPane routeActivation tree=%1 routeId=%2 source=%3 hasChildren=%4")
            .arg(m_treeView ? m_treeView->objectName() : objectName(), routeId,
                pointerActivation ? QStringLiteral("pointer") : QStringLiteral("keyboard"),
                hasChildren ? QStringLiteral("true") : QStringLiteral("false")));
    if (routeId == QStringLiteral("settings"))
        startSettingsIconRotation();
    setSelectedRouteId(routeId);

    if (pointerActivation) {
        // Only the 48px compact rail opens categories in a flyout; the wider floating drawer expands inline.
        // zh_CN: 仅 48px 窄图标栏使用浮层展开子项；加宽的浮动抽屉直接内联展开。
        const bool compactRail = m_compact && !m_surfaceVisible;
        if (compactRail && hasChildren) {
            showCompactFlyoutForIndex(index);
        } else if (m_treeView && hasChildren) {
            m_treeView->toggleExpanded(index);
        } else if (compactRail) {
            closeCompactFlyout();
        }
    }

    emit routeActivated(routeId);
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
    auto* verticalFluentBar = m_treeView->verticalFluentScrollBar();
    auto* horizontalFluentBar = m_treeView->horizontalFluentScrollBar();
    const bool verticalSignalsBlocked = verticalFluentBar && verticalFluentBar->signalsBlocked();
    const bool horizontalSignalsBlocked = horizontalFluentBar && horizontalFluentBar->signalsBlocked();
    if (verticalFluentBar)
        verticalFluentBar->blockSignals(true);
    if (horizontalFluentBar)
        horizontalFluentBar->blockSignals(true);
    // Keyboard navigation already selects the current row. Re-selecting it clears
    // the indicator without a currentChanged signal to restore it.
    // zh_CN: 键盘导航已选中当前行；重复选择会清掉指示条，却不会再次触发 currentChanged。
    if (m_treeView->currentIndex() != visualIndex || !m_treeView->selectionModel()->isSelected(visualIndex))
        m_treeView->setSelectedItem(visualIndex);
    m_treeView->scrollTo(visualIndex, QAbstractItemView::EnsureVisible);
    if (verticalFluentBar)
        verticalFluentBar->blockSignals(verticalSignalsBlocked);
    if (horizontalFluentBar)
        horizontalFluentBar->blockSignals(horizontalSignalsBlocked);
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

void GalleryNavigationPane::updateDividerPalette()
{
    if (!m_footerDivider)
        return;

    // Soften and inset the divider so it reads subtly over Mica and floating cards.
    // zh_CN: 降低分隔线透明度并留出左右内缩，使其在 Mica 与浮层上更柔和。
    const auto colors = themeColors();
    QColor divider = colors.strokeDivider;
    divider.setAlphaF(divider.alphaF() * 0.4);
    m_footerDivider->setColor(divider);
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
    ::fluent::gallery::motion::startFiniteTransition(m_compactVisualAnimation, animation.normal);
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
    ::fluent::gallery::motion::startFiniteTransition(m_settingsIconRotationAnimation, animation.slow);
}

QModelIndex GalleryNavigationPane::visualSelectionIndex(const QModelIndex& routeIndex) const
{
    if (!m_compact || !routeIndex.isValid())
        return routeIndex;

    QModelIndex visualIndex = routeIndex;
    while (visualIndex.parent().isValid())
        visualIndex = visualIndex.parent();
    return visualIndex;
}

void GalleryNavigationPane::showCompactFlyoutForIndex(const QModelIndex& index)
{
    if (!m_compact || !m_treeView || !m_model || !index.isValid() || !m_model->hasChildren(index))
        return;

    closeCompactFlyout(false);

    const QRect visualRect = m_treeView->visualRect(visualSelectionIndex(index));
    if (visualRect.isEmpty())
        return;

    if (!m_compactFlyoutAnchor) {
        m_compactFlyoutAnchor = new QWidget(m_treeView->viewport());
        m_compactFlyoutAnchor->setObjectName(QStringLiteral("galleryCompactNavigationFlyoutAnchor"));
        m_compactFlyoutAnchor->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    m_compactFlyoutAnchor->setGeometry(0, visualRect.top(), kCompactPaneWidth, visualRect.height());
    m_compactFlyoutAnchor->show();

    m_compactFlyout = new fluent::dialogs_flyouts::Popup(this);
    m_compactFlyout->setObjectName(QStringLiteral("galleryCompactNavigationFlyout"));
    m_compactFlyout->setAnimationEnabled(true);
    m_compactFlyout->setClosePolicy(fluent::dialogs_flyouts::Popup::ClosePolicy(
        fluent::dialogs_flyouts::Popup::CloseOnPressOutside | fluent::dialogs_flyouts::Popup::CloseOnEscape));
    // ComboBox-dropdown dismiss: an outside press only closes the flyout, it does not also activate the
    // control beneath — except on the rail itself, so clicking another rail item stays a single click.
    // zh_CN: ComboBox 下拉式关闭:外部点击只关闭浮窗,不会顺带激活下方控件——但在导航栏本身除外,使点击另一个导航项保持一次点击。
    m_compactFlyout->setLightDismissConsumesPress(true);
    m_compactFlyout->addLightDismissPassthrough(this);

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
        QModelIndex selectedIndex = indexForRouteId(m_selectedRouteId);
        while (selectedIndex.isValid() && selectedIndex != childIndex)
            selectedIndex = selectedIndex.parent();
        auto* itemRow = new CompactFlyoutRow(routeId, childIndex.data(Qt::DisplayRole).toString(),
            selectedIndex == childIndex, m_compactFlyoutPanel);
        itemRow->onActivated = [this](const QString& activatedRouteId) {
            setSelectedRouteId(activatedRouteId);
            emit routeActivated(activatedRouteId);
            QTimer::singleShot(0, this, [this, activatedRouteId]() {
                if (m_selectedRouteId != activatedRouteId)
                    return;
                const auto child = indexForRouteId(activatedRouteId);
                if (m_model->hasChildren(child))
                    showCompactFlyoutForIndex(child);
                else
                    closeCompactFlyout();
            });
        };
        layout->addWidget(itemRow);
    }

    const QSize contentSize = m_compactFlyoutPanel->sizeHint();
    QWidget* topLevel = m_treeView->window();
    const QRect surface = fluent::overlay::overlaySurfaceRect(topLevel);
    const QPoint anchorTopLeft = m_compactFlyoutAnchor->mapTo(topLevel, QPoint(0, 0));
    const int safeTop = qMax(surface.top() + kCompactFlyoutWindowMargin,
        m_treeView->mapTo(topLevel, QPoint(0, 0)).y() + kCompactFlyoutWindowMargin);
    const int safeBottom = surface.bottom() + 1 - kCompactFlyoutWindowMargin;
    const int maxVisibleHeight = qMax(kRouteHeight, safeBottom - safeTop);
    const QSize cardSize(contentSize.width() + kCompactFlyoutContentMargins.left()
            + kCompactFlyoutContentMargins.right(),
        qMin(contentSize.height() + kCompactFlyoutContentMargins.top() + kCompactFlyoutContentMargins.bottom(),
            maxVisibleHeight));
    const int preferredTop = anchorTopLeft.y() + kCompactFlyoutVerticalOffset;
    const int cardTop = qBound(safeTop, preferredTop, qMax(safeTop, safeBottom - cardSize.height()));
    const int cardLeft = anchorTopLeft.x() + m_compactFlyoutAnchor->width() + kCompactFlyoutHorizontalOffset;

    const QRect contentRect = QRect(QPoint(0, 0), cardSize).marginsRemoved(kCompactFlyoutContentMargins);
    m_compactFlyoutPanel->resize(QSize(contentRect.width(), contentSize.height()));
    m_compactFlyout->resize(fluent::overlay::outerSizeForVisibleCard(cardSize));
    m_compactFlyoutPanel->setGeometry(
        fluent::overlay::visibleCardRect(m_compactFlyout->rect()).marginsRemoved(kCompactFlyoutContentMargins));
    m_compactFlyoutPanel->show();
    m_compactFlyout->setPosition(topLevel, QPoint(cardLeft, cardTop));
    m_compactFlyout->open();

    const QPoint endPos = m_compactFlyout->pos();
    auto* positionAnimation = new QPropertyAnimation(m_compactFlyout, "pos", m_compactFlyout);
    positionAnimation->setObjectName(QStringLiteral("galleryCompactNavigationFlyoutEntranceAnimation"));
    const auto animation = themeAnimation();
    positionAnimation->setDuration(animation.fast);
    positionAnimation->setEasingCurve(animation.decelerate);
    positionAnimation->setStartValue(endPos - QPoint(kCompactFlyoutEntranceOffset, 0));
    positionAnimation->setEndValue(endPos);
    m_compactFlyout->move(positionAnimation->startValue().toPoint());
    ::fluent::gallery::motion::startFiniteTransition(positionAnimation, animation.fast, true,
        QAbstractAnimation::DeleteWhenStopped);

    LOG_DEBUG(QStringLiteral("GalleryNavigationPane compactFlyoutOpened parentRouteId=%1 childCount=%2")
            .arg(index.data(RouteIdRole).toString())
            .arg(m_model->rowCount(index)));
}

void GalleryNavigationPane::showCompactToolTip(const QModelIndex& index)
{
    if (!m_compact || m_compactVisualProgress < 0.999 || !m_treeView || !index.isValid())
        return;

    const auto kind = static_cast<GalleryNavigationItem::Kind>(index.data(KindRole).toInt());
    const QString text = index.data(Qt::ToolTipRole).toString();
    const QRect rowRect = m_treeView->visualRect(index);
    if (kind == GalleryNavigationItem::Kind::SectionHeader || text.isEmpty() || rowRect.isEmpty()
        || !m_treeView->viewport()->rect().intersects(rowRect)) {
        hideCompactToolTip();
        return;
    }

    if (!m_compactToolTip) {
        m_compactToolTip = new fluent::status_info::ToolTip(this);
        m_compactToolTip->setObjectName(QStringLiteral("galleryCompactNavigationToolTip"));
        m_compactToolTip->setAnimationEnabled(true);
    }
    m_compactToolTip->setText(text);
    m_compactToolTipIndex = index;

    // Center the visible bubble above the compact row. ToolTip reserves a transparent shadow
    // band, so offset that band out of the measured gap.
    const int shadow = m_compactToolTip->shadowMargin();
    const QPoint anchor = m_treeView->viewport()->mapToGlobal(QPoint(rowRect.center().x(), rowRect.top()));
    const int x = anchor.x() - m_compactToolTip->width() / 2;
    const int y = anchor.y() - kCompactToolTipGap - m_compactToolTip->height() + shadow;
    m_compactToolTip->move(x, y);
    m_compactToolTip->show();
    m_compactToolTip->raise();
}

void GalleryNavigationPane::hideCompactToolTip()
{
    m_compactToolTipIndex = QPersistentModelIndex();
    if (m_compactToolTip)
        m_compactToolTip->hide();
}

void GalleryNavigationPane::closeCompactFlyout(bool animated)
{
    if (m_compactFlyout) {
        auto* popup = m_compactFlyout;
        m_compactFlyout = nullptr;
        m_compactFlyoutPanel = nullptr;
        if (animated && popup->isVisible()) {
            connect(popup, &fluent::dialogs_flyouts::Popup::closed, popup, &QObject::deleteLater);
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
    standardItem->setData(item.depth, DepthRole);
    standardItem->setData(item.kind == GalleryNavigationItem::Kind::SectionHeader ? QString() : item.title,
        Qt::ToolTipRole);
    standardItem->setData(item.kind == GalleryNavigationItem::Kind::ComponentRoute ? QString() : item.iconGlyph,
        IconGlyphRole);
    const qreal nestedIndent =
        item.depth > 1 || (item.kind == GalleryNavigationItem::Kind::CategoryRoute && item.depth > 0)
        ? item.depth * kNestedRouteIndent
        : 0.0;
    if (item.kind == GalleryNavigationItem::Kind::ComponentRoute)
        standardItem->setData(kRowLeftInset + kTextStart - kSelectionIndicatorWidth - kSelectionIndicatorTextGap
                + nestedIndent,
            IndicatorInsetRole);
    else if (nestedIndent > 0)
        standardItem->setData(kRowLeftInset + 3.0 + nestedIndent, IndicatorInsetRole);
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
    if ((item.kind == GalleryNavigationItem::Kind::ComponentRoute
            || item.kind == GalleryNavigationItem::Kind::CategoryRoute)
        && categoryItems.contains(item.parentId)) {
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
