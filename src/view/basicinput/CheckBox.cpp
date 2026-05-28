#include "CheckBox.h"
#include <QPainter>
#include <QPropertyAnimation>
#include "design/Typography.h"
#include "design/CornerRadius.h"

namespace view::basicinput {

CheckBox::CheckBox(const QString& text, QWidget* parent)
    : QCheckBox(text, parent) {
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::ArrowCursor);
    
    auto fs = themeFont("Body");
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
    
    // 使用可配置的属性
    int w = m_boxSize + m_boxMargin * 2; // 左侧 margin + 方框 + 右侧 margin
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

    const auto& colors = themeColors();
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();
    
    bool isHover = underMouse();
    bool isPressed = isDown();
    bool enabled = isEnabled();
    Qt::CheckState state = checkState();

    // 1. 整体背景 (Hover 态) - 限制在实际内容区域
    if (enabled && isHover && m_hoverBackgroundEnabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.subtleSecondary);
        // 稍微缩进，避免背景贴边
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), radius.control, radius.control);
    }

    // 2. 绘制复选框方框
    int boxY = (height() - m_boxSize) / 2;
    QRectF boxRect(m_boxMargin, boxY, m_boxSize, m_boxSize);
    
    QColor boxBg, boxBorder, iconColor;
    
    if (!enabled) {
        boxBg = colors.controlDisabled;
        boxBorder = colors.strokeDivider;
        iconColor = colors.textDisabled;
    } else if (state == Qt::Unchecked) {
        boxBg = isPressed ? colors.controlTertiary : (isHover ? colors.controlSecondary : colors.controlDefault);
        boxBorder = isHover ? colors.strokeStrong : colors.strokeDefault;
        iconColor = Qt::transparent;
    } else {
        // Checked 或 Indeterminate：使用 Accent 颜色，且不显示独立边框
        boxBg = isPressed ? colors.accentTertiary : (isHover ? colors.accentSecondary : colors.accentDefault);
        boxBorder = Qt::transparent;
        iconColor = colors.textOnAccent;
    }

    // 绘制方框底色
    painter.setPen(Qt::NoPen);
    painter.setBrush(boxBg);
    painter.drawRoundedRect(boxRect, radius.control, radius.control);

    // 绘制方框描边 (仅在未选中时)
    if (boxBorder != Qt::transparent) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(boxBorder, 1.0));
        painter.drawRoundedRect(boxRect.adjusted(0.5, 0.5, -0.5, -0.5), radius.control, radius.control);
    }

    // 3. 绘制内部图标 (iconfont)
    if (state != Qt::Unchecked) {
        painter.save();
        
        QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
        // 使用设计 Token：Checked 使用 Body 字号，Indeterminate 使用 Caption 字号
        int fontSize = (state == Qt::Checked) ? Typography::FontSize::Body : Typography::FontSize::Caption;
        iconFont.setPixelSize(fontSize);
        painter.setFont(iconFont);
        painter.setPen(iconColor);
        
        // 动画效果
        painter.setOpacity(m_checkProgress);
        if (state == Qt::Checked) {
            painter.translate(boxRect.center());
            painter.scale(0.8 + 0.2 * m_checkProgress, 0.8 + 0.2 * m_checkProgress);
            painter.translate(-boxRect.center());
        }
        
        QString glyph = (state == Qt::Checked) ? Typography::Icons::CheckMark : Typography::Icons::Hyphen;
        painter.drawText(boxRect, Qt::AlignCenter, glyph);
        
        painter.restore();
    }

    // 4. 绘制文本
    if (!text().isEmpty()) {
        painter.setFont(font());
        painter.setPen(enabled ? colors.textPrimary : colors.textDisabled);
        
        // 文本区域起始位置 = 左侧边距 + 方框 + 文字间距
        QRectF textRect = rect().adjusted(m_boxMargin + m_boxSize + m_textGap, 0, -m_boxMargin, 0);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

} // namespace view::basicinput
