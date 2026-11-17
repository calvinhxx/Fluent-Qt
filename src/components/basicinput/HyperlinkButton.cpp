#include "compatibility/TextPaintCompat.h"
#include "HyperlinkButton.h"
#include "private/ButtonContentPainter_p.h"
#include "components/basicinput/private/HyperlinkButtonAccessibility_p.h"
#include <QDesktopServices>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionButton>

namespace fluent::basicinput {

HyperlinkButton::HyperlinkButton(const QString& text, QWidget* parent) : Button(text, parent)
{
    detail::initializeHyperlinkButtonAccessibility(this);
    setFluentStyle(Subtle);
    setCursor(Qt::PointingHandCursor);

    connect(this, &QPushButton::clicked, this, [this]() {
        if (m_url.isValid()) {
            if (QDesktopServices::openUrl(m_url) && !m_accessibilityVisited) {
                m_accessibilityVisited = true;
                detail::notifyHyperlinkButtonAccessibilityVisited(this);
            }
        }
    });

    onThemeUpdated();
}

HyperlinkButton::HyperlinkButton(const QString& text, const QUrl& url, QWidget* parent) : HyperlinkButton(text, parent)
{
    setUrl(url);
}

void HyperlinkButton::setUrl(const QUrl& url)
{
    if (m_url != url) {
        const bool visitedChanged = m_accessibilityVisited;
        m_url = url;
        m_accessibilityVisited = false;
        detail::notifyHyperlinkButtonAccessibilityUrlChanged(this, visitedChanged);
        emit urlChanged();
    }
}

void HyperlinkButton::setShowUnderline(bool show)
{
    if (m_showUnderline != show) {
        m_showUnderline = show;
        update();
        emit showUnderlineChanged();
    }
}

void HyperlinkButton::onThemeUpdated()
{
    Button::onThemeUpdated();
}

void HyperlinkButton::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();
    const auto& radius = themeRadius();
    const auto& spacing = themeSpacing();

    InteractionState state = interactionState();
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown())
            state = Pressed;
        else if (underMouse())
            state = Hover;
    }

    QColor bgColor = Qt::transparent;
    QColor textColor = colors.accentDefault;

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

    const QRectF surfaceRect = rect();
    if (bgColor.isValid() && bgColor.alpha() > 0 && bgColor != QColor(Qt::transparent)) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(bgColor);
        painter.drawRoundedRect(surfaceRect, radius.control, radius.control);
    }

    painter.setFont(font());
    painter.setPen(textColor);

    QString txt = text();
    QFontMetrics fm = painter.fontMetrics();

    QRectF textRect = rect();
    if (fluentLayout() == IconAbove || fluentLayout() == IconBelow)
        textRect = paintStackedContent(painter, textRect);
    else
        textRect = ButtonContentPainter::paintHorizontalButtonContent(*this, painter, textRect,
            fluentSize() == Small ? spacing.gap.tight : spacing.gap.normal);

    const bool underlineExplicit = m_showUnderline && state == Hover;
    if (underlineExplicit && !txt.isEmpty() && !textRect.isEmpty()) {
        const qreal textX = textRect.left();
        const int textY = fluent::painting::verticallyCenteredTextBaseline(textRect, fm);
        painter.setPen(textColor);
        painter.drawLine(QPointF(textX, textY + 2), QPointF(textRect.right(), textY + 2));
    }

    if (state != Disabled && hasFocus() && (m_keyboardFocusVisible || hasFocusVisual())) {
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120);
        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(surfaceRect.adjusted(1.5, 1.5, -1.5, -1.5), qMax<qreal>(0.0, radius.control - 1.0),
            qMax<qreal>(0.0, radius.control - 1.0));
    }

    Q_UNUSED(event);
}

void HyperlinkButton::focusInEvent(QFocusEvent* event)
{
    if (event->reason() == Qt::TabFocusReason || event->reason() == Qt::BacktabFocusReason
        || event->reason() == Qt::ShortcutFocusReason)
        m_keyboardFocusVisible = true;
    else if (event->reason() == Qt::MouseFocusReason)
        m_keyboardFocusVisible = false;
    Button::focusInEvent(event);
}

void HyperlinkButton::keyPressEvent(QKeyEvent* event)
{
    m_keyboardFocusVisible = true;
    Button::keyPressEvent(event);
    update();
}

void HyperlinkButton::mousePressEvent(QMouseEvent* event)
{
    m_keyboardFocusVisible = false;
    Button::mousePressEvent(event);
    update();
}

} // namespace fluent::basicinput
