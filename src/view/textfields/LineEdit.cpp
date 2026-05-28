#include "LineEdit.h"
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFocusEvent>
#include <QResizeEvent>
#include <QValidator>
#include "view/basicinput/Button.h"
#include "design/Typography.h"

namespace view::textfields {

LineEdit::LineEdit(QWidget* parent)
    : QLineEdit(parent) {
    setFrame(false);
    setAttribute(Qt::WA_Hover);
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);

    // 内置 Fluent 清除按钮
    m_clearButton = new ::view::basicinput::Button(this);
    m_clearButton->setFluentStyle(::view::basicinput::Button::Subtle);
    m_clearButton->setFluentSize(::view::basicinput::Button::Small);
    m_clearButton->setFocusPolicy(Qt::NoFocus);
    m_clearButton->setIconGlyph(::Typography::Icons::Dismiss,
                                ::Typography::FontSize::Body,
                                ::Typography::FontFamily::SegoeFluentIcons);
    m_clearButton->setFixedSize(m_clearButtonSize, m_clearButtonSize);
    m_clearButton->hide();

    connect(m_clearButton, &::view::basicinput::Button::clicked, this, [this]() {
        clear();
        setFocus();
    });
    connect(this, &QLineEdit::textChanged, this, [this]() {
        updateClearButtonVisibility();
    });
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
        bgColor = (currentTheme() == Dark) ? colors.bgSolid : colors.controlDefault;
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
    pal.setColor(QPalette::Text, c.textPrimary);
    pal.setColor(QPalette::PlaceholderText, c.textSecondary);
    pal.setColor(QPalette::Highlight, c.accentDefault);
    pal.setColor(QPalette::HighlightedText, c.textOnAccent);
    pal.setColor(QPalette::Inactive, QPalette::Highlight, c.accentDefault);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, c.textOnAccent);
    pal.setColor(QPalette::Disabled, QPalette::Text, c.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::PlaceholderText, c.textDisabled);
    setPalette(pal);
    setFont(themeFont(m_fontRole).toQFont());

    int rightPadding = m_contentMargins.right();
    if (m_clearButtonEnabled) {
        rightPadding += m_clearButtonSize + m_clearButtonOffset.x();
    }

    QString qss = QString("QLineEdit { background: transparent; "
                         "color: %5; "
                         "selection-background-color: %6; "
                         "selection-color: %7; "
                         "padding-left: %1px; padding-right: %2px; "
                         "padding-top: %3px; padding-bottom: %4px; "
                         "border: none; }")
                     .arg(m_contentMargins.left())
                     .arg(rightPadding)
                     .arg(m_contentMargins.top())
                     .arg(m_contentMargins.bottom())
                     .arg(c.textPrimary.name(QColor::HexArgb))
                     .arg(c.accentDefault.name(QColor::HexArgb))
                     .arg(c.textOnAccent.name(QColor::HexArgb));
    setStyleSheet(qss);
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

} // namespace view::textfields
