#include "ScrollBar.h"

#include <algorithm>

#include <QPainter>
#include <QColor>
#include <QMouseEvent>

namespace fluent::scrolling {

ScrollBar::ScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent) {
    init();
}

ScrollBar::ScrollBar(QWidget *parent)
    : QScrollBar(Qt::Vertical, parent) {
    init();
}

ScrollBar::~ScrollBar() = default;

void ScrollBar::init() {
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setContextMenuPolicy(Qt::NoContextMenu);
    ensureAnimation();

    // 任意数值变化都视为“正在滚动”，触发显示并自动隐藏
    connect(this, &QScrollBar::valueChanged, this, [this](int) {
        showWithAutoHide();
    });
}

void ScrollBar::setThickness(int thickness) {
    if (m_thickness == thickness)
        return;
    m_thickness = thickness;
    updateGeometry();
    update();
    emit thicknessChanged();
}

void ScrollBar::setOpacity(qreal value) {
    value = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_opacity, value))
        return;
    m_opacity = value;
    update();
}

void ScrollBar::ensureAnimation() {
    if (!m_opacityAnim) {
        m_opacityAnim = new QPropertyAnimation(this, "opacity", this);
        const auto anim = themeAnimation();
        m_opacityAnim->setDuration(anim.fast);              // 使用 Fluent 快速反馈时长
        m_opacityAnim->setEasingCurve(anim.decelerate);     // 进入/退出都用减速曲线，贴近 WinUI
        m_opacityAnim->setStartValue(0.0);
        m_opacityAnim->setEndValue(1.0);
    }
    if (!m_autoHideTimer) {
        m_autoHideTimer = new QTimer(this);
        m_autoHideTimer->setSingleShot(true);
        // 自动隐藏延迟基于主题动画：使用 Normal 时长的约两倍
        const auto anim = themeAnimation();
        m_autoHideTimer->setInterval(anim.normal * 2);
        connect(m_autoHideTimer, &QTimer::timeout, this, [this]() {
            if (m_isHovered || m_isPressed)
                return;
            if (!m_opacityAnim)
                return;
            m_opacityAnim->stop();
            m_opacityAnim->setStartValue(m_opacity);
            m_opacityAnim->setEndValue(0.0);
            m_opacityAnim->start();
        });
    }
}

void ScrollBar::showWithAutoHide() {
    ensureAnimation();
    m_autoHideTimer->stop();
    m_opacityAnim->stop();
    m_opacityAnim->setStartValue(m_opacity);
    m_opacityAnim->setEndValue(1.0);
    m_opacityAnim->start();
    m_autoHideTimer->start();
}

void ScrollBar::onThemeUpdated() {
    update();
}

QSize ScrollBar::sizeHint() const {
    QSize base = QScrollBar::sizeHint();
    if (orientation() == Qt::Vertical) {
        base.setWidth(m_thickness);
    } else {
        base.setHeight(m_thickness);
    }
    return base;
}

QSize ScrollBar::minimumSizeHint() const {
    return sizeHint();
}

void ScrollBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_opacity);

    const auto colors = themeColors();
    const bool isDark = (currentTheme() == FluentElement::Dark);

    // 轨道：使用轻量的 Subtle 填充，圆角为厚度的一半。
    // 与控件外框之间只留非常小的间距（可按需再引入独立的 trackPadding）。
    const int trackInset = 0;
    const QRectF trackRect = QRectF(rect()).adjusted(trackInset + 0.5,
                                                     trackInset + 0.5,
                                                     -trackInset - 0.5,
                                                     -trackInset - 0.5);
    QColor trackColor = colors.subtleSecondary;
    const qreal trackMaxAlpha = isDark ? 0.18 : 0.20;
    if (trackColor.alphaF() > trackMaxAlpha)
        trackColor.setAlphaF(trackMaxAlpha);
    if ((m_isHovered || m_isPressed) && trackColor.alpha() > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        const qreal r = (orientation() == Qt::Vertical
                         ? trackRect.width() / 2.0
                         : trackRect.height() / 2.0);
        p.drawRoundedRect(trackRect, r, r);
    }

    // 自行计算 Thumb 绘制几何，交互仍由 QScrollBar 处理。这样可以在极值位置
    // 保证端部留出完整抗锯齿空间，避免圆角被 widget 边界裁掉。
    const QRectF thumbTrack = trackRect.adjusted(m_thumbPadding.left(),
                                                 m_thumbPadding.top(),
                                                 -m_thumbPadding.right(),
                                                 -m_thumbPadding.bottom());
    const bool vertical = orientation() == Qt::Vertical;
    const qreal trackLength = vertical ? thumbTrack.height() : thumbTrack.width();
    if (trackLength <= 0.0)
        return;

    const qreal scrollRange = qMax<qreal>(0.0, maximum() - minimum());
    const qreal page = qMax<qreal>(1.0, pageStep());
    const qreal minThumbLength = qMin<qreal>(m_minThumbLength, trackLength);
    const qreal proportionalLength = scrollRange > 0.0
        ? trackLength * page / (scrollRange + page)
        : trackLength;
    const qreal thumbLength = qBound(minThumbLength, proportionalLength, trackLength);
    const qreal travel = qMax<qreal>(0.0, trackLength - thumbLength);
    const qreal ratio = scrollRange > 0.0
        ? qBound<qreal>(0.0, (value() - minimum()) / scrollRange, 1.0)
        : 0.0;
    const qreal thumbOffset = travel * ratio;

    QRectF drawRect;
    if (vertical) {
        drawRect = QRectF(thumbTrack.left(),
                          thumbTrack.top() + thumbOffset,
                          thumbTrack.width(),
                          thumbLength);
    } else {
        drawRect = QRectF(thumbTrack.left() + thumbOffset,
                          thumbTrack.top(),
                          thumbLength,
                          thumbTrack.height());
    }

    QColor thumbRest = isDark ? colors.textTertiary : colors.textTertiary;
    QColor thumbActive = isDark ? colors.textSecondary : colors.textSecondary;

    const qreal restMinAlpha = isDark ? 0.42 : 0.38;
    const qreal activeMinAlpha = isDark ? 0.70 : 0.62;
    if (thumbRest.alphaF() < restMinAlpha)
        thumbRest.setAlphaF(restMinAlpha);
    if (thumbActive.alphaF() < activeMinAlpha)
        thumbActive.setAlphaF(activeMinAlpha);

    QColor thumbColor = thumbRest;
    if (m_isPressed || m_isHovered) {
        thumbColor = thumbActive;
    }

    p.setBrush(thumbColor);
    p.setPen(Qt::NoPen);
    const qreal radius = (vertical ? drawRect.width() : drawRect.height()) / 2.0;
    p.drawRoundedRect(drawRect, radius, radius);
}

void ScrollBar::enterEvent(FluentEnterEvent *event) {
    m_isHovered = true;
    // 只有在可滚动且启用时才触发显示动画
    if (isEnabled() && maximum() > minimum()) {
        showWithAutoHide();
    }
    QScrollBar::enterEvent(event);
}

void ScrollBar::leaveEvent(QEvent *event) {
    m_isHovered = false;
    update();
    QScrollBar::leaveEvent(event);
}

void ScrollBar::mousePressEvent(QMouseEvent *event) {
    m_isPressed = true;
    // 只有在可滚动且启用时才触发显示动画
    if (isEnabled() && maximum() > minimum()) {
        showWithAutoHide();
    }
    QScrollBar::mousePressEvent(event);
}

void ScrollBar::mouseReleaseEvent(QMouseEvent *event) {
    m_isPressed = false;
    update();
    QScrollBar::mouseReleaseEvent(event);
}

void ScrollBar::showEvent(QShowEvent *event) {
    QScrollBar::showEvent(event);
}

} // namespace fluent::scrolling
