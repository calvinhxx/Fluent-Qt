#include "LineEdit.h"
#include "components/basicinput/Button.h"
#include "components/foundation/private/DpiPaintMetrics_p.h"
#include "components/menus_toolbars/private/TextEditingMenu_p.h"
#include "design/Typography.h"
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QValidator>

namespace fluent::textfields {

namespace {

QColor opaqueTextColor(const QColor& foreground, const QColor& background)
{
    const qreal alpha = foreground.alphaF();
    return QColor::fromRgbF(foreground.redF() * alpha + background.redF() * (1.0 - alpha),
                            foreground.greenF() * alpha + background.greenF() * (1.0 - alpha),
                            foreground.blueF() * alpha + background.blueF() * (1.0 - alpha), 1.0);
}

} // namespace

LineEdit::LineEdit(QWidget* parent) : QLineEdit(parent)
{
    setFrame(false);
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);

    // Built-in fluent clear button. zh_CN: 内置 Fluent 清除按钮。
    m_clearButton = new ::fluent::basicinput::Button(this);
    m_clearButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
    m_clearButton->setFluentSize(::fluent::basicinput::Button::Small);
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->setIconGlyph(::Typography::Icons::Dismiss, ::Typography::IconSize::Standard,
                                ::Typography::FontFamily::FluentIcons);
    m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
    m_clearButton->hide();

    connect(m_clearButton, &::fluent::basicinput::Button::clicked, this, [this]() {
        clear();
        setFocus();
    });
    connect(this, &QLineEdit::textChanged, this, [this]() { updateClearButtonVisibility(); });

    applyThemeStyle();
}

void LineEdit::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_frameVisible)
        paintFrame(p);
    QLineEdit::paintEvent(event);
}

void LineEdit::resizeEvent(QResizeEvent* event)
{
    QLineEdit::resizeEvent(event);
    updateClearButtonGeometry();
}

void LineEdit::contextMenuEvent(QContextMenuEvent* event)
{
    if (!event)
        return;

    auto* standardMenu = createStandardContextMenu();
    if (!::fluent::menus_toolbars::detail::showTextEditingContextMenu(
            this, standardMenu, event->globalPos(), QStringLiteral("FluentLineEdit.ContextMenu"))) {
        event->ignore();
        return;
    }
    event->accept();
}

void LineEdit::updateClearButtonGeometry()
{
    if (!m_clearButton)
        return;
    m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
    int x = width() - m_clearButtonSize - m_clearButtonOffset.x();
    int y = (height() - m_clearButtonSize) / 2 + m_clearButtonOffset.y();
    m_clearButton->move(x, y);
}

void LineEdit::paintFrame(QPainter& painter)
{
    const auto& colors = themeColorsRef();
    const auto& radius = themeRadius();
    const fluent::painting::DpiPaintMetrics paintMetrics(painter);

    // Fluent field treatment: fill + border + bottom accent underline
    // on focus. zh_CN: Fluent 字段使用填充、边框及聚焦时的底部强调线。
    const auto borderStroke = paintMetrics.alignedStroke(QRectF(rect()), 1.0);
    const QRectF bgRect = borderStroke.rect;

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
        bgColor = effectiveThemeUsesDarkAppearance() ? colors.bgSolid : colors.controlDefault;
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
    painter.setPen(QPen(borderColor, borderStroke.width));
    painter.drawPath(framePath);

    if (isEnabled() && !isReadOnly()) {
        const auto bottomStroke = paintMetrics.alignedStroke(QRectF(rect()), bottomBorderWidth);
        QPen pen(bottomBorderColor, bottomStroke.width);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        const qreal bottomY = bottomStroke.rect.bottom();
        QPainterPath bottomPath;
        bottomPath.moveTo(bgRect.left() + r, bottomY);
        bottomPath.lineTo(bgRect.right() - r, bottomY);
        painter.drawPath(bottomPath);
    }
}

void LineEdit::enterEvent(FluentEnterEvent* event)
{
    m_isHovered = true;
    update();
    updateClearButtonVisibility();
    QLineEdit::enterEvent(event);
}

void LineEdit::leaveEvent(QEvent* event)
{
    m_isHovered = false;
    update();
    updateClearButtonVisibility();
    QLineEdit::leaveEvent(event);
}

void LineEdit::focusInEvent(QFocusEvent* event)
{
    m_isFocused = true;
    update();
    updateClearButtonVisibility();
    QLineEdit::focusInEvent(event);
}

void LineEdit::focusOutEvent(QFocusEvent* event)
{
    m_isFocused = false;
    update();
    updateClearButtonVisibility();
    QLineEdit::focusOutEvent(event);
}

void LineEdit::setContentMargins(const QMargins& margins)
{
    if (m_contentMargins == margins)
        return;
    m_contentMargins = margins;
    applyThemeStyle();
    emit contentMarginsChanged();
}

void LineEdit::setFontRole(Typography::FontRole role)
{
    if (m_fontRole == role)
        return;
    m_fontRole = role;
    applyThemeStyle();
    emit fontRoleChanged();
}

void LineEdit::setClearButtonEnabled(bool enabled)
{
    if (m_clearButtonEnabled == enabled)
        return;
    m_clearButtonEnabled = enabled;
    updateClearButtonVisibility();
    applyThemeStyle();
    emit clearButtonEnabledChanged();
}

void LineEdit::setClearButtonSize(int size)
{
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

void LineEdit::setClearButtonOffset(const QPoint& offset)
{
    if (m_clearButtonOffset == offset)
        return;
    m_clearButtonOffset = offset;
    updateClearButtonGeometry();
    applyThemeStyle();
    emit clearButtonOffsetChanged();
}

void LineEdit::setFocusedBorderWidth(int width)
{
    if (m_focusedBorderWidth == width)
        return;
    m_focusedBorderWidth = width;
    update();
    emit focusedBorderWidthChanged();
}

void LineEdit::setUnfocusedBorderWidth(int width)
{
    if (m_unfocusedBorderWidth == width)
        return;
    m_unfocusedBorderWidth = width;
    update();
    emit unfocusedBorderWidthChanged();
}

void LineEdit::setFrameVisible(bool visible)
{
    if (m_frameVisible == visible)
        return;
    m_frameVisible = visible;
    update();
    emit frameVisibleChanged();
}

void LineEdit::onThemeUpdated()
{
    applyThemeStyle();
}

void LineEdit::applyThemeStyle()
{
    const auto& c = themeColorsRef();
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

void LineEdit::updateClearButtonVisibility()
{
    if (!m_clearButton)
        return;
    bool hasText = !text().isEmpty();
    bool visible = m_clearButtonEnabled && hasText && !isReadOnly() && (m_isFocused || m_isHovered);
    m_clearButton->setVisible(visible);
}

} // namespace fluent::textfields
