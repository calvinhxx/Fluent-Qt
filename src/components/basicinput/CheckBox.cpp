#include "CheckBox.h"
#include <QPainter>
#include <QPropertyAnimation>
#include "design/Typography.h"
#include "design/CornerRadius.h"

namespace fluent::basicinput {

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

    const auto& colors = themeColors();
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();
    
    bool isHover = underMouse();
    bool isPressed = isDown();
    bool enabled = isEnabled();
    Qt::CheckState state = checkState();

    // Brand design language drives the box SHAPE + hover/press feedback, mirroring Button::paintEvent.
    // zh_CN: 品牌设计语言决定方框形状与 hover/press 反馈,与 Button::paintEvent 保持一致。
    const auto lang = themeDesignLanguage();   // inherited from FluentElement. zh_CN: 继承自 FluentElement。

    // 1. Hover background, clamped to the actual content area. FLUENT ONLY: this full-widget row fill is a
    // WinUI cue; M3 and macOS supply their own per-box state layer / veil, so painting this under them
    // doubled up into a visible background artifact. zh_CN: 整体背景(Hover 态),限制在实际内容区域。仅 Fluent:
    // 该整控件行背景是 WinUI 反馈;M3 与 macOS 各自有 per-box 状态层 / 薄层,在它们之下再画会叠加成可见背景瑕疵。
    if (lang == DesignFluent && enabled && isHover && m_hoverBackgroundEnabled) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.subtleSecondary);
        // Slight inset so the fill never touches the edge. zh_CN: 稍微缩进，避免背景贴边。
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), radius.control, radius.control);
    }

    // 2. Paint the checkbox box. zh_CN: 绘制复选框方框。
    int boxY = (height() - m_boxSize) / 2;
    QRectF boxRect(m_boxMargin, boxY, m_boxSize, m_boxSize);

    const bool darkTheme = effectiveTheme() == fluent::FluentElement::Dark;
    // Theme-aware veil: darkens light surfaces, lightens dark ones — keeps hover/press visible in BOTH
    // App themes. zh_CN: 主题感知薄层:浅色面变暗、深色面变亮,使 hover/press 在明暗两主题下都可见。
    const auto veil = [darkTheme](int a){ return darkTheme ? QColor(255,255,255,a) : QColor(0,0,0,a); };

    QColor boxBg, boxBorder, iconColor;
    QColor stateHalo;       // DesignMaterial: circular accent halo behind the box. zh_CN: M3 圆形强调状态层。
    qreal boxRadius = radius.control;  // Per-language corner radius. zh_CN: 各设计语言的圆角半径。

    if (lang == DesignMaterial) {
        // Material 3: a small-radius (~2px) square. Checked/Indeterminate FILL the accent box with an
        // on-accent glyph; Unchecked is a transparent box with a ~2px strokeStrong outline. Hover/press
        // raise a faint circular accent "state layer" halo behind the box. zh_CN: Material 3:~2px 小圆角方框。
        // Checked/Indeterminate 用强调色填充并绘制 on 色图标;Unchecked 为透明填充 + ~2px strokeStrong 描边。
        // hover/press 在方框后方升起淡淡的圆形强调色状态层。
        boxRadius = 2;
        // Accent-tinted halo (selected) vs on-surface veil halo (unselected), per material-3.md §4:
        // the circular state layer is `primary` when selected and `on-surface` when not. Opacities are the
        // M3 standard 8% hover (0x14) / 10% press (0x1A). zh_CN: 选中态用强调色光晕、未选中态用 on-surface(veil)
        // 光晕,符合 material-3.md §4:状态层选中为 primary、未选中为 on-surface。透明度取 M3 标准 8% 悬停 / 10% 按下。
        const auto accentHalo = [&colors](int a){ return QColor(colors.accentDefault.red(), colors.accentDefault.green(), colors.accentDefault.blue(), a); };
        if (!enabled) {
            boxBg = (state == Qt::Unchecked) ? QColor(Qt::transparent) : colors.controlDisabled;
            boxBorder = (state == Qt::Unchecked) ? colors.strokeDivider : QColor(Qt::transparent);
            iconColor = colors.textDisabled;
        } else if (state == Qt::Unchecked) {
            // Unchecked: transparent fill + 2dp on-surface-variant (strokeStrong) outline; on-surface veil halo.
            // zh_CN: 未选中:透明填充 + 2dp on-surface-variant(strokeStrong)描边;on-surface(veil)圆形状态层。
            boxBg = Qt::transparent;
            boxBorder = colors.strokeStrong;
            iconColor = Qt::transparent;
            if (isPressed) stateHalo = veil(0x1A);
            else if (isHover) stateHalo = veil(0x14);
        } else {
            // Checked/Indeterminate: primary fill + on-primary glyph; primary-tinted halo. Modulate the accent
            // so hover/press read on top of the saturated fill. zh_CN: Checked/Indeterminate:primary 填充 + on-primary
            // 图标;primary 强调色状态层。调制强调色使 hover/press 在饱和填充上仍可辨。
            boxBg = isPressed ? colors.accentDefault.darker(112) : (isHover ? colors.accentDefault.lighter(110) : colors.accentDefault);
            boxBorder = Qt::transparent;
            iconColor = colors.textOnAccent;
            if (isPressed) stateHalo = accentHalo(0x1A);
            else if (isHover) stateHalo = accentHalo(0x14);
        }
    } else if (lang == DesignCupertino) {
        // macOS: a small rounded square (radius ~3.5, macos.md §4). Checked/Mixed FILL the accent (blue) box +
        // white glyph; Unchecked is a controlDefault box with a 1px strokeStrong hairline border. Hover/press
        // add a subtle theme-aware veil. The macOS box is physically smaller than the WinUI/M3 one, so we render
        // it slightly inset (paint-only — sizeHint/geometry untouched). zh_CN: macOS:小圆角方框(半径 ~3.5,见
        // macos.md §4)。Checked/Mixed 填充强调色(蓝) + 白色图标;Unchecked 为 controlDefault 填充 + 1px strokeStrong
        // 发丝边框。hover/press 叠加主题感知薄层。macOS 方框比 WinUI/M3 略小,故稍作内缩绘制(仅绘制,不改 sizeHint/geometry)。
        boxRadius = 3.5;
        if (!enabled) {
            boxBg = colors.controlDisabled;
            boxBorder = (state == Qt::Unchecked) ? colors.strokeDivider : QColor(Qt::transparent);
            iconColor = colors.textDisabled;
        } else if (state == Qt::Unchecked) {
            boxBg = colors.controlDefault;
            boxBorder = colors.strokeStrong;
            iconColor = Qt::transparent;
        } else {
            // Filled accent box; modulate the accent so hover/press stay visible over the saturated fill.
            // zh_CN: 填充强调色方框;调制强调色使 hover/press 在饱和填充上仍可见。
            boxBg = isPressed ? colors.accentDefault.darker(112) : (isHover ? colors.accentDefault.lighter(108) : colors.accentDefault);
            boxBorder = Qt::transparent;
            iconColor = colors.textOnAccent;
        }
    } else {
        // DesignFluent (default): unchanged WinUI treatment. zh_CN: 默认 Fluent,WinUI 处理不变。
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
    }

    // Material 3 state-layer halo: a soft circular accent wash behind the box, centered on the box.
    // The desired M3 halo extends ~55% of the box beyond each edge, but the widget's tight bounding box
    // clips anything past x=0 or the top/bottom edges into a hard-edged gray band that reads as an ugly
    // "shadow". CLAMP the radius to the largest centered circle that stays fully inside rect(), so the
    // ellipse is never cut at any edge. zh_CN: M3 状态层:方框后方柔和的圆形强调色,以方框为圆心。理想 M3
    // 光晕每侧外扩约方框的 55%,但控件紧致包围盒会把越过 x=0 或上下边的部分裁成硬边灰带,看起来像难看的"阴影"。
    // 故将半径夹到「以方框为圆心且完全落在 rect() 内的最大圆」,使椭圆在任何边都不被裁切。
    if (stateHalo.isValid() && stateHalo.alpha() > 0) {
        const QPointF center = boxRect.center();
        const qreal desiredRadius = m_boxSize * 0.5 + m_boxSize * 0.55; // box half + ~55% overhang. zh_CN: 方框半径 + 约 55% 外扩。
        // Largest radius that keeps the centered circle inside the widget on every side.
        // zh_CN: 使居中圆在每一侧都不越界的最大半径。
        const qreal maxRadius = qMin(qMin(center.x(), width() - center.x()),
                                     qMin(center.y(), height() - center.y()));
        const qreal haloRadius = qMax(0.0, qMin(desiredRadius, maxRadius));
        painter.setPen(Qt::NoPen);
        painter.setBrush(stateHalo);
        painter.drawEllipse(center, haloRadius, haloRadius);
    }

    // macOS renders a physically smaller box; inset the drawn box symmetrically (paint-only, geometry/glyph
    // centering unchanged). M3/Fluent draw the box at the full metric. zh_CN: macOS 方框更小,对称内缩绘制
    // (仅绘制,不改 geometry,图标居中不变)。M3/Fluent 仍按完整尺寸绘制。
    const qreal cupertinoInset = (lang == DesignCupertino) ? m_boxSize * 0.12 : 0.0;
    const QRectF boxDrawRect = boxRect.adjusted(cupertinoInset, cupertinoInset, -cupertinoInset, -cupertinoInset);

    // Paint the box fill. zh_CN: 绘制方框底色。
    painter.setPen(Qt::NoPen);
    painter.setBrush(boxBg);
    painter.drawRoundedRect(boxDrawRect, boxRadius, boxRadius);

    // Paint the box outline (unchecked only). zh_CN: 绘制方框描边（仅未选中时）。
    if (boxBorder != Qt::transparent) {
        // Material 3 uses a heavier ~2px outline; Fluent/macOS keep the 1px hairline. zh_CN: M3 用 ~2px 较重描边;Fluent/macOS 保持 1px 发丝边。
        const qreal borderWidth = (lang == DesignMaterial) ? 2.0 : 1.0;
        const qreal inset = borderWidth / 2.0;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(boxBorder, borderWidth));
        painter.drawRoundedRect(boxDrawRect.adjusted(inset, inset, -inset, -inset), boxRadius, boxRadius);
    }

    // macOS/Cupertino: a subtle theme-aware veil over the box marks hover/press for BOTH filled and unchecked
    // states; verified visible in light (black veil) AND dark (white veil). zh_CN: macOS:在方框上叠加主题感知薄层,
    // 为填充与未选中态都标示 hover/press;在浅色(黑薄层)与深色(白薄层)下均可见。
    if (lang == DesignCupertino && enabled && (isHover || isPressed)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(veil(isPressed ? 0x24 : 0x14));
        painter.drawRoundedRect(boxDrawRect, boxRadius, boxRadius);
    }

    // 3. Paint the inner glyph (icon font). zh_CN: 绘制内部图标。
    if (state != Qt::Unchecked) {
        painter.save();
        
        // WinUI uses the native 12 px check/subtract drawing inside its 20 px box.
        // zh_CN: WinUI 在 20 px 方框内使用原生 12 px 对勾/横线字形。
        const int fontSize = Typography::IconSize::Compact;
        const QFont iconFont = Typography::Icons::font(fontSize);
        painter.setFont(iconFont);
        painter.setPen(iconColor);
        
        // Animated reveal. zh_CN: 动画效果。
        painter.setOpacity(m_checkProgress);
        if (state == Qt::Checked) {
            painter.translate(boxRect.center());
            painter.scale(0.8 + 0.2 * m_checkProgress, 0.8 + 0.2 * m_checkProgress);
            painter.translate(-boxRect.center());
        }
        
        const QString glyph = Typography::Icons::glyphForSize(
            state == Qt::Checked ? Typography::Icons::CheckMark : Typography::Icons::Hyphen,
            fontSize);
        painter.drawText(boxRect, Qt::AlignCenter, glyph);
        
        painter.restore();
    }

    // 4. Paint the text. zh_CN: 绘制文本。
    if (!text().isEmpty()) {
        painter.setFont(font());
        painter.setPen(enabled ? colors.textPrimary : colors.textDisabled);
        
        // Text starts after the left margin, box, and text gap.
        // zh_CN: 文本起始位置 = 左侧边距 + 方框 + 文字间距。
        QRectF textRect = rect().adjusted(m_boxMargin + m_boxSize + m_textGap, 0, -m_boxMargin, 0);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
    }
}

} // namespace fluent::basicinput
