#include "DropDownButton.h"
#include <QPainter>
#include <QMouseEvent>
#include <QStyleOptionButton>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QtMath>

namespace fluent::basicinput {

DropDownButton::DropDownButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    initAnimation();
}

DropDownButton::DropDownButton(QWidget* parent)
    : Button(parent) {
    initAnimation();
}

void DropDownButton::initAnimation() {
    if (m_pressAnimation) return;
    m_pressAnimation = new QPropertyAnimation(this, "pressProgress");
    // Global motion tokens: slow contrast with a decelerate curve.
    // zh_CN: 使用全局动画规范——慢速对比效果 + 减速曲线。
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
}

void DropDownButton::setMenu(QMenu* menu) {
    if (m_menu == menu) return;
    
    m_menu = menu;
    if (m_menu) {
        connect(m_menu, &QMenu::aboutToShow, this, [this]() { setOpen(true); });
        connect(m_menu, &QMenu::aboutToHide, this, [this]() { setOpen(false); });
    }
}

void DropDownButton::setOpen(bool open) {
    if (m_isOpen == open) return;
    m_isOpen = open;
    update();
    emit openChanged();
}

void DropDownButton::setChevronGlyph(const QString& glyph) {
    if (m_chevronGlyph == glyph) return;
    m_chevronGlyph = glyph;
    update();
    emit chevronChanged();
}

void DropDownButton::setIconFontFamily(const QString& family) {
    if (m_iconFontFamily == family) return;
    m_iconFontFamily = family;
    update();
    emit chevronChanged();
}

void DropDownButton::setChevronSize(int size) {
    if (m_chevronSize == size) return;
    m_chevronSize = size;
    updateGeometry();
    update();
    emit chevronChanged();
}

void DropDownButton::setChevronOffset(const QPoint& offset) {
    if (m_chevronOffset == offset) return;
    m_chevronOffset = offset;
    updateGeometry();
    update();
    emit chevronChanged();
}

void DropDownButton::setPressProgress(qreal value) {
    qreal clamped = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_pressProgress, clamped))
        return;
    m_pressProgress = clamped;
    update();
}

QSize DropDownButton::sizeHint() const {
    QSize size = Button::sizeHint();
    size.rwidth() += chevronReserveWidth();
    return size;
}

QSize DropDownButton::minimumSizeHint() const {
    return sizeHint();
}

QRectF DropDownButton::contentPaintRect(const QRectF& surfaceRect) const {
    const qreal reserve = qMin(surfaceRect.width(),
                               static_cast<qreal>(chevronReserveWidth()));
    return surfaceRect.adjusted(0, 0, -reserve, 0);
}

int DropDownButton::chevronReserveWidth() const {
    const auto& spacing = themeSpacing();
    const int gap = fluentSize() == Small ? spacing.gap.tight : spacing.gap.normal;
    return gap + qMax(0, m_chevronSize) + qMax(0, m_chevronOffset.x());
}

void DropDownButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Clicking plays one press-down-and-rebound animation, independent of
        // the open state.
        // zh_CN: 点击触发一次“向下压+回弹”动画（与 open 状态无关）。
        if (m_pressAnimation) {
            m_pressAnimation->stop();
            m_pressAnimation->setStartValue(0.0);
            m_pressAnimation->setEndValue(1.0);
            m_pressAnimation->start();
        }

        if (m_menu) {
            m_menu->exec(mapToGlobal(QPoint(0, height())));
            return;
        }
    }
    Button::mousePressEvent(event);
}

void DropDownButton::paintEvent(QPaintEvent* event) {
    // 1. Lock the pressed look while the menu is open. zh_CN: 菜单开启时锁定为按下状态。
    InteractionState oldState = interactionState();
    if (m_isOpen) {
        const_cast<DropDownButton*>(this)->setInteractionState(Pressed);
    }

    // 2. Let the base class paint the plain button. zh_CN: 调用基类绘制基础按钮。
    Button::paintEvent(event);

    // 3. Restore the state. zh_CN: 恢复状态。
    if (m_isOpen) {
        const_cast<DropDownButton*>(this)->setInteractionState(oldState);
    }

    // 4. Paint the chevron glyph. zh_CN: 绘制 Chevron 图标。
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Icon font with the size unchanged. zh_CN: 设置图标字体（字号保持不变）。
    QFont iconFont(m_iconFontFamily);
    iconFont.setPixelSize(m_chevronSize);
    painter.setFont(iconFont);
    const qreal pressEffect = qSin(m_pressProgress * M_PI);

    // Icon color reuses Button's semantic colors with a subtle pressed tweak.
    // zh_CN: 图标颜色复用 Button 的语义色，按压时做细微变化。
    const auto& colors = themeColors();
    QColor textColor;
    if (!isEnabled()) {
        textColor = colors.textDisabled;
    } else {
        // All enabled states share the dimming animation: accent buttons use
        // textOnAccent, standard buttons textPrimary.
        // zh_CN: 启用态统一采用“变暗”动画——Accent 用 textOnAccent，普通按钮用 textPrimary。
        textColor = (fluentStyle() == Accent) ? colors.textOnAccent : colors.textPrimary;
        if (pressEffect > 0.0) {
            // 1.0 → 0.5 for a clear pressed feel. zh_CN: 1.0 → 0.5，明显的按压感。
            qreal alphaFactor = 1.0 - 0.5 * pressEffect;
            int alpha = static_cast<int>(255 * alphaFactor);
            textColor.setAlpha(alpha);
        }
    }
    
    painter.setPen(textColor);

    // Paint the glyph: it dips down along Y with the animation then rebounds,
    // plus the developer offset; chevronOffset.x() is the right-edge padding and
    // chevronOffset.y() the vertical tweak.
    // zh_CN: 绘制图标——按动画进度沿 Y 轴下移后弹回，再叠加自定义偏移；
    // chevronOffset.x() 为右缘间距，chevronOffset.y() 为垂直微调。
    QRect chevronRect = rect().adjusted(0, 0, -m_chevronOffset.x(), 0);
    const qreal maxOffset = 3.0; // Max 3px dip for the click animation. zh_CN: 最大下移 3 像素。
    qreal pressOffset = maxOffset * pressEffect; // 0→max→0
    chevronRect.translate(0,
                          static_cast<int>(pressOffset) + m_chevronOffset.y());
    painter.drawText(chevronRect, Qt::AlignRight | Qt::AlignVCenter, m_chevronGlyph);
}

} // namespace fluent::basicinput
