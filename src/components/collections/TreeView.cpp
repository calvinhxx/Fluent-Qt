#include "compatibility/InputEventCompat.h"
#include "compatibility/private/WidgetPlatformCompat_p.h"
#include "compatibility/PaintCompat.h"
#include "compatibility/WidgetCompat.h"
#include "TreeView.h"
#include <QAbstractItemModel>
#include <QApplication>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProxyStyle>
#include <QResizeEvent>
#include <QScopeGuard>
#include <QScrollBar>
#include <QShowEvent>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <QtMath>

#include "components/collections/CollectionViewBackdrop_p.h"
#include "components/collections/private/CollectionItemState_p.h"
#include "components/foundation/private/DpiPaintMetrics_p.h"
#include "components/foundation/private/MotionPolicy_p.h"
#include "components/scrolling/OverlayScrollChrome.h"
#include "components/scrolling/OverscrollController.h"
#include "components/scrolling/ScrollBar.h"
#include "design/Animation.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::collections {

namespace {

// Fallback delegate so plain text models get Fluent hover, selection, and chevron feedback.
class DefaultTreeItemDelegate final : public QStyledItemDelegate {
public:
    explicit DefaultTreeItemDelegate(TreeView* view) : QStyledItemDelegate(view), m_view(view) {}

    QRect arrowRect(const QStyleOptionViewItem& option) const
    {
        const bool rtl = option.direction == Qt::RightToLeft;
        return QRect(rtl ? option.rect.right() - 31 : option.rect.left() + 12, option.rect.center().y() - 10, 20, 20);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto content = option;
        const auto& colors = m_view->themeColorsRef();
        const auto visual = detail::collectionItemVisualStyle(option.state, colors);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(visual.background);
        painter->drawRoundedRect(QRectF(option.rect).adjusted(2, 1, -2, -1), CornerRadius::Control,
            CornerRadius::Control);
        if (index.model()->hasChildren(index) && m_view->itemsExpandable()) {
            painter->setPen(visual.foreground);
            painter->setFont(Typography::Icons::font(Typography::IconSize::Compact));
            painter->drawText(arrowRect(option), Qt::AlignCenter,
                m_view->isExpanded(index)
                    ? Typography::Icons::ChevronDown
                    : (option.direction == Qt::RightToLeft
                              ? Typography::Icons::ChevronLeft
                              : Typography::Icons::ChevronRight));
        }
        painter->restore();
        if (option.direction == Qt::RightToLeft)
            content.rect.adjust(4, 0, -36, 0);
        else
            content.rect.adjust(36, 0, -4, 0);
        content.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        QStyledItemDelegate::paint(painter, content, index);
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const auto size = QStyledItemDelegate::sizeHint(option, index);
        return QSize(size.width() + 40, qMax(size.height(), int(Spacing::ControlHeight::Standard)));
    }

    bool editorEvent(QEvent* event,
        QAbstractItemModel* model,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) override
    {
        if (event->type() == QEvent::MouseButtonRelease && (option.state & QStyle::State_Enabled)
            && (model->flags(index) & Qt::ItemIsEnabled) && m_view->itemsExpandable() && model->hasChildren(index)) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::LeftButton && arrowRect(option).contains(mouse->pos())) {
                m_view->setExpanded(index, !m_view->isExpanded(index));
                return true;
            }
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }

private:
    TreeView* m_view;
};

// Namespace-scope constant has no FluentElement instance; read the raw token directly.
constexpr int kExpandRevealDuration = ::Animation::Duration::Fast;

// Pixels scrolled per 120-delta wheel notch, matching ListView.
constexpr qreal kDiscreteWheelStepPx = ::Spacing::ControlHeight::Large;

// Suppress native PE_PanelItemViewRow (which paints across the indent gutter instead of the content edge)
// and QFrame::NoFrame (Cocoa's frame painter requests a raster CGContext even for NoFrame, breaking GPU composition).
class DelegateOwnedRowStyle final : public QProxyStyle {
public:
    void drawControl(ControlElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget = nullptr) const override
    {
        const auto* frame = qobject_cast<const QFrame*>(widget);
        if (element == CE_ShapedFrame && frame && frame->frameShape() == QFrame::NoFrame)
            return;
        QProxyStyle::drawControl(element, option, painter, widget);
    }

    void drawPrimitive(PrimitiveElement element,
        const QStyleOption* option,
        QPainter* painter,
        const QWidget* widget = nullptr) const override
    {
        if (element == PE_PanelItemViewRow)
            return;
        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

bool qrealFuzzyEquals(qreal lhs, qreal rhs)
{
    return qFuzzyCompare(lhs + 1.0, rhs + 1.0);
}

TreeView::SelectionIndicatorStyle normalizedIndicatorStyle(TreeView::SelectionIndicatorStyle style)
{
    style.inset = qMax<qreal>(0.0, style.inset);
    style.width = qMax<qreal>(0.0, style.width);
    style.height = qMax<qreal>(0.0, style.height);
    if (style.insetRole < 0)
        style.insetRole = -1;
    return style;
}

bool indicatorStylesEqual(const TreeView::SelectionIndicatorStyle& lhs, const TreeView::SelectionIndicatorStyle& rhs)
{
    return qrealFuzzyEquals(lhs.inset, rhs.inset) && qrealFuzzyEquals(lhs.width, rhs.width)
        && qrealFuzzyEquals(lhs.height, rhs.height) && lhs.insetRole == rhs.insetRole;
}

qreal indicatorInsetForIndex(const QModelIndex& index, const TreeView::SelectionIndicatorStyle& style)
{
    if (style.insetRole >= 0) {
        bool ok = false;
        const qreal roleInset = index.data(style.insetRole).toDouble(&ok);
        if (ok)
            return qMax<qreal>(0.0, roleInset);
    }
    return style.inset;
}

qreal lerp(qreal from, qreal to, qreal progress)
{
    return from + (to - from) * progress;
}

// Leading edge runs ahead and trailing edge lags to give the selection indicator a stretch-then-settle motion.
qreal indicatorLeadingProgress(qreal progress)
{
    return qBound(0.0, progress * 1.35, 1.0);
}

qreal indicatorTrailingProgress(qreal progress)
{
    return qBound(0.0, (progress - 0.18) / 0.82, 1.0);
}

} // namespace

TreeView::TreeView(QWidget* parent) : QTreeView(parent)
{
    ::compatibility::detail::disableNativeFocusRing(this);

    auto* rowStyle = new DelegateOwnedRowStyle;
    rowStyle->setParent(this);
    setStyle(rowStyle);

    m_fontRole = Typography::FontRole::Body;

    setFrameStyle(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    setIndentation(16); // WinUI 3: 16px per level
    setItemsExpandable(true);
    setRootIsDecorated(false); // Delegate paints the chevron.
    setAnimated(false);        // Custom height reveal in drawRow() avoids delayed child exposure.
    setExpandsOnDoubleClick(false);
    setHeaderHidden(true);

    QTreeView::setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setItemDelegate(new DefaultTreeItemDelegate(this));
    // Match ListView's fixed pixel step per wheel notch instead of ScrollPerItem's whole-row jumps.
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    // itemPressed enables press-time navigation while itemClicked preserves release activation.
    connect(this, &QAbstractItemView::pressed, this, [this](const QModelIndex& idx) { emit itemPressed(idx); });
    connect(this, &QAbstractItemView::clicked, this, [this](const QModelIndex& idx) { emit itemClicked(idx); });

    m_headerLabel = new QLabel(this);
    m_headerLabel->setObjectName(QStringLiteral("fluentTreeViewHeader"));
    m_headerLabel->setIndent(::Spacing::Padding::ListItemHorizontal);
    m_headerLabel->hide();

    m_vScrollBar = ::fluent::scrolling::createOverlayScrollBar(Qt::Vertical, this, verticalScrollBar(),
        QStringLiteral("fluentTreeViewVScrollBar"));
    connect(verticalScrollBar(), &QScrollBar::rangeChanged, this, &TreeView::syncFluentScrollBar);

    m_hScrollBar = ::fluent::scrolling::createOverlayScrollBar(Qt::Horizontal, this, horizontalScrollBar(),
        QStringLiteral("fluentTreeViewHScrollBar"));
    connect(horizontalScrollBar(), &QScrollBar::rangeChanged, this, &TreeView::syncFluentHScrollBar);

    fluent::scrolling::OverscrollController::Hooks hooks;
    hooks.horizontalScrollBar = [this] { return horizontalScrollBar(); };
    hooks.scrollBar = [this] { return verticalScrollBar(); };
    hooks.normalScroll = [this](qreal scrollPx) {
        QScrollBar* sb = verticalScrollBar();
        const int before = sb->value();
        sb->setValue(before - qRound(scrollPx));
        return sb->value() != before;
    };
    hooks.onOverscrollChanged = [this] { viewport()->update(); };
    hooks.fallbackWheel = [this](QWheelEvent* e) { QTreeView::wheelEvent(e); };
    m_overscroll = new fluent::scrolling::OverscrollController(viewport(), kDiscreteWheelStepPx, std::move(hooks),
        this);

    m_indicatorMotionAnim = new QVariantAnimation(this);
    m_indicatorMotionAnim->setDuration(themeAnimation().normal);
    m_indicatorMotionAnim->setEasingCurve(themeAnimation().decelerate);
    m_indicatorMotionAnim->setStartValue(0.0);
    m_indicatorMotionAnim->setEndValue(1.0);
    connect(m_indicatorMotionAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setIndicatorMotionProgress(value.toReal());
        if (viewport())
            viewport()->update(indicatorMotionDirtyRect());
    });
    connect(m_indicatorMotionAnim, &QVariantAnimation::finished, this, [this]() {
        setIndicatorMotionProgress(1.0);
        if (viewport())
            viewport()->update(indicatorMotionDirtyRect());
    });

    // Reversible height reveal (0 = collapsed, 1 = expanded); collapse stays laid out until finished.
    m_expandRevealAnim = new QVariantAnimation(this);
    m_expandRevealAnim->setDuration(kExpandRevealDuration);
    m_expandRevealAnim->setEasingCurve(themeAnimation().standard);
    connect(m_expandRevealAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant&) {
        // Full repaint: rows below the subtree translate in drawRow(), so partial updates would tear.
        if (viewport())
            viewport()->update();
    });
    connect(m_expandRevealAnim, &QVariantAnimation::finished, this, [this]() {
        if (m_pendingCollapseFinalize) {
            finalizeDeferredCollapse();
            return;
        }
        clearExpandRevealState();
    });
    connect(this, &QTreeView::expanded, this, [this](const QModelIndex& idx) {
        if (!m_animEnabled)
            return;
        startExpandReveal(idx);
    });
    connect(this, &QTreeView::collapsed, this, [this](const QModelIndex& idx) {
        // Instant collapse cleanup; deferred animated collapse finishes in toggleExpanded()/finalizeDeferredCollapse().
        if (m_animParent.isValid() && (m_animParent == idx || isDescendantOf(QModelIndex(m_animParent), idx))
            && !m_pendingCollapseFinalize) {
            clearExpandRevealState();
        }
        if ((m_currentIndicatorIndex.isValid() && isDescendantOf(m_currentIndicatorIndex, idx))
            || (m_activeIndicatorIndex.isValid() && isDescendantOf(m_activeIndicatorIndex, idx))) {
            clearIndicatorMotionState();
        } else if (m_previousIndicatorIndex.isValid() && isDescendantOf(m_previousIndicatorIndex, idx)) {
            if (m_indicatorMotionAnim)
                m_indicatorMotionAnim->stop();
            m_previousIndicatorIndex = QPersistentModelIndex();
            setIndicatorMotionDirection(IndicatorVerticalDirection::None);
            setIndicatorHierarchyTransition(IndicatorHierarchyTransition::None);
            setIndicatorMotionProgress(1.0);
            if (viewport())
                viewport()->update();
        }
    });

    syncFluentScrollBar();
    syncFluentHScrollBar();
    onThemeUpdated();
}

TreeView::~TreeView()
{
    // Suppress signals from animation/model reset and detach external model/delegate before ~QTreeView
    // so PySide6/Shiboken teardown doesn't race Qt's internal disconnect order.
    blockSignals(true);
    if (m_indicatorMotionAnim)
        m_indicatorMotionAnim->stop();
    if (m_expandRevealAnim)
        m_expandRevealAnim->stop();
    disconnectIndicatorMotionModel();

    QTreeView::setItemDelegate(nullptr);
    QTreeView::setModel(nullptr);
}

QModelIndex TreeView::indexAt(const QPoint& point) const
{
    if (m_animParent.isValid() && m_expandRevealAnim && m_expandRevealAnim->state() == QAbstractAnimation::Running
        && m_animSubtreeHeight > 0.0) {
        const QModelIndex parent = QModelIndex(m_animParent);
        const QRect parentRect = QTreeView::visualRect(parent);
        if (!parentRect.isEmpty()) {
            const qreal progress = qBound(0.0, m_expandRevealAnim->currentValue().toReal(), 1.0);
            const qreal boundaryY = parentRect.bottom() + 1 + m_animSubtreeHeight * progress;
            const qreal slideUp = m_animSubtreeHeight * (1.0 - progress);
            if (point.y() >= boundaryY && slideUp > 0.0) {
                QPoint mappedPoint = point;
                mappedPoint.setY(qRound(point.y() + slideUp));
                return QTreeView::indexAt(mappedPoint);
            }
        }
    }

    return QTreeView::indexAt(point);
}

void TreeView::setModel(QAbstractItemModel* model)
{
    disconnectIndicatorMotionModel();
    clearIndicatorMotionState();
    clearExpandRevealState();
    QTreeView::setModel(model);
    connectIndicatorMotionModel(model);
}

void TreeView::setSelectionModel(QItemSelectionModel* selectionModel)
{
    clearIndicatorMotionState();
    QTreeView::setSelectionModel(selectionModel);
}

// ── Selection mode ────────────────────────────────────────────────────────────

void TreeView::setSelectionMode(SelectionMode mode)
{
    if (m_selectionMode == mode)
        return;
    m_selectionMode = mode;

    switch (mode) {
    case SelectionMode::None:
        QTreeView::setSelectionMode(QAbstractItemView::NoSelection);
        break;
    case SelectionMode::Single:
        QTreeView::setSelectionMode(QAbstractItemView::SingleSelection);
        break;
    case SelectionMode::Multiple:
        QTreeView::setSelectionMode(QAbstractItemView::MultiSelection);
        break;
    case SelectionMode::Extended:
        QTreeView::setSelectionMode(QAbstractItemView::ExtendedSelection);
        break;
    }
    emit selectionModeChanged();
}

// ── Drag reorder ──────────────────────────────────────────────────────────────

void TreeView::setCanReorderItems(bool enabled)
{
    if (m_canReorderItems == enabled)
        return;
    m_canReorderItems = enabled;
    emit canReorderItemsChanged();
}

bool TreeView::isScrollChainingEnabled() const
{
    return m_overscroll->isScrollChainingEnabled();
}

void TreeView::setScrollChainingEnabled(bool enabled)
{
    if (m_overscroll->isScrollChainingEnabled() == enabled)
        return;
    m_overscroll->setScrollChainingEnabled(enabled);
    emit scrollChainingEnabledChanged();
}

bool TreeView::isOverscrollEnabled() const
{
    return m_overscroll->isOverscrollEnabled();
}

void TreeView::setOverscrollEnabled(bool enabled)
{
    if (m_overscroll->isOverscrollEnabled() == enabled)
        return;
    m_overscroll->setOverscrollEnabled(enabled);
    emit overscrollEnabledChanged();
}

// ── Appearance properties ────────────────────────────────────────────────────

void TreeView::setFontRole(Typography::FontRole role)
{
    if (m_fontRole == role)
        return;
    m_fontRole = role;
    applyThemeStyle();
    emit fontRoleChanged();
}

void TreeView::setBorderVisible(bool visible)
{
    if (m_borderVisible == visible)
        return;
    m_borderVisible = visible;
    update();
    emit borderVisibleChanged();
}

void TreeView::setBackgroundVisible(bool visible)
{
    if (m_backgroundVisible == visible)
        return;
    m_backgroundVisible = visible;
    if (viewport())
        viewport()->update();
    emit backgroundVisibleChanged();
}

void TreeView::setHeaderText(const QString& text)
{
    if (m_headerText == text)
        return;
    m_headerText = text;

    m_headerLabel->setText(text);
    m_headerLabel->setVisible(!text.isEmpty());
    applyThemeStyle();
    emit headerTextChanged();
}

void TreeView::setPlaceholderText(const QString& text)
{
    if (m_placeholderText == text)
        return;
    m_placeholderText = text;
    if (viewport())
        viewport()->update();
    emit placeholderTextChanged();
}

// ── Tree API ──────────────────────────────────────────────────────────────────

void TreeView::expandAll()
{
    m_animEnabled = false;
    clearExpandRevealState();
    QTreeView::expandAll();
    m_animEnabled = true;
}

void TreeView::collapseAll()
{
    m_animEnabled = false;
    clearExpandRevealState();
    QTreeView::collapseAll();
    m_animEnabled = true;
}

void TreeView::toggleExpanded(const QModelIndex& index)
{
    if (!index.isValid())
        return;

    // Reverse an active reveal on the same subtree so rapid clicks stay continuous.
    if (m_animParent.isValid() && QModelIndex(m_animParent) == index && m_expandRevealAnim
        && m_expandRevealAnim->state() == QAbstractAnimation::Running) {
        if (m_animExpanding) {
            startCollapseReveal(index);
        } else {
            m_pendingCollapseFinalize = false;
            startExpandReveal(index);
        }
        return;
    }
    if (m_animParent.isValid() && m_expandRevealAnim && m_expandRevealAnim->state() == QAbstractAnimation::Running) {
        completeActiveExpandReveal();
    }

    if (isExpanded(index)) {
        if (m_animEnabled && isVisible() && computeSubtreeHeight(index) > 0.0) {
            startCollapseReveal(index);
        } else {
            setExpanded(index, false);
        }
    } else {
        setExpanded(index, true);
    }
}

// ── Selection API ─────────────────────────────────────────────────────────────

QModelIndex TreeView::selectedItem() const
{
    auto idxList = selectionModel() ? selectionModel()->selectedIndexes() : QModelIndexList();
    return idxList.isEmpty() ? QModelIndex() : idxList.first();
}

QModelIndexList TreeView::selectedItems() const
{
    return selectionModel() ? selectionModel()->selectedIndexes() : QModelIndexList();
}

void TreeView::setSelectedItem(const QModelIndex& index)
{
    if (!index.isValid()) {
        clearSelection();
        setCurrentIndex(QModelIndex());
        clearIndicatorMotionState();
        return;
    }
    if (isVisible()) {
        setCurrentIndex(index);
    } else {
        selectionModel()->setCurrentIndex(index,
            QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Current | QItemSelectionModel::Rows);
    }
}

void TreeView::setIndicatorMotionAnimationEnabled(bool enabled)
{
    if (m_indicatorMotionAnimationEnabled == enabled)
        return;
    m_indicatorMotionAnimationEnabled = enabled;
    if (!enabled) {
        if (m_indicatorMotionAnim)
            m_indicatorMotionAnim->stop();
        setIndicatorMotionProgress(1.0);
        if (viewport())
            viewport()->update();
    }
    emit indicatorMotionAnimationEnabledChanged();
}

qreal TreeView::selectedIndicatorProgress(const QModelIndex& index) const
{
    if (index.isValid() && m_activeIndicatorIndex.isValid() && index == m_activeIndicatorIndex)
        return m_indicatorMotionProgress;
    return 1.0;
}

bool TreeView::selectionIndicatorVisible() const
{
    return m_selectionIndicatorVisible;
}

void TreeView::setSelectionIndicatorVisible(bool visible)
{
    if (m_selectionIndicatorVisible == visible)
        return;
    m_selectionIndicatorVisible = visible;
    if (viewport())
        viewport()->update();
}

void TreeView::setSelectionIndicatorStyle(const SelectionIndicatorStyle& style)
{
    const SelectionIndicatorStyle normalized = normalizedIndicatorStyle(style);
    if (indicatorStylesEqual(m_selectionIndicatorStyle, normalized))
        return;
    m_selectionIndicatorStyle = normalized;
    if (viewport())
        viewport()->update();
    emit selectionIndicatorStyleChanged();
}

void TreeView::setSelectionIndicatorInset(qreal inset)
{
    SelectionIndicatorStyle style = m_selectionIndicatorStyle;
    style.inset = inset;
    setSelectionIndicatorStyle(style);
}

void TreeView::setSelectionIndicatorHeight(qreal height)
{
    SelectionIndicatorStyle style = m_selectionIndicatorStyle;
    style.height = height;
    setSelectionIndicatorStyle(style);
}

bool TreeView::isIndicatorMotionActiveForIndex(const QModelIndex& index) const
{
    return index.isValid() && m_activeIndicatorIndex.isValid() && index == m_activeIndicatorIndex;
}

QModelIndex TreeView::indicatorMotionPreviousIndex() const
{
    return QModelIndex(m_previousIndicatorIndex);
}

QModelIndex TreeView::indicatorMotionCurrentIndex() const
{
    return QModelIndex(m_currentIndicatorIndex);
}

QRectF TreeView::selectedIndicatorRect() const
{
    return selectedIndicatorRect(m_indicatorMotionProgress);
}

QRectF TreeView::selectedIndicatorRect(qreal progress) const
{
    if (!m_currentIndicatorIndex.isValid())
        return {};

    const QRectF target = selectedIndicatorBaseRect(QModelIndex(m_currentIndicatorIndex));
    if (target.isEmpty())
        return {};

    const qreal clampedProgress = qBound(0.0, progress, 1.0);
    if (!m_previousIndicatorIndex.isValid() || m_indicatorMotionDirection == IndicatorVerticalDirection::None
        || qFuzzyCompare(clampedProgress + 1.0, 2.0)) {
        return target;
    }

    const QRectF previous = selectedIndicatorBaseRect(QModelIndex(m_previousIndicatorIndex));
    if (previous.isEmpty())
        return target;
    if (qFuzzyCompare(clampedProgress + 1.0, 1.0))
        return previous;

    const bool hierarchyTransition = m_indicatorHierarchyTransition == IndicatorHierarchyTransition::Inward
        || m_indicatorHierarchyTransition == IndicatorHierarchyTransition::Outward;
    if (hierarchyTransition) {
        const qreal horizontalProgress = qBound(0.0, clampedProgress / 0.18, 1.0);
        const qreal x = lerp(previous.left(), target.left(), horizontalProgress);
        const qreal width = lerp(previous.width(), target.width(), horizontalProgress);
        const qreal height = lerp(previous.height(), target.height(), horizontalProgress);
        const qreal centerY = lerp(previous.center().y(), target.center().y(), clampedProgress);
        return QRectF(x, centerY - height / 2.0, width, height);
    }

    const qreal leading = indicatorLeadingProgress(clampedProgress);
    const qreal trailing = indicatorTrailingProgress(clampedProgress);
    qreal top = previous.top();
    qreal bottom = previous.bottom();
    if (m_indicatorMotionDirection == IndicatorVerticalDirection::Down) {
        top = lerp(previous.top(), target.top(), trailing);
        bottom = lerp(previous.bottom(), target.bottom(), leading);
    } else {
        top = lerp(previous.top(), target.top(), leading);
        bottom = lerp(previous.bottom(), target.bottom(), trailing);
    }

    qreal x = lerp(previous.left(), target.left(), clampedProgress);
    const qreal width = lerp(previous.width(), target.width(), clampedProgress);

    return QRectF(x, qMin(top, bottom), width, qAbs(bottom - top));
}

::fluent::scrolling::ScrollBar* TreeView::verticalFluentScrollBar() const
{
    return m_vScrollBar;
}

::fluent::scrolling::ScrollBar* TreeView::horizontalFluentScrollBar() const
{
    return m_hScrollBar;
}

void TreeView::setHorizontalFluentScrollBarEnabled(bool enabled)
{
    if (m_horizontalFluentScrollBarEnabled == enabled)
        return;

    m_horizontalFluentScrollBarEnabled = enabled;
    if (!m_horizontalFluentScrollBarEnabled && m_hScrollBar)
        m_hScrollBar->hide();
    syncFluentHScrollBar();
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void TreeView::paintEvent(QPaintEvent* event)
{
    const auto& c = themeColorsRef();
    const int r = CornerRadius::Control;
    // --- 1. Container background ---
    if (m_backgroundVisible) {
        QPainter p(viewport());
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(viewport()->rect(), c.bgLayer);
        p.end();
    } else if (detail::shouldClearCompositedViewport(this)) {
        // Clear stale pixels on real OS backdrops (macOS backing store doesn't auto-clear).
        // Gate via shouldClearCompositedViewport() rather than WA_TranslucentBackground alone,
        // because Normal-mode Windows windows also carry that flag and clearing them paints black.
        QPainter p(viewport());
        p.setCompositionMode(QPainter::CompositionMode_Source);
        p.fillRect(viewport()->rect(), Qt::transparent);
        p.end();
    }

    // --- 2. Empty placeholder ---
    const bool isEmpty = !model() || model()->rowCount() == 0;
    if (isEmpty && !m_placeholderText.isEmpty()) {
        QPainter ph(viewport());
        ph.setRenderHint(QPainter::Antialiasing);
        ph.setPen(c.textTertiary);
        ph.setFont(themeFont(m_fontRole).toQFont());
        ph.drawText(viewport()->rect(), Qt::AlignCenter, m_placeholderText);
        ph.end();
    }

    // --- 3. Tree items (QTreeView default) ---
    QTreeView::paintEvent(event);

    // --- 3.5 Drop indicator: OnItem highlight ---
    if (m_isDragging && m_dropMode == DropMode::OnItem && m_dropOnIndex.isValid()) {
        QRect targetRect = visualRect(m_dropOnIndex);
        if (!targetRect.isEmpty()) {
            QPainter hp(viewport());
            hp.setRenderHint(QPainter::Antialiasing);
            QColor fill = c.accentDefault;
            fill.setAlphaF(0.12f);
            const int rr = CornerRadius::Control;
            hp.setPen(QPen(c.accentDefault, 2.0));
            hp.setBrush(fill);
            hp.drawRoundedRect(QRectF(targetRect).adjusted(1, 0, -1, 0), rr, rr);
            hp.end();
        }
    }

    // --- 3.6 Drop indicator: Between line ---
    if (m_isDragging && m_dropMode == DropMode::Between && m_dropTargetRow >= 0 && model()) {
        QPainter dp(viewport());
        dp.setRenderHint(QPainter::Antialiasing);

        int siblingCount = model()->rowCount(m_dropTargetParent);
        int y;
        if (m_dropTargetRow < siblingCount) {
            QModelIndex targetIdx = model()->index(m_dropTargetRow, 0, m_dropTargetParent);
            QRect targetRect = visualRect(targetIdx);
            y = targetRect.top();
        } else {
            // After last sibling
            QModelIndex lastIdx = model()->index(siblingCount - 1, 0, m_dropTargetParent);
            QRect lastRect = visualRect(lastIdx);
            y = lastRect.bottom() + 1;
        }

        dp.setPen(QPen(c.accentDefault, 2.0));
        dp.drawLine(::Spacing::Padding::ListItemHorizontal, y,
            viewport()->width() - ::Spacing::Padding::ListItemHorizontal, y);
        const int circleR = 3;
        dp.setBrush(c.accentDefault);
        dp.setPen(Qt::NoPen);
        dp.drawEllipse(QPoint(::Spacing::Padding::ListItemHorizontal, y), circleR, circleR);
        dp.drawEllipse(QPoint(viewport()->width() - ::Spacing::Padding::ListItemHorizontal, y), circleR, circleR);
        dp.end();
    }

    // --- 3.7 Drag floating pixmap (offset from cursor, like file manager) ---
    if (m_isDragging && !m_dragPixmap.isNull()) {
        QPainter fp(viewport());
        fp.setRenderHint(QPainter::Antialiasing);
        fp.setOpacity(0.85);
        const int pixH = qRound(m_dragPixmap.height() / m_dragPixmap.devicePixelRatio());
        // Place pixmap slightly right and below the cursor
        constexpr int kOffsetX = 8;
        constexpr int kOffsetY = 4;
        QPoint pixPos(m_dragCurrentPos.x() + kOffsetX, m_dragCurrentPos.y() - pixH / 2 + kOffsetY);
        fluentDrawPixmapAt(fp, pixPos, m_dragPixmap);
        fp.end();
    }

    // --- 3.8 Selected indicator pill overlay (moving / direction-aware) ---
    if (m_selectionIndicatorVisible && !m_isDragging) {
        QPainter ip(viewport());
        ip.setRenderHint(QPainter::Antialiasing);
        paintSelectedIndicator(ip);
        ip.end();
    }

    // --- 4. Corner masking: fill corners with parent bg (anti-alias) ---
    if (m_borderVisible || m_backgroundVisible) {
        QPainter cp(viewport());
        cp.setRenderHint(QPainter::Antialiasing);

        QPainterPath fullRect;
        fullRect.addRect(QRectF(viewport()->rect()));
        QPainterPath roundedArea;
        roundedArea.addRoundedRect(QRectF(viewport()->rect()), r, r);
        QPainterPath corners = fullRect - roundedArea;

        QColor parentBg = c.bgCanvas;
        if (parentWidget()) {
            const QPalette& pp = parentWidget()->palette();
            if (pp.color(QPalette::Window).alpha() > 0)
                parentBg = pp.color(QPalette::Window);
        }
        cp.fillPath(corners, parentBg);
        cp.end();
    }

    // --- 5. Container border ---
    if (m_borderVisible) {
        QPainter bp(viewport());
        bp.setRenderHint(QPainter::Antialiasing);

        const auto stroke = fluent::painting::DpiPaintMetrics(bp).alignedStroke(QRectF(viewport()->rect()), 1.0);
        QPainterPath borderPath;
        borderPath.addRoundedRect(stroke.rect, r, r);
        bp.setPen(QPen(c.strokeDefault, stroke.width));
        bp.setBrush(Qt::NoBrush);
        bp.drawPath(borderPath);
        bp.end();
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────

void TreeView::resizeEvent(QResizeEvent* event)
{
    QTreeView::resizeEvent(event);
    updateViewportMargins();
    syncFluentScrollBar();
    syncFluentHScrollBar();
    layoutHeader();
}

void TreeView::showEvent(QShowEvent* event)
{
    QTreeView::showEvent(event);
    updateViewportMargins();
    layoutHeader();
    // Defer initial scroll/sync until layout stabilizes to avoid a first-frame flash.
    QTimer::singleShot(0, this, [this]() {
        if (currentIndex().isValid()) {
            scrollTo(currentIndex(), QAbstractItemView::EnsureVisible);
        }
        syncFluentScrollBar();
        syncFluentHScrollBar();
    });
}

void TreeView::changeEvent(QEvent* event)
{
    QTreeView::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        applyThemeStyle(false);
        if (viewport())
            viewport()->update();
    }
}

void TreeView::enterEvent(FluentEnterEvent* event)
{
    setViewportHovered(true);
    QTreeView::enterEvent(event);
}

void TreeView::leaveEvent(QEvent* event)
{
    setViewportHovered(false);
    QTreeView::leaveEvent(event);
}

// ── Overscroll bounce ─────────────────────────────────────────────────────────

void TreeView::wheelEvent(QWheelEvent* event)
{
    m_overscroll->handleWheel(event);
}

int TreeView::verticalOffset() const
{
    // Keep indexAt()/visualRect() aligned with the overscroll shift applied in drawRow();
    // m_overscroll may be null while QTreeView queries offsets during construction.
    const qreal overscroll = m_overscroll ? m_overscroll->value() : 0.0;
    return QTreeView::verticalOffset() - qRound(overscroll);
}

void TreeView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    QTreeView::currentChanged(current, previous);
    updateIndicatorMotionForIndex(current, previous);
}

void TreeView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    QTreeView::selectionChanged(selected, deselected);
    syncCheckStatesWithSelection(selected, deselected);
    if (selected.indexes().isEmpty() && (!selectionModel() || selectionModel()->selectedIndexes().isEmpty()))
        clearIndicatorMotionState();
    // Re-selecting the current row emits no currentChanged(), so restore the indicator here.
    else if (!m_currentIndicatorIndex.isValid() && selectionModel() && selectionModel()->isSelected(currentIndex()))
        updateIndicatorMotionForIndex(currentIndex(), QModelIndex());
}

void TreeView::drawBranches(QPainter* /*painter*/, const QRect& /*rect*/, const QModelIndex& /*index*/) const
{
    // Native branches suppressed; the delegate paints Fluent chevrons.
}

void TreeView::drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const
{
    // QTreeView positions rows from scrollbar value rather than verticalOffset(),
    // so apply elastic bounce via painter translation while the container background stays put.
    const int overscrollDy = m_overscroll ? qRound(m_overscroll->value()) : 0;
    if (overscrollDy != 0)
        painter->translate(0, overscrollDy);
    const auto restoreOverscroll = qScopeGuard([painter, overscrollDy] {
        if (overscrollDy != 0)
            painter->translate(0, -overscrollDy);
    });

    // Clip the animating subtree to its revealed height and slide rows below along with that edge.
    if (m_animParent.isValid() && m_expandRevealAnim && m_expandRevealAnim->state() == QAbstractAnimation::Running) {
        const QModelIndex parent = QModelIndex(m_animParent);
        const QRect parentRect = visualRect(parent);
        const qreal progress = qBound(0.0, m_expandRevealAnim->currentValue().toReal(), 1.0);
        const qreal boundaryY = parentRect.bottom() + 1 + m_animSubtreeHeight * progress;
        const qreal slideUp = m_animSubtreeHeight * (1.0 - progress);

        if (index != parent && isDescendantOf(index, parent)) {
            if (options.rect.top() >= boundaryY)
                return;
            QRect clip = options.rect;
            clip.setBottom(qMin<int>(options.rect.bottom(), qFloor(boundaryY)));
            painter->save();
            painter->setClipRect(clip);
            QTreeView::drawRow(painter, options, index);
            painter->restore();
            return;
        }
        if (index != parent && options.rect.top() >= parentRect.bottom()) {
            painter->save();
            painter->translate(0, -slideUp);
            QTreeView::drawRow(painter, options, index);
            painter->restore();
            return;
        }
    }

    // --- Drag: dim source row ---
    if (m_isDragging && index == m_dragSourceIndex) {
        painter->save();
        painter->setOpacity(0.3);
        QTreeView::drawRow(painter, options, index);
        painter->restore();
        return;
    }
    QTreeView::drawRow(painter, options, index);
}

qreal TreeView::chevronRotation(const QModelIndex& index) const
{
    // Keep the animating parent's chevron synced with the height reveal progress.
    if (m_animParent.isValid() && QModelIndex(m_animParent) == index && m_expandRevealAnim
        && m_expandRevealAnim->state() == QAbstractAnimation::Running) {
        return qBound(0.0, m_expandRevealAnim->currentValue().toReal(), 1.0);
    }
    return isExpanded(index) ? 1.0 : 0.0;
}

// ── Drag reorder: mouse events ───────────────────────────────────────────────

void TreeView::mousePressEvent(QMouseEvent* event)
{
    if (m_canReorderItems && event->button() == Qt::LeftButton) {
        QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
            m_dragStartPos = event->pos();
            m_dragSourceIndex = QPersistentModelIndex(idx);
        }
    }
    QTreeView::mousePressEvent(event);
}

void TreeView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_canReorderItems && m_dragSourceIndex.isValid() && (event->buttons() & Qt::LeftButton)) {
        if (!m_isDragging) {
            if ((event->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                m_dragPixmap = renderRowPixmap(m_dragSourceIndex);
                m_dragCurrentPos = event->pos();
                m_isDragging = true;
            }
        }

        if (m_isDragging) {
            m_dragCurrentPos = event->pos();
            computeDropTarget(event->pos());
            viewport()->update();
            event->accept();
            return;
        }
    }
    QTreeView::mouseMoveEvent(event);
}

void TreeView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_isDragging && event->button() == Qt::LeftButton) {
        if (m_dropMode != DropMode::None && model()) {
            auto* sim = qobject_cast<QStandardItemModel*>(model());
            if (sim && m_dragSourceIndex.isValid()) {
                QModelIndex srcParentIdx = m_dragSourceIndex.parent();
                int srcRow = m_dragSourceIndex.row();

                QStandardItem* srcParentItem =
                    srcParentIdx.isValid() ? sim->itemFromIndex(srcParentIdx) : sim->invisibleRootItem();

                if (m_dropMode == DropMode::OnItem && m_dropOnIndex.isValid()) {
                    QStandardItem* targetItem = sim->itemFromIndex(QModelIndex(m_dropOnIndex));
                    if (srcParentItem && targetItem) {
                        auto row = srcParentItem->takeRow(srcRow);
                        targetItem->appendRow(row);
                        QModelIndex dropParent(m_dropOnIndex);
                        expand(dropParent);
                        QModelIndex newIdx = sim->indexFromItem(row.first());
                        setCurrentIndex(newIdx);
                        QPointer<TreeView> guard(this);
                        emit itemReordered(srcParentIdx, srcRow, dropParent, targetItem->rowCount() - 1);
                        if (!guard)
                            return;
                    }
                } else if (m_dropMode == DropMode::Between) {
                    QStandardItem* dstParentItem = m_dropTargetParent.isValid()
                        ? sim->itemFromIndex(QModelIndex(m_dropTargetParent))
                        : sim->invisibleRootItem();
                    if (srcParentItem && dstParentItem) {
                        bool sameParent = (srcParentItem == dstParentItem);
                        int dstRow = m_dropTargetRow;

                        auto row = srcParentItem->takeRow(srcRow);
                        if (sameParent && srcRow < dstRow)
                            dstRow--;
                        dstParentItem->insertRow(dstRow, row);
                        QModelIndex newIdx = sim->indexFromItem(row.first());
                        setCurrentIndex(newIdx);
                        QPointer<TreeView> guard(this);
                        emit itemReordered(srcParentIdx, srcRow, QModelIndex(m_dropTargetParent), dstRow);
                        if (!guard)
                            return;
                    }
                }
            }
        }

        m_isDragging = false;
        m_dragSourceIndex = QPersistentModelIndex();
        m_dropMode = DropMode::None;
        m_dropTargetParent = QPersistentModelIndex();
        m_dropTargetRow = -1;
        m_dropOnIndex = QPersistentModelIndex();
        m_dragPixmap = QPixmap();
        viewport()->update();
        event->accept();
        return;
    }

    if (m_canReorderItems) {
        m_dragSourceIndex = QPersistentModelIndex();
    }
    QTreeView::mouseReleaseEvent(event);
}

// ── Drag reorder: helpers ────────────────────────────────────────────────────

QPixmap TreeView::renderRowPixmap(const QModelIndex& index) const
{
    if (!index.isValid() || !model())
        return {};

    QRect rect = QTreeView::visualRect(index);
    if (rect.isEmpty())
        return {};

    // Render the row content area (respecting indentation)
    const int contentW = rect.width(); // width after indent
    const int h = rect.height();
    const qreal dpr = devicePixelRatioF();

    QPixmap pix(QSize(contentW, h) * dpr);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);

    FluentRasterCachePainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QStyleOptionViewItem opt;
    FLUENT_INIT_VIEW_ITEM_OPTION(&opt);
    opt.rect = QRect(0, 0, contentW, h);
    opt.state |= QStyle::State_Selected | QStyle::State_Enabled;
    opt.state &= ~QStyle::State_MouseOver;
    if (itemDelegate())
        itemDelegate()->paint(&p, opt, index);
    p.end();

    // Scale to 75% for a compact floating thumbnail
    constexpr qreal kScale = 0.75;
    const int sw = qRound(contentW * kScale);
    const int sh = qRound(h * kScale);
    QPixmap scaled = pix.scaled(QSize(sw, sh) * dpr, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(dpr);
    return scaled;
}

void TreeView::computeDropTarget(const QPoint& pos)
{
    m_dropMode = DropMode::None;
    m_dropTargetParent = QPersistentModelIndex();
    m_dropTargetRow = -1;
    m_dropOnIndex = QPersistentModelIndex();

    if (!model())
        return;

    QModelIndex idx = indexAt(pos);

    if (!idx.isValid()) {
        // Below all items → insert at end of root
        m_dropMode = DropMode::Between;
        m_dropTargetRow = model()->rowCount();
        return;
    }

    // Can't drop on self or on a descendant of the source (cycle)
    if (idx == m_dragSourceIndex)
        return;
    if (isDescendantOf(idx, m_dragSourceIndex))
        return;

    QRect rect = visualRect(idx);
    int rowH = rect.height();
    if (rowH <= 0)
        return;
    int relY = pos.y() - rect.top();

    bool hasChildren = model()->rowCount(idx) > 0;

    if (hasChildren) {
        // Three zones: top 25% / middle 50% / bottom 25%
        if (relY < rowH / 4) {
            m_dropMode = DropMode::Between;
            m_dropTargetParent = QPersistentModelIndex(idx.parent());
            m_dropTargetRow = idx.row();
        } else if (relY > rowH * 3 / 4) {
            m_dropMode = DropMode::Between;
            m_dropTargetParent = QPersistentModelIndex(idx.parent());
            m_dropTargetRow = idx.row() + 1;
        } else {
            m_dropMode = DropMode::OnItem;
            m_dropOnIndex = QPersistentModelIndex(idx);
        }
    } else {
        // Three zones for leaf items too: top 25% / middle 50% / bottom 25%
        // Middle zone = drop onto item (create new nesting)
        if (relY < rowH / 4) {
            m_dropMode = DropMode::Between;
            m_dropTargetParent = QPersistentModelIndex(idx.parent());
            m_dropTargetRow = idx.row();
        } else if (relY > rowH * 3 / 4) {
            m_dropMode = DropMode::Between;
            m_dropTargetParent = QPersistentModelIndex(idx.parent());
            m_dropTargetRow = idx.row() + 1;
        } else {
            m_dropMode = DropMode::OnItem;
            m_dropOnIndex = QPersistentModelIndex(idx);
        }
    }
}

bool TreeView::isDescendantOf(const QModelIndex& candidate, const QModelIndex& ancestor) const
{
    QModelIndex walk = candidate.parent();
    while (walk.isValid()) {
        if (walk == ancestor)
            return true;
        walk = walk.parent();
    }
    return false;
}

void TreeView::connectIndicatorMotionModel(QAbstractItemModel* model)
{
    if (!model)
        return;

    m_indicatorModelAboutToResetConnection = connect(model, &QAbstractItemModel::modelAboutToBeReset, this,
        &TreeView::clearIndicatorMotionState);
    m_indicatorModelResetConnection = connect(model, &QAbstractItemModel::modelReset, this,
        &TreeView::clearIndicatorMotionState);
    m_indicatorRowsAboutToBeRemovedConnection = connect(model, &QAbstractItemModel::rowsAboutToBeRemoved, this,
        [this](const QModelIndex&, int, int) { clearIndicatorMotionState(); });
}

void TreeView::disconnectIndicatorMotionModel()
{
    QObject::disconnect(m_indicatorModelAboutToResetConnection);
    QObject::disconnect(m_indicatorModelResetConnection);
    QObject::disconnect(m_indicatorRowsAboutToBeRemovedConnection);
    m_indicatorModelAboutToResetConnection = QMetaObject::Connection();
    m_indicatorModelResetConnection = QMetaObject::Connection();
    m_indicatorRowsAboutToBeRemovedConnection = QMetaObject::Connection();
}

void TreeView::clearIndicatorMotionState()
{
    if (m_indicatorMotionAnim)
        m_indicatorMotionAnim->stop();
    m_previousIndicatorIndex = QPersistentModelIndex();
    m_currentIndicatorIndex = QPersistentModelIndex();
    m_activeIndicatorIndex = QPersistentModelIndex();
    setIndicatorMotionDirection(IndicatorVerticalDirection::None);
    setIndicatorHierarchyTransition(IndicatorHierarchyTransition::None);
    setIndicatorMotionProgress(1.0);
    if (viewport())
        viewport()->update();
}

void TreeView::updateIndicatorMotionForIndex(const QModelIndex& current, const QModelIndex& previous)
{
    if (!current.isValid() || !isIndicatorEndpointUsable(current)) {
        clearIndicatorMotionState();
        return;
    }

    if (m_currentIndicatorIndex.isValid() && current == m_currentIndicatorIndex)
        return;

    QPersistentModelIndex previousIndex = m_currentIndicatorIndex;
    if (!previousIndex.isValid() && previous.isValid())
        previousIndex = QPersistentModelIndex(previous);

    const bool hasUsablePrevious = previousIndex.isValid() && isIndicatorEndpointUsable(previousIndex);
    const auto direction = hasUsablePrevious
        ? classifyIndicatorVerticalDirection(previousIndex, current)
        : IndicatorVerticalDirection::None;
    const auto hierarchyTransition = hasUsablePrevious
        ? classifyIndicatorHierarchyTransition(previousIndex, current)
        : IndicatorHierarchyTransition::None;

    m_previousIndicatorIndex = hasUsablePrevious ? previousIndex : QPersistentModelIndex();
    m_currentIndicatorIndex = QPersistentModelIndex(current);
    m_activeIndicatorIndex = QPersistentModelIndex(current);
    setIndicatorMotionDirection(direction);
    setIndicatorHierarchyTransition(hierarchyTransition);
    startIndicatorMotionAnimation(hasUsablePrevious && m_indicatorMotionAnimationEnabled);
}

void TreeView::startIndicatorMotionAnimation(bool animated)
{
    if (!m_indicatorMotionAnim)
        return;

    m_indicatorMotionAnim->stop();
    if (!animated) {
        setIndicatorMotionProgress(1.0);
        if (viewport())
            viewport()->update();
        return;
    }

    setIndicatorMotionProgress(0.0);
    m_indicatorMotionAnim->setStartValue(0.0);
    m_indicatorMotionAnim->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_indicatorMotionAnim, themeAnimation().normal,
        m_indicatorMotionAnimationEnabled);
}

bool TreeView::isIndicatorEndpointUsable(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != model())
        return false;

    QModelIndex walk = index;
    while (walk.isValid()) {
        if (isRowHidden(walk.row(), walk.parent()))
            return false;
        const QModelIndex parent = walk.parent();
        if (parent.isValid() && !isExpanded(parent))
            return false;
        walk = parent;
    }
    return true;
}

int TreeView::indicatorHierarchyDepth(const QModelIndex& index) const
{
    int depth = 0;
    QModelIndex walk = index.parent();
    while (walk.isValid()) {
        ++depth;
        walk = walk.parent();
    }
    return depth;
}

TreeView::IndicatorVerticalDirection TreeView::classifyIndicatorVerticalDirection(const QModelIndex& previous,
    const QModelIndex& current) const
{
    if (!previous.isValid() || !current.isValid() || previous == current)
        return IndicatorVerticalDirection::None;

    const QRect previousRect = visualRect(previous);
    const QRect currentRect = visualRect(current);
    if (!previousRect.isEmpty() && !currentRect.isEmpty() && previousRect.center().y() != currentRect.center().y())
        return currentRect.center().y() > previousRect.center().y()
            ? IndicatorVerticalDirection::Down
            : IndicatorVerticalDirection::Up;

    constexpr int kMaxVisibleWalk = 10000;
    QModelIndex walk = indexBelow(previous);
    for (int i = 0; i < kMaxVisibleWalk && walk.isValid(); ++i) {
        if (walk == current)
            return IndicatorVerticalDirection::Down;
        walk = indexBelow(walk);
    }

    walk = indexAbove(previous);
    for (int i = 0; i < kMaxVisibleWalk && walk.isValid(); ++i) {
        if (walk == current)
            return IndicatorVerticalDirection::Up;
        walk = indexAbove(walk);
    }

    return IndicatorVerticalDirection::None;
}

TreeView::IndicatorHierarchyTransition TreeView::classifyIndicatorHierarchyTransition(const QModelIndex& previous,
    const QModelIndex& current) const
{
    if (!previous.isValid() || !current.isValid() || previous == current)
        return IndicatorHierarchyTransition::None;

    const int previousDepth = indicatorHierarchyDepth(previous);
    const int currentDepth = indicatorHierarchyDepth(current);
    if (currentDepth > previousDepth)
        return IndicatorHierarchyTransition::Inward;
    if (currentDepth < previousDepth)
        return IndicatorHierarchyTransition::Outward;
    return IndicatorHierarchyTransition::SameLevel;
}

void TreeView::setIndicatorMotionProgress(qreal progress)
{
    const qreal clamped = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(m_indicatorMotionProgress + 1.0, clamped + 1.0))
        return;
    m_indicatorMotionProgress = clamped;
    emit indicatorMotionProgressChanged();
}

void TreeView::setIndicatorMotionDirection(IndicatorVerticalDirection direction)
{
    if (m_indicatorMotionDirection == direction)
        return;
    m_indicatorMotionDirection = direction;
    emit indicatorMotionDirectionChanged();
}

void TreeView::setIndicatorHierarchyTransition(IndicatorHierarchyTransition transition)
{
    if (m_indicatorHierarchyTransition == transition)
        return;
    m_indicatorHierarchyTransition = transition;
    emit indicatorHierarchyTransitionChanged();
}

void TreeView::syncCheckStatesWithSelection(const QItemSelection& selected, const QItemSelection& deselected)
{
    if (m_syncingCheckStateWithSelection)
        return;
    if (m_selectionMode != SelectionMode::Multiple && m_selectionMode != SelectionMode::Extended)
        return;
    if (!model() || !selectionModel())
        return;

    m_syncingCheckStateWithSelection = true;
    for (const QModelIndex& index : deselected.indexes()) {
        if (!shouldSyncCheckStateWithSelection(index))
            continue;
        applyCheckStateToSubtree(index, Qt::Unchecked);
        updateAncestorCheckStates(index);
    }
    for (const QModelIndex& index : selected.indexes()) {
        if (!shouldSyncCheckStateWithSelection(index))
            continue;
        applyCheckStateToSubtree(index, Qt::Checked);
        updateAncestorCheckStates(index);
    }
    m_syncingCheckStateWithSelection = false;

    if (viewport())
        viewport()->update();
}

bool TreeView::shouldSyncCheckStateWithSelection(const QModelIndex& index) const
{
    if (m_selectionMode != SelectionMode::Multiple && m_selectionMode != SelectionMode::Extended)
        return false;
    if (!index.isValid() || !model() || index.model() != model() || index.column() != 0)
        return false;
    if (!(model()->flags(index) & Qt::ItemIsUserCheckable))
        return false;
    return index.data(Qt::CheckStateRole).isValid();
}

void TreeView::applyCheckStateToSubtree(const QModelIndex& index, Qt::CheckState state)
{
    if (!shouldSyncCheckStateWithSelection(index))
        return;

    setCheckStateAndSelection(index, state);
    const int rows = model()->rowCount(index);
    for (int row = 0; row < rows; ++row)
        applyCheckStateToSubtree(model()->index(row, 0, index), state);
}

void TreeView::updateAncestorCheckStates(const QModelIndex& index)
{
    QModelIndex parent = index.parent();
    while (parent.isValid()) {
        if (shouldSyncCheckStateWithSelection(parent))
            setCheckStateAndSelection(parent, aggregateChildCheckState(parent));
        parent = parent.parent();
    }
}

Qt::CheckState TreeView::aggregateChildCheckState(const QModelIndex& parent) const
{
    int checkableChildren = 0;
    int checkedChildren = 0;
    int uncheckedChildren = 0;

    const int rows = model() ? model()->rowCount(parent) : 0;
    for (int row = 0; row < rows; ++row) {
        const QModelIndex child = model()->index(row, 0, parent);
        if (!shouldSyncCheckStateWithSelection(child))
            continue;

        ++checkableChildren;
        const auto state = static_cast<Qt::CheckState>(child.data(Qt::CheckStateRole).toInt());
        if (state == Qt::Checked)
            ++checkedChildren;
        else if (state == Qt::Unchecked)
            ++uncheckedChildren;
    }

    if (checkableChildren > 0 && checkedChildren == checkableChildren)
        return Qt::Checked;
    if (checkableChildren > 0 && uncheckedChildren == checkableChildren)
        return Qt::Unchecked;
    return Qt::PartiallyChecked;
}

void TreeView::setCheckStateAndSelection(const QModelIndex& index, Qt::CheckState state)
{
    if (!shouldSyncCheckStateWithSelection(index))
        return;

    if (static_cast<Qt::CheckState>(index.data(Qt::CheckStateRole).toInt()) != state)
        model()->setData(index, state, Qt::CheckStateRole);

    const QItemSelectionModel::SelectionFlags flags = (state == Qt::Checked)
        ? (QItemSelectionModel::Select | QItemSelectionModel::Rows)
        : (QItemSelectionModel::Deselect | QItemSelectionModel::Rows);
    selectionModel()->select(index, flags);
    if (state == Qt::Checked)
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
}

qreal TreeView::computeSubtreeHeight(const QModelIndex& parent) const
{
    if (!parent.isValid() || !isExpanded(parent))
        return 0.0;
    qreal height = 0.0;
    QModelIndex walk = indexBelow(parent);
    while (walk.isValid() && isDescendantOf(walk, parent)) {
        const QRect rowRect = visualRect(walk);
        height += fluentItemViewRowHeight(this, walk, rowRect);
        walk = indexBelow(walk);
    }
    return height;
}

void TreeView::startExpandReveal(const QModelIndex& parent)
{
    if (!m_expandRevealAnim || !parent.isValid())
        return;
    if (!isVisible()) {
        clearExpandRevealState(); // not on screen → no reveal animation
        return;
    }

    const qreal resume =
        (m_animParent.isValid() && QModelIndex(m_animParent) == parent
            && m_expandRevealAnim->state() == QAbstractAnimation::Running)
        ? qBound(0.0, m_expandRevealAnim->currentValue().toReal(), 1.0)
        : 0.0;

    m_expandRevealAnim->stop();
    m_animExpanding = true;
    m_pendingCollapseFinalize = false;
    m_animParent = QPersistentModelIndex(parent);
    m_animSubtreeHeight = computeSubtreeHeight(parent);
    if (m_animSubtreeHeight <= 0.0) {
        clearExpandRevealState();
        return;
    }
    const int duration = qMax(1, qRound(kExpandRevealDuration * (1.0 - resume)));
    m_expandRevealAnim->setStartValue(resume);
    m_expandRevealAnim->setEndValue(1.0);
    ::fluent::detail::startMotionTransition(m_expandRevealAnim, duration, m_animEnabled);
    if (viewport())
        viewport()->update();
}

void TreeView::startCollapseReveal(const QModelIndex& parent)
{
    if (!m_expandRevealAnim || !parent.isValid()) {
        if (parent.isValid())
            setExpanded(parent, false);
        return;
    }

    const bool resuming = m_animParent.isValid() && QModelIndex(m_animParent) == parent
        && m_expandRevealAnim->state() == QAbstractAnimation::Running;
    const qreal resume = resuming ? qBound(0.0, m_expandRevealAnim->currentValue().toReal(), 1.0) : 1.0;

    m_expandRevealAnim->stop();
    m_animExpanding = false;
    m_pendingCollapseFinalize = true;
    m_animParent = QPersistentModelIndex(parent);
    if (!resuming)
        m_animSubtreeHeight = computeSubtreeHeight(parent);
    if (m_animSubtreeHeight <= 0.0) {
        m_pendingCollapseFinalize = false;
        clearExpandRevealState();
        setExpanded(parent, false);
        return;
    }
    const int duration = qMax(1, qRound(kExpandRevealDuration * resume));
    m_expandRevealAnim->setStartValue(resume);
    m_expandRevealAnim->setEndValue(0.0);
    ::fluent::detail::startMotionTransition(m_expandRevealAnim, duration, m_animEnabled);
    if (viewport())
        viewport()->update();
}

void TreeView::finalizeDeferredCollapse()
{
    const QModelIndex parent = QModelIndex(m_animParent);
    m_pendingCollapseFinalize = false;
    m_animParent = QPersistentModelIndex();
    m_animExpanding = true;
    m_animSubtreeHeight = 0.0;
    if (parent.isValid())
        setExpanded(parent, false); // real collapse → fires collapsed()
    if (viewport())
        viewport()->update();
}

void TreeView::clearExpandRevealState()
{
    if (m_expandRevealAnim)
        m_expandRevealAnim->stop();
    m_animParent = QPersistentModelIndex();
    m_animExpanding = true;
    m_animSubtreeHeight = 0.0;
    m_pendingCollapseFinalize = false;
    if (viewport())
        viewport()->update();
}

void TreeView::completeActiveExpandReveal()
{
    if (!m_expandRevealAnim || m_expandRevealAnim->state() != QAbstractAnimation::Running || !m_animParent.isValid())
        return;

    const QModelIndex parent = QModelIndex(m_animParent);
    const bool shouldCollapse = !m_animExpanding || m_pendingCollapseFinalize;
    m_expandRevealAnim->stop();
    if (shouldCollapse) {
        finalizeDeferredCollapse();
        return;
    }
    clearExpandRevealState();
    if (parent.isValid() && viewport())
        viewport()->update(visualRect(parent));
}

// ── Selected indicator pill ────────────────────────────────────────────────────

QRectF TreeView::selectedIndicatorBaseRect(const QModelIndex& index) const
{
    if (!index.isValid() || index.model() != model() || !viewport())
        return {};

    const QRect itemRect = visualRect(index);
    if (itemRect.isEmpty() || !viewport()->rect().intersects(itemRect))
        return {};

    const QRectF itemRectF(itemRect);
    const qreal x = itemRect.left() + indicatorInsetForIndex(index, m_selectionIndicatorStyle);
    const qreal y = itemRectF.top() + itemRectF.height() / 2.0 - m_selectionIndicatorStyle.height / 2.0;
    return QRectF(x, y, m_selectionIndicatorStyle.width, m_selectionIndicatorStyle.height);
}

QRectF TreeView::currentSelectedIndicatorRect() const
{
    return selectedIndicatorRect();
}

void TreeView::paintSelectedIndicator(QPainter& painter) const
{

    if (!selectionModel() || !themeColorsRef().accentDefault.isValid())
        return;

    const QRectF rect = currentSelectedIndicatorRect();
    if (rect.isEmpty())
        return;

    const qreal radius = rect.width() / 2.0;
    painter.setPen(Qt::NoPen);
    painter.setBrush(themeColorsRef().accentDefault);
    painter.drawRoundedRect(rect, radius, radius);
}

QRect TreeView::indicatorMotionDirtyRect() const
{
    if (!viewport())
        return {};

    // Transparent views recompose the full viewport so parent surfaces don't retain accent trails;
    // opaque views only need the vertical band spanning the previous and current rows.
    if (!m_backgroundVisible)
        return viewport()->rect();

    QRect band;
    if (m_currentIndicatorIndex.isValid())
        band = visualRect(QModelIndex(m_currentIndicatorIndex));
    if (m_previousIndicatorIndex.isValid())
        band = band.united(visualRect(QModelIndex(m_previousIndicatorIndex)));
    if (band.isNull())
        return viewport()->rect();

    band.setLeft(0);
    band.setRight(viewport()->width());
    return band.adjusted(0, -2, 0, 2);
}

void TreeView::setViewportHovered(bool hovered)
{
    if (m_viewportHovered == hovered)
        return;
    m_viewportHovered = hovered;
    emit viewportHoveredChanged();
}

// ── Theme ─────────────────────────────────────────────────────────────────────

void TreeView::onThemeUpdated()
{
    applyThemeStyle();
}

void TreeView::applyThemeStyle(bool refreshFonts)
{
    const auto& c = themeColorsRef();

    QPalette pal = palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Window, Qt::transparent);
    pal.setColor(QPalette::Active, QPalette::Text, c.textPrimary);
    pal.setColor(QPalette::Inactive, QPalette::Text, c.textPrimary);
    pal.setColor(QPalette::Disabled, QPalette::Text, c.textDisabled);
    pal.setColor(QPalette::Highlight, Qt::transparent);
    pal.setColor(QPalette::Active, QPalette::HighlightedText, c.textPrimary);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, c.textPrimary);
    pal.setColor(QPalette::Disabled, QPalette::HighlightedText, c.textDisabled);
    setPalette(pal);

    if (refreshFonts)
        setFont(themeFont(m_fontRole).toQFont());

    if (viewport()) {
        viewport()->setAutoFillBackground(false);
        QPalette vpal = viewport()->palette();
        vpal.setColor(QPalette::Base, Qt::transparent);
        vpal.setColor(QPalette::Window, Qt::transparent);
        viewport()->setPalette(vpal);
    }

    if (m_headerLabel) {
        const QColor headerColor = isEnabled() ? c.textPrimary : c.textDisabled;
        if (refreshFonts)
            m_headerLabel->setFont(themeFont(Typography::FontRole::Subtitle).toQFont());
        // Style via the label's own stylesheet so ancestor QStyleSheetStyle rules won't override QPalette::WindowText.
        m_headerLabel->setStyleSheet(QStringLiteral("color: rgba(%1, %2, %3, %4); background: transparent;")
                .arg(headerColor.red())
                .arg(headerColor.green())
                .arg(headerColor.blue())
                .arg(headerColor.alpha()));
    }

    updateViewportMargins();
    layoutHeader();
    update();
}

// ── Internal layout helpers ───────────────────────────────────────────────────

void TreeView::layoutHeader()
{
    if (!m_headerLabel || m_headerLabel->isHidden())
        return;

    const int headerH = m_headerLabel->sizeHint().height() + ::Spacing::Gap::Normal;
    m_headerLabel->setGeometry(0, 0, width(), headerH);
    m_headerLabel->raise();
}

void TreeView::updateViewportMargins()
{
    int top = 2; // Top margin aligns first row spacing with ListView alongside the delegate inset.
    if (m_headerLabel && !m_headerLabel->isHidden()) {
        top = m_headerLabel->sizeHint().height() + ::Spacing::Gap::Normal;
    }
    setViewportMargins(0, top, 0, 0);
}

void TreeView::syncFluentScrollBar()
{
    ::fluent::scrolling::suppressNativeScrollBars(verticalScrollBar(), horizontalScrollBar());
    if (!m_vScrollBar)
        return;
    if (!::fluent::scrolling::mirrorNativeScrollBar(m_vScrollBar, verticalScrollBar()))
        return;

    constexpr int kScrollBarRightInset = ::Spacing::XSmall / 2;
    constexpr int kScrollBarVerticalInset = ::Spacing::XSmall / 2;
    const QRect r = rect();
    const int top = (m_headerLabel && !m_headerLabel->isHidden())
        ? m_headerLabel->geometry().bottom() + 1 + kScrollBarVerticalInset
        : r.top() + kScrollBarVerticalInset;
    ::fluent::scrolling::placeVerticalScrollBar(m_vScrollBar, r, top, kScrollBarRightInset,
        kScrollBarVerticalInset - 1);
}

void TreeView::syncFluentHScrollBar()
{
    ::fluent::scrolling::suppressNativeScrollBars(verticalScrollBar(), horizontalScrollBar());
    if (!m_hScrollBar)
        return;

    if (!m_horizontalFluentScrollBarEnabled) {
        m_hScrollBar->hide();
        return;
    }

    if (!::fluent::scrolling::mirrorNativeScrollBar(m_hScrollBar, horizontalScrollBar()))
        return;

    constexpr int kScrollBarHorizontalInset = 4;
    constexpr int kScrollBarBottomInset = 4;
    ::fluent::scrolling::placeHorizontalScrollBar(m_hScrollBar, rect(), kScrollBarHorizontalInset,
        kScrollBarHorizontalInset, kScrollBarBottomInset);
}

void TreeView::refreshFluentScrollChrome()
{
    syncFluentScrollBar();
    syncFluentHScrollBar();
}

} // namespace fluent::collections
