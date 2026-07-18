#include "Button.h"

#include <QLinearGradient>
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
                           const QPoint& offset,
                           qreal scale,
                           qreal rotation) {
    const bool usesFluentIcons = fontFamily == Typography::FontFamily::FluentIcons;
    const QString paintedGlyph = usesFluentIcons
        ? Typography::Icons::glyphForSize(glyph, pixelSize)
        : glyph;
    QFont iconFont = usesFluentIcons
        ? Typography::Icons::font(pixelSize)
        : QFont(fontFamily);
    if (!usesFluentIcons)
        iconFont.setPixelSize(pixelSize);
    painter.setFont(iconFont);

    // Measure the exact outline so optical centering remains independent of
    // font ascent/descent quirks, but let Qt's text rasterizer do the actual
    // drawing. Filling the path directly discards font hinting and makes 1 px
    // strokes noticeably soft at common button sizes on DirectWrite.
    QPainterPath measurementPath;
    measurementPath.addText(QPointF(0, 0), iconFont, paintedGlyph);
    const QRectF inkBounds = measurementPath.boundingRect();
    const QPointF targetCenter = targetRect.center() + QPointF(offset.x(), offset.y());
    const bool transformed = !qFuzzyCompare(scale, 1.0) || !qFuzzyIsNull(rotation);
    if (transformed) {
        painter.save();
        painter.translate(targetCenter);
        painter.rotate(rotation);
        painter.scale(scale, scale);
        painter.translate(-targetCenter);
    }
    if (inkBounds.isEmpty()) {
        painter.drawText(targetRect.translated(QPointF(offset.x(), offset.y())),
                         Qt::AlignCenter | Qt::AlignVCenter,
                         paintedGlyph);
        if (transformed)
            painter.restore();
        return;
    }

    QPointF baseline = targetCenter - inkBounds.center();
    const qreal devicePixelRatio = painter.device()
        ? painter.device()->devicePixelRatioF()
        : 1.0;
    if (devicePixelRatio > 0.0) {
        baseline.setX(qRound(baseline.x() * devicePixelRatio) / devicePixelRatio);
        baseline.setY(qRound(baseline.y() * devicePixelRatio) / devicePixelRatio);
    }
    painter.drawText(baseline, paintedGlyph);
    if (transformed)
        painter.restore();
}

} // namespace

Button::Button(const QString& text, QWidget* parent) : QPushButton(text, parent) {
    setAttribute(Qt::WA_Hover);
#ifdef Q_OS_MAC
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setFont(themeFont("Body").toQFont()); // Body by default; callers may setFont() later. zh_CN: 默认 Body，后续可用 setFont() 覆盖。
}

Button::Button(QWidget* parent) : QPushButton(parent) {
    setAttribute(Qt::WA_Hover);
#ifdef Q_OS_MAC
    setAttribute(Qt::WA_MacShowFocusRect, false);
#endif
    setFont(themeFont("Body").toQFont());
}

void Button::setFluentStyle(ButtonStyle style) {
    if (m_style != style) { m_style = style; update(); emit fluentStyleChanged(); }
}

void Button::setFluentSize(ButtonSize size) {
    if (m_size != size) { 
        m_size = size; 
        updateGeometry(); // Required: tell the layout system the size changed. zh_CN: 关键：通知布局系统尺寸已变化。
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

void Button::setIconRotation(qreal rotation) {
    if (qFuzzyCompare(m_iconRotation, rotation)) return;
    m_iconRotation = rotation;
    update();
}

void Button::setIconScale(qreal scale) {
    if (qFuzzyCompare(m_iconScale, scale)) return;
    m_iconScale = scale;
    update();
}

void Button::setContentOpacity(qreal opacity) {
    const qreal clamped = qBound<qreal>(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_contentOpacity, clamped)) return;
    m_contentOpacity = clamped;
    update();
}

void Button::setIconGlyph(const QString& glyph,
                          int pixelSize,
                          const QString& family) {
    if (glyph.isEmpty()) {
        m_iconGlyph.clear();
        m_iconFontFamily.clear();
        m_iconPixelSize = 0;
        setIcon(QIcon()); // Clear the regular icon. zh_CN: 清除普通图标。
        update();
        return;
    }

    // Store the icon-font glyph and paint it directly in paintEvent (same approach as DropDownButton).
    // zh_CN: 保存 iconfont 信息，在 paintEvent 中直接绘制（参考 DropDownButton 方案）。
    m_iconGlyph = glyph;
    m_iconFontFamily = family;
    m_iconPixelSize = pixelSize;
    
    // Drop the regular icon in favor of icon-font painting. zh_CN: 清除普通图标，使用 iconfont 绘制。
    setIcon(QIcon());
    
    // Refresh the size hint. zh_CN: 更新尺寸提示。
    updateGeometry();
    update();
}

QSize Button::sizeHint() const {
    const auto& spacing = themeSpacing();
    QFontMetrics fm(font());

    // 1. Padding from spacing tokens. zh_CN: 基于 Token 计算内边距。
    // Horizontal padding: Small(8px), Standard(12px), Large(16px). zh_CN: 水平内边距。
    int hPadding = (m_size == Small) ? spacing.small : (m_size == Large ? spacing.standard : spacing.padding.controlH);
    // Vertical padding: Small(4px), Standard(6px), Large(8px). zh_CN: 垂直内边距。
    int vPadding = (m_size == Small) ? spacing.gap.tight : (m_size == Large ? spacing.small : spacing.padding.controlV);
    // Icon gap: Small(4px), Standard(8px). zh_CN: 图标间距。
    int iconGap = (m_size == Small) ? spacing.gap.tight : spacing.gap.normal;

    // 2. Dynamic height: font height plus vertical padding. zh_CN: 高度 = 字体高度 + 上下内边距。
    int dynamicHeight = fm.height() + vPadding * 2;

    // 3. Total width required by the content. zh_CN: 计算内容所需的总宽度。
    QString txt = text();
    // Prefer the icon font, falling back to the regular icon. zh_CN: 优先使用 iconfont，否则使用普通图标。
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

QRectF Button::contentPaintRect(const QRectF& surfaceRect) const {
    return surfaceRect;
}

void Button::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    // Fade the whole surface uniformly when requested (e.g. reveal/hide animations). zh_CN: 需要时整体淡入淡出（如显隐动画）。
    if (m_contentOpacity < 1.0)
        painter.setOpacity(m_contentOpacity);

    const auto& colors = themeColors();
    const auto& spacing = themeSpacing();

    // 1. Resolve the interaction state. zh_CN: 确定交互状态。
    InteractionState state = m_interactionState;
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown()) state = Pressed;
        else if (underMouse()) state = Hover;
    }

    // 2. Resolve colors; the font comes from QPushButton's font(). zh_CN: 获取色值，字体使用 QPushButton 的 font()。
    painter.setFont(font());

    bool checked = isChecked();
    const DesignLanguage lang = themeDesignLanguage();
    const bool filled = (m_style == Accent || (checked && m_style == Standard));
    QColor bgColor, textColor, borderColor;
    // borderColor is the surface stroke; keyboard focus is painted by the separate focusVisual block.
    QColor stateLayer;       // DesignMaterial: translucent overlay drawn over the fill. zh_CN: M3 叠加层。
    bool pill = false;       // DesignMaterial: stadium-shaped buttons. zh_CN: M3 胶囊形。
    bool useGradient = false; // DesignCupertino: vertical bezel gradient. zh_CN: macOS 纵向斜面渐变。
    bool bezelShadow = false; // DesignCupertino: faint 1px lower-edge drop shadow on the neutral bezel. zh_CN: macOS:中性 bezel 底缘 1px 柔和投影。
    QColor gradTop, gradBottom;
    const auto withAlpha = [](QColor c, int a) { c.setAlpha(a); return c; };
    // Theme-aware interaction veil: a translucent overlay that DARKENS light surfaces and LIGHTENS dark
    // ones, so hover/press stay visible under both App themes (not just one). zh_CN: 主题感知交互薄层:浅色面
    // 变暗、深色面变亮,使 hover/press 在明暗两主题下都可见(而非只在某一种)。
    const bool darkTheme = effectiveTheme() == Dark;
    const auto veil = [darkTheme](int alpha) {
        return darkTheme ? QColor(255, 255, 255, alpha) : QColor(0, 0, 0, alpha);
    };

    if (lang == DesignMaterial) {
        // Material 3: filled buttons are stadium-shaped and stay saturated; interaction is a translucent
        // "state layer" of the on-color over the base, not a fill swap. The on-color veil adapts per theme
        // on its own (white over a dark accent, dark over a light one). Standard→outlined, Subtle→text.
        // zh_CN: Material 3:填充按钮为胶囊形且保持饱和;交互用「state layer」(on 色半透明叠加)而非换填充。
        // on 色薄层自动随主题适配(深色强调色上覆白、浅色强调色上覆深)。Standard→描边,Subtle→文字按钮。
        pill = true;
        if (filled) {
            bgColor = colors.accentDefault;
            textColor = colors.textOnAccent;
            borderColor = Qt::transparent;
            // §4 state layer: on-color veil at hover 8% (0x14) / pressed 10% (0x1A). zh_CN: §4 state layer:on 色薄层 hover 8%(0x14)/pressed 10%(0x1A)。
            if (state == Hover) stateLayer = withAlpha(colors.textOnAccent, 0x14);        // 8%
            else if (state == Pressed) stateLayer = withAlpha(colors.textOnAccent, 0x1A); // 10%
        } else {
            bgColor = Qt::transparent;
            textColor = colors.accentDefault;
            // §5: Outlined uses the 1 dp `outline` stroke (strokeStrong), not the lighter outline-variant. zh_CN: §5:描边按钮用 1dp outline 描边(strokeStrong),而非更浅的 outline-variant。
            borderColor = (m_style == Subtle) ? QColor(Qt::transparent) : colors.strokeStrong;
            // §4 state layer: accent (primary) veil at hover 8% (0x14) / pressed 10% (0x1A). zh_CN: §4 state layer:accent 色薄层 hover 8%(0x14)/pressed 10%(0x1A)。
            if (state == Hover) stateLayer = withAlpha(colors.accentDefault, 0x14);        // 8%
            else if (state == Pressed) stateLayer = withAlpha(colors.accentDefault, 0x1A); // 10%
        }
    } else if (lang == DesignCupertino) {
        // macOS: vertical bezel gradient + hairline border. The accent button modulates its OWN gradient
        // (brighten on hover, darken on press — visible in both themes because it tracks the accent hue,
        // not the surface); the neutral bezel and Subtle use the theme-aware veil. zh_CN: macOS 纵向斜面渐变
        // + 发丝描边。Accent 调制自身渐变(hover 提亮、press 压暗,因基于强调色而非表面,明暗都可见);中性 bezel
        // 与 Subtle 用主题感知薄层。
        if (m_style == Subtle) {
            bgColor = Qt::transparent;
            textColor = checked ? colors.accentDefault : colors.textPrimary;
            borderColor = Qt::transparent;
            if (state == Hover) bgColor = veil(darkTheme ? 0x14 : 0x12);
            else if (state == Pressed) bgColor = veil(darkTheme ? 0x2C : 0x24);
        } else if (filled) {
            useGradient = true;
            textColor = colors.textOnAccent;
            borderColor = colors.accentDefault.darker(128);
            if (state == Pressed) {
                gradTop = colors.accentDefault.darker(108);
                gradBottom = colors.accentDefault.darker(120);
            } else if (state == Hover) {
                gradTop = colors.accentDefault.lighter(124);
                gradBottom = colors.accentDefault.lighter(108);
            } else {
                gradTop = colors.accentDefault.lighter(113);
                gradBottom = colors.accentDefault;
            }
        } else {
            useGradient = true;
            gradTop = colors.bgLayerAlt.lighter(106);
            gradBottom = colors.bgLayerAlt.darker(105);
            textColor = colors.textPrimary;
            borderColor = colors.strokeStrong;
            // §3/§4: the standard push button is a real "bezel" — a faint 1px drop shadow under the
            // hairline border sells the raised look on near-white light backgrounds. zh_CN: §3/§4:标准按钮是真
            // 正的「bezel」——发丝描边下方 1px 柔和投影,使其在近白浅色背景上读出凸起感。
            bezelShadow = true;
            // Veil over the neutral bezel keeps hover/press visible on both light and dark bezels.
            // zh_CN: 中性 bezel 上叠加薄层,使 hover/press 在浅/深 bezel 上都可见。
            if (state == Hover) stateLayer = veil(darkTheme ? 0x1C : 0x16);
            else if (state == Pressed) stateLayer = veil(darkTheme ? 0x3A : 0x30);
        }
    } else {
        // DesignFluent (default): unchanged WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。
        if (filled) {
            bgColor = colors.accentDefault;
            textColor = colors.textOnAccent;
            borderColor = colors.strokeStrong;
            if (state == Hover) bgColor = colors.accentSecondary;
            if (state == Pressed) {
                bgColor = colors.accentTertiary;
                borderColor = Qt::transparent; // Border flattens while pressed. zh_CN: 按下时边框扁平化。
            }
        } else if (m_style == Subtle) {
            bgColor = Qt::transparent;
            textColor = (checked) ? colors.accentDefault : colors.textPrimary;
            borderColor = Qt::transparent;
            if (state == Hover) bgColor = colors.subtleSecondary;
            if (state == Pressed) bgColor = colors.subtleTertiary;
            if (checked && state == Rest) bgColor = colors.subtleSecondary; // Checked rest keeps a subtle fill. zh_CN: 选中态默认带一点背景。
        } else {
            bgColor = colors.controlDefault;
            textColor = colors.textPrimary;
            borderColor = colors.strokeDefault;
            if (state == Hover) bgColor = colors.controlSecondary;
            if (state == Pressed) {
                bgColor = colors.controlTertiary;
                borderColor = colors.strokeDivider; // Pressed border fades and flattens. zh_CN: 按下时边框颜色变淡且扁平。
                textColor = colors.textSecondary;    // Text dims slightly. zh_CN: 文字稍微变淡。
            }
        }
    }

    // Disabled and critical-hover overrides apply across every design language. zh_CN: 禁用与危险悬停覆盖,适用于所有设计语言。
    if (state == Disabled) {
        useGradient = false;
        bezelShadow = false;
        stateLayer = QColor();
        bgColor = m_style == Subtle ? Qt::transparent : colors.controlDisabled;
        textColor = colors.textDisabled;
        borderColor = m_style == Subtle ? Qt::transparent : colors.strokeDivider;
    } else if (m_criticalOnHover && (state == Hover || state == Pressed)) {
        useGradient = false;
        bezelShadow = false;
        stateLayer = QColor();
        bgColor = state == Pressed ? colors.systemCritical.darker(115) : colors.systemCritical;
        textColor = colors.textOnAccent;
        borderColor = Qt::transparent;
    }

    // 3. Paint the background and border. zh_CN: 绘制背景和边框。
    QRectF contentRect = rect();
    QMargins radii = cornerRadii();
    if (pill) {
        // Stadium shape: radius = half the shorter side. zh_CN: 胶囊形:圆角取较短边的一半。
        const int r = qRound(qMin(contentRect.width(), contentRect.height()) / 2.0);
        radii = QMargins(r, r, r, r);
    }

    // Fluent nudges content down 0.5px while pressed; Material/macOS use color, not motion. zh_CN: Fluent 按下时内容下移 0.5px;M3/macOS 用颜色而非位移。
    if (state == Pressed && m_style != Subtle && lang == DesignFluent) {
        contentRect.translate(0, 0.5);
    }

    const QPainterPath surfacePath = roundedRectPath(contentRect, radii);
    painter.setPen(Qt::NoPen);
    if (useGradient) {
        QLinearGradient gradient(contentRect.topLeft(), contentRect.bottomLeft());
        gradient.setColorAt(0.0, gradTop);
        gradient.setColorAt(1.0, gradBottom);
        painter.setBrush(gradient);
    } else {
        painter.setBrush(bgColor);
    }
    painter.drawPath(surfacePath);

    // Material 3 state layer: a translucent on-color veil over the resting fill. zh_CN: M3 state layer:静息填充上的半透明 on 色薄层。
    if (stateLayer.isValid() && stateLayer.alpha() > 0) {
        painter.setBrush(stateLayer);
        painter.drawPath(surfacePath);
    }

    // macOS bezel shadow: a faint hairline hugging the bottom inner edge, clipped to the surface so it
    // stays inside the rounded shape (paint-only, no geometry change). It reads as a soft drop shadow that
    // lifts the bezel off near-white backgrounds. zh_CN: macOS bezel 投影:贴底缘内侧的发丝线,裁剪在表面内(仅绘制,
    // 不改几何)。在近白背景上读作柔和投影,使 bezel 凸起。
    if (bezelShadow) {
        painter.save();
        painter.setClipPath(surfacePath);
        painter.setBrush(Qt::NoBrush);
        QColor shadow(0, 0, 0, darkTheme ? 0x3A : 0x1E); // softer on dark, ~12% on light. zh_CN: 深色更弱,浅色约 12%。
        painter.setPen(QPen(shadow, 1.0));
        painter.drawPath(roundedRectPath(contentRect.adjusted(0.5, 1.5, -0.5, -0.5), radii));
        painter.restore();
    }

    if (borderColor != Qt::transparent) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(borderColor, 1));
        painter.drawPath(roundedRectPath(contentRect.adjusted(0.5, 0.5, -0.5, -0.5), radii));
    }

    if (state != Disabled && hasFocus() && m_focusVisual) {
        // Use the softer secondary text color with some transparency so it reads less harsh.
        // zh_CN: 使用更柔和的文本次要色并加一定透明度，使其不那么“黑”。
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120); // About 47% opacity. zh_CN: 约 47% 不透明度。
        
        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        
        // Restore the 1.5px inset. zh_CN: 恢复为 1.5 像素内缩。
        painter.drawPath(roundedRectPath(contentRect.adjusted(1.5, 1.5, -1.5, -1.5), adjustedRadii(radii, -1)));
    }

    // 4. Lay out and paint the icon and text using token gaps. zh_CN: 使用 Token 间距计算并绘制图标和文字。
    QString txt = (m_layout == IconOnly) ? "" : text();
    bool hasIconFont = !m_iconGlyph.isEmpty();
    QPixmap pix = (m_layout == TextOnly || hasIconFont || icon().isNull()) ? QPixmap() : icon().pixmap(iconSize());
    int gap = (m_size == Small) ? spacing.gap.tight : spacing.gap.normal;

    QFontMetrics fm = painter.fontMetrics();
    int txtWidth = txt.isEmpty() ? 0 : fm.horizontalAdvance(txt);
    
    // Icon width: prefer the icon font, else the regular icon. zh_CN: 优先使用 iconfont，否则使用普通图标。
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
    
    const QRectF layoutRect = contentPaintRect(contentRect);
    double startX = layoutRect.left() + (layoutRect.width() - totalContentWidth) / 2.0;
    double centerY = layoutRect.center().y();

    painter.setPen(textColor);
    painter.setRenderHint(QPainter::TextAntialiasing); // Keep icon-font glyphs antialiased. zh_CN: 确保 iconfont 文字抗锯齿。

    const auto drawIconPixmap = [&](qreal x, qreal y) {
        if (pix.isNull()) return;
        const qreal dpr = pix.devicePixelRatio();
        const QSizeF logicalSize(pix.width() / dpr, pix.height() / dpr);
        if (qFuzzyIsNull(m_iconRotation)) {
            painter.drawPixmap(QPointF(x, y), pix);
            return;
        }
        painter.save();
        painter.translate(QPointF(x, y) + QPointF(logicalSize.width() / 2.0,
                                                   logicalSize.height() / 2.0));
        painter.rotate(m_iconRotation);
        painter.drawPixmap(QPointF(-logicalSize.width() / 2.0,
                                   -logicalSize.height() / 2.0), pix);
        painter.restore();
    };
    
    if (m_layout == IconAfter) {
        // Text first, icon after. zh_CN: 文本在前，图标在后。
        if (!txt.isEmpty()) {
            painter.drawText(QRectF(startX, layoutRect.top(), txtWidth, layoutRect.height()),
                             Qt::AlignCenter, txt);
            startX += txtWidth + gap;
        }
        if (hasIconFont) {
            QRectF iconRect(startX, layoutRect.top(), iconWidth, layoutRect.height());
            drawCenteredIconGlyph(painter, m_iconGlyph, m_iconFontFamily,
                                  m_iconPixelSize, iconRect, m_iconOffset, m_iconScale,
                                  m_iconRotation);
            painter.setFont(font());
        } else if (!pix.isNull()) {
            double dpr = pix.devicePixelRatio();
            double pixH = pix.height() / dpr;
            drawIconPixmap(startX + m_iconOffset.x(),
                           centerY - pixH / 2.0 + m_iconOffset.y());
        }
    } else {
        // Icon first, text after (or icon only). zh_CN: 图标在前，文本在后（或仅图标）。
        if (hasIconFont) {
            QRectF iconRect(startX, layoutRect.top(), iconWidth, layoutRect.height());
            drawCenteredIconGlyph(painter, m_iconGlyph, m_iconFontFamily,
                                  m_iconPixelSize, iconRect, m_iconOffset, m_iconScale,
                                  m_iconRotation);
            painter.setFont(font());
            startX += iconWidth + gap;
        } else if (!pix.isNull()) {
            double dpr = pix.devicePixelRatio();
            double pixH = pix.height() / dpr;
            drawIconPixmap(startX + m_iconOffset.x(),
                           centerY - pixH / 2.0 + m_iconOffset.y());
            startX += iconWidth + gap;
        }
        if (!txt.isEmpty()) {
            painter.drawText(QRectF(startX, layoutRect.top(), txtWidth, layoutRect.height()),
                             Qt::AlignCenter, txt);
        }
    }
}

} // namespace fluent::basicinput
