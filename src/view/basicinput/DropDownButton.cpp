#include "DropDownButton.h"
#include <QPainter>
#include <QMouseEvent>
#include <QStyleOptionButton>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QtMath>

namespace view::basicinput {

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
    // 使用全局动画规范：慢速对比效果 + 减速曲线
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
    update();
    emit chevronChanged();
}

void DropDownButton::setChevronOffset(const QPoint& offset) {
    if (m_chevronOffset == offset) return;
    m_chevronOffset = offset;
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

void DropDownButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // 点击时触发一次“向下压+回弹”的动画（与 open 状态无关）
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
    // 1. 如果菜单开启，锁定为按下状态
    InteractionState oldState = interactionState();
    if (m_isOpen) {
        const_cast<DropDownButton*>(this)->setInteractionState(Pressed);
    }

    // 2. 调用基类绘制基础按钮
    Button::paintEvent(event);

    // 3. 恢复状态
    if (m_isOpen) {
        const_cast<DropDownButton*>(this)->setInteractionState(oldState);
    }

    // 4. 绘制 Chevron 图标
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // 设置图标字体（字号保持不变）
    QFont iconFont(m_iconFontFamily);
    iconFont.setPixelSize(m_chevronSize);
    painter.setFont(iconFont);

    // 获取图标颜色 (复用 Button 的语义颜色，但在按压时做细微变化)
    const auto& colors = themeColors();
    QColor textColor;
    if (!isEnabled()) {
        textColor = colors.textDisabled;
    } else {
        // 所有启用态统一采用“变暗”动画效果：
        // Accent 使用 textOnAccent，普通按钮使用 textPrimary
        textColor = (fluentStyle() == Accent) ? colors.textOnAccent : colors.textPrimary;
        if (m_pressProgress > 0.0) {
            // 1.0 → 0.5，明显的压下去的感觉
            qreal alphaFactor = 1.0 - 0.5 * m_pressProgress;
            int alpha = static_cast<int>(255 * alphaFactor);
            textColor.setAlpha(alpha);
        }
    }
    
    painter.setPen(textColor);

    // 绘制图标：根据动画进度沿 Y 轴下移后弹回 + 开发者自定义的偏移
    // chevronOffset.x() 作为与右侧边缘的间距（padding），chevronOffset.y() 为垂直微调
    QRect chevronRect = rect().adjusted(0, 0, -m_chevronOffset.x(), 0);
    const qreal maxOffset = 3.0; // 最大下移 3 像素（点击动画）
    qreal pressOffset = maxOffset * qSin(m_pressProgress * M_PI); // 0→max→0
    chevronRect.translate(0,
                          static_cast<int>(pressOffset) + m_chevronOffset.y());
    painter.drawText(chevronRect, Qt::AlignRight | Qt::AlignVCenter, m_chevronGlyph);
}

} // namespace view::basicinput
