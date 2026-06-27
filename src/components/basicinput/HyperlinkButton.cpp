#include "HyperlinkButton.h"
#include <QDesktopServices>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionButton>

namespace fluent::basicinput {

HyperlinkButton::HyperlinkButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    setFluentStyle(Subtle);
    setCursor(Qt::PointingHandCursor);
    
    // On click, open the URL automatically when one is set.
    // zh_CN: 连接点击信号，设置了 URL 则自动打开。
    connect(this, &QPushButton::clicked, this, [this]() {
        if (m_url.isValid()) {
            QDesktopServices::openUrl(m_url);
        }
    });

    onThemeUpdated();
}

HyperlinkButton::HyperlinkButton(const QString& text, const QUrl& url, QWidget* parent)
    : HyperlinkButton(text, parent) {
    setUrl(url);
}

void HyperlinkButton::setUrl(const QUrl& url) {
    if (m_url != url) {
        m_url = url;
        emit urlChanged();
    }
}

void HyperlinkButton::setShowUnderline(bool show) {
    if (m_showUnderline != show) {
        m_showUnderline = show;
        update();
        emit showUnderlineChanged();
    }
}

void HyperlinkButton::onThemeUpdated() {
    // Force accent text: Button::paintEvent derives colors from m_style, but a
    // HyperlinkButton wants accent text even in the Subtle style.
    // zh_CN: 强制使用 Accent 文本色——Button::paintEvent 按 m_style 取色，而
    // HyperlinkButton 即使是 Subtle 样式也希望文本用 Accent 色。
    update();
}

void HyperlinkButton::paintEvent(QPaintEvent* event) {
    // A hyperlink is a text link: accent-colored text, minimal/no fill. The treatment
    // branches per design language; Fluent stays byte-for-byte unchanged, while Material 3
    // and macOS get their own idioms (accent state-layer pill / restrained accent link).
    // zh_CN: 超链接=文本链接:强调色文字、几乎无填充。按设计语言分支;Fluent 保持原状不变,
    // Material 3 与 macOS 各用其惯用法(强调色 state-layer 胶囊 / 克制的强调链接)。

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColors();
    const auto& radius = themeRadius();
    const auto& spacing = themeSpacing();
    const DesignLanguage lang = themeDesignLanguage();

    // 1. Resolve the interaction state. zh_CN: 确定交互状态。
    InteractionState state = interactionState();
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown()) state = Pressed;
        else if (underMouse()) state = Hover;
    }

    // Theme-aware veil: DARKEN light surfaces, LIGHTEN dark ones, so hover/press stay visible
    // under both App themes. (Hyperlinks mostly use the ACCENT veil, but the neutral veil is here
    // for parity with the canonical Button template.) zh_CN: 主题感知薄层:浅色面变暗、深色面变亮,
    // 使 hover/press 在明暗两主题下都可见。(超链接主要用强调色薄层,中性薄层保留以与 Button 模板一致。)
    const bool darkTheme = effectiveTheme() == Dark;
    auto veil = [&](int a) { return darkTheme ? QColor(255, 255, 255, a) : QColor(0, 0, 0, a); };
    Q_UNUSED(veil); // Hyperlink state layers use the ACCENT veil; the neutral veil is kept for template parity. zh_CN: 超链接用强调色薄层;中性薄层仅为模板一致性保留。
    const auto withAlpha = [](QColor c, int a) { c.setAlpha(a); return c; };

    // 2. Resolve colors. Default-init the fill to transparent so the invalid-QColor trap
    // (alpha()==255 on an unassigned QColor → solid black) can never fire. zh_CN: 填充默认透明,
    // 杜绝「未赋值 QColor 的 alpha()==255 → 涂黑」陷阱。
    QColor bgColor = Qt::transparent;   // resting/pressed surface fill (Fluent path). zh_CN: 静息/按下表面填充(Fluent)。
    QColor stateLayer = Qt::transparent; // M3/macOS accent state-layer veil. zh_CN: M3/macOS 强调色薄层。
    QColor textColor = colors.accentDefault;
    bool pill = false;        // M3: clip the state layer to a stadium. zh_CN: M3:state layer 裁剪为胶囊形。
    bool autoUnderline = false; // macOS: draw an underline on hover even without showUnderline. zh_CN: macOS:hover 自动下划线。

    if (lang == DesignMaterial) {
        // Material 3 TEXT button: primary/accent text, NO opaque fill at rest. Hover/press add an
        // accent (primary) state-layer veil at 8% / 10%, clipped to a pill behind the text. The
        // label stays accent-colored throughout. zh_CN: M3 文字按钮:强调色文字,静息无填充;hover/press
        // 叠加 8%/10% 强调色 state-layer 薄层,裁剪为胶囊置于文字之下;文字始终保持强调色。
        pill = true;
        textColor = colors.textAccentPrimary.isValid() ? colors.textAccentPrimary : colors.accentDefault;
        if (state == Hover) stateLayer = withAlpha(colors.accentDefault, 0x14);        // 8%
        else if (state == Pressed) stateLayer = withAlpha(colors.accentDefault, 0x1A); // 10%
        else if (state == Disabled) textColor = colors.textDisabled;
    } else if (lang == DesignCupertino) {
        // macOS LINK: restrained accent-blue text. Hover adds a faint accent state-layer AND an
        // underline (the macOS link idiom); press deepens the veil slightly. Understated — no heavy
        // fill. zh_CN: macOS 链接:克制的强调蓝文字;hover 叠加极淡强调色薄层并加下划线(macOS 链接惯用),
        // press 略加深;克制——无重填充。
        textColor = colors.accentDefault;
        if (state == Hover) {
            stateLayer = withAlpha(colors.accentDefault, 0x12); // ~7%
            autoUnderline = true;
        } else if (state == Pressed) {
            stateLayer = withAlpha(colors.accentDefault, 0x1E); // ~12%
            textColor = colors.accentSecondary.isValid() ? colors.accentSecondary : colors.accentDefault;
            autoUnderline = true;
        } else if (state == Disabled) {
            textColor = colors.textDisabled;
        }
    } else {
        // DesignFluent (default): unchanged WinUI text-link treatment. zh_CN: 默认 Fluent,WinUI 文本链接处理不变。
        if (state == Hover) {
            bgColor = colors.subtleSecondary;
            textColor = colors.accentSecondary;
        } else if (state == Pressed) {
            bgColor = colors.subtleTertiary;
            textColor = colors.accentTertiary;
        } else if (state == Disabled) {
            bgColor = Qt::transparent;
            textColor = colors.textDisabled;
        }
    }

    // 3. Paint the surface. Fluent uses a rounded subtle fill; M3/macOS use a clipped accent
    // state-layer (pill for M3, control radius for macOS) — never an oversized halo. The
    // invalid-QColor guard (isValid && alpha>0) prevents an accidental solid-black fill.
    // zh_CN: 绘制表面。Fluent 用圆角 subtle 填充;M3/macOS 用裁剪后的强调色 state-layer
    //(M3 胶囊、macOS 控件圆角),绝不溢出成光晕。isValid && alpha>0 守卫杜绝误涂黑。
    const QRectF surfaceRect = rect();
    if (bgColor.isValid() && bgColor.alpha() > 0 && bgColor != QColor(Qt::transparent)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(bgColor);
        painter.drawRoundedRect(surfaceRect, radius.control, radius.control);
    }
    if (stateLayer.isValid() && stateLayer.alpha() > 0) {
        const qreal r = pill ? qMin(surfaceRect.width(), surfaceRect.height()) / 2.0
                             : static_cast<qreal>(radius.control);
        QPainterPath pillPath;
        pillPath.addRoundedRect(surfaceRect, r, r);
        painter.save();
        painter.setClipPath(pillPath); // Keep the state layer inside the widget bounds. zh_CN: state layer 裁在控件范围内。
        painter.setPen(Qt::NoPen);
        painter.setBrush(stateLayer);
        painter.drawPath(pillPath);
        painter.restore();
    }

    // 4. Paint the text (centered). zh_CN: 居中绘制文字。
    painter.setFont(font());
    painter.setPen(textColor);

    QString txt = text();
    QFontMetrics fm = painter.fontMetrics();
    int txtWidth = fm.horizontalAdvance(txt);

    QRectF textRect = rect();
    painter.drawText(textRect, Qt::AlignCenter, txt);

    // 5. Underline: the explicit showUnderline opt-in draws on Hover (unchanged Fluent behavior,
    // and shared by every language); the macOS link idiom additionally auto-underlines on
    // Hover/Pressed. zh_CN: 下划线:showUnderline 显式开关在 Hover 绘制(沿用 Fluent 行为,各语言通用);
    // macOS 链接惯用法在 Hover/Pressed 额外自动下划线。
    const bool underlineExplicit = m_showUnderline && state == Hover;
    const bool underlineAuto = autoUnderline && (state == Hover || state == Pressed);
    if ((underlineExplicit || underlineAuto) && !txt.isEmpty()) {
        int textX = (width() - txtWidth) / 2;
        int textY = (height() + fm.ascent()) / 2; // Text baseline. zh_CN: 文字基线位置。
        // Draw the line 2px under the text in the text color. zh_CN: 在文字下方 2 像素处用文字色画线。
        painter.setPen(textColor);
        painter.drawLine(textX, textY + 2, textX + txtWidth, textY + 2);
    }

    Q_UNUSED(spacing);
    Q_UNUSED(event);
}

} // namespace fluent::basicinput
