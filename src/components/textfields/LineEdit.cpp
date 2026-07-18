#include "LineEdit.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFocusEvent>
#include <QResizeEvent>
#include <QValidator>
#include "components/basicinput/Button.h"
#include "design/Typography.h"

namespace fluent::textfields {

namespace {

QColor opaqueTextColor(const QColor& foreground, const QColor& background)
{
    const qreal alpha = foreground.alphaF();
    return QColor::fromRgbF(foreground.redF() * alpha + background.redF() * (1.0 - alpha),
                            foreground.greenF() * alpha + background.greenF() * (1.0 - alpha),
                            foreground.blueF() * alpha + background.blueF() * (1.0 - alpha),
                            1.0);
}

} // namespace

LineEdit::LineEdit(QWidget* parent)
    : QLineEdit(parent) {
    setFrame(false);
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);

    // Built-in fluent clear button. zh_CN: 内置 Fluent 清除按钮。
    m_clearButton = new ::fluent::basicinput::Button(this);
    m_clearButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
    m_clearButton->setFluentSize(::fluent::basicinput::Button::Small);
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->setIconGlyph(::Typography::Icons::Dismiss,
                                ::Typography::IconSize::Standard,
                                ::Typography::FontFamily::FluentIcons);
    m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
    m_clearButton->hide();

    connect(m_clearButton, &::fluent::basicinput::Button::clicked, this, [this]() {
        clear();
        setFocus();
    });
    connect(this, &QLineEdit::textChanged, this, [this]() {
        updateClearButtonVisibility();
    });

    applyThemeStyle();
}

void LineEdit::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_frameVisible)
        paintFrame(p);
    QLineEdit::paintEvent(event);
}

void LineEdit::resizeEvent(QResizeEvent* event) {
    QLineEdit::resizeEvent(event);
    updateClearButtonGeometry();
}

void LineEdit::updateClearButtonGeometry() {
    if (!m_clearButton) return;
    m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
    int x = width() - m_clearButtonSize - m_clearButtonOffset.x();
    int y = (height() - m_clearButtonSize) / 2 + m_clearButtonOffset.y();
    m_clearButton->move(x, y);
}

void LineEdit::paintFrame(QPainter& painter) {
    const auto& colors = themeColors();
    const auto& radius = themeRadius();

    // Branch the border/focus treatment per design language (palette is already swapped by
    // ThemeRegistry; this picks the SHAPE of the field). Fluent keeps its bottom accent underline;
    // Material is an outlined rect that thickens to a 2dp accent outline on focus; Cupertino is a
    // hairline rounded rect that gains an accent focus ring. All paths paint strictly inside rect()
    // so no halo gets clipped. zh_CN: 按设计语言分支边框/焦点处理(调色板已由 ThemeRegistry 替换,此处
    // 决定字段「形状」)。Fluent 保留底部强调下划线;Material 为描边矩形,聚焦时加粗为 2dp 强调描边;
    // Cupertino 为发丝圆角矩形,聚焦时出现强调焦点环。所有路径都严格绘制在 rect() 内,焦点光环不会被裁切。
    const auto lang = themeDesignLanguage();

    if (lang == DesignMaterial) {
        // Material 3 OUTLINED text field (docs §5): transparent fill inside a 1 dp `outline`
        // (strokeStrong) rounded rect at the control radius, thickening to a 2 dp `accentDefault`
        // outline on focus. No bottom underline in this branch — the full outline replaces the
        // Fluent look. zh_CN: Material 3 描边文本框(文档 §5):控件圆角的 1dp outline(strokeStrong)
        // 圆角矩形包裹透明填充,聚焦时加粗为 2dp accentDefault 描边。此分支无底部下划线——完整描边取代
        // Fluent 样式。
        QColor outlineColor;
        qreal outlineWidth = 1.0;
        if (!isEnabled()) {
            outlineColor = colors.strokeDivider;
        } else if (m_isFocused) {
            outlineColor = colors.accentDefault;
            outlineWidth = 2.0;
        } else if (m_isHovered) {
            outlineColor = colors.strokeStrong;
        } else {
            outlineColor = colors.strokeStrong;
        }

        // Inset by half the stroke so a thick (2dp) focus outline stays within the widget bounds.
        // zh_CN: 按描边一半内缩,使加粗(2dp)焦点描边仍处于控件范围内。
        const qreal inset = outlineWidth / 2.0;
        QRectF outlineRect = QRectF(rect()).adjusted(inset, inset, -inset, -inset);
        const qreal r = radius.control;
        QPainterPath framePath;
        framePath.addRoundedRect(outlineRect, r, r);

        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::NoBrush); // Outlined variant has no opaque fill. zh_CN: 描边变体无不透明填充。
        painter.drawPath(framePath);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outlineColor, outlineWidth));
        painter.drawPath(framePath);
        return;
    }

    if (lang == DesignCupertino) {
        // macOS field (docs §3/§4): a small-radius (~6) rounded rect with a 1 px hairline
        // `strokeStrong` border at rest. On focus it gains an accent (blue) RING — a 2 px
        // accentDefault border plus a faint outer accent glow drawn INSET so it never paints
        // beyond rect() (which would clip). Non-focused = just the hairline. zh_CN: macOS 字段
        // (文档 §3/§4):小圆角(~6)圆角矩形,静息态 1px strokeStrong 发丝边框。聚焦时出现强调(蓝)环
        // ——2px accentDefault 边框 + 内缩绘制的淡淡外层强调辉光(绝不超出 rect(),否则被裁切)。非聚焦=仅发丝。
        const qreal r = 6.0; // macOS regular-control radius. zh_CN: macOS 常规控件圆角。

        // Bezel fill (docs §4): a restrained near-white / white@10% surface — the same opaque bezel
        // as the macOS push-button and ComboBox, so the field reads as a raised control rather than a
        // transparent hole that only shows when it sits on a card. zh_CN: bezel 填充(文档 §4):克制的
        // near-white / white@10% 表面——与 macOS 按钮、ComboBox 同款不透明 bezel,使字段读作凸起控件,
        // 而非仅在卡片上才可见的透明洞。
        {
            const QColor fill = !isEnabled() ? colors.controlDisabled : colors.bgLayerAlt;
            QRectF fillRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
            QPainterPath fillPath;
            fillPath.addRoundedRect(fillRect, r, r);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawPath(fillPath);
        }

        if (isEnabled() && m_isFocused) {
            // Faint outer glow ring first, inset 0.5px so its 2px stroke stays inside the widget.
            // zh_CN: 先画淡淡外层辉光环,内缩 0.5px 使其 2px 描边留在控件内。
            QColor glow = colors.accentDefault;
            glow.setAlpha(0x40); // ~25% — a soft halo, not a hard line. zh_CN: 约 25%——柔和光晕而非硬线。
            QRectF glowRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
            QPainterPath glowPath;
            glowPath.addRoundedRect(glowRect, r, r);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(glow, 2.0));
            painter.drawPath(glowPath);

            // Crisp 2px accent ring inset further so it sits inside the glow. zh_CN: 再内缩画清晰 2px 强调环,位于辉光内侧。
            QRectF ringRect = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
            QPainterPath ringPath;
            ringPath.addRoundedRect(ringRect, qMax<qreal>(0.0, r - 1.0), qMax<qreal>(0.0, r - 1.0));
            painter.setPen(QPen(colors.accentDefault, 2.0));
            painter.drawPath(ringPath);
            return;
        }

        // Rest / hover / disabled: just the 1px hairline. zh_CN: 静息/悬停/禁用:仅 1px 发丝边框。
        QColor hairline = !isEnabled() ? colors.strokeDivider
                                       : (m_isHovered ? colors.strokeStrong : colors.strokeDefault);
        QRectF hairRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath hairPath;
        hairPath.addRoundedRect(hairRect, r, r);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(hairline, 1.0));
        painter.drawPath(hairPath);
        return;
    }

    // DesignFluent (default): unchanged WinUI treatment — fill + border + bottom accent underline
    // on focus. zh_CN: 默认 Fluent:WinUI 处理不变——填充 + 边框 + 聚焦时底部强调下划线。
    QRectF bgRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bgColor, borderColor, bottomBorderColor;
    int bottomBorderWidth = m_unfocusedBorderWidth;
    if (!isEnabled()) {
        bgColor = colors.controlDisabled;
        borderColor = colors.strokeDivider;
        bottomBorderColor = borderColor;
    } else if (isReadOnly()) {
        bgColor = colors.controlAltSecondary;
        borderColor = colors.strokeDefault;
        bottomBorderColor = colors.strokeDivider;
    } else if (m_isFocused) {
        bgColor = (effectiveTheme() == Dark) ? colors.bgSolid : colors.controlDefault;
        borderColor = colors.strokeSecondary;
        bottomBorderColor = colors.accentDefault;
        bottomBorderWidth = m_focusedBorderWidth;
    } else if (m_isHovered) {
        bgColor = colors.controlSecondary;
        borderColor = colors.strokeSecondary;
        bottomBorderColor = colors.strokeSecondary;
    } else {
        bgColor = colors.controlDefault;
        borderColor = colors.strokeDefault;
        bottomBorderColor = colors.strokeDivider;
    }

    qreal r = radius.control;
    QPainterPath framePath;
    framePath.addRoundedRect(bgRect, r, r);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawPath(framePath);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, 1));
    painter.drawPath(framePath);

    if (isEnabled() && !isReadOnly()) {
        QPen pen(bottomBorderColor, bottomBorderWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        qreal bottomY = bgRect.bottom() - (bottomBorderWidth > 1 ? (bottomBorderWidth - 1) / 2.0 : 0);
        QPainterPath bottomPath;
        bottomPath.moveTo(bgRect.left() + r, bottomY);
        bottomPath.lineTo(bgRect.right() - r, bottomY);
        painter.drawPath(bottomPath);
    }
}

void LineEdit::enterEvent(FluentEnterEvent* event) {
    m_isHovered = true;
    update();
    updateClearButtonVisibility();
    QLineEdit::enterEvent(event);
}

void LineEdit::leaveEvent(QEvent* event) {
    m_isHovered = false;
    update();
    updateClearButtonVisibility();
    QLineEdit::leaveEvent(event);
}

void LineEdit::focusInEvent(QFocusEvent* event) {
    m_isFocused = true;
    update();
    updateClearButtonVisibility();
    QLineEdit::focusInEvent(event);
}

void LineEdit::focusOutEvent(QFocusEvent* event) {
    m_isFocused = false;
    update();
    updateClearButtonVisibility();
    QLineEdit::focusOutEvent(event);
}

void LineEdit::setContentMargins(const QMargins& margins) {
    if (m_contentMargins == margins)
        return;
    m_contentMargins = margins;
    applyThemeStyle();
    emit contentMarginsChanged();
}

void LineEdit::setFontRole(const QString& role) {
    if (m_fontRole == role)
        return;
    m_fontRole = role;
    applyThemeStyle();
    emit fontRoleChanged();
}

void LineEdit::setClearButtonEnabled(bool enabled) {
    if (m_clearButtonEnabled == enabled)
        return;
    m_clearButtonEnabled = enabled;
    updateClearButtonVisibility();
    applyThemeStyle();
    emit clearButtonEnabledChanged();
}

void LineEdit::setClearButtonSize(int size) {
    if (m_clearButtonSize == size)
        return;
    m_clearButtonSize = size;
    if (m_clearButton) {
        m_clearButton->setFixedSize(size, size);
        updateClearButtonGeometry();
    }
    applyThemeStyle();
    updateClearButtonVisibility();
    emit clearButtonSizeChanged();
}

void LineEdit::setClearButtonOffset(const QPoint& offset) {
    if (m_clearButtonOffset == offset)
        return;
    m_clearButtonOffset = offset;
    updateClearButtonGeometry();
    applyThemeStyle();
    emit clearButtonOffsetChanged();
}

void LineEdit::setFocusedBorderWidth(int width) {
    if (m_focusedBorderWidth == width)
        return;
    m_focusedBorderWidth = width;
    update();
    emit focusedBorderWidthChanged();
}

void LineEdit::setUnfocusedBorderWidth(int width) {
    if (m_unfocusedBorderWidth == width)
        return;
    m_unfocusedBorderWidth = width;
    update();
    emit unfocusedBorderWidthChanged();
}

void LineEdit::setFrameVisible(bool visible) {
    if (m_frameVisible == visible) return;
    m_frameVisible = visible;
    update();
    emit frameVisibleChanged();
}

void LineEdit::onThemeUpdated() {
    applyThemeStyle();
}

void LineEdit::applyThemeStyle() {
    const auto& c = themeColors();
    QPalette pal = palette();
    pal.setColor(QPalette::Base, Qt::transparent);
    pal.setColor(QPalette::Window, Qt::transparent);
    pal.setColor(QPalette::Text, opaqueTextColor(c.textPrimary, c.bgLayerAlt));
    // Some Linux Qt styles discard the alpha channel of PlaceholderText and
    // therefore turn Fluent's translucent black token into solid black. Resolve
    // the token over the field surface first so every platform receives the
    // same final, opaque placeholder colour.
    // zh_CN: 部分 Linux Qt 样式会丢弃 PlaceholderText 的 alpha，导致半透明黑色
    // token 变成纯黑；先与输入框表面合成，保证各平台得到相同的最终占位文字色。
    pal.setColor(QPalette::PlaceholderText, opaqueTextColor(c.textTertiary, c.bgLayerAlt));
    pal.setColor(QPalette::Highlight, c.accentDefault);
    pal.setColor(QPalette::HighlightedText, c.textOnAccent);
    pal.setColor(QPalette::Inactive, QPalette::Highlight, c.accentDefault);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, c.textOnAccent);
    pal.setColor(QPalette::Disabled, QPalette::Text, c.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::PlaceholderText,
                 opaqueTextColor(c.textDisabled, c.bgLayerAlt));
    int rightPadding = m_contentMargins.right();
    if (m_clearButtonEnabled) {
        rightPadding += m_clearButtonSize + m_clearButtonOffset.x();
    }

    // Do not set the generic QSS `color`: QStyleSheetStyle on Qt 5 and Qt 6.2
    // uses it for the placeholder as well and bypasses PlaceholderText. Keeping
    // foreground colours in QPalette preserves the dedicated role on every
    // supported Qt version. (`placeholder-text-color` only exists since 6.5.)
    // zh_CN: 不设置通用 QSS `color`；Qt 5/6.2 的 QStyleSheetStyle 会把它也用于
    // placeholder 并绕过 PlaceholderText。前景色统一交给 QPalette，兼容全部 Qt 版本。
    QString qss = QString("QLineEdit { background: transparent; "
                         "selection-background-color: %5; "
                         "selection-color: %6; "
                         "padding-left: %1px; padding-right: %2px; "
                         "padding-top: %3px; padding-bottom: %4px; "
                         "border: none; }")
                     .arg(m_contentMargins.left())
                     .arg(rightPadding)
                     .arg(m_contentMargins.top())
                     .arg(m_contentMargins.bottom())
                     .arg(c.accentDefault.name(QColor::HexArgb))
                     .arg(c.textOnAccent.name(QColor::HexArgb));
    // Applying a style sheet repolishes the widget and QStyleSheetStyle may
    // replace palette roles with its default (black) foreground.  Install the
    // geometry-only sheet first, then restore the semantic palette.  This is
    // especially important when a LineEdit is hosted by a styled Gallery card
    // or embedded transparently in ComboBox.
    // zh_CN: 设置样式表会触发重新 polish，QStyleSheetStyle 可能用默认黑色覆盖
    // palette 前景色。先应用只负责几何的样式表，再恢复语义调色板，保证位于带样式
    // 表的 Gallery 卡片中或透明嵌入 ComboBox 时文字仍遵循当前主题。
    setStyleSheet(qss);
    setPalette(pal);
    setFont(themeFont(m_fontRole).toQFont());
}

void LineEdit::updateClearButtonVisibility() {
    if (!m_clearButton) return;
    bool hasText = !text().isEmpty();
    bool visible = m_clearButtonEnabled
                   && hasText
                   && !isReadOnly()
                   && (m_isFocused || m_isHovered);
    m_clearButton->setVisible(visible);
}

} // namespace fluent::textfields
