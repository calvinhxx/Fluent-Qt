#include "GalleryCompactFlyout.h"

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>

#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "GalleryNavigationMetrics.h"

namespace fluent::gallery {

CompactFlyoutRow::CompactFlyoutRow(const QString& routeId,
                                   const QString& text,
                                   bool selected,
                                   QWidget* parent)
    : QWidget(parent)
    , m_routeId(routeId)
    , m_text(text)
    , m_selected(selected)
{
    setObjectName(QStringLiteral("galleryCompactNavigationFlyoutRow_%1").arg(routeId));
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(kRouteHeight);
}

QSize CompactFlyoutRow::sizeHint() const
{
    const QFont font = themeFont(Typography::FontRole::Body).toQFont();
    const QFontMetrics metrics(font);
    return QSize(qMax(208, metrics.horizontalAdvance(m_text) + 28), kRouteHeight);
}

void CompactFlyoutRow::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto colors = themeColors();
    const auto radius = themeRadius();
    const QRectF itemRect = rect().adjusted(4, 2, -4, -2);

    QColor background = Qt::transparent;
    if (m_pressed)
        background = colors.subtleTertiary;
    else if (m_selected || m_hovered)
        background = colors.subtleSecondary;

    if (background.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        painter.drawRoundedRect(itemRect, radius.control, radius.control);
    }

    QFont textFont = themeFont(Typography::FontRole::Body).toQFont();
    textFont.setPixelSize(kRouteTextPixelSize);
    painter.setFont(textFont);
    painter.setPen(colors.textPrimary);
    const QRect textRect(14, 0, qMax(0, width() - 24), height());
    painter.drawText(textRect,
                     Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                     painter.fontMetrics().elidedText(m_text, Qt::ElideRight, textRect.width()));
}

void CompactFlyoutRow::enterEvent(FluentEnterEvent* event)
{
    QWidget::enterEvent(event);
    m_hovered = true;
    update();
}

void CompactFlyoutRow::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    m_pressed = false;
    update();
}

void CompactFlyoutRow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void CompactFlyoutRow::mouseReleaseEvent(QMouseEvent* event)
{
    const bool activate = m_pressed && rect().contains(event->pos()) && event->button() == Qt::LeftButton;
    m_pressed = false;
    update();
    if (activate) {
        if (onActivated)
            onActivated(m_routeId);
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

CompactFlyoutPanel::CompactFlyoutPanel(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void CompactFlyoutPanel::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), themeColors().bgLayer);
}

} // namespace fluent::gallery
