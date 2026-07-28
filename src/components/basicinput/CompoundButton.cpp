#include "components/basicinput/CompoundButton.h"

#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>

#include "design/Typography.h"

namespace fluent::basicinput {

CompoundButton::CompoundButton(QWidget* parent)
    : Button(parent)
{
    setObjectName(QStringLiteral("fluentCompoundButton"));
    setFluentSize(Button::Large);
}

CompoundButton::CompoundButton(const QString& text, QWidget* parent)
    : Button(text, parent)
{
    setObjectName(QStringLiteral("fluentCompoundButton"));
    setFluentSize(Button::Large);
}

CompoundButton::CompoundButton(const QString& text,
                               const QString& secondaryText,
                               QWidget* parent)
    : CompoundButton(text, parent)
{
    setSecondaryText(secondaryText);
}

void CompoundButton::setSecondaryText(const QString& text)
{
    if (m_secondaryText == text)
        return;

    const QString previousText = m_secondaryText;
    const bool tracksSecondary = accessibleDescription().isEmpty()
        || accessibleDescription() == previousText;
    m_secondaryText = text;
    if (tracksSecondary)
        setAccessibleDescription(m_secondaryText);
    updateGeometry();
    update();
    emit secondaryTextChanged(m_secondaryText);
}

QSize CompoundButton::sizeHint() const
{
    const QSize baseSize = Button::sizeHint();
    if (m_secondaryText.isEmpty())
        return baseSize;

    const auto& spacing = themeSpacing();
    const QFontMetrics secondaryMetrics(secondaryFont());
    const int horizontalPadding = spacing.standard;
    const int contentWidth =
        secondaryMetrics.horizontalAdvance(m_secondaryText)
        + horizontalPadding * 2;
    const int contentHeight =
        QFontMetrics(font()).height()
        + secondaryMetrics.height()
        + spacing.gap.tight
        + spacing.small * 2;
    return QSize(qMax(baseSize.width(), contentWidth),
                 qMax(48, contentHeight));
}

QSize CompoundButton::minimumSizeHint() const
{
    return sizeHint();
}

void CompoundButton::onThemeUpdated()
{
    Button::onThemeUpdated();
    updateGeometry();
}

QRectF CompoundButton::contentPaintRect(const QRectF& surfaceRect) const
{
    if (m_secondaryText.isEmpty())
        return Button::contentPaintRect(surfaceRect);

    const QFontMetrics primaryMetrics(font());
    const QFontMetrics secondaryMetrics(secondaryFont());
    const qreal contentHeight =
        primaryMetrics.height()
        + themeSpacing().gap.tight
        + secondaryMetrics.height();
    const qreal top =
        surfaceRect.top() + (surfaceRect.height() - contentHeight) / 2.0;
    return QRectF(surfaceRect.left(),
                  top,
                  surfaceRect.width(),
                  primaryMetrics.height());
}

void CompoundButton::paintEvent(QPaintEvent* event)
{
    Button::paintEvent(event);
    if (m_secondaryText.isEmpty())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setOpacity(contentOpacity());
    painter.setFont(secondaryFont());
    painter.setPen(secondaryTextColor());

    QRectF surfaceRect(rect());
    InteractionState state = interactionState();
    if (!isEnabled()) {
        state = Disabled;
    } else if (state == Rest) {
        if (isDown())
            state = Pressed;
        else if (underMouse())
            state = Hover;
    }
    if (state == Pressed && fluentStyle() != Subtle
        && themeDesignLanguage() == DesignFluent) {
        surfaceRect.translate(0, 0.5);
    }

    const QRectF textRect = secondaryPaintRect(surfaceRect);
    const QString elided = QFontMetrics(painter.font()).elidedText(
        m_secondaryText,
        Qt::ElideRight,
        qMax(0, qRound(textRect.width())));
    painter.drawText(textRect,
                     Qt::AlignHCenter | Qt::AlignVCenter
                         | Qt::TextSingleLine,
                     elided);
}

QFont CompoundButton::secondaryFont() const
{
    return themeFont(Typography::FontRole::Caption).toQFont();
}

QRectF CompoundButton::secondaryPaintRect(const QRectF& surfaceRect) const
{
    const QRectF primaryRect = contentPaintRect(surfaceRect);
    const qreal top =
        primaryRect.bottom() + themeSpacing().gap.tight;
    return QRectF(surfaceRect.left() + themeSpacing().standard,
                  top,
                  qMax<qreal>(0.0,
                              surfaceRect.width()
                                  - themeSpacing().standard * 2),
                  QFontMetrics(secondaryFont()).height());
}

QColor CompoundButton::secondaryTextColor() const
{
    const auto& colors = themeColorsRef();
    InteractionState state = interactionState();
    if (!isEnabled())
        state = Disabled;
    else if (state == Rest && isDown())
        state = Pressed;
    else if (state == Rest && underMouse())
        state = Hover;

    if (state == Disabled)
        return colors.textDisabled;
    if (criticalOnHover() && (state == Hover || state == Pressed))
        return colors.textOnAccent;
    if (fluentStyle() == Accent
        || (isCheckable() && isChecked() && fluentStyle() == Standard)) {
        QColor color = colors.textOnAccent;
        color.setAlpha(220);
        return color;
    }
    return colors.textSecondary;
}

} // namespace fluent::basicinput
