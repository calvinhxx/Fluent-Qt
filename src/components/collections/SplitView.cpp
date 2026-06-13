#include "SplitView.h"

#include <algorithm>
#include <limits>

#include <QCursor>
#include <QDataStream>
#include <QEvent>
#include <QIODevice>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>

namespace fluent::collections {

namespace {
constexpr quint32 kStateMagic = 0x53505631;
constexpr quint16 kStateVersion = 1;
}

SplitView::SplitView(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

SplitView::~SplitView()
{
    for (PaneRecord& pane : m_panes) {
        if (pane.widget)
            pane.widget->removeEventFilter(this);
        QObject::disconnect(pane.destroyedConnection);
    }
}

void SplitView::onThemeUpdated()
{
    update();
}

int SplitView::addPane(QWidget* pane, const SplitViewPaneOptions& options)
{
    return insertPane(m_panes.size(), pane, options);
}

int SplitView::insertPane(int index, QWidget* pane, const SplitViewPaneOptions& options)
{
    if (!pane || indexOf(pane) >= 0)
        return -1;

    index = qBound(0, index, m_panes.size());
    const int oldCount = m_panes.size();
    PaneRecord record;
    record.widget = pane;
    record.rawWidget = pane;
    const SplitViewPaneOptions normalized = normalizedOptions(options);
    record.minimumSize = normalized.minimumSize;
    record.preferredSize = normalized.preferredSize;
    record.maximumSize = normalized.maximumSize;
    record.fill = normalized.fill;
    record.destroyedConnection = connect(pane, &QObject::destroyed, this, [this, pane]() {
        handlePaneDestroyed(pane);
    });

    pane->setParent(this);
    pane->installEventFilter(this);
    pane->show();

    m_panes.insert(index, record);
    updateLayout();
    emit paneAdded(index, pane);
    if (oldCount != m_panes.size())
        emit paneCountChanged(m_panes.size());
    emit fillPaneIndexChanged(fillPaneIndex());
    return index;
}

bool SplitView::removePane(QWidget* pane)
{
    return removePaneAt(indexOf(pane)) != nullptr;
}

QWidget* SplitView::removePaneAt(int index)
{
    if (!isValidPaneIndex(index))
        return nullptr;

    const int oldCount = m_panes.size();
    PaneRecord record = m_panes.takeAt(index);
    QObject::disconnect(record.destroyedConnection);

    QWidget* pane = record.widget.data();
    if (pane) {
        pane->removeEventFilter(this);
        pane->hide();
        pane->setParent(nullptr);
    }

    if (m_hoveredHandle >= m_handleRects.size())
        m_hoveredHandle = -1;
    clearDragState();
    updateLayout();
    emit paneRemoved(index, pane);
    if (oldCount != m_panes.size())
        emit paneCountChanged(m_panes.size());
    emit fillPaneIndexChanged(fillPaneIndex());
    return pane;
}

QWidget* SplitView::paneAt(int index) const
{
    if (index < 0 || index >= m_panes.size())
        return nullptr;
    return m_panes.at(index).widget.data();
}

int SplitView::indexOf(QWidget* pane) const
{
    if (!pane)
        return -1;
    for (int index = 0; index < m_panes.size(); ++index) {
        if (m_panes.at(index).rawWidget == pane)
            return index;
    }
    return -1;
}

void SplitView::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation)
        return;

    m_orientation = orientation;
    clearDragState();
    updateLayout();
    emit orientationChanged(m_orientation);
}

void SplitView::setHandleWidth(int width)
{
    const int normalized = std::max(1, width);
    if (m_handleWidth == normalized)
        return;

    m_handleWidth = normalized;
    if (m_handleVisualThickness > m_handleWidth)
        m_handleVisualThickness = m_handleWidth;
    updateLayout();
    emit handleWidthChanged(m_handleWidth);
}

void SplitView::setHandleVisualThickness(int thickness)
{
    const int normalized = qBound(1, thickness, m_handleWidth);
    if (m_handleVisualThickness == normalized)
        return;

    m_handleVisualThickness = normalized;
    update();
    emit handleVisualThicknessChanged(m_handleVisualThickness);
}

int SplitView::fillPaneIndex() const
{
    return effectiveFillPaneIndex();
}

void SplitView::setFillPaneIndex(int index)
{
    if (!isValidPaneIndex(index))
        index = -1;

    const int oldFill = fillPaneIndex();
    for (int paneIndex = 0; paneIndex < m_panes.size(); ++paneIndex)
        m_panes[paneIndex].fill = paneIndex == index;

    updateLayout();
    const int newFill = fillPaneIndex();
    if (oldFill != newFill)
        emit fillPaneIndexChanged(newFill);
    if (index >= 0)
        emit paneConfigurationChanged(index);
}

void SplitView::setDefaultSizeHint(const QSize& size)
{
    const QSize normalized = normalizedHintSize(size);
    if (m_defaultSizeHint == normalized)
        return;

    m_defaultSizeHint = normalized;
    updateGeometry();
    emit defaultSizeHintChanged(m_defaultSizeHint);
}

void SplitView::setDefaultMinimumSizeHint(const QSize& size)
{
    const QSize normalized = normalizedHintSize(size);
    if (m_defaultMinimumSizeHint == normalized)
        return;

    m_defaultMinimumSizeHint = normalized;
    updateGeometry();
    emit defaultMinimumSizeHintChanged(m_defaultMinimumSizeHint);
}

void SplitView::setPaneMinimumSize(QWidget* pane, int size)
{
    setPaneMinimumSize(indexOf(pane), size);
}

void SplitView::setPaneMinimumSize(int index, int size)
{
    if (!isValidPaneIndex(index))
        return;

    PaneRecord& pane = m_panes[index];
    const int normalized = std::max(0, size);
    if (pane.minimumSize == normalized)
        return;

    pane.minimumSize = normalized;
    normalizePane(pane);
    updateLayout();
    emit paneConfigurationChanged(index);
}

int SplitView::paneMinimumSize(QWidget* pane) const
{
    return paneMinimumSize(indexOf(pane));
}

int SplitView::paneMinimumSize(int index) const
{
    return isValidPaneIndex(index) ? m_panes.at(index).minimumSize : -1;
}

void SplitView::setPanePreferredSize(QWidget* pane, int size)
{
    setPanePreferredSize(indexOf(pane), size);
}

void SplitView::setPanePreferredSize(int index, int size)
{
    if (!isValidPaneIndex(index))
        return;

    PaneRecord& pane = m_panes[index];
    const int oldSize = pane.preferredSize;
    pane.preferredSize = size;
    normalizePane(pane);
    updateLayout();
    emitPaneSizeIfChanged(index, oldSize);
    emit paneConfigurationChanged(index);
}

int SplitView::panePreferredSize(QWidget* pane) const
{
    return panePreferredSize(indexOf(pane));
}

int SplitView::panePreferredSize(int index) const
{
    return isValidPaneIndex(index) ? m_panes.at(index).preferredSize : -1;
}

void SplitView::setPaneMaximumSize(QWidget* pane, int size)
{
    setPaneMaximumSize(indexOf(pane), size);
}

void SplitView::setPaneMaximumSize(int index, int size)
{
    if (!isValidPaneIndex(index))
        return;

    PaneRecord& pane = m_panes[index];
    const int normalized = std::max(0, size);
    if (pane.maximumSize == normalized)
        return;

    pane.maximumSize = normalized;
    normalizePane(pane);
    updateLayout();
    emit paneConfigurationChanged(index);
}

int SplitView::paneMaximumSize(QWidget* pane) const
{
    return paneMaximumSize(indexOf(pane));
}

int SplitView::paneMaximumSize(int index) const
{
    return isValidPaneIndex(index) ? m_panes.at(index).maximumSize : -1;
}

void SplitView::setPaneFill(QWidget* pane, bool fill)
{
    setPaneFill(indexOf(pane), fill);
}

void SplitView::setPaneFill(int index, bool fill)
{
    if (!isValidPaneIndex(index))
        return;

    if (m_panes[index].fill == fill)
        return;

    const int oldFill = fillPaneIndex();
    m_panes[index].fill = fill;
    updateLayout();
    const int newFill = fillPaneIndex();
    if (oldFill != newFill)
        emit fillPaneIndexChanged(newFill);
    emit paneConfigurationChanged(index);
}

bool SplitView::isPaneFill(QWidget* pane) const
{
    return isPaneFill(indexOf(pane));
}

bool SplitView::isPaneFill(int index) const
{
    return isValidPaneIndex(index) && m_panes.at(index).fill;
}

QRect SplitView::paneGeometry(int index) const
{
    return isValidPaneIndex(index) ? m_panes.at(index).geometry : QRect();
}

QRect SplitView::handleGeometry(int index) const
{
    if (index < 0 || index >= m_handleRects.size())
        return QRect();
    return m_handleRects.at(index);
}

QByteArray SplitView::saveState() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << kStateMagic << kStateVersion;
    stream << static_cast<qint32>(m_orientation);
    stream << static_cast<qint32>(fillPaneIndex());
    stream << static_cast<qint32>(m_panes.size());
    for (const PaneRecord& pane : m_panes)
        stream << static_cast<qint32>(pane.preferredSize);
    return data;
}

bool SplitView::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    quint32 magic = 0;
    quint16 version = 0;
    qint32 orientationValue = 0;
    qint32 fillIndex = -1;
    qint32 paneCount = 0;
    QVector<int> preferredSizes;

    stream >> magic >> version >> orientationValue >> fillIndex >> paneCount;
    if (stream.status() != QDataStream::Ok || magic != kStateMagic || version != kStateVersion)
        return false;
    if (paneCount != m_panes.size())
        return false;
    if (orientationValue != Qt::Horizontal && orientationValue != Qt::Vertical)
        return false;
    if (fillIndex < -1 || fillIndex >= paneCount)
        return false;

    preferredSizes.reserve(paneCount);
    for (int index = 0; index < paneCount; ++index) {
        qint32 preferredSize = 0;
        stream >> preferredSize;
        if (stream.status() != QDataStream::Ok)
            return false;
        preferredSizes.append(preferredSize);
    }

    m_orientation = static_cast<Qt::Orientation>(orientationValue);
    for (int index = 0; index < m_panes.size(); ++index) {
        m_panes[index].preferredSize = preferredSizes.at(index);
        m_panes[index].fill = index == fillIndex;
        normalizePane(m_panes[index]);
    }
    clearDragState();
    updateLayout();
    emit orientationChanged(m_orientation);
    emit fillPaneIndexChanged(this->fillPaneIndex());
    return true;
}

QSize SplitView::sizeHint() const
{
    if (m_panes.isEmpty())
        return m_defaultSizeHint;

    const QVector<int> visibleIndexes = visiblePaneIndexes();
    int totalLength = 0;
    int maximumCross = 0;
    for (int index : visibleIndexes) {
        const QWidget* pane = m_panes.at(index).widget.data();
        const QSize paneHint = pane ? pane->sizeHint() : QSize();
        totalLength += m_panes.at(index).preferredSize;
        maximumCross = std::max(maximumCross, crossLength(paneHint));
    }
    totalLength += std::max(0, static_cast<int>(visibleIndexes.size()) - 1) * m_handleWidth;
    maximumCross = std::max(maximumCross, crossLength(m_defaultSizeHint));
    return m_orientation == Qt::Horizontal ? QSize(totalLength, maximumCross) : QSize(maximumCross, totalLength);
}

QSize SplitView::minimumSizeHint() const
{
    if (m_panes.isEmpty())
        return m_defaultMinimumSizeHint;

    const QVector<int> visibleIndexes = visiblePaneIndexes();
    int totalLength = 0;
    int maximumCross = 0;
    for (int index : visibleIndexes) {
        const QWidget* pane = m_panes.at(index).widget.data();
        const QSize paneHint = pane ? pane->minimumSizeHint() : QSize();
        totalLength += m_panes.at(index).minimumSize;
        maximumCross = std::max(maximumCross, crossLength(paneHint));
    }
    totalLength += std::max(0, static_cast<int>(visibleIndexes.size()) - 1) * m_handleWidth;
    maximumCross = std::max(maximumCross, crossLength(m_defaultMinimumSizeHint));
    return m_orientation == Qt::Horizontal ? QSize(totalLength, maximumCross) : QSize(maximumCross, totalLength);
}

bool SplitView::eventFilter(QObject* watched, QEvent* event)
{
    if (!m_layingOut && (event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        for (const PaneRecord& pane : m_panes) {
            if (pane.rawWidget == watched) {
                updateLayout();
                break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SplitView::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto colors = themeColors();
    for (int index = 0; index < m_handleRects.size(); ++index) {
        const QRect handleRect = m_handleRects.at(index);
        QColor color = colors.strokeDivider;
        if (!isEnabled()) {
            color = colors.textDisabled;
        } else if (index == m_pressedHandle) {
            color = colors.accentDefault;
        } else if (index == m_hoveredHandle) {
            color = colors.strokeStrong;
            painter.fillRect(handleRect, colors.subtleSecondary);
        }

        QRect visualRect = handleRect;
        if (m_orientation == Qt::Horizontal) {
            const int x = handleRect.center().x() - m_handleVisualThickness / 2;
            visualRect = QRect(x, handleRect.top() + 4, m_handleVisualThickness, std::max(0, handleRect.height() - 8));
        } else {
            const int y = handleRect.center().y() - m_handleVisualThickness / 2;
            visualRect = QRect(handleRect.left() + 4, y, std::max(0, handleRect.width() - 8), m_handleVisualThickness);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawRoundedRect(visualRect, m_handleVisualThickness / 2.0, m_handleVisualThickness / 2.0);
    }
}

void SplitView::resizeEvent(QResizeEvent*)
{
    updateLayout();
}

void SplitView::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
    setHoveredHandle(hitTestHandle(mapFromGlobal(QCursor::pos())));
}

void SplitView::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    if (!m_resizing)
        setHoveredHandle(-1);
}

void SplitView::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const int handleIndex = hitTestHandle(fluentMousePos(event));
    if (handleIndex < 0) {
        QWidget::mousePressEvent(event);
        return;
    }

    const HandlePair pair = m_handlePairs.at(handleIndex);
    m_pressedHandle = handleIndex;
    m_drag.handleIndex = handleIndex;
    m_drag.leadingPane = pair.leadingPane;
    m_drag.trailingPane = pair.trailingPane;
    m_drag.startPosition = axisPosition(fluentMousePos(event));
    // Baseline the drag on the panes' actual laid-out lengths, not their stored
    // preferredSize. The fill pane never writes its displayed length back to
    // preferredSize, so using the stored value as a baseline let a non-fill pane
    // ratchet larger on every drag and eventually swallow the fill pane. The real
    // geometry always sums to the true combined span the boundary moves within.
    // zh_CN: 以面板实际布局长度（而非存储的 preferredSize）作为拖拽基准。填充面板从不把
    // 显示长度写回 preferredSize，用存储值作基准会让非填充面板每次拖拽不断变大、最终吞掉
    // 填充面板。实际 geometry 始终等于边界可移动的真实合并跨度。
    m_drag.leadingStartSize = dragStartLength(pair.leadingPane);
    m_drag.trailingStartSize = dragStartLength(pair.trailingPane);
    m_drag.changed = false;
    setResizing(true);
    grabMouse();
    update();
    event->accept();
}

void SplitView::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint position = fluentMousePos(event);
    if (!m_resizing || m_drag.handleIndex < 0) {
        setHoveredHandle(hitTestHandle(position));
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int delta = axisPosition(position) - m_drag.startPosition;
    PaneRecord& leadingPane = m_panes[m_drag.leadingPane];
    PaneRecord& trailingPane = m_panes[m_drag.trailingPane];
    const int sum = m_drag.leadingStartSize + m_drag.trailingStartSize;
    const int lower = std::max(leadingPane.minimumSize, sum - trailingPane.maximumSize);
    const int upper = std::min(leadingPane.maximumSize, sum - trailingPane.minimumSize);
    const int oldLeading = leadingPane.preferredSize;
    const int oldTrailing = trailingPane.preferredSize;
    const int leadingSize = lower <= upper ? qBound(lower, m_drag.leadingStartSize + delta, upper) : oldLeading;
    const int trailingSize = sum - leadingSize;

    leadingPane.preferredSize = leadingSize;
    trailingPane.preferredSize = trailingSize;
    normalizePane(leadingPane);
    normalizePane(trailingPane);
    m_drag.changed = m_drag.changed || oldLeading != leadingPane.preferredSize || oldTrailing != trailingPane.preferredSize;
    updateLayout();
    emitPaneSizeIfChanged(m_drag.leadingPane, oldLeading);
    emitPaneSizeIfChanged(m_drag.trailingPane, oldTrailing);
    event->accept();
}

void SplitView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_resizing && event->button() == Qt::LeftButton) {
        releaseMouse();
        setResizing(false);
        m_pressedHandle = -1;
        clearDragState();
        setHoveredHandle(hitTestHandle(fluentMousePos(event)));
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

int SplitView::axisPosition(const QPoint& point) const
{
    return m_orientation == Qt::Horizontal ? point.x() : point.y();
}

int SplitView::axisLength(const QSize& size) const
{
    return m_orientation == Qt::Horizontal ? size.width() : size.height();
}

int SplitView::dragStartLength(int index) const
{
    if (!isValidPaneIndex(index))
        return 0;
    const int geometryLength = axisLength(m_panes.at(index).geometry.size());
    return geometryLength > 0 ? geometryLength : m_panes.at(index).preferredSize;
}

int SplitView::crossLength(const QSize& size) const
{
    return m_orientation == Qt::Horizontal ? size.height() : size.width();
}

QRect SplitView::makePaneRect(int origin, int length) const
{
    return m_orientation == Qt::Horizontal
        ? QRect(origin, 0, length, height())
        : QRect(0, origin, width(), length);
}

QRect SplitView::makeHandleRect(int origin) const
{
    return m_orientation == Qt::Horizontal
        ? QRect(origin, 0, m_handleWidth, height())
        : QRect(0, origin, width(), m_handleWidth);
}

QSize SplitView::normalizedHintSize(const QSize& size)
{
    return QSize(std::max(1, size.width()), std::max(1, size.height()));
}

SplitViewPaneOptions SplitView::normalizedOptions(const SplitViewPaneOptions& options)
{
    SplitViewPaneOptions normalized = options;
    normalized.minimumSize = std::max(0, normalized.minimumSize);
    normalized.maximumSize = std::max(normalized.minimumSize, normalized.maximumSize);
    normalized.preferredSize = qBound(normalized.minimumSize, normalized.preferredSize, normalized.maximumSize);
    return normalized;
}

void SplitView::normalizePane(PaneRecord& pane)
{
    pane.minimumSize = std::max(0, pane.minimumSize);
    pane.maximumSize = std::max(pane.minimumSize, pane.maximumSize);
    pane.preferredSize = qBound(pane.minimumSize, pane.preferredSize, pane.maximumSize);
}

bool SplitView::isPaneExplicitlyVisible(const PaneRecord& pane) const
{
    return pane.widget && !pane.widget->isHidden();
}

QVector<int> SplitView::visiblePaneIndexes() const
{
    QVector<int> indexes;
    for (int index = 0; index < m_panes.size(); ++index) {
        if (isPaneExplicitlyVisible(m_panes.at(index)))
            indexes.append(index);
    }
    return indexes;
}

int SplitView::effectiveFillPaneIndex(const QVector<int>& visibleIndexes) const
{
    if (visibleIndexes.isEmpty())
        return -1;
    for (int index : visibleIndexes) {
        if (m_panes.at(index).fill)
            return index;
    }
    return visibleIndexes.last();
}

int SplitView::effectiveFillPaneIndex() const
{
    return effectiveFillPaneIndex(visiblePaneIndexes());
}

void SplitView::updateLayout()
{
    m_layingOut = true;
    m_handleRects.clear();
    m_handlePairs.clear();

    const QVector<int> visibleIndexes = visiblePaneIndexes();
    if (visibleIndexes.isEmpty()) {
        for (PaneRecord& pane : m_panes)
            pane.geometry = QRect();
        m_layingOut = false;
        update();
        return;
    }

    const int fillIndex = effectiveFillPaneIndex(visibleIndexes);
    const int handleTotal = std::max(0, static_cast<int>(visibleIndexes.size()) - 1) * m_handleWidth;
    const int availableLength = std::max(0, axisLength(size()) - handleTotal);
    QVector<int> lengths(m_panes.size(), 0);
    int fixedLength = 0;

    for (int index : visibleIndexes) {
        PaneRecord& pane = m_panes[index];
        normalizePane(pane);
        if (index == fillIndex)
            continue;
        lengths[index] = pane.preferredSize;
        fixedLength += lengths[index];
    }

    if (fillIndex >= 0) {
        PaneRecord& fillPane = m_panes[fillIndex];
        lengths[fillIndex] = qBound(fillPane.minimumSize, availableLength - fixedLength, fillPane.maximumSize);
    }

    int totalLength = 0;
    for (int index : visibleIndexes)
        totalLength += lengths[index];

    if (totalLength > availableLength) {
        int overflow = totalLength - availableLength;
        for (int reverseIndex = visibleIndexes.size() - 1; reverseIndex >= 0 && overflow > 0; --reverseIndex) {
            const int index = visibleIndexes.at(reverseIndex);
            const int shrink = std::min(overflow, lengths[index] - m_panes.at(index).minimumSize);
            lengths[index] -= shrink;
            overflow -= shrink;
        }
    } else if (totalLength < availableLength && fillIndex >= 0) {
        const int grow = std::min(availableLength - totalLength, m_panes.at(fillIndex).maximumSize - lengths[fillIndex]);
        lengths[fillIndex] += grow;
    }

    int origin = 0;
    for (int visibleOffset = 0; visibleOffset < visibleIndexes.size(); ++visibleOffset) {
        const int index = visibleIndexes.at(visibleOffset);
        PaneRecord& pane = m_panes[index];
        pane.geometry = makePaneRect(origin, lengths[index]);
        if (pane.widget) {
            pane.widget->setGeometry(pane.geometry);
            pane.widget->show();
        }
        origin += lengths[index];

        if (visibleOffset < visibleIndexes.size() - 1) {
            m_handleRects.append(makeHandleRect(origin));
            m_handlePairs.append({index, visibleIndexes.at(visibleOffset + 1)});
            origin += m_handleWidth;
        }
    }

    for (int index = 0; index < m_panes.size(); ++index) {
        if (!visibleIndexes.contains(index))
            m_panes[index].geometry = QRect();
    }

    if (m_hoveredHandle >= m_handleRects.size())
        m_hoveredHandle = -1;
    if (m_pressedHandle >= m_handleRects.size())
        m_pressedHandle = -1;
    updateCursorForHover();
    m_layingOut = false;
    update();
}

void SplitView::updateCursorForHover()
{
    if (m_hoveredHandle >= 0 || m_resizing)
        setCursor(m_orientation == Qt::Horizontal ? Qt::SplitHCursor : Qt::SplitVCursor);
    else
        unsetCursor();
}

int SplitView::hitTestHandle(const QPoint& position) const
{
    if (!isEnabled())
        return -1;
    for (int index = 0; index < m_handleRects.size(); ++index) {
        if (m_handleRects.at(index).contains(position))
            return index;
    }
    return -1;
}

void SplitView::setResizing(bool resizing)
{
    if (m_resizing == resizing)
        return;
    m_resizing = resizing;
    updateCursorForHover();
    emit resizingChanged(m_resizing);
}

void SplitView::setHoveredHandle(int index)
{
    if (m_hoveredHandle == index)
        return;
    m_hoveredHandle = index;
    updateCursorForHover();
    update();
}

void SplitView::clearDragState()
{
    m_drag = DragState();
    m_pressedHandle = -1;
    setResizing(false);
}

void SplitView::emitPaneSizeIfChanged(int index, int oldSize)
{
    if (!isValidPaneIndex(index))
        return;
    const int size = m_panes.at(index).preferredSize;
    if (size != oldSize)
        emit paneSizeChanged(index, size);
}

void SplitView::handlePaneDestroyed(QWidget* pane)
{
    const int index = indexOf(pane);
    if (index < 0)
        return;

    const int oldCount = m_panes.size();
    m_panes.removeAt(index);
    clearDragState();
    updateLayout();
    if (oldCount != m_panes.size())
        emit paneCountChanged(m_panes.size());
    emit paneRemoved(index, nullptr);
    emit fillPaneIndexChanged(fillPaneIndex());
}

bool SplitView::isValidPaneIndex(int index) const
{
    return index >= 0 && index < m_panes.size();
}

} // namespace fluent::collections
