#include "GalleryEntryGrid.h"

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>

#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "view/support/GalleryStyleSupport.h"

namespace fluent::gallery {
namespace {

// Mirrors the old GalleryEntryCard layout (QHBoxLayout 16px margins, 40px icon,
// 16px gap, title + caption column) so the painted grid matches the previous look.
// zh_CN: 对齐旧 GalleryEntryCard 布局，使绘制网格与原来外观一致。
constexpr int kColumns = 2;
constexpr int kGridSpacing = 12;
constexpr int kCardHeight = 86;
constexpr int kCardPadding = 16;
constexpr int kIconSize = 40;
constexpr int kIconTextGap = 16;
constexpr int kTitleDescGap = 3;

} // namespace

GalleryEntryGrid::GalleryEntryGrid(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("galleryEntryGrid"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
}

void GalleryEntryGrid::setEntries(const QVector<Entry>& entries)
{
    m_entries = entries;
    m_hoveredIndex = -1;
    updateGeometry();
    update();
}

int GalleryEntryGrid::rowCount() const
{
    return m_entries.isEmpty() ? 0 : (m_entries.size() + kColumns - 1) / kColumns;
}

int GalleryEntryGrid::gridHeight() const
{
    const int rows = rowCount();
    if (rows == 0)
        return 0;
    return rows * kCardHeight + (rows - 1) * kGridSpacing;
}

int GalleryEntryGrid::columnWidth() const
{
    return qMax(0, (width() - (kColumns - 1) * kGridSpacing) / kColumns);
}

QRect GalleryEntryGrid::cardRect(int index) const
{
    const int row = index / kColumns;
    const int column = index % kColumns;
    const int cardWidth = columnWidth();
    const int x = column * (cardWidth + kGridSpacing);
    const int y = row * (kCardHeight + kGridSpacing);
    return QRect(x, y, cardWidth, kCardHeight);
}

int GalleryEntryGrid::cardIndexAt(const QPoint& pos) const
{
    const int cardWidth = columnWidth();
    if (cardWidth <= 0)
        return -1;
    const int column = pos.x() / (cardWidth + kGridSpacing);
    const int row = pos.y() / (kCardHeight + kGridSpacing);
    if (column < 0 || column >= kColumns || row < 0)
        return -1;
    const int index = row * kColumns + column;
    if (index < 0 || index >= m_entries.size())
        return -1;
    // Reject the gaps between cards so hover/click only land on a card body.
    // zh_CN: 排除卡片之间的间隙，使悬停/点击只落在卡片本体上。
    return cardRect(index).contains(pos) ? index : -1;
}

QSize GalleryEntryGrid::sizeHint() const
{
    return QSize(480, gridHeight());
}

QSize GalleryEntryGrid::minimumSizeHint() const
{
    return QSize(0, gridHeight());
}

void GalleryEntryGrid::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void GalleryEntryGrid::leaveEvent(QEvent* event)
{
    setHoveredIndex(-1);
    QWidget::leaveEvent(event);
}

void GalleryEntryGrid::setHoveredIndex(int index)
{
    if (m_hoveredIndex == index)
        return;
    const int previous = m_hoveredIndex;
    m_hoveredIndex = index;
    if (previous >= 0 && previous < m_entries.size())
        update(cardRect(previous));
    if (index >= 0 && index < m_entries.size())
        update(cardRect(index));
}

void GalleryEntryGrid::mouseMoveEvent(QMouseEvent* event)
{
    setHoveredIndex(cardIndexAt(event->pos()));
    QWidget::mouseMoveEvent(event);
}

void GalleryEntryGrid::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const int index = cardIndexAt(event->pos());
        if (index >= 0)
            emit activated(m_entries.at(index).routeId);
    }
    QWidget::mouseReleaseEvent(event);
}

void GalleryEntryGrid::onThemeUpdated()
{
    update();
}

void GalleryEntryGrid::paintEvent(QPaintEvent* event)
{
    if (m_entries.isEmpty())
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const Colors colors = themeColors();
    QFont titleFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
    QFont descFont = themeFont(Typography::FontRole::Caption).toQFont();
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics descMetrics(descFont);

    // Only visit the cards intersecting the exposed rect — this is what makes the
    // grid virtual: a viewport-sized repaint touches a screenful of cards, not all of
    // them. zh_CN: 只遍历与曝光区相交的卡片——这正是网格"虚拟"之处。
    const QRect exposed = event->rect();
    for (int index = 0; index < m_entries.size(); ++index) {
        const QRect rect = cardRect(index);
        if (rect.bottom() < exposed.top())
            continue;
        if (rect.top() > exposed.bottom())
            break;  // rows below are all further down

        const Entry& entry = m_entries.at(index);
        const bool hovered = index == m_hoveredIndex;

        painter.setPen(QPen(colors.strokeCard, 1.0));
        painter.setBrush(hovered ? colors.subtleSecondary : colors.bgLayer);
        const QRectF body = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.drawRoundedRect(body, ::CornerRadius::Overlay, ::CornerRadius::Overlay);

        const QRect iconRect(rect.left() + kCardPadding,
                             rect.top() + kCardPadding,
                             kIconSize, kIconSize);
        if (!entry.icon.isNull())
            painter.drawPixmap(iconRect, entry.icon);

        const int textLeft = iconRect.right() + 1 + kIconTextGap;
        const int textWidth = rect.right() - kCardPadding - textLeft;
        if (textWidth <= 0)
            continue;

        const int titleY = rect.top() + kCardPadding;
        painter.setFont(titleFont);
        painter.setPen(colors.textPrimary);
        painter.drawText(QRect(textLeft, titleY, textWidth, titleMetrics.height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         titleMetrics.elidedText(entry.title, Qt::ElideRight, textWidth));

        if (!entry.description.isEmpty()) {
            const int descY = titleY + titleMetrics.height() + kTitleDescGap;
            const int descBottom = rect.bottom() - kCardPadding;
            const QRect descRect(textLeft, descY, textWidth, qMax(0, descBottom - descY));
            painter.setFont(descFont);
            painter.setPen(colors.textSecondary);
            painter.drawText(descRect,
                             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                             entry.description);
        }
    }
}

} // namespace fluent::gallery
