#include "TeachingTip.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QWidget>

#include "compatibility/QtCompat.h"
#include "design/Spacing.h"

namespace view::dialogs_flyouts {

static constexpr int kShadowMargin = ::Spacing::Standard;
static constexpr int kWindowClampMargin = 4;
static constexpr int kTailSize = 12;
static constexpr int kTailHalfWidth = 10;

namespace {

bool isTopPlacement(TeachingTip::PreferredPlacement placement) {
    return placement == TeachingTip::Top ||
           placement == TeachingTip::TopLeft ||
           placement == TeachingTip::TopRight;
}

bool isBottomPlacement(TeachingTip::PreferredPlacement placement) {
    return placement == TeachingTip::Bottom ||
           placement == TeachingTip::BottomLeft ||
           placement == TeachingTip::BottomRight;
}

bool isLeftPlacement(TeachingTip::PreferredPlacement placement) {
    return placement == TeachingTip::Left ||
           placement == TeachingTip::LeftTop ||
           placement == TeachingTip::LeftBottom;
}

bool isRightPlacement(TeachingTip::PreferredPlacement placement) {
    return placement == TeachingTip::Right ||
           placement == TeachingTip::RightTop ||
           placement == TeachingTip::RightBottom;
}

} // namespace

TeachingTip::TeachingTip(QWidget* parent) : Popup(parent) {
    setModal(false);
    setDim(false);

    m_contentHost = new QWidget(this);
    m_contentHost->setAttribute(Qt::WA_TranslucentBackground);

    connect(this, &Popup::aboutToHide, this, &TeachingTip::emitClosingReason);
    setLightDismissEnabled(m_lightDismissEnabled);
    updateWidgetSize();
    onThemeUpdated();
}

void TeachingTip::onThemeUpdated() {
    Popup::onThemeUpdated();
    update();
}

void TeachingTip::setTarget(QWidget* targetWidget) {
    if (m_target == targetWidget) return;

    if (m_target) m_target->removeEventFilter(this);
    if (m_targetTopLevel) m_targetTopLevel->removeEventFilter(this);

    m_target = targetWidget;
    m_targetTopLevel = targetWidget ? targetWidget->window() : nullptr;

    if (m_target) m_target->installEventFilter(this);
    if (m_targetTopLevel && m_targetTopLevel != m_target) m_targetTopLevel->installEventFilter(this);

    if (isOpen()) {
        updateWidgetSize();
        move(computePosition());
    }
}

void TeachingTip::setPreferredPlacement(PreferredPlacement placement) {
    if (m_preferredPlacement == placement) return;
    m_preferredPlacement = placement;
    if (isOpen()) {
        updateWidgetSize();
        move(computePosition());
    }
}

void TeachingTip::setPlacementMargin(int margin) {
    margin = qMax(0, margin);
    if (m_placementMargin == margin) return;
    m_placementMargin = margin;
    if (isOpen()) move(computePosition());
}

void TeachingTip::setLightDismissEnabled(bool enabled) {
    const bool changed = m_lightDismissEnabled != enabled;
    m_lightDismissEnabled = enabled;
    setClosePolicy(enabled ? ClosePolicy(CloseOnPressOutside | CloseOnEscape)
                           : ClosePolicy(NoAutoClose));
    if (!changed) return;
}

void TeachingTip::setTailVisible(bool visible) {
    if (m_tailVisible == visible) return;
    m_tailVisible = visible;
    updateWidgetSize();
    if (isOpen()) move(computePosition());
    update();
}

void TeachingTip::setCardSize(const QSize& size) {
    if (m_cardSizeHint == size) return;
    m_cardSizeHint = size;
    updateWidgetSize();
    if (isOpen()) move(computePosition());
}

void TeachingTip::showAt(QWidget* targetWidget) {
    setTarget(targetWidget);
    updateWidgetSize();  // target 已设置，重新计算含 tailInsets 的 widget 尺寸
    open();
}

void TeachingTip::closeWithReason(CloseReason reason) {
    markPendingCloseReason(reason);
    Popup::close();
}

QPoint TeachingTip::computePosition() const {
    if (!m_target || !m_target->window()) {
        return Popup::computePosition();
    }

    const QRect targetRect = targetRectInTopLevel();
    const QSize cardSize = m_cardSizeHint;
    const PreferredPlacement placement = resolvedPlacement();
    QPoint cardTopLeft = cardTopLeftForPlacement(placement, targetRect, cardSize);
    cardTopLeft = clampCardTopLeft(cardTopLeft, cardSize);
    return widgetTopLeftForCardTopLeft(cardTopLeft, placement);
}

bool TeachingTip::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_target) {
        switch (event->type()) {
        case QEvent::Destroy:
            m_target = nullptr;
            if (isVisible() || isOpen()) closeWithReason(TargetDestroyed);
            break;
        case QEvent::Hide:
            if (isVisible() || isOpen()) closeWithReason(Programmatic);
            break;
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::Show:
            if (isVisible() || isOpen()) move(computePosition());
            break;
        default:
            break;
        }
    } else if (watched == m_targetTopLevel) {
        switch (event->type()) {
        case QEvent::Move:
        case QEvent::Resize:
            if (isVisible() || isOpen()) move(computePosition());
            break;
        default:
            break;
        }
    }

    if ((isVisible() || isOpen()) && isLightDismissEnabled() && event->type() == QEvent::MouseButtonPress) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint globalPos = fluentMouseGlobalPos(mouseEvent);
        if (!rect().contains(mapFromGlobal(globalPos))) {
            markPendingCloseReason(LightDismiss);
        }
    }

    return Popup::eventFilter(watched, event);
}

void TeachingTip::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && isLightDismissEnabled()) {
        markPendingCloseReason(LightDismiss);
    }
    Popup::keyPressEvent(event);
}

void TeachingTip::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRect visualCardRect = cardRect();
    const PreferredPlacement placement = resolvedPlacement();
    QPainterPath bubblePath;
    const int radius = themeRadius().overlay;
    bubblePath.addRoundedRect(visualCardRect, radius, radius);

    if (m_tailVisible && !targetRectInTopLevel().isEmpty()) {
        QPolygon tail;
        const QPoint targetTopLeft = m_target->mapTo(m_target->window(), QPoint(0, 0));
        const QRect localTargetRect = QRect(mapFrom(m_target->window(), targetTopLeft), m_target->size());

        if (isBottomPlacement(placement)) {
            int centerX = localTargetRect.center().x();
            if (placement == BottomLeft)  centerX = localTargetRect.left()  + qMin(24, localTargetRect.width() / 2);
            if (placement == BottomRight) centerX = localTargetRect.right() - qMin(24, localTargetRect.width() / 2);
            centerX = qBound(visualCardRect.left() + radius + kTailHalfWidth,
                             centerX, visualCardRect.right() - radius - kTailHalfWidth);
            // 底边两点向 card 内 +2px，确保 united() 有面积重叠，消除接缝线
            tail << QPoint(centerX - kTailHalfWidth, visualCardRect.top() + 2)
                 << QPoint(centerX + kTailHalfWidth, visualCardRect.top() + 2)
                 << QPoint(centerX, visualCardRect.top() - kTailSize);
        } else if (isTopPlacement(placement)) {
            int centerX = localTargetRect.center().x();
            if (placement == TopLeft)  centerX = localTargetRect.left()  + qMin(24, localTargetRect.width() / 2);
            if (placement == TopRight) centerX = localTargetRect.right() - qMin(24, localTargetRect.width() / 2);
            centerX = qBound(visualCardRect.left() + radius + kTailHalfWidth,
                             centerX, visualCardRect.right() - radius - kTailHalfWidth);
            tail << QPoint(centerX - kTailHalfWidth, visualCardRect.bottom() - 2)
                 << QPoint(centerX + kTailHalfWidth, visualCardRect.bottom() - 2)
                 << QPoint(centerX, visualCardRect.bottom() + kTailSize);
        } else if (isRightPlacement(placement)) {
            int centerY = localTargetRect.center().y();
            if (placement == RightTop)    centerY = localTargetRect.top()    + qMin(24, localTargetRect.height() / 2);
            if (placement == RightBottom) centerY = localTargetRect.bottom() - qMin(24, localTargetRect.height() / 2);
            centerY = qBound(visualCardRect.top() + radius + kTailHalfWidth,
                             centerY, visualCardRect.bottom() - radius - kTailHalfWidth);
            tail << QPoint(visualCardRect.left() + 2, centerY - kTailHalfWidth)
                 << QPoint(visualCardRect.left() + 2, centerY + kTailHalfWidth)
                 << QPoint(visualCardRect.left() - kTailSize, centerY);
        } else if (isLeftPlacement(placement)) {
            int centerY = localTargetRect.center().y();
            if (placement == LeftTop)    centerY = localTargetRect.top()    + qMin(24, localTargetRect.height() / 2);
            if (placement == LeftBottom) centerY = localTargetRect.bottom() - qMin(24, localTargetRect.height() / 2);
            centerY = qBound(visualCardRect.top() + radius + kTailHalfWidth,
                             centerY, visualCardRect.bottom() - radius - kTailHalfWidth);
            tail << QPoint(visualCardRect.right() - 2, centerY - kTailHalfWidth)
                 << QPoint(visualCardRect.right() - 2, centerY + kTailHalfWidth)
                 << QPoint(visualCardRect.right() + kTailSize, centerY);
        }

        if (!tail.isEmpty()) {
            QPainterPath tailPath;
            tailPath.addPolygon(tail);
            bubblePath = bubblePath.united(tailPath);
        }
    }

    const auto& shadow = themeShadow(Elevation::High);
    for (int layer = 0; layer < 8; ++layer) {
        QColor shadowColor = shadow.color;
        shadowColor.setAlphaF(shadow.opacity * (1.0 - (static_cast<double>(layer) / 8.0)) * 0.25);
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadowColor);
        painter.drawPath(bubblePath.translated(0, 2 + layer / 2));
    }

    const auto& colors = themeColors();
    // 先填充整个 bubble（card + tail），无描边
    painter.setBrush(colors.bgLayer);
    painter.setPen(Qt::NoPen);
    painter.drawPath(bubblePath);
    // 对整个 bubble 外轮廓描边：united path 无内部接缝，tail 基部不会出现横线
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(colors.strokeDefault, 1));
    painter.drawPath(bubblePath);
}

void TeachingTip::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    syncContentHostGeometry();
}

void TeachingTip::syncContentHostGeometry() {
    if (m_contentHost) {
        m_contentHost->setGeometry(cardRect());
    }
}

void TeachingTip::updateWidgetSize() {
    const PreferredPlacement placement = (m_preferredPlacement == Auto)
        ? resolveAutoPlacement(m_cardSizeHint)
        : m_preferredPlacement;
    const QMargins tail = tailInsets(placement);
    resize(m_cardSizeHint.width()  + 2 * kShadowMargin + tail.left() + tail.right(),
           m_cardSizeHint.height() + 2 * kShadowMargin + tail.top()  + tail.bottom());
    syncContentHostGeometry();
}

QRect TeachingTip::cardRect() const {
    QRect visualRect = rect().adjusted(kShadowMargin, kShadowMargin, -kShadowMargin, -kShadowMargin);
    const QMargins insets = tailInsets(resolvedPlacement());
    return visualRect.adjusted(insets.left(), insets.top(), -insets.right(), -insets.bottom());
}

QRect TeachingTip::targetRectInTopLevel() const {
    if (!m_target || !m_target->window()) return QRect();
    return QRect(m_target->mapTo(m_target->window(), QPoint(0, 0)), m_target->size());
}

TeachingTip::PreferredPlacement TeachingTip::resolvedPlacement() const {
    return m_preferredPlacement == Auto ? resolveAutoPlacement(m_cardSizeHint) : m_preferredPlacement;
}

TeachingTip::PreferredPlacement TeachingTip::resolveAutoPlacement(const QSize& cardSize) const {
    const QRect targetRect = targetRectInTopLevel();
    QWidget* top = m_target ? m_target->window() : nullptr;
    if (targetRect.isEmpty() || !top) return Bottom;

    // 只检查主轴方向是否有足够空间（与 Flyout 一致）。
    // 横轴溢出由 clampCardTopLeft 修正，不纳入 fits 判断，避免 Bottom/Top 因
    // 卡片横向居中超出窗口边缘而被错误跳过。
    const auto fits = [&](PreferredPlacement placement) {
        const QPoint p = cardTopLeftForPlacement(placement, targetRect, cardSize);
        if (isBottomPlacement(placement))
            return p.y() + cardSize.height() <= top->height() - kWindowClampMargin;
        if (isTopPlacement(placement))
            return p.y() >= kWindowClampMargin;
        if (isRightPlacement(placement))
            return p.x() + cardSize.width() <= top->width() - kWindowClampMargin;
        if (isLeftPlacement(placement))
            return p.x() >= kWindowClampMargin;
        return true;
    };

    if (fits(Bottom)) return Bottom;
    if (fits(Top))    return Top;
    if (fits(Right))  return Right;
    if (fits(Left))   return Left;
    return Bottom;
}

QPoint TeachingTip::cardTopLeftForPlacement(PreferredPlacement placement, const QRect& targetRect,
                                             const QSize& cardSize) const {
    QPoint cardTopLeft(targetRect.center().x() - (cardSize.width() / 2),
                       targetRect.bottom() + m_placementMargin);

    // placementMargin = gap between tail TIP and the target edge.
    // The card body sits further away by kTailSize (the tail length).
    // When tail is hidden / no target, kTailSize offset is omitted.
    const int tail = (m_tailVisible && m_target) ? kTailSize : 0;

    if (isTopPlacement(placement)) {
        cardTopLeft.setY(targetRect.top() - m_placementMargin - tail - cardSize.height());
    } else if (isBottomPlacement(placement)) {
        cardTopLeft.setY(targetRect.bottom() + m_placementMargin + tail);
    } else if (isLeftPlacement(placement)) {
        cardTopLeft.setX(targetRect.left() - m_placementMargin - tail - cardSize.width());
    } else if (isRightPlacement(placement)) {
        cardTopLeft.setX(targetRect.right() + m_placementMargin + tail);
    }

    switch (placement) {
    case TopLeft:
    case BottomLeft:
        cardTopLeft.setX(targetRect.left());
        break;
    case TopRight:
    case BottomRight:
        cardTopLeft.setX(targetRect.right() - cardSize.width() + 1);
        break;
    case LeftTop:
    case RightTop:
        cardTopLeft.setY(targetRect.top());
        break;
    case LeftBottom:
    case RightBottom:
        cardTopLeft.setY(targetRect.bottom() - cardSize.height() + 1);
        break;
    case Top:
    case Bottom:
        cardTopLeft.setX(targetRect.center().x() - (cardSize.width() / 2));
        break;
    case Left:
    case Right:
        cardTopLeft.setY(targetRect.center().y() - (cardSize.height() / 2));
        break;
    case Auto:
        break;
    }

    return cardTopLeft;
}

QPoint TeachingTip::clampCardTopLeft(const QPoint& cardTopLeft, const QSize& cardSize) const {
    QWidget* top = m_target ? m_target->window() : nullptr;
    if (!top) return cardTopLeft;

    int x = qBound(kWindowClampMargin, cardTopLeft.x(), top->width()  - cardSize.width()  - kWindowClampMargin);
    int y = qBound(kWindowClampMargin, cardTopLeft.y(), top->height() - cardSize.height() - kWindowClampMargin);
    return QPoint(x, y);
}

QPoint TeachingTip::widgetTopLeftForCardTopLeft(const QPoint& cardTopLeft,
                                                 PreferredPlacement placement) const {
    const QMargins insets = tailInsets(placement);
    return cardTopLeft - QPoint(kShadowMargin + insets.left(), kShadowMargin + insets.top());
}

QMargins TeachingTip::tailInsets(PreferredPlacement placement) const {
    if (!m_tailVisible || !m_target) return QMargins();

    if (isBottomPlacement(placement)) return QMargins(0, kTailSize, 0, 0);
    if (isTopPlacement(placement))    return QMargins(0, 0, 0, kTailSize);
    if (isRightPlacement(placement))  return QMargins(kTailSize, 0, 0, 0);
    if (isLeftPlacement(placement))   return QMargins(0, 0, kTailSize, 0);
    return QMargins();
}

void TeachingTip::markPendingCloseReason(CloseReason reason) {
    m_pendingCloseReason = reason;
    m_closeReasonExplicit = true;
}

void TeachingTip::emitClosingReason() {
    const CloseReason reason = m_closeReasonExplicit ? m_pendingCloseReason : Programmatic;
    emit closing(reason);
    m_pendingCloseReason = Programmatic;
    m_closeReasonExplicit = false;
}

} // namespace view::dialogs_flyouts
