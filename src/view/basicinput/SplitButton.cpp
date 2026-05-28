#include "SplitButton.h"
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionButton>

namespace view::basicinput {

SplitButton::SplitButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setMouseTracking(true);
}

void SplitButton::setMenu(QMenu* menu) {
    if (m_menu != menu) {
        m_menu = menu;
        emit menuChanged();
    }
}

void SplitButton::setSecondaryWidth(int width) {
    if (m_secondaryWidth != width) {
        m_secondaryWidth = width;
        updateGeometry();
        update();
        emit secondaryWidthChanged();
    }
}

void SplitButton::mouseMoveEvent(QMouseEvent* event) {
    SplitPart part = getPartAt(event->pos());
    if (m_hoverPart != part) {
        m_hoverPart = part;
        update();
    }
    Button::mouseMoveEvent(event);
}

void SplitButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressPart = getPartAt(event->pos());
        update();
    }
    Button::mousePressEvent(event);
}

void SplitButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        SplitPart releasePart = getPartAt(event->pos());
        
        if (releasePart == Secondary && m_pressPart == Secondary && m_menu) {
            // 弹出菜单
            QPoint popupPos = mapToGlobal(rect().bottomLeft());
            m_menu->exec(popupPos);
        } else if (releasePart == Primary && m_pressPart == Primary) {
            // 主按钮点击，信号已经在 Button (QPushButton) 内部处理
        }
        
        m_pressPart = None;
        update();
    }
    Button::mouseReleaseEvent(event);
}

void SplitButton::leaveEvent(QEvent* event) {
    m_hoverPart = None;
    m_pressPart = None;
    update();
    Button::leaveEvent(event);
}

SplitButton::SplitPart SplitButton::getPartAt(const QPoint& pos) const {
    if (!rect().contains(pos)) return None;
    
    // 使用配置的下拉区域宽度
    if (pos.x() > width() - m_secondaryWidth) return Secondary;
    return Primary;
}

QSize SplitButton::sizeHint() const {
    QSize base = Button::sizeHint();
    // 宽度 = 原本 Button 所需宽度 + 下拉区配置宽度
    return QSize(base.width() + m_secondaryWidth, base.height());
}

QSize SplitButton::minimumSizeHint() const {
    return sizeHint();
}

void SplitButton::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColors();
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();

    // 1. 获取尺寸参数
    int sWidth = m_secondaryWidth;
    int sepMargin = spacing.gap.tight; 
    
    // Chevron 字号跟随密度：Small 使用 Caption(12px), 其他使用 Body(14px)
    int chevronSize = (fluentSize() == Small) ? Typography::FontSize::Caption : Typography::FontSize::Body;

    QRectF fullRect = rect();
    QRectF primaryRect = fullRect.adjusted(0, 0, -sWidth, 0);
    QRectF secondaryRect = fullRect.adjusted(width() - sWidth, 0, 0, 0);

    // 2. 确定状态颜色
    bool checked = isChecked();
    QColor baseBg, textColor;
    
    if (fluentStyle() == Accent || checked) {
        baseBg = colors.accentDefault;
        textColor = colors.textOnAccent;
    } else {
        baseBg = colors.controlDefault;
        textColor = colors.textPrimary;
    }

    if (!isEnabled()) {
        baseBg = colors.controlDisabled;
        textColor = colors.textDisabled;
    }

    // 3. 绘制整体背景
    painter.setPen(Qt::NoPen);
    painter.setBrush(baseBg);
    painter.drawRoundedRect(fullRect, radius.control, radius.control);

    // 4. 绘制分区域高亮
    if (isEnabled()) {
        auto drawHighlight = [&](const QRectF& r, SplitPart part) {
            QColor highlight;
            if (m_pressPart == part) {
                highlight = (fluentStyle() == Accent || checked) ? colors.accentTertiary : colors.controlTertiary;
            } else if (m_hoverPart == part) {
                highlight = (fluentStyle() == Accent || checked) ? colors.accentSecondary : colors.controlSecondary;
            } else {
                return;
            }
            painter.setBrush(highlight);
            painter.save();
            painter.setClipRect(r);
            painter.drawRoundedRect(fullRect, radius.control, radius.control);
            painter.restore();
        };

        drawHighlight(primaryRect, Primary);
        drawHighlight(secondaryRect, Secondary);
    }

    // 5. 绘制分割线 (使用 Token 颜色)
    if (isEnabled()) {
        // 分割线颜色：Accent 风格下变淡，Standard 风格下使用标准边框色
        QColor sepColor = (fluentStyle() == Accent || checked) ? colors.strokeDivider : colors.strokeDefault;
        painter.setPen(QPen(sepColor, 1));
        painter.drawLine(QPointF(width() - sWidth, sepMargin), 
                         QPointF(width() - sWidth, height() - sepMargin));
    }

    // 6. 绘制主内容 (Text/Icon)
    painter.setPen(textColor);
    painter.setFont(font());
    // 两侧独立计算下沉偏移，模拟各自的点击触感
    const double primaryOffset   = (m_pressPart == Primary)   ? 0.5 : 0.0;
    const double secondaryOffset = (m_pressPart == Secondary) ? 0.5 : 0.0;

    QString txt = (fluentLayout() == IconOnly) ? "" : text();
    bool hasIconFont = !iconGlyph().isEmpty();
    int gap = (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;
    
    QFontMetrics fm = painter.fontMetrics();
    int txtWidth = txt.isEmpty() ? 0 : fm.horizontalAdvance(txt);
    int iconWidth = hasIconFont ? iconPixelSize() : 0;
    int totalContentWidth = txtWidth + iconWidth + ((!txt.isEmpty() && hasIconFont) ? gap : 0);
    
    // 计算在 primaryRect 内的起始位置
    double startX = primaryRect.left() + (primaryRect.width() - totalContentWidth) / 2.0;
    
    if (hasIconFont) {
        QFont iconFont(iconFontFamily());
        iconFont.setPixelSize(iconPixelSize());
        painter.setFont(iconFont);
        QRectF iconRect(startX, primaryRect.top() + primaryOffset, iconWidth, primaryRect.height());
        painter.drawText(iconRect, Qt::AlignCenter, iconGlyph());
        painter.setFont(font());
        startX += iconWidth + gap;
    }
    
    if (!txt.isEmpty()) {
        QRectF textRect(startX, primaryRect.top() + primaryOffset, txtWidth, primaryRect.height());
        painter.drawText(textRect, Qt::AlignCenter, txt);
    }

    // 7. 绘制 Chevron (下拉箭头)，按下时同样下沉 0.5px
    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(chevronSize);
    painter.setFont(iconFont);
    painter.drawText(secondaryRect.translated(0, secondaryOffset),
                     Qt::AlignCenter, Typography::Icons::ChevronDown);

    // 8. 焦点框
    if (hasFocus() && isEnabled()) {
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120);
        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(fullRect.adjusted(1.5, 1.5, -1.5, -1.5), radius.control - 1, radius.control - 1);
    }
}

} // namespace view::basicinput
