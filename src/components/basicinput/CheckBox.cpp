#include "CheckBox.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyle>

namespace fluent::basicinput {

CheckBox::CheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent) {
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::ArrowCursor);
    
    auto fs = themeFont(Typography::FontRole::Body);
    setFont(fs.toQFont());
    initAnimation();
}

CheckBox::CheckBox(QWidget* parent)
    : CheckBox("", parent) {
}

void CheckBox::initAnimation() {
    m_checkAnimation = new QPropertyAnimation(this, "checkProgress");
    m_checkAnimation->setDuration(themeAnimation().fast);
    m_checkAnimation->setEasingCurve(themeAnimation().decelerate);
}

void CheckBox::setCheckProgress(qreal progress) {
    m_checkProgress = progress;
    update();
}

void CheckBox::setBoxSize(int size) {
    if (m_boxSize != size) {
        m_boxSize = size;
        updateGeometry();
        update();
        emit boxSizeChanged();
    }
}

void CheckBox::setBoxMargin(int margin) {
    if (m_boxMargin != margin) {
        m_boxMargin = margin;
        updateGeometry();
        update();
        emit boxMarginChanged();
    }
}

void CheckBox::setTextGap(int gap) {
    if (m_textGap != gap) {
        m_textGap = gap;
        updateGeometry();
        update();
        emit textGapChanged();
    }
}

void CheckBox::setHoverBackgroundEnabled(bool enabled) {
    if (m_hoverBackgroundEnabled != enabled) {
        m_hoverBackgroundEnabled = enabled;
        update();
        emit hoverBackgroundEnabledChanged();
    }
}

void CheckBox::nextCheckState() {
    QCheckBox::nextCheckState();
    if (m_checkAnimation) {
        m_checkAnimation->stop();
        m_checkAnimation->setStartValue(0.0);
        m_checkAnimation->setEndValue(1.0);
        m_checkAnimation->start();
    }
}

void CheckBox::onThemeUpdated() {
    updateGeometry();
    update();
}

QSize CheckBox::sizeHint() const {
    const auto& spacing = themeSpacing();
    QFontMetrics fm(font());
    
    // Configurable metrics. zh_CN: 使用可配置的属性。
    int w = m_boxSize + m_boxMargin * 2; // Left margin + box + right margin. zh_CN: 左 margin + 方框 + 右 margin。
    if (!text().isEmpty()) {
        w += m_textGap + fm.horizontalAdvance(text());
    }
    int h = qMax(m_boxSize, fm.height()) + spacing.gap.tight * 2;
    
    return QSize(w, h);
}

QSize CheckBox::minimumSizeHint() const {
    return sizeHint();
}

void CheckBox::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();
    const auto& radius = themeRadius();
    
    bool isHover = underMouse();
    bool isPressed = isDown();
    bool enabled = isEnabled();
    Qt::CheckState state = checkState();

    // 1. Hover background, clamped to the actual content area. zh_CN: 整体悬停背景限制在实际内容区域。
    if (enabled && isHover && m_hoverBackgroundEnabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.subtleSecondary);
        // Slight inset so the fill never touches the edge. zh_CN: 稍微缩进，避免背景贴边。
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), radius.control, radius.control);
    }

    // 2. Paint the checkbox box. zh_CN: 绘制复选框方框。
    int boxY = (height() - m_boxSize) / 2;
    const QRect logicalBoxRect(m_boxMargin, boxY, m_boxSize, m_boxSize);
    const QRectF boxRect =
        QStyle::visualRect(layoutDirection(), rect(), logicalBoxRect);

    QColor boxBg, boxBorder, iconColor;
    const qreal boxRadius = radius.control;

    // Fluent treatment. zh_CN: Fluent 样式。
        if (!enabled) {
            boxBg = colors.controlDisabled;
            boxBorder = colors.strokeDivider;
            iconColor = colors.textDisabled;
        } else if (state == Qt::Unchecked) {
            boxBg = isPressed ? colors.controlTertiary : (isHover ? colors.controlSecondary : colors.controlDefault);
            boxBorder = isHover ? colors.strokeStrong : colors.strokeDefault;
            iconColor = Qt::transparent;
        } else {
            // Checked or indeterminate: accent fill without a separate border.
            // zh_CN: Checked 或 Indeterminate——使用 Accent 颜色且无独立边框。
            boxBg = isPressed ? colors.accentTertiary : (isHover ? colors.accentSecondary : colors.accentDefault);
            boxBorder = Qt::transparent;
            iconColor = colors.textOnAccent;
        }
    const QRectF boxDrawRect = boxRect;

    // Paint the box fill. zh_CN: 绘制方框底色。
    painter.setPen(Qt::NoPen);
    painter.setBrush(boxBg);
    painter.drawRoundedRect(boxDrawRect, boxRadius, boxRadius);

    // Paint the box outline (unchecked only). zh_CN: 绘制方框描边（仅未选中时）。
    if (boxBorder != Qt::transparent) {
        const qreal borderWidth = 1.0;
        const qreal inset = borderWidth / 2.0;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(boxBorder, borderWidth));
        painter.drawRoundedRect(boxDrawRect.adjusted(inset, inset, -inset, -inset), boxRadius, boxRadius);
    }

    // 3. Paint the inner glyph (icon font). zh_CN: 绘制内部图标。
    if (state != Qt::Unchecked) {
        painter.save();
        
        // WinUI uses the native 12 px check/subtract drawing inside its 20 px box.
        // zh_CN: WinUI 在 20 px 方框内使用原生 12 px 对勾/横线字形。
        const int fontSize = Typography::IconSize::Compact;
        painter.setPen(iconColor);

        // Animated reveal. zh_CN: 动画效果。
        painter.setOpacity(m_checkProgress);
        if (state == Qt::Checked) {
            painter.translate(boxRect.center());
            painter.scale(0.8 + 0.2 * m_checkProgress, 0.8 + 0.2 * m_checkProgress);
            painter.translate(-boxRect.center());
        }

        const QString glyph = state == Qt::Checked ? Typography::Icons::CheckMark : Typography::Icons::Hyphen;
        Typography::Icons::paintGlyph(painter, QRectF(boxRect), glyph, fontSize, Qt::AlignCenter);
        
        painter.restore();
    }

    // 4. Paint the text. zh_CN: 绘制文本。
    if (!text().isEmpty()) {
        painter.setFont(font());
        painter.setPen(enabled ? colors.textPrimary : colors.textDisabled);
        
        // Text starts after the left margin, box, and text gap.
        // zh_CN: 文本起始位置 = 左侧边距 + 方框 + 文字间距。
        const QRect logicalTextRect =
            rect().adjusted(m_boxMargin + m_boxSize + m_textGap,
                            0, -m_boxMargin, 0);
        const QRect textRect =
            QStyle::visualRect(layoutDirection(), rect(), logicalTextRect);
        painter.drawText(textRect,
                         QStyle::visualAlignment(layoutDirection(),
                                                 Qt::AlignVCenter | Qt::AlignLeft),
                         text());
    }

    if (enabled && hasFocus() && m_keyboardFocusVisible) {
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120);
        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1),
                                radius.control, radius.control);
    }
}

void CheckBox::focusInEvent(QFocusEvent* event)
{
    QCheckBox::focusInEvent(event);
    if (event->reason() == Qt::MouseFocusReason)
        m_keyboardFocusVisible = false;
    else if (event->reason() == Qt::TabFocusReason
             || event->reason() == Qt::BacktabFocusReason
             || event->reason() == Qt::ShortcutFocusReason)
        m_keyboardFocusVisible = true;
    update();
}

void CheckBox::focusOutEvent(QFocusEvent* event)
{
    QCheckBox::focusOutEvent(event);
    update();
}

void CheckBox::keyPressEvent(QKeyEvent* event)
{
    m_keyboardFocusVisible = true;
    update();
    QCheckBox::keyPressEvent(event);
}

void CheckBox::mousePressEvent(QMouseEvent* event)
{
    m_keyboardFocusVisible = false;
    update();
    QCheckBox::mousePressEvent(event);
}

} // namespace fluent::basicinput
