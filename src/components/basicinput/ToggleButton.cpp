#include "ToggleButton.h"
#include "design/CornerRadius.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

namespace fluent::basicinput {

namespace {

// Local rounded-rect path that mirrors Button's surface geometry (uniform radii).
// Kept private to ToggleButton's Material/macOS branches; the Fluent path still
// delegates to Button::paintEvent so it stays byte-for-byte identical.
// zh_CN: 复刻 Button 表面几何(等圆角)的本地圆角矩形路径,仅用于 ToggleButton 的 M3/macOS 分支;
// Fluent 路径仍委托 Button::paintEvent,保持完全一致。
QPainterPath uniformRoundedRectPath(const QRectF& rect, qreal radius) {
    const qreal maxRadius = qMax<qreal>(0.0, qMin(rect.width(), rect.height()) / 2.0);
    const qreal r = qBound<qreal>(0.0, radius, maxRadius);
    QPainterPath path;
    path.addRoundedRect(rect, r, r);
    return path;
}

} // namespace

ToggleButton::ToggleButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setCheckable(true);
    // Keep m_checkState in sync via the toggled signal. zh_CN: 连接 toggled 信号同步 m_checkState。
    connect(this, &QPushButton::toggled, this, [this](bool checked) {
        setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    });
}

ToggleButton::ToggleButton(QWidget* parent)
    : ToggleButton("", parent) {
}

void ToggleButton::setThreeState(bool threeState) {
    if (m_threeState != threeState) {
        m_threeState = threeState;
        emit threeStateChanged();
    }
}

Qt::CheckState ToggleButton::checkState() const {
    return m_checkState;
}

void ToggleButton::setCheckState(Qt::CheckState state) {
    if (m_checkState != state) {
        m_checkState = state;
        setChecked(m_checkState != Qt::Unchecked);
        update();
        emit checkStateChanged(m_checkState);
    }
}

void ToggleButton::nextCheckState() {
    if (m_threeState) {
        // Unchecked -> Checked -> PartiallyChecked -> Unchecked
        if (m_checkState == Qt::Unchecked) setCheckState(Qt::Checked);
        else if (m_checkState == Qt::Checked) setCheckState(Qt::PartiallyChecked);
        else setCheckState(Qt::Unchecked);
    } else {
        Button::nextCheckState();
    }
}

void ToggleButton::onThemeUpdated() {
    Button::onThemeUpdated();
}

void ToggleButton::paintBrandSurface(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    if (contentOpacity() < 1.0)
        painter.setOpacity(contentOpacity());
    painter.setFont(font());

    const auto& colors = themeColors();
    const DesignLanguage lang = themeDesignLanguage();
    const bool checked = isChecked();

    // Resolve the interaction state exactly like Button. zh_CN: 与 Button 一致地解析交互状态。
    InteractionState state = interactionState();
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown()) state = Pressed;
        else if (underMouse()) state = Hover;
    }

    // Theme-aware interaction veil: darkens light surfaces, lightens dark ones, so feedback is
    // visible under BOTH App themes. zh_CN: 主题感知交互薄层:浅色面变暗、深色面变亮,明暗两主题下都可见。
    const bool darkTheme = effectiveTheme() == Dark;
    const auto veil = [darkTheme](int a) {
        return darkTheme ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a);
    };
    const auto withAlpha = [](QColor c, int a) { c.setAlpha(a); return c; };

    // All overlays are guarded with isValid() && alpha() > 0 before setBrush — a default-constructed
    // QColor is INVALID yet reports alpha()==255, which would paint SOLID OPAQUE BLACK.
    // zh_CN: 所有叠加层在 setBrush 前都用 isValid() && alpha()>0 守卫——默认构造的 QColor 无效却返回 alpha==255,
    // 会涂成不透明纯黑。
    QColor bgColor = Qt::transparent;   // surface fill
    QColor textColor = colors.textPrimary;
    QColor borderColor = Qt::transparent;
    QColor stateLayer = Qt::transparent; // translucent overlay clipped to the surface

    // Geometry mirrors Button — no sizeHint/geometry change. zh_CN: 几何与 Button 一致,不改尺寸/几何。
    QRectF contentRect = rect();
    qreal radius = themeRadius().control;

    if (lang == DesignMaterial) {
        // Material 3: a SELECTABLE button. Unchecked = Outlined (1dp outline, accent text,
        // transparent fill); checked = Filled-tonal (secondary-container tonal surface + accent text).
        // The on-color/accent state layer (hover 8% / pressed 10%) is clipped to the pill.
        // zh_CN: Material 3 可选按钮。未选中=描边(1dp outline,accent 文字,透明填充);选中=填充色调
        //(secondary-container 色调面 + accent 文字)。state layer(hover 8%/pressed 10%)裁剪到胶囊内。
        radius = qMin(contentRect.width(), contentRect.height()) / 2.0; // stadium / pill
        if (checked) {
            // Tonal "secondary container": prefer the neutral control surface; if that is transparent
            // (e.g. unseeded), fall back to a low-alpha accent tint so the checked fill is never empty.
            // zh_CN: 色调「secondary container」:优先用中性 control 面;若其透明则回退到低透明强调色调,
            // 确保选中填充不为空。
            QColor tonal = colors.controlSecondary;
            if (!tonal.isValid() || tonal.alpha() == 0)
                tonal = withAlpha(colors.accentDefault, darkTheme ? 0x3A : 0x24);
            bgColor = tonal;
            textColor = colors.accentDefault;
            borderColor = Qt::transparent;
        } else {
            bgColor = Qt::transparent;
            textColor = colors.accentDefault;
            borderColor = colors.strokeStrong; // 1 dp outline
        }
        // §4 state layer: accent veil over the resting fill. zh_CN: §4 state layer:静息填充上的 accent 薄层。
        if (state == Hover) stateLayer = withAlpha(colors.accentDefault, 0x14);        // 8%
        else if (state == Pressed) stateLayer = withAlpha(colors.accentDefault, 0x1A); // 10%
    } else { // DesignCupertino
        // macOS: unchecked = neutral bezel push-button (bgLayerAlt fill + hairline + radius≈6);
        // checked = the recessed/selected look = accent fill + white text. Hover/press modulate via
        // the theme-aware veil (bezel) or accent darkening (selected). zh_CN: macOS:未选中=中性 bezel 推钮
        //(bgLayerAlt 填充 + 发丝描边 + 圆角≈6);选中=凹陷/选中外观=accent 填充 + 白字。hover/press 用
        // 主题感知薄层(bezel)或 accent 压暗(选中)调制。
        radius = 6.0;
        if (checked) {
            bgColor = colors.accentDefault;
            textColor = colors.textOnAccent;
            borderColor = colors.accentDefault.darker(128);
            if (state == Pressed) bgColor = colors.accentDefault.darker(112);
            else if (state == Hover) bgColor = colors.accentDefault.lighter(108);
        } else {
            bgColor = colors.bgLayerAlt;
            textColor = colors.textPrimary;
            borderColor = colors.strokeStrong;
            if (state == Hover) stateLayer = veil(darkTheme ? 0x1C : 0x16);
            else if (state == Pressed) stateLayer = veil(darkTheme ? 0x3A : 0x30);
        }
    }

    // Disabled override across both brands. zh_CN: 禁用覆盖,两种品牌通用。
    if (state == Disabled) {
        stateLayer = Qt::transparent;
        bgColor = checked ? colors.controlDisabled : Qt::transparent;
        textColor = colors.textDisabled;
        borderColor = colors.strokeDivider;
    }

    // Paint surface fill. zh_CN: 绘制表面填充。
    const QPainterPath surfacePath = uniformRoundedRectPath(contentRect, radius);
    painter.setPen(Qt::NoPen);
    if (bgColor.isValid() && bgColor.alpha() > 0) {
        painter.setBrush(bgColor);
        painter.drawPath(surfacePath);
    }

    // State layer: translucent overlay CLIPPED to the surface (never an oversized halo).
    // zh_CN: state layer:裁剪到表面的半透明叠加(绝不溢出成光晕)。
    if (stateLayer.isValid() && stateLayer.alpha() > 0) {
        painter.save();
        painter.setClipPath(surfacePath);
        painter.setBrush(stateLayer);
        painter.drawPath(surfacePath);
        painter.restore();
    }

    // Border / hairline. zh_CN: 边框 / 发丝描边。
    if (borderColor.isValid() && borderColor.alpha() > 0 && borderColor != QColor(Qt::transparent)) {
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(borderColor, 1));
        painter.drawPath(uniformRoundedRectPath(contentRect.adjusted(0.5, 0.5, -0.5, -0.5), radius));
    }

    // Focus visual, reusing Button's softened secondary-text ring. zh_CN: 焦点视觉,复用 Button 的柔化次要文本环。
    if (state != Disabled && hasFocus() && hasFocusVisual()) {
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(focusColor, 1.0));
        painter.drawPath(uniformRoundedRectPath(contentRect.adjusted(1.5, 1.5, -1.5, -1.5),
                                                qMax<qreal>(0.0, radius - 1.0)));
    }

    // Content: text + optional iconfont glyph, centered (mirrors Button's TextOnly/IconBefore path).
    // zh_CN: 内容:居中文字 + 可选 iconfont 字形(复刻 Button 的 TextOnly/IconBefore 路径)。
    const QString txt = (fluentLayout() == IconOnly) ? QString() : text();
    const bool hasGlyph = !iconGlyph().isEmpty();
    const auto& spacing = themeSpacing();
    const int gap = (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;

    QFontMetrics fm(font());
    const int txtWidth = txt.isEmpty() ? 0 : fm.horizontalAdvance(txt);
    const int glyphWidth = hasGlyph ? iconPixelSize() : 0;
    const int totalWidth = txtWidth + glyphWidth + ((!txt.isEmpty() && hasGlyph) ? gap : 0);

    qreal startX = contentRect.left() + (contentRect.width() - totalWidth) / 2.0;
    painter.setPen(textColor);
    painter.setRenderHint(QPainter::TextAntialiasing);

    if (hasGlyph) {
        const bool usesFluentIcons = iconFontFamily() == Typography::FontFamily::FluentIcons;
        QRectF iconRect(startX, contentRect.top(), glyphWidth, contentRect.height());
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
        startX += glyphWidth + (txt.isEmpty() ? 0 : gap);
    }
    if (!txt.isEmpty()) {
        painter.drawText(QRectF(startX, contentRect.top(), txtWidth, contentRect.height()),
                         Qt::AlignCenter, txt);
    }
}

void ToggleButton::paintEvent(QPaintEvent* event) {
    const DesignLanguage lang = themeDesignLanguage();

    // The indeterminate state keeps its Fluent-derived treatment (plain Button + bottom accent bar)
    // across every language so it stays consistent with Button. zh_CN: 中间态在所有语言下沿用源自 Fluent 的
    // 处理(普通 Button + 底部 accent 指示条),与 Button 保持一致。
    if (m_threeState && m_checkState == Qt::PartiallyChecked) {
        // Rendered as the plain button plus a bottom accent bar; a translucent
        // accent fill would be an alternative.
        // zh_CN: 以普通按钮加底部 Accent 指示条呈现；半透明 Accent 背景是可选方案。
        Button::paintEvent(event);

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto& colors = themeColors();

        // A small bottom bar marks the indeterminate state. zh_CN: 底部小横条表示中间态。
        int barHeight = 2;
        int barWidth = width() / 2;
        QRect barRect((width() - barWidth) / 2, height() - barHeight - 4, barWidth, barHeight);
        p.setPen(Qt::NoPen);
        p.setBrush(colors.accentDefault);
        p.drawRoundedRect(barRect, ::CornerRadius::Indicator, ::CornerRadius::Indicator);
        return;
    }

    if (lang == DesignFluent) {
        // DesignFluent (default): unchanged WinUI treatment. Button already paints the checked
        // (Standard→filled) and unchecked surfaces. zh_CN: 默认 Fluent,WinUI 处理不变。
        Button::paintEvent(event);
        return;
    }

    // Material 3 / macOS get a brand-specific selectable surface. zh_CN: Material 3 / macOS 使用品牌专属可选表面。
    paintBrandSurface(event);
}

} // namespace fluent::basicinput
