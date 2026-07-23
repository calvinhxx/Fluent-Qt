#include "SplitButton.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionButton>
#include <QVariantAnimation>
#include <QtMath>

namespace fluent::basicinput {

namespace {

int splitButtonTextWidth(const QFontMetrics& fm, const QString& text)
{
    if (text.isEmpty())
        return 0;
    // Ink can extend past horizontalAdvance; reserve the largest metric so sizeHint
    // matches painted glyphs (same gap rule as DropDownButton chevron reserve).
    // zh_CN: 墨水可能超出 horizontalAdvance;取最大度量保证 sizeHint 与绘制一致
    // (与 DropDownButton 预留 chevron 区同理)。
    const int advance = fm.horizontalAdvance(text);
    const int bounds = fm.boundingRect(text).width();
    const int tight = fm.tightBoundingRect(text).width();
    return qMax(advance, qMax(bounds, tight));
}

} // namespace

SplitButton::SplitButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setMouseTracking(true);
    m_pressAnimation = new QVariantAnimation(this);
    connect(m_pressAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                m_pressProgress = value.toReal();
                update();
            });
    connect(m_pressAnimation, &QVariantAnimation::finished, this, [this]() {
        m_pressProgress = 0.0;
        m_animatedPart = None;
        update();
    });
}

void SplitButton::startPressAnimation(SplitPart part) {
    if (!m_pressAnimation || part == None)
        return;
    m_animatedPart = part;
    m_pressAnimation->stop();
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
    m_pressAnimation->setStartValue(0.0);
    m_pressAnimation->setEndValue(1.0);
    m_pressAnimation->start();
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
        startPressAnimation(m_pressPart);
        update();
    }
    Button::mousePressEvent(event);
}

void SplitButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        SplitPart releasePart = getPartAt(event->pos());

        if (releasePart == Secondary && m_pressPart == Secondary && m_menu) {
            // Pop the menu. zh_CN: 弹出菜单。
            QPoint popupPos = mapToGlobal(rect().bottomLeft());
            m_menu->exec(popupPos);
        } else if (releasePart == Primary && m_pressPart == Primary) {
            // Primary click; the signal is already handled by Button (QPushButton).
            // zh_CN: 主按钮点击，信号已在 Button (QPushButton) 内部处理。
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

    // Use the configured drop-down zone width. zh_CN: 使用配置的下拉区域宽度。
    if (pos.x() > width() - m_secondaryWidth) return Secondary;
    return Primary;
}

int SplitButton::primaryTrailingInset() const
{
    const auto& spacing = themeSpacing();
    return (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;
}

QRectF SplitButton::primaryContentRect(const QRectF& primaryRect) const
{
    const qreal inset = qMin(primaryRect.width(), static_cast<qreal>(primaryTrailingInset()));
    return primaryRect.adjusted(0, 0, -inset, 0);
}

QSize SplitButton::sizeHint() const {
    const auto& spacing = themeSpacing();
    QFontMetrics fm(font());

    const int hPadding = (fluentSize() == Small) ? spacing.small
                         : (fluentSize() == Large ? spacing.standard : spacing.padding.controlH);
    const int vPadding = (fluentSize() == Small) ? spacing.gap.tight
                         : (fluentSize() == Large ? spacing.small : spacing.padding.controlV);
    const int iconGap = (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;

    const bool iconOnly = fluentLayout() == IconOnly;
    const QString txt = iconOnly ? QString() : text();
    const bool hasIconFont = !iconGlyph().isEmpty();
    const int txtWidth = splitButtonTextWidth(fm, txt);
    const int iconWidth = hasIconFont ? iconPixelSize() : 0;
    const int contentWidth = txtWidth + iconWidth
        + ((!txt.isEmpty() && hasIconFont) ? iconGap : 0);
    const int primaryWidth = qMax(Button::sizeHint().width(), contentWidth + hPadding * 2);
    // Trailing inset is for text clearance only; icon-only already centers in the primary zone.
    // zh_CN: 尾缘 inset 只服务于文字避让；纯图标已在主区内居中，不必再加宽。
    const int trailing = iconOnly ? 0 : primaryTrailingInset();

    return QSize(primaryWidth + m_secondaryWidth + trailing,
                 fm.height() + vPadding * 2);
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

    // 1. Size parameters (IDENTICAL across all design languages — only the painted surface changes,
    //    never the split geometry / hit-testing). zh_CN: 尺寸参数(所有设计语言一致——仅绘制表面变化,
    //    拆分几何/命中测试不变)。
    int sWidth = m_secondaryWidth;
    int sepMargin = spacing.gap.tight;

    // WinUI keeps the chevron on its native 12 px optical drawing at both
    // standard and compact control densities.
    // zh_CN: WinUI 在常规与紧凑密度下都使用原生 12 px chevron 字形。
    const int chevronSize = Typography::IconSize::Compact;

    QRectF fullRect = rect();
    QRectF primaryRect = fullRect.adjusted(0, 0, -sWidth, 0);
    QRectF secondaryRect = fullRect.adjusted(width() - sWidth, 0, 0, 0);

    const bool checked = isChecked();
    const bool accentLike = (fluentStyle() == Accent || checked);
    const DesignLanguage lang = themeDesignLanguage();

    // Theme-aware interaction veil: a translucent overlay that DARKENS light surfaces and LIGHTENS dark
    // ones, so hover/press stay visible under both App themes. zh_CN: 主题感知交互薄层:浅色面变暗、深色面
    // 变亮,使 hover/press 在明暗两主题下都可见。
    const bool darkTheme = effectiveTheme() == Dark;
    auto veil = [&](int a) { return darkTheme ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a); };

    // Surface corner radius per design language — the focus ring (drawn last) must match it so it stays
    // concentric with the painted outline instead of reading as a second, differently-rounded border.
    // M3 is a stadium (h/2); macOS clamps to ~6; Fluent uses the control radius. zh_CN: 各设计语言的表面圆角
    //——最后绘制的焦点环必须与之一致,才能与描边同心,而非读作另一圈圆角不同的边框。M3=胶囊(h/2),macOS≈6,Fluent=控件圆角。
    qreal surfaceRadius = radius.control;
    if (lang == DesignMaterial)
        surfaceRadius = qMin(fullRect.width(), fullRect.height()) / 2.0;
    else if (lang == DesignCupertino)
        surfaceRadius = qMax<qreal>(0.0, qMin<qreal>(radius.control, 6.0));

    // Colors resolved per-branch below. textColor/chevronColor are shared by the content paint.
    // zh_CN: 颜色按分支解析;textColor/chevronColor 供下方内容绘制共用。
    QColor textColor = accentLike ? colors.textOnAccent : colors.textPrimary;
    QColor chevronColor = textColor;
    if (!isEnabled()) {
        textColor = colors.textDisabled;
        chevronColor = colors.textDisabled;
    }

    if (lang == DesignMaterial) {
        // Material 3: the overall surface follows Button's M3 treatment for the active Style. Accent/checked
        // → filled stadium (accentDefault + textOnAccent); Standard → outlined (transparent fill + 1dp
        // `outline` stroke). The two segments keep a subtle 1dp divider; the trailing dropdown arrow uses
        // `on-surface-variant` (textSecondary). Hover/press = M3 state layer CLIPPED to the segment shape.
        // zh_CN: Material 3:整体表面沿用 Button 的 M3 处理。Accent/选中→填充胶囊(accentDefault + textOnAccent);
        // Standard→描边(透明填充 + 1dp outline 描边)。两段保留 1dp 分割线;下拉箭头用 on-surface-variant
        // (textSecondary)。Hover/press = 裁剪到对应段形状的 M3 state layer。
        const bool filled = accentLike;
        // Stadium shape: radius = half the shorter side. zh_CN: 胶囊形:圆角取较短边的一半。
        const qreal r = qMin(fullRect.width(), fullRect.height()) / 2.0;

        QPainterPath surfacePath;
        surfacePath.addRoundedRect(fullRect, r, r);

        QColor fill = Qt::transparent;
        QColor border = Qt::transparent;
        if (!isEnabled()) {
            fill = colors.controlDisabled;
            textColor = colors.textDisabled;
            chevronColor = colors.textDisabled;
            border = Qt::transparent;
        } else if (filled) {
            fill = colors.accentDefault;
            textColor = colors.textOnAccent;
            chevronColor = colors.textOnAccent;
        } else {
            fill = Qt::transparent;
            textColor = colors.accentDefault;
            // Dropdown arrow stays on-surface-variant so it reads as a chrome affordance, not the action.
            // zh_CN: 下拉箭头用 on-surface-variant,读作 chrome 而非操作本身。
            chevronColor = colors.textSecondary;
            border = colors.strokeStrong; // 1 dp `outline`. zh_CN: 1dp outline。
        }

        painter.setPen(Qt::NoPen);
        if (fill.isValid() && fill.alpha() > 0) {
            painter.setBrush(fill);
            painter.drawPath(surfacePath);
        }

        // Per-segment state layer, CLIPPED to the segment rect ∩ stadium so it never bleeds beyond the pill.
        // zh_CN: 分段 state layer,裁剪到 段矩形 ∩ 胶囊,绝不溢出胶囊外。
        if (isEnabled()) {
            auto drawStateLayer = [&](const QRectF& segRect, SplitPart part) {
                QColor layer; // default-constructed → INVALID; guard with isValid() below.
                const QColor on = filled ? colors.textOnAccent : colors.accentDefault;
                if (m_pressPart == part) layer = QColor(on.red(), on.green(), on.blue(), 0x1A);  // 10%
                else if (m_hoverPart == part) layer = QColor(on.red(), on.green(), on.blue(), 0x14); // 8%
                if (!(layer.isValid() && layer.alpha() > 0)) return;
                painter.save();
                painter.setClipPath(surfacePath);   // clip to pill …
                painter.setClipRect(segRect, Qt::IntersectClip); // … ∩ segment.
                painter.setBrush(layer);
                painter.setPen(Qt::NoPen);
                painter.drawPath(surfacePath);
                painter.restore();
            };
            drawStateLayer(primaryRect, Primary);
            drawStateLayer(secondaryRect, Secondary);
        }

        // 1 dp outline (Standard/outlined only). zh_CN: 1dp 描边(仅 Standard/描边)。
        if (border.isValid() && border.alpha() > 0) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(border, 1.0));
            QPainterPath borderPath;
            borderPath.addRoundedRect(fullRect.adjusted(0.5, 0.5, -0.5, -0.5), r, r);
            painter.drawPath(borderPath);
        }

        // Divider: a subtle on-surface hairline kept visible in both themes. zh_CN: 分割线:on-surface 发丝线,明暗都可见。
        if (isEnabled()) {
            QColor sepColor = filled ? QColor(colors.textOnAccent.red(), colors.textOnAccent.green(),
                                              colors.textOnAccent.blue(), 0x3D)  // ~24% on the filled pill
                                     : colors.strokeStrong;
            painter.setPen(QPen(sepColor, 1));
            painter.drawLine(QPointF(width() - sWidth, sepMargin),
                             QPointF(width() - sWidth, height() - sepMargin));
        }
    } else if (lang == DesignCupertino) {
        // macOS: bezel push-button — fill bgLayerAlt + 1px hairline + small radius (~6) + a faint bottom
        // drop shadow. The action + chevron segments are separated by a 1px hairline divider. Pressed/hover
        // feedback comes from the theme-aware veil, clipped per segment. zh_CN: macOS:bezel 按钮——填充
        // bgLayerAlt + 1px 发丝边 + 小圆角(~6) + 底缘柔和投影。操作段与箭头段以 1px 发丝分割线分隔。按下/悬停
        // 由主题感知薄层提供,按段裁剪。
        const qreal mR = qMax<qreal>(0.0, qMin<qreal>(radius.control, 6.0));
        QColor fill = isEnabled() ? colors.bgLayerAlt : colors.controlDisabled;
        textColor = isEnabled() ? colors.textPrimary : colors.textDisabled;
        chevronColor = textColor;

        QPainterPath surfacePath;
        surfacePath.addRoundedRect(fullRect.adjusted(0.5, 0.5, -0.5, -0.5), mR, mR);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPath(surfacePath);

        // Per-segment veil over the bezel keeps hover/press visible on both light and dark.
        // zh_CN: bezel 上叠加分段薄层,使 hover/press 在浅/深主题上都可见。
        if (isEnabled()) {
            auto drawVeil = [&](const QRectF& segRect, SplitPart part) {
                QColor layer; // INVALID by default — guard below.
                if (m_pressPart == part) layer = veil(darkTheme ? 0x3A : 0x30);
                else if (m_hoverPart == part) layer = veil(darkTheme ? 0x1C : 0x16);
                if (!(layer.isValid() && layer.alpha() > 0)) return;
                painter.save();
                painter.setClipPath(surfacePath);
                painter.setClipRect(segRect, Qt::IntersectClip);
                painter.setBrush(layer);
                painter.setPen(Qt::NoPen);
                painter.drawPath(surfacePath);
                painter.restore();
            };
            drawVeil(primaryRect, Primary);
            drawVeil(secondaryRect, Secondary);
        }

        // Faint 1px drop shadow hugging the bottom inner edge — sells the raised bezel. zh_CN: 贴底缘内侧 1px 柔和投影——读出凸起 bezel。
        if (isEnabled()) {
            painter.save();
            painter.setClipPath(surfacePath);
            painter.setBrush(Qt::NoBrush);
            QColor shadow(0, 0, 0, darkTheme ? 0x3A : 0x1E);
            painter.setPen(QPen(shadow, 1.0));
            QPainterPath shadowPath;
            shadowPath.addRoundedRect(fullRect.adjusted(0.5, 1.5, -0.5, -0.5), mR, mR);
            painter.drawPath(shadowPath);
            painter.restore();
        }

        // Hairline border. zh_CN: 发丝边框。
        const QColor hairline = isEnabled() ? colors.strokeStrong : colors.strokeSecondary;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(hairline, 1.0));
        painter.drawPath(surfacePath);

        // 1px hairline divider between the two segments. zh_CN: 两段之间 1px 发丝分割线。
        if (isEnabled()) {
            painter.setPen(QPen(colors.strokeStrong, 1));
            painter.drawLine(QPointF(width() - sWidth, sepMargin),
                             QPointF(width() - sWidth, height() - sepMargin));
        }
    } else {
        // DesignFluent (default): UNCHANGED WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。

        // 2. State colors. zh_CN: 确定状态颜色。
        QColor baseBg;

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
        chevronColor = textColor;

        // 3. Paint the shared background. zh_CN: 绘制整体背景。
        painter.setPen(Qt::NoPen);
        painter.setBrush(baseBg);
        painter.drawRoundedRect(fullRect, radius.control, radius.control);

        // 4. Paint per-zone highlights. zh_CN: 绘制分区域高亮。
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

        // 5. Paint the divider with token colors. zh_CN: 绘制分割线（使用 Token 颜色）。
        if (isEnabled()) {
            // Divider: lighter on Accent, standard stroke on Standard.
            // zh_CN: 分割线——Accent 风格下变淡，Standard 风格用标准边框色。
            QColor sepColor = (fluentStyle() == Accent || checked) ? colors.strokeDivider : colors.strokeDefault;
            painter.setPen(QPen(sepColor, 1));
            painter.drawLine(QPointF(width() - sWidth, sepMargin),
                             QPointF(width() - sWidth, height() - sepMargin));
        }
    }

    // 6. Paint the primary content (text/icon). Shared across design languages. zh_CN: 绘制主内容(各设计语言共用)。
    painter.setPen(textColor);
    painter.setFont(font());
    // Each zone sinks independently to suggest its own press feedback.
    // zh_CN: 两侧独立计算下沉偏移，模拟各自的点击触感。
    constexpr qreal kPi = 3.14159265358979323846;
    const qreal rebound = qSin(qBound<qreal>(0.0, m_pressProgress, 1.0) * kPi);
    const double primaryOffset = (m_pressPart == Primary ? 0.5 : 0.0)
        + (m_animatedPart == Primary ? rebound * 2.0 : 0.0);
    const double secondaryOffset = (m_pressPart == Secondary ? 0.5 : 0.0)
        + (m_animatedPart == Secondary ? rebound * 2.0 : 0.0);

    QString txt = (fluentLayout() == IconOnly) ? "" : text();
    bool hasIconFont = !iconGlyph().isEmpty();
    int gap = (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;

    int iconWidth = hasIconFont ? iconPixelSize() : 0;

    // Text layouts keep a trailing inset before the divider (DropDownButton pattern). Icon-only
    // centers in the full primary zone — applying that inset there slides the glyph left and
    // leaves an awkward gap before the chevron.
    // zh_CN: 带文字布局在分割线前保留尾缘间距(对齐 DropDownButton)。纯图标则在完整主区内居中——
    // 若再套该 inset 会把字形挤向左侧，与箭头之间留下别扭空隙。
    const bool iconOnly = fluentLayout() == IconOnly;
    const QRectF layoutRect = iconOnly ? primaryRect : primaryContentRect(primaryRect);
    const int hPadding = (fluentSize() == Small) ? spacing.small
                         : (fluentSize() == Large ? spacing.standard : spacing.padding.controlH);
    double startX = layoutRect.left();
    if (iconOnly && hasIconFont) {
        startX += (layoutRect.width() - iconWidth) / 2.0;
    } else {
        startX += hPadding;
    }

    if (hasIconFont) {
        const bool usesFluentIcons = iconFontFamily() == Typography::FontFamily::FluentIcons;
        QRectF iconRect(startX, primaryRect.top() + primaryOffset, iconWidth, primaryRect.height());
        if (usesFluentIcons) {
            Typography::Icons::paintGlyph(
                painter, iconRect, iconGlyph(), iconPixelSize(), Qt::AlignCenter);
        } else {
            QFont iconFont(iconFontFamily());
            iconFont.setPixelSize(iconPixelSize());
            painter.setFont(iconFont);
            painter.drawText(iconRect, Qt::AlignCenter, iconGlyph());
            painter.setFont(font());
        }
        if (fluentLayout() != IconOnly)
            startX += iconWidth + gap;
    }

    if (!txt.isEmpty()) {
        const qreal textWidth = qMax<qreal>(0.0, layoutRect.right() - startX);
        painter.save();
        painter.setClipRect(layoutRect);
        QRectF textRect(startX, primaryRect.top() + primaryOffset, textWidth, primaryRect.height());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, txt);
        painter.restore();
    }

    // 7. Paint the chevron; it also sinks 0.5px while pressed. zh_CN: 绘制下拉箭头，按下时同样下沉 0.5px。
    if (m_animatedPart == Secondary && rebound > 0.0)
        chevronColor.setAlphaF(chevronColor.alphaF() * (1.0 - 0.25 * rebound));
    painter.setPen(chevronColor);
    Typography::Icons::paintGlyph(
        painter,
        secondaryRect.translated(0, secondaryOffset),
        Typography::Icons::ChevronDown,
        chevronSize,
        Qt::AlignCenter);

    // 8. Focus ring. Fluent keeps its original always-on-focus inset ring (radius.control - 1). M3/macOS
    //    draw it ONLY for the keyboard focus visual (matching Button), and at the per-language surface
    //    radius so it stays concentric with the outline instead of adding a second, mismatched border.
    //    zh_CN: 焦点框。Fluent 保留原有"任何焦点都画"的内缩环;M3/macOS 仅在键盘焦点(与 Button 一致)绘制,
    //    且半径取表面圆角,与描边同心,不再多出一圈圆角不符的边。
    if (hasFocus() && isEnabled()) {
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120);
        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        if (lang == DesignFluent) {
            painter.drawRoundedRect(fullRect.adjusted(1.5, 1.5, -1.5, -1.5),
                                    radius.control - 1, radius.control - 1);
        } else if (hasFocusVisual()) {
            const qreal fr = qMax<qreal>(0.0, surfaceRadius - 1.0);
            painter.drawRoundedRect(fullRect.adjusted(1.5, 1.5, -1.5, -1.5), fr, fr);
        }
    }
}

} // namespace fluent::basicinput
