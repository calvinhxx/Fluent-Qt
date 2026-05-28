#include "ScrollBar.h"

#include <QPainter>
#include <QColor>
#include <QStyleOptionSlider>
#include <QStyle>
#include <QMouseEvent>

namespace view::scrolling {

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
    const QRect trackRect = rect().adjusted(trackInset,
                                            trackInset,
                                            -trackInset,
                                            -trackInset);
    QColor trackColor = colors.subtleSecondary;
    // 轨道尽量轻（比 Thumb 明显更淡），让淡入淡出主要体现在 Thumb 上
    // 深色保守一点，浅色再实心一些
    const qreal trackMaxAlpha = isDark ? 0.20 : 0.30;
    if (trackColor.alphaF() > trackMaxAlpha) {
        trackColor.setAlphaF(trackMaxAlpha);
    }
    if (trackColor.alpha() > 0) {
        p.setPen(Qt::NoPen);
        p.setBrush(trackColor);
        const qreal r = (orientation() == Qt::Vertical
                         ? trackRect.width() / 2.0
                         : trackRect.height() / 2.0);
        p.drawRoundedRect(trackRect, r, r);
    }

    // 通过 QStyle 计算 Thumb 几何，保持与交互逻辑一致
    QStyleOptionSlider opt;
    initStyleOption(&opt);
    QRect handleRect = style()->subControlRect(QStyle::CC_ScrollBar, &opt,
                                               QStyle::SC_ScrollBarSlider, this);
    if (!handleRect.isValid())
        return;

    // 最小长度
    if (orientation() == Qt::Vertical && handleRect.height() < m_minThumbLength) {
        handleRect.setHeight(m_minThumbLength);
    } else if (orientation() == Qt::Horizontal && handleRect.width() < m_minThumbLength) {
        handleRect.setWidth(m_minThumbLength);
    }

    // 在最小值/最大值时，Qt 样式会预留一小段边距，使 Thumb 看起来离两端有间隙。
    // 这里仅在 value() 处于极值时做一个平移修正，结合 m_thumbPadding 保持统一的端部间距。
    if (orientation() == Qt::Horizontal) {
        if (value() == minimum()) {
            const int desiredLeft = trackRect.left() + m_thumbPadding.left();
            const int delta = desiredLeft - handleRect.left();
            handleRect.translate(delta, 0);
        } else if (value() == maximum()) {
            const int desiredRight = trackRect.right() - m_thumbPadding.right();
            const int delta = desiredRight - handleRect.right();
            handleRect.translate(delta, 0);
        }
    } else { // 垂直方向
        if (value() == minimum()) {
            const int desiredTop = trackRect.top() + m_thumbPadding.top();
            const int delta = desiredTop - handleRect.top();
            handleRect.translate(0, delta);
        } else if (value() == maximum()) {
            const int desiredBottom = trackRect.bottom() - m_thumbPadding.bottom();
            const int delta = desiredBottom - handleRect.bottom();
            handleRect.translate(0, delta);
        }
    }

    // 厚度方向和长度方向都使用 m_thumbPadding 内缩，让 Thumb 看起来更“悬浮”
    QRect drawRect = handleRect;
    drawRect.adjust(m_thumbPadding.left(),
                    m_thumbPadding.top(),
                    -m_thumbPadding.right(),
                    -m_thumbPadding.bottom());

    // 根据交互状态选择 Thumb 颜色（浅色下更接近实心，深色保持当前效果）
    QColor thumbRest  = isDark ? colors.controlSecondary : colors.controlDefault;
    QColor thumbActive= isDark ? colors.controlDefault   : colors.controlSecondary;

    // Rest 态略淡，Hover/Press 基本实心（浅色）
    const qreal restMinAlpha   = isDark ? 0.42 : 0.80;
    const qreal activeMinAlpha = isDark ? 0.70 : 0.98;
    if (thumbRest.alphaF() < restMinAlpha) {
        thumbRest.setAlphaF(restMinAlpha);
    }
    if (thumbActive.alphaF() < activeMinAlpha) {
        thumbActive.setAlphaF(activeMinAlpha);
    }

    QColor thumbColor = thumbRest;
    if (m_isPressed || m_isHovered) {
        thumbColor = thumbActive;
    }

    p.setBrush(thumbColor);
    p.setPen(Qt::NoPen);
    const int radius = (orientation() == Qt::Vertical
                        ? drawRect.width()
                        : drawRect.height()) / 2;
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

} // namespace view::scrolling
