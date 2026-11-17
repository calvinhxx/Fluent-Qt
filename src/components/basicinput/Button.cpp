#include "compatibility/TextPaintCompat.h"
#include "components/foundation/private/ControlMetrics_p.h"
#include "compatibility/PaintCompat.h"
#include "compatibility/private/WidgetPlatformCompat_p.h"
#include "Button.h"
#include "private/ButtonContentPainter_p.h"

#include <QLinearGradient>
#include <QPainterPath>
#include <QPointer>
#include <QScopedValueRollback>

#include "components/foundation/private/DpiPaintMetrics_p.h"

namespace fluent::basicinput {

namespace {

QMargins normalizedRadii(const QMargins& radii)
{
    return QMargins(qMax(0, radii.left()), qMax(0, radii.top()), qMax(0, radii.right()), qMax(0, radii.bottom()));
}

QMargins adjustedRadii(const QMargins& radii, int delta)
{
    return normalizedRadii(
        QMargins(radii.left() + delta, radii.top() + delta, radii.right() + delta, radii.bottom() + delta));
}

QPainterPath roundedRectPath(const QRectF& rect, const QMargins& radii)
{
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

} // namespace

void Button::mousePressEvent(QMouseEvent* event)
{
    QPointer<Button> guard(this);
    QPushButton::mousePressEvent(event);
    if (guard)
        guard->update();
}

void Button::mouseReleaseEvent(QMouseEvent* event)
{
    QPointer<Button> guard(this);
    QPushButton::mouseReleaseEvent(event);
    if (guard)
        guard->update();
}

Button::Button(const QString& text, QWidget* parent) : QPushButton(text, parent)
{
    // The custom surface fills the widget; native layout insets must not consume its spacing.
    // zh_CN: 自绘表面覆盖完整控件，原生布局内缩不能抵消控件间距。
    setAttribute(Qt::WA_LayoutUsesWidgetRect);
    setAttribute(Qt::WA_Hover);
    ::compatibility::detail::disableNativeFocusRing(this);
    applyFontRole();
}

Button::Button(QWidget* parent) : QPushButton(parent)
{
    setAttribute(Qt::WA_LayoutUsesWidgetRect);
    setAttribute(Qt::WA_Hover);
    ::compatibility::detail::disableNativeFocusRing(this);
    applyFontRole();
}

void Button::setFluentStyle(ButtonStyle style)
{
    if (m_style != style) {
        m_style = style;
        update();
        emit fluentStyleChanged();
    }
}

void Button::setFluentSize(ButtonSize size)
{
    if (m_size != size) {
        m_size = size;
        updateGeometry(); // Required: tell the layout system the size changed. zh_CN: 关键：通知布局系统尺寸已变化。
        update();
        emit fluentSizeChanged();
    }
}

void Button::setFontRole(Typography::FontRole role)
{
    const bool roleChanged = m_fontRole != role;
    if (!roleChanged && !m_hasExplicitFont)
        return;

    m_fontRole = role;
    m_hasExplicitFont = false;
    applyFontRole();
    updateGeometry();
    update();
    if (roleChanged)
        emit fontRoleChanged();
}

void Button::setFont(const QFont& font)
{
    m_hasExplicitFont = true;
    QPushButton::setFont(font);
}

void Button::setTextColorRole(TextColorRole role)
{
    if (m_textColorRole == role)
        return;
    m_textColorRole = role;
    update();
    emit textColorRoleChanged();
}

void Button::onThemeUpdated()
{
    if (!m_hasExplicitFont)
        applyFontRole();
    update();
}

void Button::changeEvent(QEvent* event)
{
    QPushButton::changeEvent(event);
    if (event->type() != QEvent::FontChange)
        return;

    if (!m_applyingFontRole)
        m_hasExplicitFont = true;
    updateGeometry();
    update();
}

void Button::applyFontRole()
{
    const QFont resolvedFont = themeFont(m_fontRole).toQFont();
    if (font() == resolvedFont)
        return;

    const QScopedValueRollback<bool> applyingFontRole(m_applyingFontRole, true);
    QPushButton::setFont(resolvedFont);
}

void Button::setFluentLayout(ButtonLayout layout)
{
    m_hasExplicitLayout = true;
    if (m_layout != layout) {
        m_layout = layout;
        updateGeometry();
        update();
        emit fluentLayoutChanged();
    }
}

void Button::setIcon(const QIcon& newIcon)
{
    const bool changed = icon().cacheKey() != newIcon.cacheKey();
    QPushButton::setIcon(newIcon);
    updateIconPresentation(changed);
}

void Button::updateIconPresentation(bool nativeIconChanged)
{
    const bool promote = !m_hasExplicitLayout && m_layout == TextOnly && (!m_iconGlyph.isEmpty() || !icon().isNull());
    if (promote)
        m_layout = IconBefore;
    updateGeometry();
    update();

    // A layout observer may synchronously destroy the button.
    // zh_CN: 布局观察者可能同步销毁按钮。
    QPointer<Button> guard(this);
    if (promote)
        emit fluentLayoutChanged();
    if (guard && nativeIconChanged)
        emit iconChanged();
}

void Button::setFocusVisual(bool focus)
{
    if (m_focusVisual != focus) {
        m_focusVisual = focus;
        update();
        emit focusVisualChanged();
    }
}

void Button::setInteractionState(InteractionState state)
{
    if (m_interactionState != state) {
        m_interactionState = state;
        update();
        emit interactionStateChanged();
    }
}

void Button::setCriticalOnHover(bool enabled)
{
    if (m_criticalOnHover == enabled)
        return;
    m_criticalOnHover = enabled;
    update();
    emit criticalOnHoverChanged();
}

QMargins Button::cornerRadii() const
{
    if (m_hasCustomCornerRadii)
        return m_cornerRadii;

    const int radius = themeRadius().control;
    return QMargins(radius, radius, radius, radius);
}

void Button::setCornerRadii(const QMargins& radii)
{
    const QMargins normalized = normalizedRadii(radii);
    if (m_hasCustomCornerRadii && m_cornerRadii == normalized)
        return;

    m_hasCustomCornerRadii = true;
    m_cornerRadii = normalized;
    update();
    emit cornerRadiiChanged();
}

void Button::resetCornerRadii()
{
    if (!m_hasCustomCornerRadii)
        return;

    m_hasCustomCornerRadii = false;
    m_cornerRadii = QMargins();
    update();
    emit cornerRadiiChanged();
}

void Button::setIconOffset(const QPoint& offset)
{
    if (m_iconOffset == offset)
        return;
    m_iconOffset = offset;
    update();
}

void Button::setIconRotation(qreal rotation)
{
    if (qFuzzyCompare(m_iconRotation, rotation))
        return;
    m_iconRotation = rotation;
    update();
}

void Button::setIconScale(qreal scale)
{
    if (qFuzzyCompare(m_iconScale, scale))
        return;
    m_iconScale = scale;
    update();
}

void Button::setContentOpacity(qreal opacity)
{
    const qreal clamped = qBound<qreal>(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_contentOpacity, clamped))
        return;
    m_contentOpacity = clamped;
    update();
}

void Button::setIconGlyph(const QString& glyph, int pixelSize, const QString& family)
{
    const bool nativeIconChanged = !icon().isNull();
    if (glyph.isEmpty()) {
        m_iconGlyph.clear();
        m_iconFontFamily.clear();
        m_iconPixelSize = 0;
        QPushButton::setIcon(QIcon()); // Clear the regular icon. zh_CN: 清除普通图标。
        updateIconPresentation(nativeIconChanged);
        return;
    }

    // Store the icon-font glyph and paint it directly in paintEvent (same approach as DropDownButton).
    // zh_CN: 保存 iconfont 信息，在 paintEvent 中直接绘制（参考 DropDownButton 方案）。
    m_iconGlyph = glyph;
    m_iconFontFamily = family;
    m_iconPixelSize = pixelSize;

    // Drop the regular icon in favor of icon-font painting. zh_CN: 清除普通图标，使用 iconfont 绘制。
    QPushButton::setIcon(QIcon());

    // Refresh the size hint. zh_CN: 更新尺寸提示。
    updateIconPresentation(nativeIconChanged);
}

QSize Button::sizeHint() const
{
    const auto& spacing = themeSpacing();
    QFontMetrics fm(font());

    // 1. Padding from spacing tokens. zh_CN: 基于 Token 计算内边距。
    // Horizontal padding: Small(8px), Standard(12px), Large(16px). zh_CN: 水平内边距。
    int hPadding = (m_size == Small) ? spacing.small : (m_size == Large ? spacing.standard : spacing.padding.controlH);
    // Vertical padding: Small(4px), Standard(6px), Large(8px). zh_CN: 垂直内边距。
    int vPadding = (m_size == Small) ? spacing.gap.tight : (m_size == Large ? spacing.small : spacing.padding.controlV);
    // Icon gap: Small(4px), Standard(8px). zh_CN: 图标间距。
    int iconGap = (m_size == Small) ? spacing.gap.tight : spacing.gap.normal;

    const QString txt = (m_layout == IconOnly) ? QString() : text();
    const QSize textSize =
        txt.contains(QLatin1Char('\n')) ? fm.size(0, txt) : QSize(fm.horizontalAdvance(txt), fm.height());

    // 2. Include every explicit text line in the preferred height.
    // zh_CN: 建议高度包含全部显式换行的文本。
    int dynamicHeight = textSize.height() + vPadding * 2;
    if (m_size == StandardSize && !txt.contains(QLatin1Char('\n')))
        dynamicHeight = ::fluent::detail::standardControlHeight(fm);

    if (m_layout == IconAbove || m_layout == IconBelow) {
        if (m_iconGlyph.isEmpty() && icon().isNull())
            return QSize(textSize.width() + hPadding * 2, dynamicHeight);
        const QSize content = stackedContentSize();
        return QSize(content.width() + hPadding * 2, qMax(dynamicHeight, content.height() + vPadding * 2));
    }

    // 3. Total width required by the content. zh_CN: 计算内容所需的总宽度。
    // Prefer the icon font, falling back to the regular icon. zh_CN: 优先使用 iconfont，否则使用普通图标。
    bool hasIconFont = (m_layout != TextOnly) && !m_iconGlyph.isEmpty();
    bool hasRegularIcon = (m_layout != TextOnly) && !icon().isNull();
    QSize icSize = hasIconFont ? QSize(m_iconPixelSize, m_iconPixelSize) : iconSize();
    int contentWidth = (m_layout == IconOnly) ? 0 : textSize.width();
    if (!txt.isEmpty() && (hasIconFont || hasRegularIcon))
        contentWidth += iconGap;
    if (hasIconFont || hasRegularIcon)
        contentWidth += icSize.width();

    return QSize(contentWidth + hPadding * 2, dynamicHeight);
}

QSize Button::minimumSizeHint() const
{
    return sizeHint();
}

QRectF Button::contentPaintRect(const QRectF& surfaceRect) const
{
    return surfaceRect;
}

QSize Button::stackedContentSize() const
{
    const QSize label = text().isEmpty() ? QSize(0, 0) : QFontMetrics(font()).size(0, text());
    const QSize image = !m_iconGlyph.isEmpty()
        ? QSize(qMax(0, m_iconPixelSize), qMax(0, m_iconPixelSize))
        : (icon().isNull() ? QSize(0, 0) : iconSize());
    const auto& spacing = themeSpacing();
    const int gap =
        !label.isEmpty() && !image.isEmpty() ? (m_size == Small ? spacing.gap.tight : spacing.gap.normal) : 0;
    return QSize(qMax(label.width(), image.width()), label.height() + image.height() + gap);
}

QRectF Button::paintStackedContent(QPainter& painter, const QRectF& bounds) const
{
    const QSize label = text().isEmpty() ? QSize(0, 0) : QFontMetrics(font()).size(0, text());
    const QSize image = !m_iconGlyph.isEmpty()
        ? QSize(qMax(0, m_iconPixelSize), qMax(0, m_iconPixelSize))
        : (icon().isNull() ? QSize(0, 0) : iconSize());
    const QSize content = stackedContentSize();
    const qreal top = bounds.center().y() - content.height() / 2.0;
    const bool below = m_layout == IconBelow;
    const qreal iconTop = below ? top + content.height() - image.height() : top;
    const QRectF iconRect(bounds.center().x() - image.width() / 2.0, iconTop, image.width(), image.height());
    const qreal labelWidth = qMin<qreal>(label.width(), qMax<qreal>(0.0, bounds.width()));
    const qreal labelTop = below ? top : top + content.height() - label.height();
    const QRectF labelRect(bounds.center().x() - labelWidth / 2.0, labelTop, labelWidth, label.height());
    painter.save();
    painter.setClipRect(bounds, Qt::IntersectClip);
    if (!image.isEmpty())
        ButtonContentPainter::paintButtonIcon(*this, painter, iconRect);
    painter.setFont(font());
    fluent::painting::drawVerticallyCenteredText(painter, labelRect, text(), Qt::AlignHCenter);
    painter.restore();
    return labelRect;
}

void Button::paintEvent(QPaintEvent*)
{
    paintButton(m_interactionState);
}

void Button::paintButton(InteractionState state)
{
    paintButtonState(state, hasFocus());
}

void Button::paintButtonState(InteractionState state, bool focused)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    // Fade the whole surface uniformly when requested (e.g. reveal/hide animations). zh_CN: 需要时整体淡入淡出（如显隐动画）。
    if (m_contentOpacity < 1.0)
        painter.setOpacity(m_contentOpacity);

    const auto& colors = themeColorsRef();
    const auto& spacing = themeSpacing();

    // 1. Resolve the interaction state. zh_CN: 确定交互状态。
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown())
            state = Pressed;
        else if (underMouse())
            state = Hover;
    }

    // 2. Resolve colors; the font comes from QPushButton's font(). zh_CN: 获取色值，字体使用 QPushButton 的 font()。
    painter.setFont(font());

    bool checked = isChecked();
    const bool filled = (m_style == Accent || (checked && m_style == Standard));
    QColor bgColor, textColor, borderColor;
    // borderColor is the surface stroke; keyboard focus is painted by the separate focusVisual block.

    // Fluent interaction colors. zh_CN: Fluent 交互色。
    if (filled) {
        bgColor = colors.accentDefault;
        textColor = colors.textOnAccent;
        borderColor = colors.strokeStrong;
        if (state == Hover)
            bgColor = colors.accentSecondary;
        if (state == Pressed) {
            bgColor = colors.accentTertiary;
            borderColor = Qt::transparent; // Border flattens while pressed. zh_CN: 按下时边框扁平化。
        }
        if (checked && m_style == Accent && state == Rest) {
            // Checked accent button shows a deeper active state so toggle is visually distinct.
            // zh_CN: 选中的强调按钮显示更深的激活态底色，使开关状态清晰可见。
            bgColor = colors.accentTertiary;
        }
    } else if (m_style == Subtle) {
        bgColor = Qt::transparent;
        textColor = (checked) ? colors.accentDefault : colors.textPrimary;
        borderColor = Qt::transparent;
        if (state == Hover)
            bgColor = colors.subtleSecondary;
        if (state == Pressed)
            bgColor = colors.subtleTertiary;
        if (checked && state == Rest)
            bgColor = colors.subtleSecondary; // Checked rest keeps a subtle fill. zh_CN: 选中态默认带一点背景。
    } else {
        bgColor = colors.controlDefault;
        textColor = colors.textPrimary;
        borderColor = colors.strokeDefault;
        if (state == Hover)
            bgColor = colors.controlSecondary;
        if (state == Pressed) {
            bgColor = colors.controlTertiary;
            borderColor = colors.strokeDivider; // Pressed border fades and flattens. zh_CN: 按下时边框颜色变淡且扁平。
            textColor = colors.textSecondary;   // Text dims slightly. zh_CN: 文字稍微变淡。
        }
    }

    if (!filled && !checked) {
        if (m_textColorRole == TextColorRole::Secondary)
            textColor = colors.textSecondary;
        else if (m_textColorRole == TextColorRole::Accent)
            textColor = colors.textAccentPrimary;
    }

    // Disabled and critical-hover overrides. zh_CN: 禁用与危险悬停覆盖。
    if (state == Disabled) {
        bgColor = m_style == Subtle ? Qt::transparent : colors.controlDisabled;
        textColor = colors.textDisabled;
        borderColor = m_style == Subtle ? Qt::transparent : colors.strokeDivider;
    } else if (m_criticalOnHover && (state == Hover || state == Pressed)) {
        bgColor = state == Pressed ? colors.systemCritical.darker(115) : colors.systemCritical;
        textColor = colors.textOnAccent;
        borderColor = Qt::transparent;
    }

    // 3. Paint the background and border. zh_CN: 绘制背景和边框。
    const QRectF surfaceRect = rect();
    QMargins radii = cornerRadii();

    const QPainterPath surfacePath = roundedRectPath(surfaceRect, radii);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawPath(surfacePath);

    if (borderColor != Qt::transparent) {
        const auto borderStroke = fluent::painting::DpiPaintMetrics(painter).alignedStroke(surfaceRect, 1.0);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(borderColor, borderStroke.width));
        painter.drawPath(roundedRectPath(borderStroke.rect, radii));
    }

    // Opting into a focus cue must not paint every unfocused composite header.
    // zh_CN: 启用焦点提示不应让未聚焦的复合控件标题也绘制焦点框。
    if (state != Disabled && focused && m_focusVisual) {
        // High contrast focus line: clean white on filled accent, soft text secondary on standard.
        // zh_CN: 高对比度焦点线：强调色背景上使用清晰白色，普通背景使用柔和文本次要色。
        QColor focusColor = filled ? colors.textOnAccent : colors.textSecondary;
        focusColor.setAlpha(filled ? 220 : 130);

        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);

        // Restore the 1.5px inset. zh_CN: 恢复为 1.5 像素内缩。
        painter.drawPath(roundedRectPath(surfaceRect.adjusted(1.5, 1.5, -1.5, -1.5), adjustedRadii(radii, -1)));
    }

    // Shared content keeps inherited icon configuration consistent across button surfaces.
    // zh_CN: 共享内容绘制，使按钮派生组件继承相同的图标配置。
    QRectF contentRect = surfaceRect;
    if (state == Pressed && m_style != Subtle)
        contentRect.translate(0, 0.5);
    const QRectF layoutRect = contentPaintRect(contentRect);
    painter.setPen(textColor);
    painter.setRenderHint(QPainter::TextAntialiasing);
    if (m_layout == IconAbove || m_layout == IconBelow)
        paintStackedContent(painter, layoutRect);
    else
        ButtonContentPainter::paintHorizontalButtonContent(*this, painter, layoutRect,
            m_size == Small ? spacing.gap.tight : spacing.gap.normal);
}

} // namespace fluent::basicinput
