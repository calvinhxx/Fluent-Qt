#include "FlowView.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QAbstractItemDelegate>
#include <QAbstractAnimation>
#include <QAbstractItemModel>
#include <QApplication>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QStyleOptionViewItem>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "compatibility/QtCompat.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/foundation/private/DpiPaintMetrics_p.h"
#include "components/scrolling/OverlayScrollChrome.h"
#include "components/scrolling/OverscrollController.h"
#include "components/scrolling/ScrollBar.h"

namespace fluent::collections {

namespace {

// Pixels scrolled per wheel notch (delta 120), shared with the other collection views so the
// wheel feel matches ListView. zh_CN: 每个滚轮刻度（delta 120）滚动的像素数，与 ListView 统一手感。
constexpr qreal kDiscreteWheelStepPx = ::Spacing::ControlHeight::Large;

bool pointsEqual(const QPointF& lhs, const QPointF& rhs)
{
    return std::abs(lhs.x() - rhs.x()) < 0.01 && std::abs(lhs.y() - rhs.y()) < 0.01;
}

int validDimension(int value, int fallback)
{
    return value > 0 ? value : fallback;
}

} // namespace

FlowView::FlowView(QWidget* parent)
    : QAbstractItemView(parent)
{
    m_fontRole = Typography::FontRole::Body;

    setObjectName(QStringLiteral("FlowView"));
    setFrameStyle(QFrame::NoFrame);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    QAbstractItemView::setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setDragEnabled(false);
    setDefaultDropAction(Qt::IgnoreAction);

    m_headerLabel = new QLabel(this);
    m_headerLabel->hide();
    m_headerLabel->setIndent(::Spacing::Padding::ListItemHorizontal);

    m_vScrollBar = ::fluent::scrolling::createOverlayScrollBar(
        Qt::Vertical, this, verticalScrollBar(),
        QStringLiteral("fluentFlowViewScrollBar"));
    connect(verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &FlowView::syncFluentScrollBar);

    // --- Overscroll bounce (shared controller) ---
    fluent::scrolling::OverscrollController::Hooks hooks;
    hooks.scrollBar = [this] { return verticalScrollBar(); };
    hooks.normalScroll = [this](qreal scrollPx) {
        QScrollBar* bar = verticalScrollBar();
        const int previous = bar->value();
        const int target = qBound(bar->minimum(), previous - qRound(scrollPx), bar->maximum());
        if (target == previous)
            return false;
        bar->setValue(target);
        syncFluentScrollBar();
        viewport()->update();
        return true;
    };
    hooks.onOverscrollChanged = [this] { viewport()->update(); };
    // No fallbackWheel: a flow with no overflow ignores the wheel so an enclosing scroller takes it.
    // zh_CN: 无 fallbackWheel：内容未超出时忽略滚轮，让外层滚动容器接管。
    m_overscroll = new fluent::scrolling::OverscrollController(
        viewport(), kDiscreteWheelStepPx, std::move(hooks), this);

    applyThemeStyle();
    updateViewportMargins();
    syncFluentScrollBar();
}

FlowView::~FlowView()
{
    clearDragAnimations();
}

void FlowView::setSelectionMode(SelectionMode mode)
{
    if (m_selectionMode == mode)
        return;
    m_selectionMode = mode;

    switch (mode) {
    case SelectionMode::None:
        QAbstractItemView::setSelectionMode(QAbstractItemView::NoSelection);
        break;
    case SelectionMode::Single:
        QAbstractItemView::setSelectionMode(QAbstractItemView::SingleSelection);
        break;
    case SelectionMode::Multiple:
        QAbstractItemView::setSelectionMode(QAbstractItemView::MultiSelection);
        break;
    case SelectionMode::Extended:
        QAbstractItemView::setSelectionMode(QAbstractItemView::ExtendedSelection);
        break;
    }

    emit selectionModeChanged();
}

void FlowView::setFontRole(Typography::FontRole role)
{
    if (m_fontRole == role)
        return;
    m_fontRole = role;
    applyThemeStyle();
    invalidateFlowLayout();
    emit fontRoleChanged();
}

void FlowView::setBorderVisible(bool visible)
{
    if (m_borderVisible == visible)
        return;
    m_borderVisible = visible;
    viewport()->update();
    emit borderVisibleChanged();
}

void FlowView::setHeaderText(const QString& text)
{
    if (m_headerText == text)
        return;
    const QString previousHeader = m_headerText;
    m_headerText = text;
    if (m_headerLabel) {
        m_headerLabel->setText(text);
        m_headerLabel->setVisible(!text.isEmpty());
    }
    if (accessibleName().isEmpty() || accessibleName() == previousHeader || accessibleName() == m_autoAccessibleName) {
        m_autoAccessibleName = m_headerText;
        refreshAccessibleName();
    }
    updateViewportMargins();
    layoutHeader();
    invalidateFlowLayout();
    emit headerTextChanged();
}

void FlowView::setPlaceholderText(const QString& text)
{
    if (m_placeholderText == text)
        return;
    m_placeholderText = text;
    viewport()->update();
    emit placeholderTextChanged();
}

void FlowView::setDefaultItemSize(const QSize& size)
{
    const QSize next(validDimension(size.width(), 120), validDimension(size.height(), 64));
    if (m_defaultItemSize == next)
        return;
    m_defaultItemSize = next;
    invalidateFlowLayout();
    emit defaultItemSizeChanged();
}

void FlowView::setMinimumItemSize(const QSize& size)
{
    const QSize next(validDimension(size.width(), 1), validDimension(size.height(), 1));
    if (m_minimumItemSize == next)
        return;
    m_minimumItemSize = next;
    invalidateFlowLayout();
    emit minimumItemSizeChanged();
}

void FlowView::setMaximumItemSize(const QSize& size)
{
    if (m_maximumItemSize == size)
        return;
    m_maximumItemSize = size;
    invalidateFlowLayout();
    emit maximumItemSizeChanged();
}

void FlowView::setItemSizeRole(int role)
{
    if (m_itemSizeRole == role)
        return;
    m_itemSizeRole = role;
    invalidateFlowLayout();
    emit itemSizeRoleChanged();
}

void FlowView::setHorizontalSpacing(int spacing)
{
    const int next = qMax(0, spacing);
    if (m_hSpacing == next)
        return;
    m_hSpacing = next;
    invalidateFlowLayout();
    emit horizontalSpacingChanged();
}

void FlowView::setVerticalSpacing(int spacing)
{
    const int next = qMax(0, spacing);
    if (m_vSpacing == next)
        return;
    m_vSpacing = next;
    invalidateFlowLayout();
    emit verticalSpacingChanged();
}

void FlowView::setContentMargins(const QMargins& margins)
{
    const QMargins next(qMax(0, margins.left()), qMax(0, margins.top()),
                        qMax(0, margins.right()), qMax(0, margins.bottom()));
    if (m_contentMargins == next)
        return;
    m_contentMargins = next;
    invalidateFlowLayout();
    emit contentMarginsChanged();
}

void FlowView::setCanReorderItems(bool enabled)
{
    if (m_canReorderItems == enabled)
        return;
    m_canReorderItems = enabled;
    if (!m_canReorderItems) {
        m_isDragging = false;
        m_dragSourceIndex = -1;
        m_dragSourceIndices.clear();
        m_dragPressIntercepted = false;
        resetDragReorderFeedback();
    }
    emit canReorderItemsChanged();
}

bool FlowView::isScrollChainingEnabled() const { return m_overscroll->isScrollChainingEnabled(); }

void FlowView::setScrollChainingEnabled(bool enabled)
{
    if (m_overscroll->isScrollChainingEnabled() == enabled)
        return;
    m_overscroll->setScrollChainingEnabled(enabled);
    emit scrollChainingEnabledChanged();
}

bool FlowView::isOverscrollEnabled() const { return m_overscroll->isOverscrollEnabled(); }

void FlowView::setOverscrollEnabled(bool enabled)
{
    if (m_overscroll->isOverscrollEnabled() == enabled)
        return;
    m_overscroll->setOverscrollEnabled(enabled);
    emit overscrollEnabledChanged();
}

int FlowView::selectedIndex() const
{
    if (!selectionModel())
        return -1;
    const auto indexes = selectionModel()->selectedIndexes();
    return indexes.isEmpty() ? -1 : indexes.first().row();
}

QList<int> FlowView::selectedRows() const
{
    QList<int> rows;
    if (!selectionModel())
        return rows;
    QSet<int> seen;
    for (const QModelIndex& index : selectionModel()->selectedIndexes())
        seen.insert(index.row());
    rows = QList<int>(seen.begin(), seen.end());
    std::sort(rows.begin(), rows.end());
    return rows;
}

void FlowView::setSelectedIndex(int index)
{
    if (!model() || index < 0 || index >= modelRowCount()) {
        clearSelection();
        return;
    }
    setCurrentIndex(indexForRow(index));
}

::fluent::scrolling::ScrollBar* FlowView::verticalFluentScrollBar() const
{
    return m_vScrollBar;
}

void FlowView::refreshFluentScrollChrome()
{
    syncFluentScrollBar();
}

void FlowView::setModel(QAbstractItemModel* newModel)
{
    clearModelConnections();
    QAbstractItemView::setModel(newModel);
    connectModelSignals(newModel);
    invalidateFlowLayout();
}

void FlowView::setItemDelegate(QAbstractItemDelegate* delegate)
{
    QAbstractItemView::setItemDelegate(delegate);
    invalidateFlowLayout();
}

QRect FlowView::visualRect(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= modelRowCount())
        return QRect();
    ensureLayout();
    QRect rect = contentToViewport(m_itemRects.value(index.row()));
    if (m_paintingWithOffsets) {
        const QPointF offset = m_dragOffsets.value(index.row(), QPointF(0.0, 0.0));
        rect.translate(qRound(offset.x()), qRound(offset.y()));
    }
    return rect;
}

void FlowView::scrollTo(const QModelIndex& index, ScrollHint hint)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= modelRowCount())
        return;
    ensureLayout();
    const QRect rect = m_itemRects.at(index.row());
    QScrollBar* bar = verticalScrollBar();
    int value = bar->value();
    const int top = rect.top();
    const int bottom = rect.bottom();
    const int viewportH = viewport()->height();

    switch (hint) {
    case PositionAtTop:
        value = top;
        break;
    case PositionAtBottom:
        value = bottom - viewportH + 1;
        break;
    case PositionAtCenter:
        value = rect.center().y() - viewportH / 2;
        break;
    case EnsureVisible:
    default:
        if (top < bar->value())
            value = top;
        else if (bottom >= bar->value() + viewportH)
            value = bottom - viewportH + 1;
        break;
    }

    bar->setValue(qBound(bar->minimum(), value, bar->maximum()));
}

QModelIndex FlowView::indexAt(const QPoint& point) const
{
    const int row = rowAt(point);
    return row >= 0 ? indexForRow(row) : QModelIndex();
}

void FlowView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    ensureLayout();

    const auto& colors = themeColorsRef();
    const int radius = CornerRadius::Control;

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(viewport()->rect(), colors.bgLayer);

    const bool isEmpty = modelRowCount() == 0;
    if (isEmpty && !m_placeholderText.isEmpty()) {
        painter.setPen(colors.textTertiary);
        painter.setFont(themeFont(m_fontRole).toQFont());
        painter.drawText(viewport()->rect(), Qt::AlignCenter, m_placeholderText);
    }

    const QRect visibleContent(QPoint(0, verticalOffset()), viewport()->size());
    if (itemDelegate() && model()) {
        m_paintingWithOffsets = !m_dragOffsets.isEmpty();
        const int firstBand = firstLayoutBandIntersectingY(visibleContent.top());
        for (int bandIndex = firstBand; bandIndex < m_layoutBands.size(); ++bandIndex) {
            const LayoutBand& band = m_layoutBands.at(bandIndex);
            if (band.top > visibleContent.bottom())
                break;
            for (int row = band.firstRow; row < band.pastLastRow; ++row) {
                if (!m_itemRects.at(row).intersects(visibleContent))
                    continue;
                if (m_isDragging && m_dragSourceIndices.contains(row))
                    continue;
                const QModelIndex index = indexForRow(row);
                const QRect rect = visualRect(index);
                QStyleOptionViewItem option = optionForIndex(index, rect);
                itemDelegate()->paint(&painter, option, index);
            }
        }
        m_paintingWithOffsets = false;
    }

    if (m_isDragging && m_dropTargetIndex >= 0) {
        const QRect indicatorRect = contentToViewport(
            dropIndicatorRectForSlot(m_dropTargetIndex));
        if (!indicatorRect.isEmpty()) {
            const int x = indicatorRect.left();
            const int yTop = indicatorRect.top();
            const int yBottom = indicatorRect.bottom();
            painter.setPen(QPen(colors.accentDefault, 2.0));
            painter.drawLine(x, yTop, x, yBottom);
            painter.setBrush(colors.accentDefault);
            painter.setPen(Qt::NoPen);
            painter.drawEllipse(QPoint(x, yTop), 3, 3);
            painter.drawEllipse(QPoint(x, yBottom), 3, 3);
        }
    }

    if (m_isDragging && !m_dragPixmap.isNull()) {
        painter.setOpacity(0.85);
        const qreal dpr = m_dragPixmap.devicePixelRatio();
        const QPoint pixPos = m_dragCurrentPos - QPoint(qRound(m_dragPixmap.width() / (2 * dpr)),
                                                        qRound(m_dragPixmap.height() / (2 * dpr)));
        painter.drawPixmap(pixPos, m_dragPixmap);
        painter.setOpacity(1.0);
    }

    if (m_borderVisible) {
        const auto stroke = fluent::painting::DpiPaintMetrics(painter).alignedStroke(
            QRectF(viewport()->rect()), 1.0);
        QPainterPath borderPath;
        borderPath.addRoundedRect(stroke.rect, radius, radius);
        painter.setPen(QPen(colors.strokeDefault, stroke.width));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(borderPath);
    }
}

void FlowView::resizeEvent(QResizeEvent* event)
{
    QAbstractItemView::resizeEvent(event);
    layoutHeader();
    invalidateFlowLayout();
}

void FlowView::showEvent(QShowEvent* event)
{
    QAbstractItemView::showEvent(event);
    layoutHeader();
    syncFluentScrollBar();
}

void FlowView::enterEvent(FluentEnterEvent* event)
{
    setViewportHovered(true);
    QAbstractItemView::enterEvent(event);
}

void FlowView::leaveEvent(QEvent* event)
{
    setViewportHovered(false);
    m_hoveredRow = -1;
    viewport()->update();
    QAbstractItemView::leaveEvent(event);
}

void FlowView::mousePressEvent(QMouseEvent* event)
{
    m_pressedRow = -1;
    m_dragSourceIndex = -1;
    m_dragPressIntercepted = false;
    if (!isEnabled()) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(fluentMousePos(event));
        if (index.isValid()) {
            m_pressedRow = index.row();
            if (m_canReorderItems) {
                m_dragStartPos = fluentMousePos(event);
                m_dragSourceIndex = index.row();
                if ((m_selectionMode == SelectionMode::Multiple || m_selectionMode == SelectionMode::Extended) &&
                    selectionModel() && selectionModel()->isSelected(index)) {
                    m_dragPressIntercepted = true;
                    event->accept();
                    return;
                }
            }
        }
        setFocus(Qt::MouseFocusReason);
        event->accept();
        return;
    }

    QAbstractItemView::mousePressEvent(event);
}

void FlowView::mouseMoveEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        event->ignore();
        return;
    }

    const int hoverRow = rowAt(fluentMousePos(event));
    if (m_hoveredRow != hoverRow) {
        m_hoveredRow = hoverRow;
        viewport()->update();
    }

    if (m_canReorderItems && m_dragSourceIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!m_isDragging && (fluentMousePos(event) - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_isDragging = true;
            m_dragSourceIndices.clear();
            if (!m_dragPressIntercepted)
                applyPointerSelection(indexForRow(m_dragSourceIndex), event->modifiers());
            if (m_dragPressIntercepted && selectionModel() && !selectionModel()->selectedIndexes().isEmpty()) {
                for (const QModelIndex& index : selectionModel()->selectedIndexes())
                    m_dragSourceIndices.append(index.row());
            }
            if (!m_dragSourceIndices.contains(m_dragSourceIndex))
                m_dragSourceIndices.append(m_dragSourceIndex);
            std::sort(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
            m_dragSourceIndices.erase(std::unique(m_dragSourceIndices.begin(), m_dragSourceIndices.end()), m_dragSourceIndices.end());
            m_dropTargetIndex = -1;
            clearDragAnimations();
            rebuildDropIndicatorRects();
            m_dragPixmap = renderDragPixmap();
        }

        if (m_isDragging) {
            m_dragCurrentPos = fluentMousePos(event);
            const int target = stabilizedDropIndicatorIndex(m_dragCurrentPos);
            if (target != m_dropTargetIndex) {
                m_dropTargetIndex = target;
                updateDragDisplacement();
            }
            viewport()->update();
            event->accept();
            return;
        }
    }

    QAbstractItemView::mouseMoveEvent(event);
}

void FlowView::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        event->ignore();
        return;
    }

    if (m_isDragging && event->button() == Qt::LeftButton) {
        const int targetSlot = m_dropTargetIndex;
        const QList<int> sources = m_dragSourceIndices;

        if (targetSlot >= 0 && !sources.isEmpty() && model()) {
            auto* standardModel = qobject_cast<QStandardItemModel*>(model());
            if (standardModel) {
                QSet<QStandardItem*> selectedItems;
                if (selectionModel()) {
                    for (const QModelIndex& index : selectionModel()->selectedIndexes())
                        selectedItems.insert(standardModel->itemFromIndex(index));
                }

                QList<QList<QStandardItem*>> takenRows;
                for (int i = sources.size() - 1; i >= 0; --i)
                    takenRows.prepend(standardModel->takeRow(sources.at(i)));

                const int insertAt = qMin(targetSlot, standardModel->rowCount());
                int row = insertAt;
                for (auto& takenRow : takenRows)
                    standardModel->insertRow(row++, takenRow);

                if (selectionModel()) {
                    selectionModel()->clearSelection();
                    for (int r = 0; r < standardModel->rowCount(); ++r) {
                        if (selectedItems.contains(standardModel->item(r)))
                            selectionModel()->select(standardModel->index(r, 0), QItemSelectionModel::Select);
                    }
                    selectionModel()->setCurrentIndex(standardModel->index(insertAt, 0), QItemSelectionModel::NoUpdate);
                }

                emit itemReordered(sources.first(), insertAt);
            }
        }

        m_isDragging = false;
        m_dragSourceIndex = -1;
        m_dragSourceIndices.clear();
        m_dragPressIntercepted = false;
        resetDragReorderFeedback();
        invalidateFlowLayout();
        event->accept();
        return;
    }

    const int releasedRow = rowAt(fluentMousePos(event));
    const bool clickOnPressedItem = event->button() == Qt::LeftButton && releasedRow >= 0 && releasedRow == m_pressedRow;
    if (m_canReorderItems && m_dragPressIntercepted && m_dragSourceIndex >= 0 && model()) {
        applyPointerSelection(indexForRow(m_dragSourceIndex), event->modifiers());
    } else if (clickOnPressedItem && selectionModel()) {
        applyPointerSelection(indexForRow(releasedRow), event->modifiers());
        event->accept();
    } else if (event->button() != Qt::LeftButton) {
        QAbstractItemView::mouseReleaseEvent(event);
    } else {
        event->accept();
    }

    if (clickOnPressedItem)
        emit itemClicked(releasedRow);

    m_dragSourceIndex = -1;
    m_dragSourceIndices.clear();
    m_dragPressIntercepted = false;
    m_pressedRow = -1;
}

void FlowView::wheelEvent(QWheelEvent* event)
{
    if (!isEnabled()) {
        event->ignore();
        return;
    }
    // The shared controller owns the overscroll/bounce state machine; a scrollable flow keeps
    // boundary wheel input (unless scrollChainingEnabled) so an enclosing page doesn't pan.
    // zh_CN: 共享控制器持有 overscroll/回弹状态机；可滚动的 flow 默认持有边界滚轮（除非开启链式滚动），
    // 避免外层页面跟着平移。
    m_overscroll->handleWheel(event);
}

QModelIndex FlowView::moveCursor(CursorAction cursorAction, Qt::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers);
    const int count = modelRowCount();
    if (count <= 0)
        return QModelIndex();

    int row = currentIndex().isValid() ? currentIndex().row() : 0;
    switch (cursorAction) {
    case MoveLeft:
    case MovePrevious:
        row = qMax(0, row - 1);
        break;
    case MoveRight:
    case MoveNext:
        row = qMin(count - 1, row + 1);
        break;
    case MoveHome:
        row = 0;
        break;
    case MoveEnd:
        row = count - 1;
        break;
    case MoveUp:
        return nearestVerticalIndex(row, -1);
    case MoveDown:
        return nearestVerticalIndex(row, 1);
    case MovePageUp:
        scrollTo(indexForRow(row), PositionAtBottom);
        row = qMax(0, row - 1);
        break;
    case MovePageDown:
        scrollTo(indexForRow(row), PositionAtTop);
        row = qMin(count - 1, row + 1);
        break;
    default:
        break;
    }
    return indexForRow(row);
}

int FlowView::horizontalOffset() const
{
    return 0;
}

int FlowView::verticalOffset() const
{
    if (!verticalScrollBar())
        return 0;
    // m_overscroll may be null while the base view queries the offset during construction.
    // zh_CN: 构造期间基类会查询偏移，此时 m_overscroll 可能尚未创建。
    const qreal overscroll = m_overscroll ? m_overscroll->value() : 0.0;
    return verticalScrollBar()->value() - qRound(overscroll);
}

bool FlowView::isIndexHidden(const QModelIndex& index) const
{
    Q_UNUSED(index);
    return false;
}

void FlowView::setSelection(const QRect& rect, QItemSelectionModel::SelectionFlags flags)
{
    Q_UNUSED(rect);
    Q_UNUSED(flags);
}

QRegion FlowView::visualRegionForSelection(const QItemSelection& selection) const
{
    QRegion region;
    for (const QItemSelectionRange& range : selection) {
        for (int row = range.top(); row <= range.bottom(); ++row) {
            const QRect rect = visualRect(indexForRow(row));
            if (!rect.isEmpty())
                region += rect;
        }
    }
    return region;
}

void FlowView::applyPointerSelection(const QModelIndex& index, Qt::KeyboardModifiers modifiers)
{
    if (!selectionModel() || !index.isValid())
        return;

    selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    if (m_selectionMode == SelectionMode::None) {
        selectionModel()->clearSelection();
    } else if ((m_selectionMode == SelectionMode::Multiple || m_selectionMode == SelectionMode::Extended) &&
               (modifiers & (Qt::ControlModifier | Qt::MetaModifier))) {
        selectionModel()->select(index, QItemSelectionModel::Toggle);
    } else {
        selectionModel()->select(index, QItemSelectionModel::ClearAndSelect);
    }
    viewport()->update();
}

void FlowView::rowsInserted(const QModelIndex& parent, int start, int end)
{
    QAbstractItemView::rowsInserted(parent, start, end);
    invalidateFlowLayout();
}

void FlowView::rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end)
{
    QAbstractItemView::rowsAboutToBeRemoved(parent, start, end);
    invalidateFlowLayout();
}

void FlowView::dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const FluentItemDataRoles& roles)
{
    QAbstractItemView::dataChanged(topLeft, bottomRight, roles);
    if (roles.isEmpty() || roles.contains(m_itemSizeRole) || roles.contains(Qt::SizeHintRole) || roles.contains(Qt::DisplayRole))
        invalidateFlowLayout();
    else
        viewport()->update();
}

void FlowView::reset()
{
    QAbstractItemView::reset();
    m_hoveredRow = -1;
    resetDragReorderFeedback();
    invalidateFlowLayout();
}

void FlowView::onThemeUpdated()
{
    applyThemeStyle();
    if (m_vScrollBar)
        m_vScrollBar->update();
    viewport()->update();
}

void FlowView::applyThemeStyle()
{
    const auto& colors = themeColorsRef();

    QPalette pal = palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Window, Qt::transparent);
    pal.setColor(QPalette::Text, colors.textPrimary);
    pal.setColor(QPalette::Highlight, Qt::transparent);
    pal.setColor(QPalette::HighlightedText, colors.textPrimary);
    setPalette(pal);
    setFont(themeFont(m_fontRole).toQFont());

    if (viewport()) {
        viewport()->setAutoFillBackground(false);
        QPalette viewportPalette = viewport()->palette();
        viewportPalette.setColor(QPalette::Base, Qt::transparent);
        viewportPalette.setColor(QPalette::Window, Qt::transparent);
        viewport()->setPalette(viewportPalette);
    }

    if (m_headerLabel) {
        m_headerLabel->setFont(themeFont(Typography::FontRole::Subtitle).toQFont());
        // Color via the label's OWN style sheet rather than its palette: a palette WindowText color is
        // dropped whenever an ancestor sets a style sheet (Qt installs QStyleSheetStyle over the subtree
        // and ignores child palettes) — e.g. the gallery sample card, where the header then renders
        // near-black in dark theme. A style-sheet color always wins. zh_CN: 用 label 自身样式表上色而非
        // palette：任何祖先设置样式表时会安装 QStyleSheetStyle 并忽略子 palette，header 在深色主题里变近黑；样式表颜色始终生效。
        m_headerLabel->setStyleSheet(QStringLiteral("color: rgba(%1, %2, %3, %4); background: transparent;")
                                         .arg(colors.textPrimary.red()).arg(colors.textPrimary.green())
                                         .arg(colors.textPrimary.blue()).arg(colors.textPrimary.alpha()));
    }
}

void FlowView::layoutHeader()
{
    if (!m_headerLabel)
        return;
    if (m_headerText.isEmpty()) {
        m_headerLabel->hide();
        return;
    }

    const int headerHeight = m_headerLabel->sizeHint().height() + ::Spacing::Gap::Normal;
    m_headerLabel->setText(m_headerText);
    m_headerLabel->setGeometry(0, 0, width(), headerHeight);
    m_headerLabel->show();
    m_headerLabel->raise();
}

void FlowView::updateViewportMargins()
{
    if (m_headerLabel && !m_headerText.isEmpty()) {
        const int headerHeight = m_headerLabel->sizeHint().height() + ::Spacing::Gap::Normal;
        setViewportMargins(0, headerHeight, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }
}

void FlowView::refreshAccessibleName()
{
    if (!m_autoAccessibleName.isEmpty())
        setAccessibleName(m_autoAccessibleName);
}

void FlowView::setViewportHovered(bool hovered)
{
    if (m_viewportHovered == hovered)
        return;
    m_viewportHovered = hovered;
    emit viewportHoveredChanged();
}

void FlowView::invalidateFlowLayout()
{
    m_layoutDirty = true;
    m_layoutBands.clear();
    m_dropIndicatorRects.clear();
    syncFluentScrollBar();
    viewport()->update();
}

void FlowView::syncFluentScrollBar()
{
    ::fluent::scrolling::suppressNativeScrollBars(verticalScrollBar(), horizontalScrollBar());

    // FlowView owns its scroll model: derive the native range from the laid-out
    // content height before mirroring it onto the overlay bar.
    // zh_CN: FlowView 自管滚动模型：先按排版后的内容高度推导原生范围，再镜像到覆盖条。
    ensureLayout();
    QScrollBar* native = verticalScrollBar();
    const int maxValue = qMax(0, m_contentSize.height() - viewport()->height());
    native->setRange(0, maxValue);
    native->setPageStep(qMax(1, viewport()->height()));
    native->setSingleStep(24);

    if (!m_vScrollBar)
        return;

    m_vScrollBar->setValue(native->value());
    if (!::fluent::scrolling::mirrorNativeScrollBar(m_vScrollBar, native))
        return;

    const QRect r = rect();
    const int top = (m_headerLabel && m_headerLabel->isVisible()) ? m_headerLabel->geometry().bottom() + 2 : r.top() + 2;
    ::fluent::scrolling::placeVerticalScrollBar(m_vScrollBar, r, top,
                                                /*rightInset=*/0, /*bottomInset=*/2);
}

void FlowView::ensureLayout() const
{
    if (!m_layoutDirty)
        return;

    const int count = modelRowCount();
    const int left = m_contentMargins.left();
    const int top = m_contentMargins.top();
    const int availableWidth = qMax(
        1, viewport()->width() - m_contentMargins.left() - m_contentMargins.right());

    m_itemRects.resize(count);
    m_layoutBands.clear();
    m_layoutBands.reserve(count);

    int x = left;
    int y = top;
    int rowHeight = 0;
    int maxRight = left;
    int bandFirstRow = 0;

    for (int row = 0; row < count; ++row) {
        const QSize size = itemSizeForIndex(indexForRow(row));
        if (x != left && x + size.width() > left + availableWidth) {
            m_layoutBands.append(
                LayoutBand{bandFirstRow, row, y, y + rowHeight - 1});
            x = left;
            y += rowHeight + m_vSpacing;
            rowHeight = 0;
            bandFirstRow = row;
        }

        const QRect rect(x, y, size.width(), size.height());
        m_itemRects[row] = rect;
        x += size.width() + m_hSpacing;
        rowHeight = qMax(rowHeight, size.height());
        maxRight = qMax(maxRight, rect.right() + 1);
    }

    if (count > 0)
        m_layoutBands.append(
            LayoutBand{bandFirstRow, count, y, y + rowHeight - 1});

    const int totalHeight = count == 0
        ? m_contentMargins.top() + m_contentMargins.bottom()
        : y + rowHeight + m_contentMargins.bottom();
    const int totalWidth = qMax(viewport()->width(), maxRight + m_contentMargins.right());
    m_contentSize = QSize(totalWidth, qMax(totalHeight, viewport()->height()));
    m_layoutDirty = false;
}

int FlowView::firstLayoutBandIntersectingY(int contentY) const
{
    ensureLayout();

    int first = 0;
    int pastLast = m_layoutBands.size();
    while (first < pastLast) {
        const int middle = first + (pastLast - first) / 2;
        if (m_layoutBands.at(middle).bottom < contentY)
            first = middle + 1;
        else
            pastLast = middle;
    }
    return first;
}

void FlowView::computeLayoutForRows(const QList<int>& rows, QHash<int, QRect>* rects, QSize* contentSize) const
{
    rects->clear();
    const int left = m_contentMargins.left();
    const int top = m_contentMargins.top();
    const int availableWidth = qMax(1, viewport()->width() - m_contentMargins.left() - m_contentMargins.right());

    int x = left;
    int y = top;
    int rowHeight = 0;
    int maxRight = left;

    for (int modelRow : rows) {
        const QSize size = itemSizeForIndex(indexForRow(modelRow));
        if (x != left && x + size.width() > left + availableWidth) {
            x = left;
            y += rowHeight + m_vSpacing;
            rowHeight = 0;
        }

        const QRect rect(x, y, size.width(), size.height());
        rects->insert(modelRow, rect);
        x += size.width() + m_hSpacing;
        rowHeight = qMax(rowHeight, size.height());
        maxRight = qMax(maxRight, rect.right() + 1);
    }

    const int totalHeight = rows.isEmpty() ? m_contentMargins.top() + m_contentMargins.bottom()
                                           : y + rowHeight + m_contentMargins.bottom();
    const int totalWidth = qMax(viewport()->width(), maxRight + m_contentMargins.right());
    *contentSize = QSize(totalWidth, qMax(totalHeight, viewport()->height()));
}

QSize FlowView::itemSizeForIndex(const QModelIndex& index) const
{
    QSize size;
    if (index.isValid() && m_itemSizeRole >= 0) {
        const QVariant value = index.data(m_itemSizeRole);
        if (value.canConvert<QSize>())
            size = value.toSize();
    }

    if ((!size.isValid() || size.isEmpty()) && itemDelegate() && index.isValid()) {
        QStyleOptionViewItem option;
        FLUENT_INIT_VIEW_ITEM_OPTION(&option);
        option.font = font();
        option.rect = QRect(QPoint(0, 0), m_defaultItemSize);
        size = itemDelegate()->sizeHint(option, index);
    }

    if (!size.isValid() || size.isEmpty())
        size = m_defaultItemSize;

    return clampedItemSize(size);
}

QSize FlowView::clampedItemSize(const QSize& size) const
{
    int width = validDimension(size.width(), m_defaultItemSize.width());
    int height = validDimension(size.height(), m_defaultItemSize.height());

    if (m_minimumItemSize.isValid()) {
        width = qMax(width, validDimension(m_minimumItemSize.width(), 1));
        height = qMax(height, validDimension(m_minimumItemSize.height(), 1));
    }
    if (m_maximumItemSize.isValid() && !m_maximumItemSize.isEmpty()) {
        width = qMin(width, validDimension(m_maximumItemSize.width(), width));
        height = qMin(height, validDimension(m_maximumItemSize.height(), height));
    }
    return QSize(width, height);
}

QRect FlowView::contentToViewport(const QRect& rect) const
{
    return rect.translated(-horizontalOffset(), -verticalOffset());
}

QPoint FlowView::viewportToContent(const QPoint& point) const
{
    return point + QPoint(horizontalOffset(), verticalOffset());
}

QStyleOptionViewItem FlowView::optionForIndex(const QModelIndex& index, const QRect& rect) const
{
    QStyleOptionViewItem option;
    FLUENT_INIT_VIEW_ITEM_OPTION(&option);
    option.rect = rect;
    option.font = font();
    option.widget = viewport();
    option.state |= QStyle::State_Enabled;
    if (!isEnabled())
        option.state &= ~QStyle::State_Enabled;
    if (selectionModel() && selectionModel()->isSelected(index))
        option.state |= QStyle::State_Selected;
    if (hasFocus())
        option.state |= QStyle::State_Active;
    if (m_hoveredRow == index.row() && isEnabled())
        option.state |= QStyle::State_MouseOver;
    return option;
}

int FlowView::modelRowCount() const
{
    return model() ? model()->rowCount(rootIndex()) : 0;
}

QModelIndex FlowView::indexForRow(int row) const
{
    return model() && row >= 0 && row < modelRowCount() ? model()->index(row, 0, rootIndex()) : QModelIndex();
}

QModelIndex FlowView::nearestVerticalIndex(int currentRow, int direction) const
{
    ensureLayout();
    if (currentRow < 0 || currentRow >= m_itemRects.size())
        return QModelIndex();

    const QRect current = m_itemRects.at(currentRow);
    const QPoint currentCenter = current.center();
    const int currentBandIndex = firstLayoutBandIntersectingY(currentCenter.y());
    int bestRow = currentRow;
    qreal bestScore = std::numeric_limits<qreal>::max();

    for (int bandIndex = currentBandIndex + (direction < 0 ? -1 : 1);
         bandIndex >= 0 && bandIndex < m_layoutBands.size();
         bandIndex += direction < 0 ? -1 : 1) {
        const LayoutBand& band = m_layoutBands.at(bandIndex);
        const int minimumDy = direction < 0
            ? qMax(0, currentCenter.y() - band.bottom)
            : qMax(0, band.top - currentCenter.y());
        if (minimumDy * 4.0 > bestScore)
            break;

        for (int row = band.firstRow; row < band.pastLastRow; ++row) {
            const QPoint center = m_itemRects.at(row).center();
            const int dy = center.y() - currentCenter.y();
            if ((direction < 0 && dy >= 0) || (direction > 0 && dy <= 0))
                continue;
            const int dx = center.x() - currentCenter.x();
            const qreal score = std::abs(dy) * 4.0 + std::abs(dx);
            if (score < bestScore || (qFuzzyCompare(score + 1.0, bestScore + 1.0)
                                      && row < bestRow)) {
                bestScore = score;
                bestRow = row;
            }
        }
    }

    return indexForRow(bestRow);
}

void FlowView::clearModelConnections()
{
    for (const QMetaObject::Connection& connection : m_modelConnections)
        disconnect(connection);
    m_modelConnections.clear();
}

void FlowView::connectModelSignals(QAbstractItemModel* newModel)
{
    if (!newModel)
        return;
    m_modelConnections.append(connect(newModel, &QAbstractItemModel::layoutChanged, this, [this]() {
        invalidateFlowLayout();
    }));
    m_modelConnections.append(connect(newModel, &QAbstractItemModel::rowsMoved, this, [this]() {
        invalidateFlowLayout();
    }));
}

int FlowView::rowAt(const QPoint& point) const
{
    ensureLayout();
    const QPoint contentPoint = viewportToContent(point);
    const int bandIndex = firstLayoutBandIntersectingY(contentPoint.y());
    if (bandIndex >= m_layoutBands.size())
        return -1;

    const LayoutBand& band = m_layoutBands.at(bandIndex);
    if (contentPoint.y() < band.top || contentPoint.y() > band.bottom)
        return -1;

    for (int row = band.firstRow; row < band.pastLastRow; ++row) {
        if (m_itemRects.at(row).contains(contentPoint))
            return row;
    }
    return -1;
}

void FlowView::rebuildDropIndicatorRects() const
{
    ensureLayout();
    m_dropIndicatorRects.clear();
    if (!model() || m_dragSourceIndices.isEmpty())
        return;

    const QSet<int> sourceRows(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
    QList<int> remaining;
    remaining.reserve(qMax(0, modelRowCount() - sourceRows.size()));
    for (int row = 0; row < modelRowCount(); ++row) {
        if (!sourceRows.contains(row))
            remaining.append(row);
    }

    const int sourceRow = m_dragSourceIndices.first();
    if (sourceRow < 0 || sourceRow >= m_itemRects.size())
        return;

    const QSize sourceSize = m_itemRects.at(sourceRow).size();
    const int left = m_contentMargins.left();
    const int top = m_contentMargins.top();
    const int availableWidth = qMax(
        1, viewport()->width() - m_contentMargins.left() - m_contentMargins.right());

    int x = left;
    int y = top;
    int rowHeight = 0;
    QRect previousRect;
    m_dropIndicatorRects.reserve(remaining.size() + 1);

    for (int slot = 0; slot <= remaining.size(); ++slot) {
        int sourceX = x;
        int sourceY = y;
        if (sourceX != left && sourceX + sourceSize.width() > left + availableWidth) {
            sourceX = left;
            sourceY += rowHeight + m_vSpacing;
        }

        const QRect sourceRect(sourceX, sourceY, sourceSize.width(), sourceSize.height());
        int indicatorX = sourceRect.left();
        const bool sameRow = previousRect.isValid()
            && previousRect.bottom() >= sourceRect.top()
            && previousRect.top() <= sourceRect.bottom()
            && previousRect.right() < sourceRect.left();
        if (sameRow)
            indicatorX -= qMax(2, m_hSpacing / 2);
        m_dropIndicatorRects.append(
            QRect(indicatorX, sourceRect.top() + 2, 2,
                  qMax(1, sourceRect.height() - 4)));

        if (slot == remaining.size())
            break;

        const int modelRow = remaining.at(slot);
        const QSize size = m_itemRects.at(modelRow).size();
        if (x != left && x + size.width() > left + availableWidth) {
            x = left;
            y += rowHeight + m_vSpacing;
            rowHeight = 0;
        }
        previousRect = QRect(x, y, size.width(), size.height());
        x += size.width() + m_hSpacing;
        rowHeight = qMax(rowHeight, size.height());
    }
}

int FlowView::dropIndicatorIndex(const QPoint& point) const
{
    if (m_dropIndicatorRects.isEmpty())
        rebuildDropIndicatorRects();
    if (m_dropIndicatorRects.isEmpty())
        return 0;

    const QPoint contentPoint = viewportToContent(point);
    int bestSlot = 0;
    qreal bestDistance = std::numeric_limits<qreal>::max();
    for (int slot = 0; slot < m_dropIndicatorRects.size(); ++slot) {
        const QRect indicatorRect = m_dropIndicatorRects.at(slot);
        const int clampedY = qBound(
            indicatorRect.top(), contentPoint.y(), indicatorRect.bottom());
        const qreal distance = std::hypot(
            contentPoint.x() - indicatorRect.left(),
            contentPoint.y() - clampedY);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestSlot = slot;
        }
    }
    return bestSlot;
}

QRect FlowView::dropIndicatorRectForSlot(int slot) const
{
    if (m_dropIndicatorRects.isEmpty())
        rebuildDropIndicatorRects();
    if (slot < 0 || slot >= m_dropIndicatorRects.size())
        return QRect();
    return m_dropIndicatorRects.at(slot);
}

qreal FlowView::dropIndicatorDistance(const QPoint& point, int slot) const
{
    const QPoint contentPoint = viewportToContent(point);
    const QRect indicatorRect = dropIndicatorRectForSlot(slot);
    if (indicatorRect.isEmpty())
        return std::numeric_limits<qreal>::infinity();

    const int clampedY = qBound(indicatorRect.top(), contentPoint.y(), indicatorRect.bottom());
    return std::hypot(contentPoint.x() - indicatorRect.left(), contentPoint.y() - clampedY);
}

qreal FlowView::dropTargetHysteresis() const
{
    const int basis = qMax(1, qMin(m_defaultItemSize.width(), m_defaultItemSize.height()));
    return qBound<qreal>(4.0, basis * 0.06, 12.0);
}

int FlowView::stabilizedDropIndicatorIndex(const QPoint& point) const
{
    const int candidate = dropIndicatorIndex(point);
    if (!model() || m_dropTargetIndex < 0 || candidate == m_dropTargetIndex)
        return candidate;

    const QSet<int> sourceRows(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
    const int remainingCount = qMax(0, modelRowCount() - sourceRows.size());
    if (m_dropTargetIndex > remainingCount || candidate < 0 || candidate > remainingCount)
        return candidate;
    if (std::abs(candidate - m_dropTargetIndex) != 1)
        return candidate;

    const qreal currentDistance = dropIndicatorDistance(point, m_dropTargetIndex);
    const qreal candidateDistance = dropIndicatorDistance(point, candidate);
    if (!std::isfinite(currentDistance) || !std::isfinite(candidateDistance))
        return candidate;
    return candidateDistance + dropTargetHysteresis() < currentDistance ? candidate : m_dropTargetIndex;
}

void FlowView::updateDragDisplacement()
{
    ensureLayout();
    if (m_dragSourceIndices.isEmpty() || m_dropTargetIndex < 0 || !model()) {
        clearDragAnimations();
        return;
    }

    const QSet<int> sourceRows(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
    QList<int> order;
    for (int row = 0; row < modelRowCount(); ++row) {
        if (!sourceRows.contains(row))
            order.append(row);
    }
    const int insertAt = qBound(0, m_dropTargetIndex, order.size());
    for (int i = 0; i < m_dragSourceIndices.size(); ++i)
        order.insert(insertAt + i, m_dragSourceIndices.at(i));

    QHash<int, QRect> finalRects;
    QSize ignoredSize;
    computeLayoutForRows(order, &finalRects, &ignoredSize);

    QHash<int, QPointF> nextTargets;
    nextTargets.reserve(modelRowCount());
    for (int row = 0; row < modelRowCount(); ++row) {
        if (sourceRows.contains(row))
            continue;

        QPointF target(0.0, 0.0);
        if (finalRects.contains(row)) {
            const QPoint delta = finalRects.value(row).topLeft() - m_itemRects.value(row).topLeft();
            target = QPointF(delta);
        }
        if (!pointsEqual(target, QPointF()) || m_dragOffsets.contains(row))
            nextTargets.insert(row, target);
    }

    if (m_dragAnimation) {
        m_dragAnimation->stop();
        m_dragAnimation->deleteLater();
        m_dragAnimation = nullptr;
    }

    m_dragStartOffsets.clear();
    m_dragTargetOffsets = nextTargets;
    bool needsAnimation = false;
    for (auto it = m_dragTargetOffsets.cbegin(); it != m_dragTargetOffsets.cend(); ++it) {
        const QPointF start = m_dragOffsets.value(it.key(), QPointF());
        m_dragStartOffsets.insert(it.key(), start);
        needsAnimation = needsAnimation || !pointsEqual(start, it.value());
    }

    if (!needsAnimation) {
        m_dragOffsets = m_dragTargetOffsets;
        viewport()->update();
        return;
    }

    const auto animationTokens = themeAnimation();
    auto* animation = new QVariantAnimation(this);
    animation->setObjectName(QStringLiteral("_q_fluentFlowDragDisplacementAnimation"));
    m_dragAnimation = animation;
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(animationTokens.fast);
    animation->setEasingCurve(animationTokens.decelerate);
    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        const qreal progress = value.toReal();
        for (auto it = m_dragTargetOffsets.cbegin(); it != m_dragTargetOffsets.cend(); ++it) {
            const QPointF start = m_dragStartOffsets.value(it.key(), QPointF());
            m_dragOffsets[it.key()] = start + (it.value() - start) * progress;
        }
        viewport()->update();
    });
    connect(animation, &QVariantAnimation::finished, this, [this, animation]() {
        if (m_dragAnimation != animation)
            return;
        m_dragOffsets = m_dragTargetOffsets;
        m_dragAnimation = nullptr;
        animation->deleteLater();
        viewport()->update();
    });
    animation->start();
}

void FlowView::resetDragReorderFeedback()
{
    m_dropTargetIndex = -1;
    m_dropIndicatorRects.clear();
    m_dragPixmap = QPixmap();
    clearDragAnimations();
}

void FlowView::clearDragAnimations()
{
    if (m_dragAnimation) {
        m_dragAnimation->stop();
        m_dragAnimation->deleteLater();
        m_dragAnimation = nullptr;
    }
    m_dragOffsets.clear();
    m_dragStartOffsets.clear();
    m_dragTargetOffsets.clear();
}

QPixmap FlowView::renderItemPixmap(int row) const
{
    if (!model() || !itemDelegate() || row < 0 || row >= modelRowCount())
        return QPixmap();
    ensureLayout();

    const QModelIndex index = indexForRow(row);
    const QSize size = m_itemRects.at(row).size();
    const qreal dpr = devicePixelRatioF();
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QStyleOptionViewItem option = optionForIndex(index, QRect(QPoint(0, 0), size));
    itemDelegate()->paint(&painter, option, index);
    return pixmap;
}

QPixmap FlowView::renderDragPixmap() const
{
    if (m_dragSourceIndices.isEmpty())
        return QPixmap();

    const QPixmap primary = renderItemPixmap(m_dragSourceIndex);
    if (primary.isNull() || m_dragSourceIndices.size() == 1)
        return primary;

    constexpr int stackOffset = 4;
    const int maxStack = qMin(3, m_dragSourceIndices.size());
    const qreal dpr = primary.devicePixelRatio();
    const QSize baseSize(primary.width() / dpr, primary.height() / dpr);
    const QSize compositeSize(baseSize.width() + stackOffset * (maxStack - 1),
                              baseSize.height() + stackOffset * (maxStack - 1));

    QPixmap composite(compositeSize * dpr);
    composite.setDevicePixelRatio(dpr);
    composite.fill(Qt::transparent);

    QPainter painter(&composite);
    painter.setRenderHint(QPainter::Antialiasing);
    for (int layer = maxStack - 1; layer >= 0; --layer) {
        const int sourceRow = layer == 0 ? m_dragSourceIndex : m_dragSourceIndices.value(layer, m_dragSourceIndex);
        const QPixmap pixmap = sourceRow == m_dragSourceIndex ? primary : renderItemPixmap(sourceRow);
        if (pixmap.isNull())
            continue;
        painter.setOpacity(layer > 0 ? 0.6 : 1.0);
        painter.drawPixmap(stackOffset * layer, stackOffset * layer, pixmap);
    }

    painter.setOpacity(1.0);
    const auto& colors = themeColorsRef();
    constexpr int badgeSize = 20;
    const QRect badgeRect(compositeSize.width() - badgeSize - 2, 2, badgeSize, badgeSize);
    painter.setBrush(colors.accentDefault);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(badgeRect);
    QFont badgeFont = painter.font();
    badgeFont.setPixelSize(11);
    badgeFont.setBold(true);
    painter.setFont(badgeFont);
    painter.setPen(Qt::white);
    painter.drawText(badgeRect, Qt::AlignCenter, QString::number(m_dragSourceIndices.size()));
    return composite;
}

} // namespace fluent::collections
