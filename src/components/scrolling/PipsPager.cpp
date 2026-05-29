#include "PipsPager.h"

#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QSizePolicy>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace fluent::scrolling {

namespace {
int normalizedMetric(int value)
{
    return std::max(1, value);
}

int hoveredDiameter(int restDiameter)
{
    return restDiameter + 1;
}

int pressedDiameter(int restDiameter)
{
    return std::max(1, restDiameter - 1);
}
}

PipsPager::PipsPager(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setAccessibleName(QStringLiteral("PipsPager"));
    m_selectionAnimationDuration = themeAnimation().normal;
    m_selectedVisualOffsetAnimation = new QPropertyAnimation(this, "selectedVisualOffset", this);
    m_selectedVisualOffsetAnimation->setDuration(m_selectionAnimationDuration);
    m_selectedVisualOffsetAnimation->setEasingCurve(themeAnimation().decelerate);
    m_visibleWindowOffsetAnimation = new QPropertyAnimation(this, "visibleWindowOffset", this);
    m_visibleWindowOffsetAnimation->setDuration(m_selectionAnimationDuration);
    m_visibleWindowOffsetAnimation->setEasingCurve(themeAnimation().decelerate);
    updateAccessibleText();
}

void PipsPager::setNumberOfPages(int pages)
{
    const int normalizedPages = std::max(0, pages);
    const int oldPages = m_numberOfPages;
    const int oldSelectedIndex = m_selectedPageIndex;

    m_numberOfPages = normalizedPages;
    m_selectedPageIndex = clampedPageIndex(m_selectedPageIndex);

    const bool pagesChanged = oldPages != m_numberOfPages;
    const bool selectionChanged = oldSelectedIndex != m_selectedPageIndex;
    if (!pagesChanged && !selectionChanged) return;

    clearInteractionState();
    syncSelectedVisualOffset();
    syncVisibleWindowOffset();
    updateAccessibleText();
    updateGeometry();
    update();

    if (pagesChanged) emit numberOfPagesChanged(m_numberOfPages);
    if (selectionChanged) {
        emit selectedPageIndexChanged(m_selectedPageIndex);
        emit selectedIndexChanged(oldSelectedIndex, m_selectedPageIndex);
    }
}

void PipsPager::setSelectedPageIndex(int index)
{
    const int nextIndex = clampedPageIndex(index);
    if (m_selectedPageIndex == nextIndex) return;

    const int oldIndex = m_selectedPageIndex;
    const qreal oldVisualOffset = m_selectedVisualOffset;
    const qreal oldWindowOffset = m_visibleWindowOffset;
    m_selectedPageIndex = nextIndex;
    const qreal nextVisualOffset = currentSelectedVisualOffset();
    const qreal nextWindowOffset = currentVisibleWindowOffset();
    clearInteractionState();
    updateAccessibleText();
    animateSelectedVisualOffset(oldVisualOffset, nextVisualOffset);
    animateVisibleWindowOffset(oldWindowOffset, nextWindowOffset);
    update();

    emit selectedPageIndexChanged(m_selectedPageIndex);
    emit selectedIndexChanged(oldIndex, m_selectedPageIndex);
}

void PipsPager::setMaxVisiblePips(int count)
{
    const int normalizedCount = std::max(0, count);
    if (m_maxVisiblePips == normalizedCount) return;

    m_maxVisiblePips = normalizedCount;
    clearInteractionState();
    syncSelectedVisualOffset();
    syncVisibleWindowOffset();
    updateGeometry();
    update();
    emit maxVisiblePipsChanged(m_maxVisiblePips);
}

void PipsPager::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation) return;

    m_orientation = orientation;
    clearInteractionState();
    syncSelectedVisualOffset();
    syncVisibleWindowOffset();
    updateGeometry();
    update();
    emit orientationChanged(m_orientation);
}

void PipsPager::setPreviousButtonVisibility(PipsPagerButtonVisibility visibility)
{
    if (m_previousButtonVisibility == visibility) return;

    m_previousButtonVisibility = visibility;
    clearInteractionState();
    syncSelectedVisualOffset();
    syncVisibleWindowOffset();
    updateGeometry();
    update();
    emit previousButtonVisibilityChanged(m_previousButtonVisibility);
}

void PipsPager::setNextButtonVisibility(PipsPagerButtonVisibility visibility)
{
    if (m_nextButtonVisibility == visibility) return;

    m_nextButtonVisibility = visibility;
    clearInteractionState();
    syncSelectedVisualOffset();
    syncVisibleWindowOffset();
    updateGeometry();
    update();
    emit nextButtonVisibilityChanged(m_nextButtonVisibility);
}

void PipsPager::setPipCellSize(int size)
{
    const int normalizedSize = normalizedMetric(size);
    if (m_pipCellSize == normalizedSize) return;

    m_pipCellSize = normalizedSize;
    syncSelectedVisualOffset();
    syncVisibleWindowOffset();
    updateGeometry();
    update();
    emit pipCellSizeChanged(m_pipCellSize);
}

void PipsPager::setInactivePipDiameter(int diameter)
{
    const int normalizedDiameter = normalizedMetric(diameter);
    if (m_inactivePipDiameter == normalizedDiameter) return;

    m_inactivePipDiameter = normalizedDiameter;
    update();
    emit inactivePipDiameterChanged(m_inactivePipDiameter);
}

void PipsPager::setSelectedPipDiameter(int diameter)
{
    const int normalizedDiameter = normalizedMetric(diameter);
    if (m_selectedPipDiameter == normalizedDiameter) return;

    m_selectedPipDiameter = normalizedDiameter;
    update();
    emit selectedPipDiameterChanged(m_selectedPipDiameter);
}

void PipsPager::setNavigationButtonSize(int size)
{
    const int normalizedSize = normalizedMetric(size);
    if (m_navigationButtonSize == normalizedSize) return;

    m_navigationButtonSize = normalizedSize;
    syncSelectedVisualOffset();
    syncVisibleWindowOffset();
    updateGeometry();
    update();
    emit navigationButtonSizeChanged(m_navigationButtonSize);
}

void PipsPager::setNavigationIconSize(int size)
{
    const int normalizedSize = normalizedMetric(size);
    if (m_navigationIconSize == normalizedSize) return;

    m_navigationIconSize = normalizedSize;
    update();
    emit navigationIconSizeChanged(m_navigationIconSize);
}

void PipsPager::setSelectionAnimationEnabled(bool enabled)
{
    if (m_selectionAnimationEnabled == enabled) return;

    m_selectionAnimationEnabled = enabled;
    if (!m_selectionAnimationEnabled) {
        syncSelectedVisualOffset();
        syncVisibleWindowOffset();
    }
    emit selectionAnimationEnabledChanged(m_selectionAnimationEnabled);
}

void PipsPager::setSelectionAnimationDuration(int durationMs)
{
    const int normalizedDuration = std::max(0, durationMs);
    if (m_selectionAnimationDuration == normalizedDuration) return;

    m_selectionAnimationDuration = normalizedDuration;
    if (m_selectedVisualOffsetAnimation) {
        m_selectedVisualOffsetAnimation->setDuration(m_selectionAnimationDuration);
    }
    if (m_visibleWindowOffsetAnimation) {
        m_visibleWindowOffsetAnimation->setDuration(m_selectionAnimationDuration);
    }
    emit selectionAnimationDurationChanged(m_selectionAnimationDuration);
}

int PipsPager::visiblePipCount() const
{
    if (m_numberOfPages <= 0 || m_maxVisiblePips <= 0) return 0;
    return std::min(m_numberOfPages, m_maxVisiblePips);
}

int PipsPager::firstVisiblePage() const
{
    const int count = visiblePipCount();
    if (count <= 0) return 0;

    const int maxFirst = m_numberOfPages - count;
    const int centeredFirst = m_selectedPageIndex - count / 2;
    return std::max(0, std::min(centeredFirst, maxFirst));
}

QRect PipsPager::pipHitRect(int pageIndex) const
{
    const int count = visiblePipCount();
    const int firstPage = firstVisiblePage();
    if (count <= 0 || pageIndex < firstPage || pageIndex >= firstPage + count) {
        return QRect();
    }

    const int indexInWindow = pageIndex - firstPage;
    return pipCellRect(indexInWindow);
}

QRect PipsPager::previousButtonRect() const
{
    if (!reservesPreviousButton()) return QRect();

    const QRect bounds = controlRect();
    if (m_orientation == Qt::Horizontal) {
        return QRect(bounds.left(), bounds.top(), m_navigationButtonSize, m_navigationButtonSize);
    }
    return QRect(bounds.left(), bounds.top(), m_navigationButtonSize, m_navigationButtonSize);
}

QRect PipsPager::nextButtonRect() const
{
    if (!reservesNextButton()) return QRect();

    const QRect bounds = controlRect();
    if (m_orientation == Qt::Horizontal) {
        return QRect(bounds.right() - m_navigationButtonSize + 1,
                     bounds.top(),
                     m_navigationButtonSize,
                     m_navigationButtonSize);
    }
    return QRect(bounds.left(),
                 bounds.bottom() - m_navigationButtonSize + 1,
                 m_navigationButtonSize,
                 m_navigationButtonSize);
}

bool PipsPager::hasPreviousPage() const
{
    return m_numberOfPages > 0 && m_selectedPageIndex > 0;
}

bool PipsPager::hasNextPage() const
{
    return m_numberOfPages > 0 && m_selectedPageIndex < m_numberOfPages - 1;
}

bool PipsPager::goToPreviousPage()
{
    if (!hasPreviousPage()) return false;
    setSelectedPageIndex(m_selectedPageIndex - 1);
    return true;
}

bool PipsPager::goToNextPage()
{
    if (!hasNextPage()) return false;
    setSelectedPageIndex(m_selectedPageIndex + 1);
    return true;
}

QSize PipsPager::sizeHint() const
{
    return controlSize();
}

QSize PipsPager::minimumSizeHint() const
{
    return controlSize();
}

void PipsPager::onThemeUpdated()
{
    if (m_selectedVisualOffsetAnimation) {
        m_selectedVisualOffsetAnimation->setEasingCurve(themeAnimation().decelerate);
    }
    if (m_visibleWindowOffsetAnimation) {
        m_visibleWindowOffsetAnimation->setEasingCurve(themeAnimation().decelerate);
    }
    update();
}

void PipsPager::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int count = visiblePipCount();
    if (count > 0) {
        painter.save();
        painter.setClipRect(pipsClipRect());
        const int firstPaintedPage = std::max(0, static_cast<int>(std::floor(m_visibleWindowOffset)) - 1);
        const int lastPaintedPage = std::min(m_numberOfPages - 1,
                                             static_cast<int>(std::ceil(m_visibleWindowOffset + count)) + 1);
        for (int pageIndex = firstPaintedPage; pageIndex <= lastPaintedPage; ++pageIndex) {
            if (pageIndex == m_selectedPageIndex)
                continue;
            drawPipAtVisualOffset(painter, pageIndex, pageIndex - m_visibleWindowOffset);
        }
        painter.restore();
    }
    drawSelectedPip(painter);

    if (shouldPaintPreviousButton()) {
        drawButton(painter, previousButtonRect(), true);
    }
    if (shouldPaintNextButton()) {
        drawButton(painter, nextButtonRect(), false);
    }
}

void PipsPager::enterEvent(FluentEnterEvent* event)
{
    m_controlHovered = true;
    update();
    QWidget::enterEvent(event);
}

void PipsPager::leaveEvent(QEvent* event)
{
    m_controlHovered = false;
    m_hoveredTarget = HitTarget();
    unsetCursor();
    update();
    QWidget::leaveEvent(event);
}

void PipsPager::mouseMoveEvent(QMouseEvent* event)
{
    if (!isEnabled()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const HitTarget target = hitTest(fluentMousePos(event));
    if (m_hoveredTarget != target) {
        m_hoveredTarget = target;
        if (target.kind == HitKind::None) {
            unsetCursor();
        } else {
            setCursor(Qt::PointingHandCursor);
        }
        update();
    }

    QWidget::mouseMoveEvent(event);
}

void PipsPager::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const HitTarget target = hitTest(fluentMousePos(event));
    if (target.kind == HitKind::None) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_pressedTarget = target;
    setFocus(Qt::MouseFocusReason);
    update();
    event->accept();
}

void PipsPager::mouseReleaseEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const HitTarget pressedTarget = m_pressedTarget;
    const HitTarget releaseTarget = hitTest(fluentMousePos(event));
    m_pressedTarget = HitTarget();
    update();

    if (pressedTarget.kind != HitKind::None && pressedTarget == releaseTarget) {
        if (releaseTarget.kind == HitKind::PreviousButton) {
            goToPreviousPage();
        } else if (releaseTarget.kind == HitKind::NextButton) {
            goToNextPage();
        } else if (releaseTarget.kind == HitKind::Pip) {
            setSelectedPageIndex(releaseTarget.pageIndex);
        }
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void PipsPager::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled() || m_numberOfPages <= 0) {
        QWidget::keyPressEvent(event);
        return;
    }

    bool handled = false;
    switch (event->key()) {
    case Qt::Key_Left:
        if (m_orientation == Qt::Horizontal) goToPreviousPage();
        handled = (m_orientation == Qt::Horizontal);
        break;
    case Qt::Key_Right:
        if (m_orientation == Qt::Horizontal) goToNextPage();
        handled = (m_orientation == Qt::Horizontal);
        break;
    case Qt::Key_Up:
        if (m_orientation == Qt::Vertical) goToPreviousPage();
        handled = (m_orientation == Qt::Vertical);
        break;
    case Qt::Key_Down:
        if (m_orientation == Qt::Vertical) goToNextPage();
        handled = (m_orientation == Qt::Vertical);
        break;
    case Qt::Key_Home:
        setSelectedPageIndex(0);
        handled = true;
        break;
    case Qt::Key_End:
        setSelectedPageIndex(m_numberOfPages - 1);
        handled = true;
        break;
    default:
        break;
    }

    if (handled) {
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void PipsPager::focusInEvent(QFocusEvent* event)
{
    update();
    QWidget::focusInEvent(event);
}

void PipsPager::focusOutEvent(QFocusEvent* event)
{
    m_pressedTarget = HitTarget();
    update();
    QWidget::focusOutEvent(event);
}

void PipsPager::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange) {
        clearInteractionState();
        update();
    }
}

int PipsPager::clampedPageIndex(int index) const
{
    if (m_numberOfPages <= 0) return 0;
    return std::max(0, std::min(index, m_numberOfPages - 1));
}

QSize PipsPager::controlSize() const
{
    const int previousSlot = reservesPreviousButton() ? m_navigationButtonSize : 0;
    const int nextSlot = reservesNextButton() ? m_navigationButtonSize : 0;
    const int pipLength = visiblePipCount() * m_pipCellSize;
    const int mainLength = previousSlot + pipLength + nextSlot;
    const int crossLength = (previousSlot > 0 || nextSlot > 0)
        ? m_navigationButtonSize
        : m_pipCellSize;

    if (m_orientation == Qt::Horizontal) {
        return QSize(mainLength, crossLength);
    }
    return QSize(crossLength, mainLength);
}

QRect PipsPager::controlRect() const
{
    QRect bounds(QPoint(0, 0), controlSize());
    bounds.moveCenter(rect().center());
    return bounds;
}

bool PipsPager::reservesPreviousButton() const
{
    return m_previousButtonVisibility != PipsPagerButtonVisibility::Collapsed;
}

bool PipsPager::reservesNextButton() const
{
    return m_nextButtonVisibility != PipsPagerButtonVisibility::Collapsed;
}

bool PipsPager::shouldPaintPreviousButton() const
{
    return shouldPaintButton(m_previousButtonVisibility, hasPreviousPage());
}

bool PipsPager::shouldPaintNextButton() const
{
    return shouldPaintButton(m_nextButtonVisibility, hasNextPage());
}

bool PipsPager::shouldPaintButton(PipsPagerButtonVisibility visibility, bool hasPage) const
{
    if (!hasPage) return false;
    if (visibility == PipsPagerButtonVisibility::Visible) return true;
    if (visibility == PipsPagerButtonVisibility::VisibleOnPointerOver) {
        return m_controlHovered || hasFocus();
    }
    return false;
}

PipsPager::HitTarget PipsPager::makeTarget(HitKind kind, int pageIndex)
{
    HitTarget target;
    target.kind = kind;
    target.pageIndex = pageIndex;
    return target;
}

PipsPager::HitTarget PipsPager::hitTest(const QPoint& pos) const
{
    if (m_numberOfPages <= 0) return HitTarget();

    if (hasPreviousPage() && previousButtonRect().contains(pos)) {
        return makeTarget(HitKind::PreviousButton);
    }
    if (hasNextPage() && nextButtonRect().contains(pos)) {
        return makeTarget(HitKind::NextButton);
    }

    const int firstPage = firstVisiblePage();
    const int count = visiblePipCount();
    for (int offset = 0; offset < count; ++offset) {
        const int pageIndex = firstPage + offset;
        if (pipHitRect(pageIndex).contains(pos)) {
            return makeTarget(HitKind::Pip, pageIndex);
        }
    }

    return HitTarget();
}

QRect PipsPager::pipCellRect(int visibleOffset) const
{
    return pipCellRectF(visibleOffset).toAlignedRect();
}

QRectF PipsPager::pipCellRectF(qreal visibleOffset) const
{
    const QRect bounds = controlRect();
    const int previousOffset = reservesPreviousButton() ? m_navigationButtonSize : 0;
    const qreal pipOffset = previousOffset + visibleOffset * m_pipCellSize;

    if (m_orientation == Qt::Horizontal) {
        return QRectF(bounds.left() + pipOffset,
                      bounds.top() + (bounds.height() - m_pipCellSize) / 2.0,
                      m_pipCellSize,
                      m_pipCellSize);
    }

    return QRectF(bounds.left() + (bounds.width() - m_pipCellSize) / 2.0,
                  bounds.top() + pipOffset,
                  m_pipCellSize,
                  m_pipCellSize);
}

QRectF PipsPager::pipsClipRect() const
{
    const int count = visiblePipCount();
    if (count <= 0)
        return QRectF();

    return pipCellRectF(0).united(pipCellRectF(count - 1));
}

void PipsPager::drawPip(QPainter& painter, int pageIndex) const
{
    const QRect cell = pipHitRect(pageIndex);
    if (!cell.isValid()) return;

    drawPipAtVisualOffset(painter, pageIndex, pageIndex - firstVisiblePage());
}

void PipsPager::drawPipAtVisualOffset(QPainter& painter, int pageIndex, qreal visualOffset) const
{
    const QRectF cell = pipCellRectF(visualOffset);
    if (!cell.isValid()) return;

    const int diameter = pipDiameter(pageIndex, false);
    const QPointF center = cell.center();
    const QRectF dotRect(center.x() - diameter / 2.0,
                         center.y() - diameter / 2.0,
                         diameter,
                         diameter);

    painter.setPen(Qt::NoPen);
    painter.setBrush(pipColor(false));
    painter.drawEllipse(dotRect);
}

void PipsPager::drawSelectedPip(QPainter& painter) const
{
    const int count = visiblePipCount();
    if (count <= 0) return;

    const qreal visualOffset = qBound(0.0,
                                      m_selectedVisualOffset,
                                      static_cast<qreal>(count - 1));
    const QRectF firstCell = pipCellRectF(0);
    if (!firstCell.isValid()) return;

    QPointF center;
    if (m_orientation == Qt::Horizontal) {
        center = QPointF(firstCell.left() + firstCell.width() / 2.0 + visualOffset * m_pipCellSize,
                         firstCell.top() + firstCell.height() / 2.0);
    } else {
        center = QPointF(firstCell.left() + firstCell.width() / 2.0,
                         firstCell.top() + firstCell.height() / 2.0 + visualOffset * m_pipCellSize);
    }

    const int diameter = pipDiameter(m_selectedPageIndex, true);
    const QRectF dotRect(center.x() - diameter / 2.0,
                         center.y() - diameter / 2.0,
                         diameter,
                         diameter);

    painter.setPen(Qt::NoPen);
    painter.setBrush(pipColor(true));
    painter.drawEllipse(dotRect);
}

void PipsPager::drawButton(QPainter& painter, const QRect& buttonRect, bool previous) const
{
    if (!buttonRect.isValid()) return;

    const HitTarget target = makeTarget(previous ? HitKind::PreviousButton : HitKind::NextButton);
    const QColor fill = buttonFillColor(target);
    if (fill.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawRoundedRect(buttonRect, themeRadius().control, themeRadius().control);
    }

    const ushort codepoint = previous
        ? (m_orientation == Qt::Horizontal ? 0xEDD9 : 0xEDDB)
        : (m_orientation == Qt::Horizontal ? 0xEDDA : 0xEDDC);

    QFont iconFont(QStringLiteral("Segoe Fluent Icons"));
    iconFont.setPixelSize(m_navigationIconSize);
    iconFont.setHintingPreference(QFont::PreferNoHinting);

    painter.setFont(iconFont);
    painter.setPen(caretColor(target));
    painter.drawText(buttonRect, Qt::AlignCenter, QString(QChar(codepoint)));
}

QColor PipsPager::pipColor(bool selected) const
{
    const auto colors = themeColors();
    if (!isEnabled()) {
        return colors.textDisabled;
    }
    return selected ? colors.textPrimary : colors.textSecondary;
}

QColor PipsPager::caretColor(const HitTarget& target) const
{
    const auto colors = themeColors();
    if (!isEnabled()) return colors.textDisabled;
    if (m_pressedTarget == target || m_hoveredTarget == target) return colors.textPrimary;
    return colors.textSecondary;
}

QColor PipsPager::buttonFillColor(const HitTarget& target) const
{
    const auto colors = themeColors();
    if (!isEnabled()) return QColor(Qt::transparent);
    if (m_pressedTarget == target) return colors.subtleTertiary;
    if (m_hoveredTarget == target) return colors.subtleSecondary;
    return QColor(Qt::transparent);
}

int PipsPager::pipDiameter(int pageIndex, bool selected) const
{
    if (!isEnabled()) {
        return selected ? m_selectedPipDiameter : m_inactivePipDiameter;
    }

    const HitTarget target = makeTarget(HitKind::Pip, pageIndex);
    if (m_pressedTarget == target) {
        return selected ? pressedDiameter(m_selectedPipDiameter) : pressedDiameter(m_inactivePipDiameter);
    }
    if (m_hoveredTarget == target) {
        return selected ? hoveredDiameter(m_selectedPipDiameter) : hoveredDiameter(m_inactivePipDiameter);
    }
    return selected ? m_selectedPipDiameter : m_inactivePipDiameter;
}

void PipsPager::setSelectedVisualOffset(qreal offset)
{
    const int count = visiblePipCount();
    const qreal normalizedOffset = count > 0
        ? qBound(0.0, offset, static_cast<qreal>(count - 1))
        : 0.0;
    if (qFuzzyCompare(m_selectedVisualOffset + 1.0, normalizedOffset + 1.0)) return;

    m_selectedVisualOffset = normalizedOffset;
    update();
}

void PipsPager::setVisibleWindowOffset(qreal offset)
{
    const int count = visiblePipCount();
    const qreal maxOffset = count > 0
        ? static_cast<qreal>(std::max(0, m_numberOfPages - count))
        : 0.0;
    const qreal normalizedOffset = qBound(0.0, offset, maxOffset);
    if (qFuzzyCompare(m_visibleWindowOffset + 1.0, normalizedOffset + 1.0)) return;

    m_visibleWindowOffset = normalizedOffset;
    update();
}

qreal PipsPager::currentSelectedVisualOffset() const
{
    const int count = visiblePipCount();
    if (count <= 0) return 0.0;

    const int firstPage = firstVisiblePage();
    return static_cast<qreal>(qBound(0, m_selectedPageIndex - firstPage, count - 1));
}

qreal PipsPager::currentVisibleWindowOffset() const
{
    return static_cast<qreal>(firstVisiblePage());
}

void PipsPager::syncSelectedVisualOffset()
{
    if (m_selectedVisualOffsetAnimation) {
        m_selectedVisualOffsetAnimation->stop();
    }
    setSelectedVisualOffset(currentSelectedVisualOffset());
}

void PipsPager::syncVisibleWindowOffset()
{
    if (m_visibleWindowOffsetAnimation) {
        m_visibleWindowOffsetAnimation->stop();
    }
    setVisibleWindowOffset(currentVisibleWindowOffset());
}

void PipsPager::animateSelectedVisualOffset(qreal fromOffset, qreal toOffset)
{
    if (!m_selectedVisualOffsetAnimation) {
        setSelectedVisualOffset(toOffset);
        return;
    }

    if (!m_selectionAnimationEnabled
        || m_selectionAnimationDuration <= 0
        || qFuzzyCompare(fromOffset + 1.0, toOffset + 1.0)) {
        m_selectedVisualOffsetAnimation->stop();
        setSelectedVisualOffset(toOffset);
        return;
    }

    m_selectedVisualOffsetAnimation->stop();
    m_selectedVisualOffsetAnimation->setDuration(m_selectionAnimationDuration);
    m_selectedVisualOffsetAnimation->setEasingCurve(themeAnimation().decelerate);
    setSelectedVisualOffset(fromOffset);
    m_selectedVisualOffsetAnimation->setStartValue(fromOffset);
    m_selectedVisualOffsetAnimation->setKeyValueAt(0.5, (fromOffset + toOffset) / 2.0);
    m_selectedVisualOffsetAnimation->setEndValue(toOffset);
    m_selectedVisualOffsetAnimation->start();
}

void PipsPager::animateVisibleWindowOffset(qreal fromOffset, qreal toOffset)
{
    if (!m_visibleWindowOffsetAnimation) {
        setVisibleWindowOffset(toOffset);
        return;
    }

    if (!m_selectionAnimationEnabled
        || m_selectionAnimationDuration <= 0
        || qFuzzyCompare(fromOffset + 1.0, toOffset + 1.0)) {
        m_visibleWindowOffsetAnimation->stop();
        setVisibleWindowOffset(toOffset);
        return;
    }

    m_visibleWindowOffsetAnimation->stop();
    m_visibleWindowOffsetAnimation->setDuration(m_selectionAnimationDuration);
    m_visibleWindowOffsetAnimation->setEasingCurve(themeAnimation().decelerate);
    setVisibleWindowOffset(fromOffset);
    m_visibleWindowOffsetAnimation->setStartValue(fromOffset);
    m_visibleWindowOffsetAnimation->setKeyValueAt(0.5, (fromOffset + toOffset) / 2.0);
    m_visibleWindowOffsetAnimation->setEndValue(toOffset);
    m_visibleWindowOffsetAnimation->start();
}

void PipsPager::clearInteractionState()
{
    m_hoveredTarget = HitTarget();
    m_pressedTarget = HitTarget();
    unsetCursor();
}

void PipsPager::updateAccessibleText()
{
    if (m_numberOfPages <= 0) {
        setAccessibleDescription(QStringLiteral("No pages selected"));
        return;
    }

    setAccessibleDescription(QStringLiteral("Page %1 of %2 selected")
        .arg(m_selectedPageIndex + 1)
        .arg(m_numberOfPages));
}

} // namespace fluent::scrolling
