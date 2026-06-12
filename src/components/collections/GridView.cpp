#include "GridView.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QShowEvent>
#include <QStandardItemModel>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>

#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "components/scrolling/OverlayScrollChrome.h"
#include "components/scrolling/ScrollBar.h"

namespace fluent::collections {

namespace {

bool dragPointsEqual(const QPointF& lhs, const QPointF& rhs) {
    return std::abs(lhs.x() - rhs.x()) < 0.01 &&
           std::abs(lhs.y() - rhs.y()) < 0.01;
}

} // namespace

GridView::GridView(QWidget* parent)
    : QListView(parent) {

    m_fontRole = Typography::FontRole::Body;

    // --- QListView IconMode + Wrapping = GridView ---
    setViewMode(QListView::IconMode);
    setWrapping(true);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setFlow(QListView::LeftToRight);
    setUniformItemSizes(true);

    setFrameStyle(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setMouseTracking(true);
    setDragEnabled(false);           // Disable QListView's built-in drag; reorder is custom. zh_CN: 禁用内置拖拽（自行实现）。
    setDefaultDropAction(Qt::IgnoreAction);

    QListView::setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectItems);
    setEditTriggers(QAbstractItemView::NoEditTriggers);

    connect(this, &QAbstractItemView::clicked, this, [this](const QModelIndex& idx) {
        emit itemClicked(idx.row());
    });

    // --- Header label ---
    m_headerLabel = new QLabel(this);
    m_headerLabel->hide();
    m_headerLabel->setIndent(::Spacing::Padding::ListItemHorizontal);

    // --- Fluent scroll bar ---
    m_vScrollBar = ::fluent::scrolling::createOverlayScrollBar(
        Qt::Vertical, this, verticalScrollBar(),
        QStringLiteral("fluentGridViewScrollBar"));
    connect(verticalScrollBar(), &QScrollBar::rangeChanged,
            this, &GridView::syncFluentScrollBar);

    // --- Overscroll bounce ---
    m_bounceAnim = new QVariantAnimation(this);
    m_bounceAnim->setDuration(themeAnimation().normal);
    m_bounceAnim->setEasingCurve(themeAnimation().decelerate);
    connect(m_bounceAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_overscrollY = v.toReal();
        viewport()->update();
    });

    m_bounceTimer = new QTimer(this);
    m_bounceTimer->setSingleShot(true);
    m_bounceTimer->setInterval(150);
    connect(m_bounceTimer, &QTimer::timeout, this, &GridView::startBounceBack);

    updateGridSize();
    syncFluentScrollBar();
    onThemeUpdated();
}

// ── Selection mode ────────────────────────────────────────────────────────────

void GridView::setSelectionMode(GridSelectionMode mode) {
    if (m_selectionMode == mode) return;
    m_selectionMode = mode;

    switch (mode) {
    case GridSelectionMode::None:
        QListView::setSelectionMode(QAbstractItemView::NoSelection);
        break;
    case GridSelectionMode::Single:
        QListView::setSelectionMode(QAbstractItemView::SingleSelection);
        break;
    case GridSelectionMode::Multiple:
        QListView::setSelectionMode(QAbstractItemView::MultiSelection);
        break;
    case GridSelectionMode::Extended:
        QListView::setSelectionMode(QAbstractItemView::ExtendedSelection);
        break;
    }
    emit selectionModeChanged();
}

// ── Appearance properties ────────────────────────────────────────────────────

void GridView::setFontRole(const QString& role) {
    if (m_fontRole == role) return;
    m_fontRole = role;
    applyThemeStyle();
    emit fontRoleChanged();
}

void GridView::setBorderVisible(bool visible) {
    if (m_borderVisible == visible) return;
    m_borderVisible = visible;
    update();
    emit borderVisibleChanged();
}

void GridView::setHeaderText(const QString& text) {
    if (m_headerText == text) return;
    m_headerText = text;
    if (m_headerLabel) {
        m_headerLabel->setText(text);
        m_headerLabel->setVisible(!text.isEmpty());
    }
    updateViewportMargins();
    layoutHeader();
    emit headerTextChanged();
}

void GridView::setPlaceholderText(const QString& text) {
    if (m_placeholderText == text) return;
    m_placeholderText = text;
    if (viewport()) viewport()->update();
    emit placeholderTextChanged();
}

// ── Grid layout properties ────────────────────────────────────────────────────

void GridView::setCellSize(const QSize& size) {
    if (m_cellSize == size) return;
    m_cellSize = size;
    updateGridSize();
    emit cellSizeChanged();
}

void GridView::setHorizontalSpacing(int spacing) {
    if (m_hSpacing == spacing) return;
    m_hSpacing = spacing;
    updateGridSize();
    emit horizontalSpacingChanged();
}

void GridView::setVerticalSpacing(int spacing) {
    if (m_vSpacing == spacing) return;
    m_vSpacing = spacing;
    updateGridSize();
    emit verticalSpacingChanged();
}

void GridView::setMaxColumns(int maxCols) {
    if (m_maxColumns == maxCols) return;
    m_maxColumns = maxCols;
    updateGridSize();
    emit maxColumnsChanged();
}

void GridView::updateGridSize() {
    // gridSize covers cell + spacing; QListView lays out IconMode slots with it.
    // zh_CN: gridSize 包含 cell + spacing（QListView 以此布局 IconMode 每格）。
    setGridSize(QSize(m_cellSize.width() + m_hSpacing,
                      m_cellSize.height() + m_vSpacing));
}

// ── Selection API ─────────────────────────────────────────────────────────────

int GridView::selectedIndex() const {
    auto idxList = selectionModel()->selectedIndexes();
    return idxList.isEmpty() ? -1 : idxList.first().row();
}

QList<int> GridView::selectedRows() const {
    QSet<int> seen;
    for (const auto& idx : selectionModel()->selectedIndexes())
        seen.insert(idx.row());
    QList<int> rows(seen.begin(), seen.end());
    std::sort(rows.begin(), rows.end());
    return rows;
}

void GridView::setSelectedIndex(int index) {
    const QAbstractItemModel* m = model();
    if (!m || index < 0 || index >= m->rowCount()) {
        clearSelection();
        return;
    }
    setCurrentIndex(m->index(index, 0));
}

::fluent::scrolling::ScrollBar* GridView::verticalFluentScrollBar() const {
    return m_vScrollBar;
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void GridView::paintEvent(QPaintEvent* event) {
    const auto& c = themeColors();
    const int r = CornerRadius::Control;

    // --- 1. Container background. zh_CN: 绘制容器背景。---
    {
        QPainter p(viewport());
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(viewport()->rect(), c.bgLayer);
        p.end();
    }

    // --- 2. Empty placeholder. zh_CN: 空网格占位符。---
    const bool isEmpty = !model() || model()->rowCount() == 0;
    if (isEmpty && !m_placeholderText.isEmpty()) {
        QPainter ph(viewport());
        ph.setRenderHint(QPainter::Antialiasing);
        ph.setPen(c.textTertiary);
        ph.setFont(themeFont(m_fontRole).toQFont());
        ph.drawText(viewport()->rect(), Qt::AlignCenter, m_placeholderText);
        ph.end();
    }

    // --- 3. Items via QListView's default painting; drags apply displacement. ---
    // zh_CN: 绘制网格项（QListView 默认绘制，拖拽时应用位移偏移）。
    m_paintingWithOffsets = !m_dragOffsets.isEmpty();
    QListView::paintEvent(event);
    m_paintingWithOffsets = false;

    // --- 3.1 Translucent mask over the drag source. zh_CN: 拖拽源项半透明遮罩。---
    if (m_isDragging && !m_dragSourceIndices.isEmpty() && model()) {
        QPainter ghost(viewport());
        ghost.setRenderHint(QPainter::Antialiasing);
        QColor overlay = c.bgLayer;
        overlay.setAlphaF(0.7f);
        for (int srcIdx : m_dragSourceIndices) {
            if (srcIdx >= 0 && srcIdx < model()->rowCount()) {
                QRect srcRect = QListView::visualRect(model()->index(srcIdx, 0));
                if (!srcRect.isEmpty())
                    ghost.fillRect(srcRect, overlay);
            }
        }
        ghost.end();
    }

    // --- 3.2 Drop indicator. zh_CN: 拖拽指示线。---
    if (m_isDragging && m_dropTargetIndex >= 0 && model()) {
        m_paintingWithOffsets = !m_dragOffsets.isEmpty();
        QPainter dp(viewport());
        dp.setRenderHint(QPainter::Antialiasing);

        // m_dropTargetIndex is a slot among non-source items.
        // Find the visual rect of the item at that slot (or after last).
        const QSet<int> srcSet(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
        QList<int> remaining;
        for (int i = 0; i < model()->rowCount(); ++i)
            if (!srcSet.contains(i)) remaining.append(i);

        QRect targetRect;
        if (m_dropTargetIndex < remaining.size()) {
            int modelIdx = remaining[m_dropTargetIndex];
            targetRect = visualRect(model()->index(modelIdx, 0));
        } else if (!remaining.isEmpty()) {
            // Drop at end: draw after the last remaining item
            int lastIdx = remaining.last();
            targetRect = visualRect(model()->index(lastIdx, 0));
            targetRect.moveLeft(targetRect.right() + m_hSpacing);
        }

        if (!targetRect.isEmpty()) {
            const int x = targetRect.left() - m_hSpacing / 2;
            const int yTop = targetRect.top() + 2;
            const int yBot = targetRect.bottom() - 2;

            // A 2px vertical accent line with dot caps. zh_CN: 2px 竖向 accent 指示线 + 圆点端点。
            dp.setPen(QPen(c.accentDefault, 2.0));
            dp.drawLine(x, yTop, x, yBot);

            const int circleR = 3;
            dp.setBrush(c.accentDefault);
            dp.setPen(Qt::NoPen);
            dp.drawEllipse(QPoint(x, yTop), circleR, circleR);
            dp.drawEllipse(QPoint(x, yBot), circleR, circleR);
        }
        m_paintingWithOffsets = false;
        dp.end();
    }

    // --- 3.3 Drag ghost layer. zh_CN: 拖拽浮动图层。---
    if (m_isDragging && !m_dragPixmap.isNull()) {
        QPainter fp(viewport());
        fp.setRenderHint(QPainter::Antialiasing);
        fp.setOpacity(0.85);
        QPoint pixPos = m_dragCurrentPos - QPoint(m_dragPixmap.width() / (2 * m_dragPixmap.devicePixelRatio()),
                                                   m_dragPixmap.height() / (2 * m_dragPixmap.devicePixelRatio()));
        fp.drawPixmap(pixPos, m_dragPixmap);
        fp.end();
    }

    // --- 4. Corner masking: fill corners with parent bg (anti-alias) ---
    if (m_borderVisible) {
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

    // --- 5. Container border. zh_CN: 容器边框。---
    if (m_borderVisible) {
        QPainter bp(viewport());
        bp.setRenderHint(QPainter::Antialiasing);

        QPainterPath borderPath;
        borderPath.addRoundedRect(QRectF(viewport()->rect()).adjusted(0.5, 0.5, -0.5, -0.5), r, r);
        bp.setPen(QPen(c.strokeDefault, 1.0));
        bp.setBrush(Qt::NoBrush);
        bp.drawPath(borderPath);
        bp.end();
    }
}

// ── Layout ────────────────────────────────────────────────────────────────────

void GridView::resizeEvent(QResizeEvent* event) {
    QListView::resizeEvent(event);
    syncFluentScrollBar();
    layoutHeader();
}

void GridView::showEvent(QShowEvent* event) {
    QListView::showEvent(event);
    syncFluentScrollBar();
    layoutHeader();
    QTimer::singleShot(0, this, &GridView::syncFluentScrollBar);
}

void GridView::enterEvent(FluentEnterEvent* event) {
    setViewportHovered(true);
    QListView::enterEvent(event);
}

void GridView::leaveEvent(QEvent* event) {
    setViewportHovered(false);
    QListView::leaveEvent(event);
}

// ── Overscroll bounce ─────────────────────────────────────────────────────────

void GridView::wheelEvent(QWheelEvent* event) {
    const int delta = event->angleDelta().y();
    const auto phase = event->phase();

    // Zero-delta event (e.g. ScrollEnd on Windows touchpad with no residual)
    if (delta == 0 && event->pixelDelta().isNull()) {
        if (!qFuzzyIsNull(m_overscrollY) &&
            (phase == Qt::ScrollEnd || phase == Qt::ScrollMomentum)) {
            startBounceBack();
            event->accept();
            return;
        }
        QListView::wheelEvent(event);
        return;
    }

    const qreal scrollPx = !event->pixelDelta().isNull()
                               ? static_cast<qreal>(event->pixelDelta().y())
                               : delta / 120.0 * 20.0;

    if (!qFuzzyIsNull(m_overscrollY)) {
        if (m_bounceAnim->state() == QAbstractAnimation::Running) {
            // Bounce in progress: consume stale NoScrollPhase events (RDP / mouse wheel)
            if (phase == Qt::NoScrollPhase) {
                event->accept();
                return;
            }
            m_bounceAnim->stop();
        }
        m_bounceTimer->stop();

        if (phase == Qt::ScrollMomentum || phase == Qt::ScrollEnd) {
            startBounceBack();
            event->accept();
            return;
        }

        constexpr qreal kMax = 100.0;
        const qreal ratio = qMin(qAbs(m_overscrollY) / kMax, 1.0);
        const qreal damping = (1.0 - ratio) * (1.0 - ratio);

        const qreal prev = m_overscrollY;
        m_overscrollY += scrollPx * qMax(damping, 0.05) * 0.5;
        m_overscrollY = qBound(-kMax, m_overscrollY, kMax);

        if ((prev > 0.0 && m_overscrollY <= 0.0) ||
            (prev < 0.0 && m_overscrollY >= 0.0)) {
            m_overscrollY = 0.0;
        }

        viewport()->update();

        // NoScrollPhase (mouse wheel / Windows touchpad fallback) → timer bounce
        if (!qFuzzyIsNull(m_overscrollY) && phase == Qt::NoScrollPhase)
            m_bounceTimer->start();

        event->accept();
        return;
    }

    QScrollBar* vsb = verticalScrollBar();
    const bool atTop    = vsb->value() <= vsb->minimum();
    const bool atBottom = vsb->value() >= vsb->maximum();

    if ((atTop && scrollPx > 0) || (atBottom && scrollPx < 0)) {
        // Don't enter overscroll from inertia or finger-lift
        if (phase == Qt::ScrollMomentum || phase == Qt::ScrollEnd) {
            event->accept();
            return;
        }

        m_overscrollY = scrollPx * 0.5;
        viewport()->update();

        if (phase == Qt::NoScrollPhase)
            m_bounceTimer->start();

        event->accept();
        return;
    }

    QListView::wheelEvent(event);
}

int GridView::verticalOffset() const {
    return QListView::verticalOffset() - qRound(m_overscrollY);
}

QRect GridView::visualRect(const QModelIndex& index) const {
    QRect r = QListView::visualRect(index);
    if (m_paintingWithOffsets && index.isValid()) {
        const QPointF off = m_dragOffsets.value(index.row(), QPointF(0.0, 0.0));
        r.translate(qRound(off.x()), qRound(off.y()));
    }
    return r;
}

void GridView::startBounceBack() {
    if (qFuzzyIsNull(m_overscrollY))
        return;
    m_bounceAnim->stop();
    m_bounceAnim->setStartValue(m_overscrollY);
    m_bounceAnim->setEndValue(0.0);
    m_bounceAnim->start();
}

void GridView::setViewportHovered(bool hovered) {
    if (m_viewportHovered == hovered)
        return;
    m_viewportHovered = hovered;
    emit viewportHoveredChanged();
}

// ── Theme ─────────────────────────────────────────────────────────────────────

void GridView::onThemeUpdated() {
    applyThemeStyle();
}

void GridView::applyThemeStyle() {
    const auto& c = themeColors();

    QPalette pal = palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Window, Qt::transparent);
    pal.setColor(QPalette::Text, c.textPrimary);
    pal.setColor(QPalette::Highlight, Qt::transparent);
    pal.setColor(QPalette::HighlightedText, c.textPrimary);
    setPalette(pal);

    setFont(themeFont(m_fontRole).toQFont());

    if (viewport()) {
        viewport()->setAutoFillBackground(false);
        QPalette vpal = viewport()->palette();
        vpal.setColor(QPalette::Base, Qt::transparent);
        vpal.setColor(QPalette::Window, Qt::transparent);
        viewport()->setPalette(vpal);
    }

    if (m_headerLabel) {
        m_headerLabel->setFont(themeFont(Typography::FontRole::Subtitle).toQFont());
        QPalette hpal = m_headerLabel->palette();
        hpal.setColor(QPalette::WindowText, c.textPrimary);
        m_headerLabel->setPalette(hpal);
    }

    update();
}

// ── Internal layout helpers ───────────────────────────────────────────────────

void GridView::layoutHeader() {
    if (!m_headerLabel) return;
    if (m_headerText.isEmpty()) {
        m_headerLabel->hide();
        return;
    }
    m_headerLabel->setText(m_headerText);
    m_headerLabel->setVisible(true);

    const int headerH = m_headerLabel->sizeHint().height() + ::Spacing::Gap::Normal;
    m_headerLabel->setGeometry(0, 0, width(), headerH);
    m_headerLabel->raise();
}

void GridView::updateViewportMargins() {
    if (m_headerLabel && !m_headerText.isEmpty()) {
        const int headerH = m_headerLabel->sizeHint().height() + ::Spacing::Gap::Normal;
        setViewportMargins(0, headerH, 0, 0);
    } else {
        setViewportMargins(0, 0, 0, 0);
    }
}

void GridView::syncFluentScrollBar() {
    ::fluent::scrolling::suppressNativeScrollBars(verticalScrollBar(), horizontalScrollBar());
    if (!m_vScrollBar) return;
    if (!::fluent::scrolling::mirrorNativeScrollBar(m_vScrollBar, verticalScrollBar()))
        return;

    const QRect r = rect();
    const int top = (m_headerLabel && m_headerLabel->isVisible())
                        ? m_headerLabel->geometry().bottom() + 2
                        : r.top() + 2;
    ::fluent::scrolling::placeVerticalScrollBar(m_vScrollBar, r, top,
                                                /*rightInset=*/0, /*bottomInset=*/2);
}

void GridView::refreshFluentScrollChrome() {
    syncFluentScrollBar();
}

// ── Drag reorder ──────────────────────────────────────────────────────────────

void GridView::setCanReorderItems(bool enabled) {
    if (m_canReorderItems == enabled) return;
    m_canReorderItems = enabled;
    if (!m_canReorderItems && m_isDragging) {
        m_isDragging = false;
        m_dragSourceIndex = -1;
        m_dragSourceIndices.clear();
        m_dragPressIntercepted = false;
        resetDragReorderFeedback();
    }
    emit canReorderItemsChanged();
}

void GridView::mousePressEvent(QMouseEvent* event) {
    m_dragPressIntercepted = false;
    if (m_canReorderItems && event->button() == Qt::LeftButton) {
        QModelIndex idx = indexAt(event->pos());
        if (idx.isValid()) {
            m_dragStartPos = event->pos();
            m_dragSourceIndex = idx.row();

            // In Multiple/Extended mode, pressing on an already-selected item
            // should NOT change selection (to allow multi-drag).
            // We handle selection change on release if no drag occurred.
            if ((m_selectionMode == GridSelectionMode::Multiple ||
                 m_selectionMode == GridSelectionMode::Extended) &&
                selectionModel() && selectionModel()->isSelected(idx)) {
                // Don't pass to QListView — preserve selection for drag
                m_dragPressIntercepted = true;
                event->accept();
                return;
            }
        }
    }
    QListView::mousePressEvent(event);
}

void GridView::mouseMoveEvent(QMouseEvent* event) {
    if (m_canReorderItems && m_dragSourceIndex >= 0 && (event->buttons() & Qt::LeftButton)) {
        if (!m_isDragging) {
            if ((event->pos() - m_dragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
                m_isDragging = true;

                // Collect dragged indices: multi-drag only when pressed on
                // an already-selected item (m_dragPressIntercepted).
                // Pressing on an unselected item drags only that item,
                // same behavior as Extended mode.
                m_dragSourceIndices.clear();
                if (m_dragPressIntercepted && selectionModel()
                    && !selectionModel()->selectedIndexes().isEmpty()) {
                    for (const auto& idx : selectionModel()->selectedIndexes())
                        m_dragSourceIndices.append(idx.row());
                }
                // Ensure primary item is included
                if (!m_dragSourceIndices.contains(m_dragSourceIndex))
                    m_dragSourceIndices.append(m_dragSourceIndex);
                std::sort(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
                m_dragSourceIndices.erase(
                    std::unique(m_dragSourceIndices.begin(), m_dragSourceIndices.end()),
                    m_dragSourceIndices.end());

                m_dropTargetIndex = -1;
                clearDragAnimations();
                m_dragPixmap = renderDragPixmap();
            }
        }

        if (m_isDragging) {
            m_dragCurrentPos = event->pos();
            int target = stabilizedDropIndicatorIndex(event->pos());
            if (target != m_dropTargetIndex) {
                m_dropTargetIndex = target;
                updateDragDisplacement();
            }
            viewport()->update();
            event->accept();
            return;
        }
    }
    // Never pass mouseMoveEvent to QListView — prevents rubber band selection.
    // Selection only needs click events; our custom drag handles all move logic.
}

void GridView::mouseReleaseEvent(QMouseEvent* event) {
    if (m_isDragging && event->button() == Qt::LeftButton) {
        const int dst = m_dropTargetIndex;
        const QList<int> srcs = m_dragSourceIndices;

        if (dst >= 0 && !srcs.isEmpty() && model()) {
            auto* sim = qobject_cast<QStandardItemModel*>(model());
            if (sim) {
                // Remember selected item pointers before rearranging —
                // selection state should be preserved across drag.
                QSet<QStandardItem*> selectedItems;
                if (selectionModel()) {
                    for (const auto& idx : selectionModel()->selectedIndexes())
                        selectedItems.insert(sim->itemFromIndex(idx));
                }

                // Take rows in reverse order to preserve indices
                QList<QList<QStandardItem*>> takenRows;
                for (int i = srcs.size() - 1; i >= 0; --i)
                    takenRows.prepend(sim->takeRow(srcs[i]));

                // dst is a slot among remaining items, which is now the model size
                int insertAt = qMin(dst, sim->rowCount());
                for (auto& row : takenRows) {
                    sim->insertRow(insertAt, row);
                    insertAt++;
                }

                // Restore original selection state by item identity
                selectionModel()->clearSelection();
                for (int r = 0; r < sim->rowCount(); ++r) {
                    if (selectedItems.contains(sim->item(r)))
                        selectionModel()->select(sim->index(r, 0),
                                                 QItemSelectionModel::Select);
                }

                int firstInserted = qMin(dst, sim->rowCount() - (int)srcs.size());
                // Use NoUpdate to avoid setCurrentIndex clearing selection
                // (Extended mode's selectionCommand returns ClearAndSelect)
                selectionModel()->setCurrentIndex(
                    sim->index(firstInserted, 0), QItemSelectionModel::NoUpdate);

                emit itemReordered(srcs.first(), firstInserted);
            }
        }

        m_isDragging = false;
        m_dragSourceIndex = -1;
        m_dragSourceIndices.clear();
        m_dragPressIntercepted = false;
        resetDragReorderFeedback();
        viewport()->update();
        event->accept();
        return;
    }

    if (m_canReorderItems) {
        // No drag happened — apply deferred selection only if we intercepted the press
        if (m_dragPressIntercepted && m_dragSourceIndex >= 0 && model()) {
            QModelIndex idx = model()->index(m_dragSourceIndex, 0);
            if (m_selectionMode == GridSelectionMode::Multiple) {
                selectionModel()->select(idx, QItemSelectionModel::Toggle);
            } else if (m_selectionMode == GridSelectionMode::Extended) {
                if (event->modifiers() & Qt::ControlModifier)
                    selectionModel()->select(idx, QItemSelectionModel::Toggle);
                else
                    selectionModel()->select(idx, QItemSelectionModel::ClearAndSelect);
            }
        }
        m_dragSourceIndex = -1;
        m_dragSourceIndices.clear();
        m_dragPressIntercepted = false;
    }
    m_pressedOnBlank = false;
    QListView::mouseReleaseEvent(event);
}

QPixmap GridView::renderItemPixmap(int row) const {
    if (!model() || row < 0 || row >= model()->rowCount())
        return {};

    QModelIndex idx = model()->index(row, 0);
    QRect rect = QListView::visualRect(idx);
    if (rect.isEmpty()) return {};

    const qreal dpr = devicePixelRatioF();
    QPixmap pix(rect.size() * dpr);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    QStyleOptionViewItem opt;
    FLUENT_INIT_VIEW_ITEM_OPTION(&opt);
    opt.rect = QRect(QPoint(0, 0), rect.size());
    opt.state |= QStyle::State_Selected;
    itemDelegate()->paint(&p, opt, idx);
    p.end();

    return pix;
}

QPixmap GridView::renderDragPixmap() const {
    if (m_dragSourceIndices.isEmpty()) return {};

    // Render the primary item
    QPixmap primary = renderItemPixmap(m_dragSourceIndex);
    if (primary.isNull()) return {};

    const int count = m_dragSourceIndices.size();
    if (count == 1) return primary;

    // Stack effect: offset each layer by a few pixels
    const int stackOffset = 4;
    const int maxStack = qMin(count, 3); // Show at most 3 stacked layers
    const qreal dpr = primary.devicePixelRatio();

    const int baseW = primary.width() / dpr;
    const int baseH = primary.height() / dpr;
    const int totalOffset = stackOffset * (maxStack - 1);
    const int compositeW = baseW + totalOffset;
    const int compositeH = baseH + totalOffset;

    QPixmap composite(QSize(compositeW, compositeH) * dpr);
    composite.setDevicePixelRatio(dpr);
    composite.fill(Qt::transparent);

    QPainter p(&composite);
    p.setRenderHint(QPainter::Antialiasing);

    // Draw back-to-front: farthest stack layer first
    for (int layer = maxStack - 1; layer >= 0; --layer) {
        int srcIdx = (layer == 0) ? m_dragSourceIndex : -1;
        // For layers > 0, pick another source item
        if (layer > 0 && layer < count) {
            int picked = 0;
            for (int s : m_dragSourceIndices) {
                if (s != m_dragSourceIndex) { srcIdx = s; picked++; if (picked >= layer) break; }
            }
        }
        if (srcIdx < 0) continue;

        QPixmap layerPix = (srcIdx == m_dragSourceIndex) ? primary : renderItemPixmap(srcIdx);
        if (layerPix.isNull()) continue;

        int x = stackOffset * layer;
        int y = stackOffset * layer;

        if (layer > 0) p.setOpacity(0.6);
        else p.setOpacity(1.0);
        p.drawPixmap(x, y, layerPix);
    }

    // Draw count badge at top-right
    p.setOpacity(1.0);
    const auto& c = themeColors();
    const int badgeSize = 20;
    const int badgeX = compositeW - badgeSize - 2;
    const int badgeY = 2;
    p.setBrush(c.accentDefault);
    p.setPen(Qt::NoPen);
    p.drawEllipse(badgeX, badgeY, badgeSize, badgeSize);

    QFont badgeFont = p.font();
    badgeFont.setPixelSize(11);
    badgeFont.setBold(true);
    p.setFont(badgeFont);
    p.setPen(Qt::white);
    p.drawText(QRect(badgeX, badgeY, badgeSize, badgeSize),
               Qt::AlignCenter, QString::number(count));

    p.end();
    return composite;
}

int GridView::dropIndicatorIndex(const QPoint& pos) const {
    if (!model()) return 0;

    const QSet<int> srcSet(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
    const int count = model()->rowCount();

    int slot = 0;
    int bestSlot = 0;
    qreal bestDist = std::numeric_limits<qreal>::max();

    for (int i = 0; i < count; ++i) {
        if (srcSet.contains(i)) continue;
        QRect r = QListView::visualRect(model()->index(i, 0));

        // Distance to left edge (insert before this item)
        qreal distL = std::hypot(pos.x() - r.left(), pos.y() - r.center().y());
        if (distL < bestDist) {
            bestDist = distL;
            bestSlot = slot;
        }

        // Distance to right edge (insert after this item)
        qreal distR = std::hypot(pos.x() - r.right(), pos.y() - r.center().y());
        if (distR < bestDist) {
            bestDist = distR;
            bestSlot = slot + 1;
        }

        slot++;
    }
    return bestSlot;
}

int GridView::stabilizedDropIndicatorIndex(const QPoint& pos) const {
    const int candidate = dropIndicatorIndex(pos);
    if (!model() || m_dropTargetIndex < 0 || candidate == m_dropTargetIndex)
        return candidate;

    const QSet<int> srcSet(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
    const int remainingCount = qMax(0, model()->rowCount() - srcSet.size());
    if (m_dropTargetIndex > remainingCount || candidate < 0 || candidate > remainingCount)
        return candidate;

    if (std::abs(candidate - m_dropTargetIndex) != 1)
        return candidate;

    const qreal currentDistance = dropIndicatorDistance(pos, m_dropTargetIndex);
    const qreal candidateDistance = dropIndicatorDistance(pos, candidate);
    if (!std::isfinite(currentDistance) || !std::isfinite(candidateDistance))
        return candidate;

    return (candidateDistance + dropTargetHysteresis() < currentDistance)
               ? candidate
               : m_dropTargetIndex;
}

qreal GridView::dropIndicatorDistance(const QPoint& pos, int slot) const {
    if (!model()) return std::numeric_limits<qreal>::max();

    const QSet<int> srcSet(m_dragSourceIndices.begin(), m_dragSourceIndices.end());
    qreal bestDistance = std::numeric_limits<qreal>::max();
    int currentSlot = 0;

    for (int row = 0; row < model()->rowCount(); ++row) {
        if (srcSet.contains(row)) continue;
        const QRect rect = QListView::visualRect(model()->index(row, 0));

        if (slot == currentSlot) {
            bestDistance = qMin(bestDistance,
                                std::hypot(pos.x() - rect.left(), pos.y() - rect.center().y()));
        }

        if (slot == currentSlot + 1) {
            bestDistance = qMin(bestDistance,
                                std::hypot(pos.x() - rect.right(), pos.y() - rect.center().y()));
        }

        ++currentSlot;
    }

    return bestDistance;
}

qreal GridView::dropTargetHysteresis() const {
    const int basis = qMax(1, qMin(m_cellSize.width(), m_cellSize.height()));
    return qBound<qreal>(4.0, basis * 0.06, 10.0);
}

void GridView::resetDragReorderFeedback() {
    m_dropTargetIndex = -1;
    m_dragPixmap = QPixmap();
    clearDragAnimations();
}

void GridView::updateDragDisplacement() {
    if (m_dragSourceIndices.isEmpty() || m_dropTargetIndex < 0 || !model()) {
        clearDragAnimations();
        return;
    }

    const int itemCount = model()->rowCount();
    const int dragCount = m_dragSourceIndices.size();
    const QSet<int> srcSet(m_dragSourceIndices.begin(), m_dragSourceIndices.end());

    // Grid cell size including spacing
    const QSize cell(m_cellSize.width() + m_hSpacing, m_cellSize.height() + m_vSpacing);

    // Calculate columns from viewport width
    const int vpW = viewport()->width();
    int cols = vpW / cell.width();
    if (cols < 1) cols = 1;
    if (m_maxColumns > 0 && cols > m_maxColumns) cols = m_maxColumns;

    // Helper: compute grid position for a given flat index
    auto gridPos = [&](int idx) -> QPointF {
        int col = idx % cols;
        int row = idx / cols;
        return QPointF(col * cell.width(), row * cell.height());
    };

    // Build ordered list of non-source indices
    QList<int> remaining;
    remaining.reserve(itemCount - dragCount);
    for (int i = 0; i < itemCount; ++i) {
        if (!srcSet.contains(i)) remaining.append(i);
    }

    const int dst = qBound(0, m_dropTargetIndex, remaining.size());

    for (int i = 0; i < itemCount; ++i) {
        QPointF target(0.0, 0.0);
        if (srcSet.contains(i)) {
            // Source items: no displacement (follow cursor via drag pixmap)
            target = QPointF(0.0, 0.0);
        } else {
            // Find rank among remaining items
            int rank = remaining.indexOf(i);
            // Items before dst keep compact position;
            // items at/after dst shift right by dragCount slots
            int finalSlot = (rank < dst) ? rank : rank + dragCount;
            target = gridPos(finalSlot) - gridPos(i);
        }

        auto* oldAnim = m_dragAnims.value(i, nullptr);
        QPointF current = m_dragOffsets.value(i, QPointF(0.0, 0.0));
        if (oldAnim && oldAnim->currentValue().canConvert<QPointF>())
            current = oldAnim->currentValue().toPointF();

        if (oldAnim && m_dragTargetOffsets.contains(i) &&
            dragPointsEqual(m_dragTargetOffsets.value(i), target)) {
            continue;
        }

        if (oldAnim) {
            oldAnim->stop();
            oldAnim->deleteLater();
            m_dragAnims.remove(i);
        }

        m_dragTargetOffsets[i] = target;

        if (dragPointsEqual(current, target)) {
            m_dragOffsets[i] = target;
            continue;
        }

        auto* anim = new QVariantAnimation(this);
        anim->setStartValue(current);
        anim->setEndValue(target);
        anim->setDuration(themeAnimation().fast);
        anim->setEasingCurve(themeAnimation().decelerate);
        connect(anim, &QVariantAnimation::valueChanged, this, [this, i](const QVariant& v) {
            m_dragOffsets[i] = v.toPointF();
            viewport()->update();
        });
        connect(anim, &QVariantAnimation::finished, this, [this, i, target]() {
            m_dragOffsets[i] = target;
            if (auto* a = m_dragAnims.value(i, nullptr)) {
                m_dragAnims.remove(i);
                a->deleteLater();
            }
            viewport()->update();
        });
        m_dragAnims[i] = anim;
        anim->start();
    }
}

void GridView::clearDragAnimations() {
    for (auto it = m_dragAnims.begin(); it != m_dragAnims.end(); ++it) {
        if (it.value()) {
            it.value()->stop();
            it.value()->deleteLater();
        }
    }
    m_dragAnims.clear();
    m_dragOffsets.clear();
    m_dragTargetOffsets.clear();
    viewport()->update();
}

} // namespace fluent::collections
