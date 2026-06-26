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
    // Snug content-driven width with a modest floor (the old 208 floor left short single-word items
    // like "DrawerView" floating in empty space). zh_CN: 贴合内容的宽度，floor 收小（旧的 208 让 “DrawerView”
    // 这类短词条右侧大片留白）。
    return QSize(qMax(160, metrics.horizontalAdvance(m_text) + 28), kRouteHeight);
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
    // Transparent: the host Popup already paints the rounded bgLayer card + border + layered shadow.
    // The old opaque SQUARE fill here overpainted that rounded card — hiding the border and squaring
    // the corners — so the flyout read as a flat box. Letting the Popup's card show through restores
    // the proper rounded, bordered, shadowed flyout (matching the native WinUI MenuFlyout look).
    // zh_CN: 透明:宿主 Popup 已绘制圆角 bgLayer 卡片 + 边框 + 分层阴影。旧的不透明方角填充把这张圆角卡片盖掉——
    // 遮住边框、把角变方——弹窗于是像个扁平方盒。让 Popup 的卡片透出，即恢复正确的圆角 + 边框 + 阴影浮层
    //（贴近原生 WinUI MenuFlyout）。
    setAttribute(Qt::WA_NoSystemBackground);
}

void CompactFlyoutPanel::paintEvent(QPaintEvent*)
{
    // Intentionally paints nothing — see the constructor. The Popup card shows through.
    // zh_CN: 故意不绘制——见构造函数。透出 Popup 卡片。
}

} // namespace fluent::gallery
