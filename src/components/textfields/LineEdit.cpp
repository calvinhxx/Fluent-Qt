#include "components/foundation/private/TextInputFrame_p.h"
#include "components/foundation/private/ControlMetrics_p.h"
#include "components/foundation/private/FieldTrailingContent_p.h"
#include "components/foundation/private/FieldTextGeometry_p.h"
#include "compatibility/InputEventCompat.h"
#include "compatibility/private/LineEditMetricsCompat_p.h"
#include "LineEdit.h"
#include "components/basicinput/Button.h"
#include "components/menus_toolbars/private/TextEditingMenu_p.h"
#include "design/Typography.h"
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QPaintEvent>
#include <QPainter>
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
    m_clearButton->setObjectName(QStringLiteral("fluentLineEditClearButton"));
    m_clearButton->setAccessibleName(tr("Clear text"));
    m_clearButton->setFluentStyle(::fluent::basicinput::Button::Subtle);
    m_clearButton->setFluentSize(::fluent::basicinput::Button::Small);
    m_clearButton->setTextColorRole(::fluent::basicinput::Button::TextColorRole::Secondary);
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->setIconGlyph(::Typography::Icons::Dismiss, ::Typography::IconSize::Compact,
        ::Typography::FontFamily::FluentIcons);
    m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
    m_clearButton->hide();

    connect(m_clearButton, &::fluent::basicinput::Button::clicked, this, [this]() {
        clear();
        setFocus();
    });
    connect(this, &QLineEdit::textChanged, this, [this]() { updateClearButtonVisibility(); });

    m_trailingContent = std::make_unique<::fluent::detail::FieldTrailingContent>(this);
    connect(m_trailingContent.get(), &::fluent::detail::FieldTrailingContent::geometryChanged, this,
        &LineEdit::updateTrailingContentGeometry);
    connect(m_trailingContent.get(), &::fluent::detail::FieldTrailingContent::textChanged, this,
        &LineEdit::trailingTextChanged);
    connect(m_trailingContent.get(), &::fluent::detail::FieldTrailingContent::widgetChanged, this,
        &LineEdit::trailingWidgetChanged);
    connect(m_trailingContent.get(), &::fluent::detail::FieldTrailingContent::ownershipChanged, this,
        &LineEdit::trailingWidgetOwnershipChanged);

    applyThemeStyle();
}

LineEdit::~LineEdit() = default;

QString LineEdit::trailingText() const
{
    return m_trailingContent->text();
}
void LineEdit::setTrailingText(const QString& text)
{
    m_trailingContent->setText(text);
}
QWidget* LineEdit::trailingWidget() const
{
    return m_trailingContent->widget();
}
WidgetOwnership LineEdit::trailingWidgetOwnership() const
{
    return m_trailingContent->ownership();
}
bool LineEdit::setTrailingWidget(QWidget* widget, WidgetOwnership ownership)
{
    return m_trailingContent->setWidget(widget, ownership);
}
QWidget* LineEdit::takeTrailingWidget()
{
    return m_trailingContent->takeWidget();
}
void LineEdit::releaseTrailingWidget()
{
    m_trailingContent->releaseWidget();
}

QString LineEdit::clearButtonAccessibleName() const
{
    const auto* button = clearActionButton();
    return button ? button->accessibleName() : QString();
}

void LineEdit::setClearButtonAccessibleName(const QString& name)
{
    auto* button = clearActionButton();
    if (!button || button->accessibleName() == name)
        return;
    button->setAccessibleName(name);
    emit clearButtonAccessibleNameChanged();
}

Qt::FocusPolicy LineEdit::clearButtonFocusPolicy() const
{
    const auto* button = clearActionButton();
    return button ? button->focusPolicy() : Qt::NoFocus;
}

void LineEdit::setClearButtonFocusPolicy(Qt::FocusPolicy policy)
{
    auto* button = clearActionButton();
    if (!button || button->focusPolicy() == policy)
        return;
    button->setFocusPolicy(policy);
    updateClearButtonVisibility();
    emit clearButtonFocusPolicyChanged();
}

QSize LineEdit::trailingContentSizeHint() const
{
    return m_trailingContent ? m_trailingContent->sizeHint() : QSize(0, 0);
}

QSize LineEdit::sizeHint() const
{
    QSize hint = QLineEdit::sizeHint();
    hint.setHeight(qMax(hint.height(), ::fluent::detail::standardControlHeight(fontMetrics())));
    const QSize trailing = trailingContentSizeHint();
    if (trailing.width() > 0) {
        hint.rwidth() += qMax(0, trailing.width() + ::Spacing::XSmall - m_trailingContentWidth);
        hint.setHeight(qMax(hint.height(), trailing.height() + m_contentMargins.top() + m_contentMargins.bottom()));
    }
    return hint;
}

QSize LineEdit::minimumSizeHint() const
{
    QSize hint = QLineEdit::minimumSizeHint();
    hint.setHeight(qMax(hint.height(), ::fluent::detail::standardControlHeight(fontMetrics())));
    const QSize trailing = trailingContentSizeHint();
    if (trailing.width() > 0) {
        const QMargins margins = chromeContentMargins() + textMargins()
            + compatibility::detail::lineEditSideActionMargins(*this);
        const int trailingWidth = trailingWidget() ? trailing.width() : qMin(trailing.width(), 24);
        hint.setWidth(qMax(hint.width(),
            margins.left() + margins.right() + trailingWidth + ::Spacing::XSmall
                + fontMetrics().horizontalAdvance(QLatin1Char('M')) * 4));
        hint.setHeight(qMax(hint.height(), trailing.height() + m_contentMargins.top() + m_contentMargins.bottom()));
    }
    return hint;
}

bool LineEdit::event(QEvent* event)
{
    const QPointer<LineEdit> guard(this);
    const bool handled = QLineEdit::event(event);
    if (guard && event
        && (event->type() == QEvent::ActionAdded || event->type() == QEvent::ActionRemoved
            || event->type() == QEvent::ActionChanged || event->type() == QEvent::ContentsRectChange))
        updateTrailingContentGeometry();
    return handled;
}

void LineEdit::changeEvent(QEvent* event)
{
    QLineEdit::changeEvent(event);
    // An ancestor can repolish us after our theme update and restore Qt's cached
    // old palette. Restore colors without repolishing the subtree again.
    // zh_CN: 祖先在主题更新后重新 polish 时可能还原 Qt 缓存的旧调色板；仅恢复颜色，避免再次刷新整棵子树。
    if (event->type() == QEvent::StyleChange)
        applyThemePalette();
    if (event->type() == QEvent::LayoutDirectionChange) {
        updateClearButtonGeometry();
    }
    if (event->type() == QEvent::LayoutDirectionChange || event->type() == QEvent::FontChange)
        updateTrailingContentGeometry();
    if (event->type() == QEvent::ReadOnlyChange || event->type() == QEvent::EnabledChange)
        updateClearButtonVisibility();
}

void LineEdit::paintEvent(QPaintEvent* event)
{
    // End the frame painter before Qt paints text. QWidget::render shares its
    // painter with the GPU target; overlapping painters lose the text state.
    // zh_CN: 先结束边框画笔；重定向到 GPU 时，重叠画笔会让文字绘制状态失效。
    if (m_frameVisible) {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        paintFrame(p);
    }
    QLineEdit::paintEvent(event);
}

void LineEdit::resizeEvent(QResizeEvent* event)
{
    QLineEdit::resizeEvent(event);
    updateClearButtonGeometry();
    updateTrailingContentGeometry();
}

void LineEdit::contextMenuEvent(QContextMenuEvent* event)
{
    if (!event)
        return;

    auto* standardMenu = createStandardContextMenu();
    if (!::fluent::menus_toolbars::detail::showTextEditingContextMenu(this, standardMenu, event->globalPos(),
            QStringLiteral("FluentLineEdit.ContextMenu"))) {
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
    const QMargins native = compatibility::detail::lineEditSideActionMargins(*this);
    const int actionWidth = layoutDirection() == Qt::RightToLeft ? native.left() : native.right();
    int x = width() - m_clearButtonSize - m_clearButtonOffset.x() - actionWidth;
    int y = (height() - m_clearButtonSize) / 2 + m_clearButtonOffset.y();
    m_clearButton->setGeometry(
        QStyle::visualRect(layoutDirection(), rect(), QRect(x, y, m_clearButtonSize, m_clearButtonSize)));
}

QMargins LineEdit::chromeContentMargins() const
{
    QMargins padding = m_contentMargins;
    if (m_clearButtonEnabled)
        padding.setRight(padding.right() + m_clearButtonSize + m_clearButtonOffset.x());
    return padding;
}

void LineEdit::updateTrailingContentGeometry()
{
    if (!m_trailingContent || m_updatingTrailingContent)
        return;
    const QPointer<LineEdit> guard(this);
    m_updatingTrailingContent = true;
    m_trailingContent->refreshFont();
    const QRect available =
        ::fluent::detail::fieldTextRect(trailingContentBounds(), layoutDirection(), chromeContentMargins())
            .marginsRemoved(compatibility::detail::lineEditSideActionMargins(*this));
    const int reserved = m_trailingContent->layout(available, layoutDirection(),
        fontMetrics().horizontalAdvance(QLatin1Char('M')) * 4);
    if (!guard)
        return;
    m_trailingContentWidth = reserved;
    updateClearButtonGeometry();
    applyTextPadding();
    if (!guard)
        return;
    trailingContentGeometryChanged();
    if (guard) {
        m_updatingTrailingContent = false;
        updateGeometry();
    }
}

void LineEdit::trailingContentGeometryChanged() {}

void LineEdit::paintFrame(QPainter& painter)
{
    paintInputFrame(painter, QRectF(rect()), m_isFocused, m_isHovered);
}

void LineEdit::
    paintInputFrame(QPainter& painter, const QRectF& bounds, bool focused, bool hovered, bool pressed, bool invalid)
{
    ::fluent::detail::TextInputFrameState state;
    state.enabled = isEnabled();
    state.readOnly = isReadOnly();
    state.focused = focused;
    state.hovered = hovered;
    state.pressed = pressed;
    state.invalid = invalid;
    state.normalWidth = m_unfocusedBorderWidth;
    state.focusedWidth = m_focusedBorderWidth;
    ::fluent::detail::paintTextInputFrame(painter, bounds, themeColorsRef(), themeRadius().control, state);
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

void LineEdit::applyThemePalette()
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
    pal.setColor(QPalette::Disabled, QPalette::PlaceholderText, opaqueTextColor(c.textDisabled, c.bgLayerAlt));
    if (pal != palette())
        setPalette(pal);
}

void LineEdit::applyThemeStyle()
{
    setFont(themeFont(m_fontRole).toQFont());
    updateTrailingContentGeometry();
    applyTextPadding();
}

void LineEdit::applyTextPadding()
{
    const auto& c = themeColorsRef();
    QMargins padding = chromeContentMargins();
    // QSS mirrors logical padding itself; textMargins remain caller-owned.
    // zh_CN: QSS 自行镜像逻辑边距；textMargins 仍归调用方所有。
    padding.setRight(padding.right() + m_trailingContentWidth);
    padding = compatibility::detail::lineEditStylePadding(padding);

    // Do not set the generic QSS `color`: QStyleSheetStyle on Qt 5 and Qt 6.2
    // uses it for the placeholder as well and bypasses PlaceholderText. Keeping
    // foreground colours in QPalette preserves the dedicated role on every
    // supported Qt version. (`placeholder-text-color` only exists since 6.5.)
    // zh_CN: 不设置通用 QSS `color`；Qt 5/6.2 的 QStyleSheetStyle 会把它也用于
    // placeholder 并绕过 PlaceholderText。前景色统一交给 QPalette，兼容全部 Qt 版本。
    QString qss =
        QString(
            "QLineEdit { background: transparent; "
            "selection-background-color: %5; "
            "selection-color: %6; "
            "padding-left: %1px; padding-right: %2px; "
            "padding-top: %3px; padding-bottom: %4px; "
            "border: none; }")
            .arg(padding.left())
            .arg(padding.right())
            .arg(padding.top())
            .arg(padding.bottom())
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
    if (styleSheet() != qss)
        setStyleSheet(qss);
    applyThemePalette();
}

void LineEdit::updateClearButtonVisibility()
{
    if (!m_clearButton)
        return;
    bool hasText = !text().isEmpty();
    bool visible = m_clearButtonEnabled && hasText && !isReadOnly() && isEnabled()
        && (m_isFocused || m_isHovered || m_clearButton->focusPolicy() != Qt::NoFocus);
    m_clearButton->setVisible(visible);
}

} // namespace fluent::textfields
