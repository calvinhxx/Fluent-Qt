#include "Button.h"

#include <QPainterPath>

namespace fluent::basicinput {

namespace {

QMargins normalizedRadii(const QMargins& radii) {
    return QMargins(qMax(0, radii.left()),
                    qMax(0, radii.top()),
                    qMax(0, radii.right()),
                    qMax(0, radii.bottom()));
}

QMargins adjustedRadii(const QMargins& radii, int delta) {
    return normalizedRadii(QMargins(radii.left() + delta,
                                    radii.top() + delta,
                                    radii.right() + delta,
                                    radii.bottom() + delta));
}

QPainterPath roundedRectPath(const QRectF& rect, const QMargins& radii) {
    const qreal maxRadius = qMax<qreal>(0.0, qMin(rect.width(), rect.height()) / 2.0);
    const qreal topLeft = qBound<qreal>(0.0, radii.left(), maxRadius);
    const qreal topRight = qBound<qreal>(0.0, radii.top(), maxRadius);
    const qreal bottomRight = qBound<qreal>(0.0, radii.right(), maxRadius);
    const qreal bottomLeft = qBound<qreal>(0.0, radii.bottom(), maxRadius);

    QPainterPath path;
    path.moveTo(rect.left() + topLeft, rect.top());
    path.lineTo(rect.right() - topRight, rect.top());
    if (topRight > 0) {
        path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + topRight);
    } else {
        path.lineTo(rect.right(), rect.top());
    }

    path.lineTo(rect.right(), rect.bottom() - bottomRight);
    if (bottomRight > 0) {
        path.quadTo(rect.right(), rect.bottom(), rect.right() - bottomRight, rect.bottom());
    } else {
        path.lineTo(rect.right(), rect.bottom());
    }

    path.lineTo(rect.left() + bottomLeft, rect.bottom());
    if (bottomLeft > 0) {
        path.quadTo(rect.left(), rect.bottom(), rect.left(), rect.bottom() - bottomLeft);
    } else {
        path.lineTo(rect.left(), rect.bottom());
    }

    path.lineTo(rect.left(), rect.top() + topLeft);
    if (topLeft > 0) {
        path.quadTo(rect.left(), rect.top(), rect.left() + topLeft, rect.top());
    } else {
        path.lineTo(rect.left(), rect.top());
    }

    path.closeSubpath();
    return path;
}

void drawCenteredIconGlyph(QPainter& painter,
                           const QString& glyph,
                           const QString& fontFamily,
                           int pixelSize,
                           const QRectF& targetRect,
                           const QPoint& offset) {
    QFont iconFont(fontFamily);
    iconFont.setPixelSize(pixelSize);
    painter.setFont(iconFont);

    QPainterPath glyphPath;
    glyphPath.addText(QPointF(0, 0), iconFont, glyph);
    const QRectF inkBounds = glyphPath.boundingRect();
    if (inkBounds.isEmpty()) {
        painter.drawText(targetRect.translated(QPointF(offset.x(), offset.y())),
                         Qt::AlignCenter | Qt::AlignVCenter,
                         glyph);
        return;
    }

    const QPointF targetCenter = targetRect.center() + QPointF(offset.x(), offset.y());
    const QPointF pathOffset = targetCenter - inkBounds.center();
    painter.fillPath(glyphPath.translated(pathOffset), painter.pen().brush());
}

} // namespace

Button::Button(const QString& text, QWidget* parent) : QPushButton(text, parent) {
    setAttribute(Qt::WA_Hover);
    setFont(themeFont("Body").toQFont()); // 默认 Body，后续可用 setFont() 覆盖
}

Button::Button(QWidget* parent) : QPushButton(parent) {
    setAttribute(Qt::WA_Hover);
    setFont(themeFont("Body").toQFont());
}

void Button::setFluentStyle(ButtonStyle style) {
    if (m_style != style) { m_style = style; update(); emit fluentStyleChanged(); }
}

void Button::setFluentSize(ButtonSize size) {
    if (m_size != size) { 
        m_size = size; 
        updateGeometry(); // 关键：通知布局系统尺寸已变化
        update(); 
        emit fluentSizeChanged(); 
    }
}

void Button::setFluentLayout(ButtonLayout layout) {
    if (m_layout != layout) { 
        m_layout = layout; 
        updateGeometry();
        update(); 
        emit fluentLayoutChanged(); 
    }
}

void Button::setFocusVisual(bool focus) {
    if (m_focusVisual != focus) { m_focusVisual = focus; update(); emit focusVisualChanged(); }
}

void Button::setInteractionState(InteractionState state) {
    if (m_interactionState != state) { m_interactionState = state; update(); emit interactionStateChanged(); }
}

void Button::setCriticalOnHover(bool enabled) {
    if (m_criticalOnHover == enabled) return;
    m_criticalOnHover = enabled;
    update();
    emit criticalOnHoverChanged();
}

QMargins Button::cornerRadii() const {
    if (m_hasCustomCornerRadii)
        return m_cornerRadii;

    const int radius = themeRadius().control;
    return QMargins(radius, radius, radius, radius);
}

void Button::setCornerRadii(const QMargins& radii) {
    const QMargins normalized = normalizedRadii(radii);
    if (m_hasCustomCornerRadii && m_cornerRadii == normalized)
        return;

    m_hasCustomCornerRadii = true;
    m_cornerRadii = normalized;
    update();
    emit cornerRadiiChanged();
}

void Button::resetCornerRadii() {
    if (!m_hasCustomCornerRadii)
        return;

    m_hasCustomCornerRadii = false;
    m_cornerRadii = QMargins();
    update();
    emit cornerRadiiChanged();
}

void Button::setIconOffset(const QPoint& offset) {
    if (m_iconOffset == offset) return;
    m_iconOffset = offset;
    update();
}

void Button::setIconGlyph(const QString& glyph,
                          int pixelSize,
                          const QString& family) {
    if (glyph.isEmpty()) {
        m_iconGlyph.clear();
        m_iconFontFamily.clear();
        m_iconPixelSize = 0;
        setIcon(QIcon()); // 清除普通图标
        update();
        return;
    }

    // 保存 iconfont 信息，在 paintEvent 中直接绘制（参考 DropDownButton 方案）
    m_iconGlyph = glyph;
    m_iconFontFamily = family;
    m_iconPixelSize = pixelSize;
    
    // 清除普通图标，使用 iconfont 绘制
    setIcon(QIcon());
    
    // 更新尺寸提示
    updateGeometry();
    update();
}

QSize Button::sizeHint() const {
    const auto& spacing = themeSpacing();
    QFontMetrics fm(font());

    // 1. 基于 Token 计算内边距 (Padding)
    // 水平内边距：Small(8px), Standard(12px), Large(16px)
    int hPadding = (m_size == Small) ? spacing.small : (m_size == Large ? spacing.standard : spacing.padding.controlH);
    // 垂直内边距：Small(4px), Standard(6px), Large(8px)
    int vPadding = (m_size == Small) ? spacing.gap.tight : (m_size == Large ? spacing.small : spacing.padding.controlV);
    // 图标间距：Small(4px), Standard(8px)
    int iconGap = (m_size == Small) ? spacing.gap.tight : spacing.gap.normal;

    // 2. 计算动态高度：高度 = 字体高度 + 上下内边距
    int dynamicHeight = fm.height() + vPadding * 2;

    // 3. 计算内容所需的总宽度
    QString txt = text();
    // 优先使用 iconfont，否则使用普通图标
    bool hasIconFont = !m_iconGlyph.isEmpty();
    QSize icSize = hasIconFont ? QSize(m_iconPixelSize, m_iconPixelSize) : iconSize();
    int contentWidth = fm.horizontalAdvance(txt);
    if (!txt.isEmpty() && (hasIconFont || !icon().isNull())) contentWidth += iconGap;
    if (hasIconFont || !icon().isNull()) contentWidth += icSize.width();

    return QSize(contentWidth + hPadding * 2, dynamicHeight);
}

QSize Button::minimumSizeHint() const {
    return sizeHint();
}

void Button::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const auto& colors = themeColors();
    const auto& spacing = themeSpacing();

    // 1. 确定交互状态
    InteractionState state = m_interactionState;
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown()) state = Pressed;
        else if (underMouse()) state = Hover;
    }

    // 2. 获取色值，字体使用 QPushButton 的 font()
    painter.setFont(font());

    bool checked = isChecked();
    QColor bgColor, textColor, borderColor;
    // borderColor is the Fluent surface stroke; keyboard focus is painted by
    // the separate focusVisual block below.
    
    // 如果是 Accent 风格，或者按钮处于 Checked 状态且是 Standard 风格
    if (m_style == Accent || (checked && m_style == Standard)) {
        bgColor = colors.accentDefault;
        textColor = colors.textOnAccent;
        borderColor = colors.strokeStrong;
        if (state == Hover) bgColor = colors.accentSecondary;
        if (state == Pressed) {
            bgColor = colors.accentTertiary;
            borderColor = Qt::transparent; // 按下时边框扁平化
        }
    } else if (m_style == Subtle) {
        bgColor = Qt::transparent;
        textColor = (checked) ? colors.accentDefault : colors.textPrimary;
        borderColor = Qt::transparent;
        if (state == Hover) bgColor = colors.subtleSecondary;
        if (state == Pressed) bgColor = colors.subtleTertiary;
        if (checked && state == Rest) bgColor = colors.subtleSecondary; // 选中态默认带一点背景
    } else {
        bgColor = colors.controlDefault;
        textColor = colors.textPrimary;
        borderColor = colors.strokeDefault;
        if (state == Hover) bgColor = colors.controlSecondary;
        if (state == Pressed) {
            bgColor = colors.controlTertiary;
            borderColor = colors.strokeDivider; // 按下时边框颜色变淡且扁平
            textColor = colors.textSecondary;    // 文字稍微变淡
        }
    }

    if (state == Disabled) {
        bgColor = m_style == Subtle ? Qt::transparent : colors.controlDisabled;
        textColor = colors.textDisabled;
        borderColor = m_style == Subtle ? Qt::transparent : colors.strokeDivider;
    } else if (m_criticalOnHover && (state == Hover || state == Pressed)) {
        bgColor = state == Pressed ? colors.systemCritical.darker(115) : colors.systemCritical;
        textColor = colors.textOnAccent;
        borderColor = Qt::transparent;
    }

    // 3. 绘制背景和边框
    QRectF contentRect = rect();
    const QMargins radii = cornerRadii();
    
    // 如果是按下状态，内容稍微向下偏移 0.5 像素（模拟点击感）
    if (state == Pressed && m_style != Subtle) {
        contentRect.translate(0, 0.5);
    }
    
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawPath(roundedRectPath(contentRect, radii));

    if (borderColor != Qt::transparent) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(borderColor, 1)); 
        painter.drawPath(roundedRectPath(contentRect.adjusted(0.5, 0.5, -0.5, -0.5), radii));
    }

    if (state != Disabled && hasFocus() && m_focusVisual) {
        // 使用更柔和的文本次要色，并设置一定的透明度，使其不那么“黑”
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120); // 约 47% 不透明度
        
        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        
        // 恢复为 1.5 像素内缩
        painter.drawPath(roundedRectPath(contentRect.adjusted(1.5, 1.5, -1.5, -1.5), adjustedRadii(radii, -1)));
    }

    // 4. 计算并绘制图标和文字 (使用 Token 间距)
    QString txt = (m_layout == IconOnly) ? "" : text();
    bool hasIconFont = !m_iconGlyph.isEmpty();
    QPixmap pix = (m_layout == TextOnly || hasIconFont || icon().isNull()) ? QPixmap() : icon().pixmap(iconSize());
    int gap = (m_size == Small) ? spacing.gap.tight : spacing.gap.normal;

    QFontMetrics fm = painter.fontMetrics();
    int txtWidth = txt.isEmpty() ? 0 : fm.horizontalAdvance(txt);
    
    // 计算图标宽度：优先使用 iconfont，否则使用普通图标
    int iconWidth = 0;
    int iconHeight = 0;
    if (hasIconFont) {
        iconWidth = m_iconPixelSize;
        iconHeight = m_iconPixelSize;
    } else if (!pix.isNull()) {
        double dpr = pix.devicePixelRatio();
        iconWidth = pix.width() / dpr;
        iconHeight = pix.height() / dpr;
    }
    
    int totalContentWidth = txtWidth + iconWidth + ((!txt.isEmpty() && (hasIconFont || !pix.isNull())) ? gap : 0);
    
    double startX = contentRect.left() + (contentRect.width() - totalContentWidth) / 2.0;
    double centerY = contentRect.center().y();

    painter.setPen(textColor);
    painter.setRenderHint(QPainter::TextAntialiasing); // 确保 iconfont 文字抗锯齿
    
    if (m_layout == IconAfter) {
        // 文本在前，图标在后
        if (!txt.isEmpty()) {
            painter.drawText(QRectF(startX, 0, txtWidth, height()), Qt::AlignCenter, txt);
            startX += txtWidth + gap;
        }
        if (hasIconFont) {
            QRectF iconRect(startX, contentRect.top(), iconWidth, contentRect.height());
            drawCenteredIconGlyph(painter, m_iconGlyph, m_iconFontFamily,
                                  m_iconPixelSize, iconRect, m_iconOffset);
            painter.setFont(font());
        } else if (!pix.isNull()) {
            double dpr = pix.devicePixelRatio();
            double pixH = pix.height() / dpr;
            painter.drawPixmap(startX + m_iconOffset.x(),
                               centerY - pixH / 2.0 + m_iconOffset.y(),
                               pix);
        }
    } else {
        // 图标在前，文本在后（或仅图标）
        if (hasIconFont) {
            QRectF iconRect(startX, contentRect.top(), iconWidth, contentRect.height());
            drawCenteredIconGlyph(painter, m_iconGlyph, m_iconFontFamily,
                                  m_iconPixelSize, iconRect, m_iconOffset);
            painter.setFont(font());
            startX += iconWidth + gap;
        } else if (!pix.isNull()) {
            double dpr = pix.devicePixelRatio();
            double pixH = pix.height() / dpr;
            painter.drawPixmap(startX + m_iconOffset.x(),
                               centerY - pixH / 2.0 + m_iconOffset.y(),
                               pix);
            startX += iconWidth + gap;
        }
        if (!txt.isEmpty()) {
            painter.drawText(QRectF(startX, 0, txtWidth, height()), Qt::AlignCenter, txt);
        }
    }
}

} // namespace fluent::basicinput
