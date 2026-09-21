#include "GalleryEntryGrid.h"

#include <QFont>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QPainterPath>
#include <QtMath>
#include "view/support/GalleryDepth.h"

#include "compatibility/QtCompat.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"

namespace fluent::gallery {
namespace {

// Mirrors the old GalleryEntryCard layout (QHBoxLayout 16px margins, 40px icon,
// 16px gap, title + caption column) so the painted grid matches the previous look.
// zh_CN: 对齐旧 GalleryEntryCard 布局，使绘制网格与原来外观一致。
constexpr int kGridSpacing = 12;
constexpr int kMinCardHeight = 86;
constexpr int kCardPadding = 16;
constexpr int kIconSize = 40;
// Control artwork is authored at 72 px. Keep a 40 px layout slot, but paint the
// bitmap into 36 logical px so a 2x backing store consumes the source pixels
// one-for-one instead of upsampling 72 -> 80. Glyph tiles still use the full
// slot because they are rendered from the bundled vector-like icon font.
// zh_CN: 控件图以 72px 制作。布局槽位仍为 40px，但位图只绘制到 36 个逻辑像素，
// 使 2x 背板恰好按 72→72 映射，避免 72→80 放大；字体图标仍使用完整槽位。
constexpr int kControlImageSize = 36;
constexpr int kIconTextGap = 16;
constexpr int kTitleDescGap = 3;

// The grid wraps to as many columns as fit at >= kMinCardWidth each, growing 1 -> 2 ->
// 3 -> 4 as the window widens (WinUI-style), capped at kMaxColumns so cards never get
// too narrow nor sprawl past four across.
// zh_CN: 网格按每列至少 kMinCardWidth 宽排布，随窗口变宽 1→2→3→4 列（对齐 WinUI），封顶 kMaxColumns 列，
// 既不让卡片过窄，也不超过四列。
constexpr int kMinCardWidth = 240;
constexpr int kMaxColumns = 4;

} // namespace

GalleryEntryGrid::GalleryEntryGrid(QWidget* parent) : QWidget(parent), m_depthMotion(this)
{
    setObjectName(QStringLiteral("galleryEntryGrid"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    m_depthMotion.setObjectName(QStringLiteral("galleryEntryDepthMotion"));
    m_depthMotion.setDuration(150);
    m_depthMotion.setEasingCurve(QEasingCurve::OutCubic);
}

void GalleryEntryGrid::setEntries(const QVector<Entry>& entries)
{
    m_entries = entries;
    resetDepthMotion();
    recalculateRowLayout();
    updateGeometry();
    update();
}

int GalleryEntryGrid::columns() const
{
    return qBound(1, (width() + kGridSpacing) / (kMinCardWidth + kGridSpacing), kMaxColumns);
}

int GalleryEntryGrid::rowCount() const
{
    if (m_entries.isEmpty())
        return 0;
    const int cols = columns();
    return (m_entries.size() + cols - 1) / cols;
}

int GalleryEntryGrid::gridHeight() const
{
    const int rows = rowCount();
    if (rows == 0)
        return 0;
    if (m_rowHeights.size() == rows && m_rowTops.size() == rows) {
        return m_rowTops.constLast() + m_rowHeights.constLast();
    }
    return rows * kMinCardHeight + (rows - 1) * kGridSpacing;
}

int GalleryEntryGrid::columnWidth() const
{
    const int cols = columns();
    return qMax(0, (width() - (cols - 1) * kGridSpacing) / cols);
}

QRect GalleryEntryGrid::cardRect(int index) const
{
    const int cols = columns();
    const int row = index / cols;
    const int column = index % cols;
    const int cardWidth = columnWidth();
    const int x = column * (cardWidth + kGridSpacing);
    const int y =
        row < m_rowTops.size() ? m_rowTops.at(row) : row * (kMinCardHeight + kGridSpacing);
    const int height = row < m_rowHeights.size() ? m_rowHeights.at(row) : kMinCardHeight;
    return QRect(x, y, cardWidth, height);
}

int GalleryEntryGrid::cardIndexAt(const QPoint& pos) const
{
    if (depth::enabled(this)) {
        // Hit-test the same projected surface that is painted, including lifted edges.
        // zh_CN: 用绘制时的透视矩阵命中，包含抬升后的边缘。
        for (int index = 0; index < m_entries.size(); ++index) {
            const QRectF body = QRectF(cardRect(index)).adjusted(6, 6, -6, -6);
            if (!body.adjusted(-16, -16, 16, 16).contains(pos))
                continue;
            bool invertible = false;
            const QTransform inverse = cardTransform(index).inverted(&invertible);
            QPainterPath path;
            path.addRoundedRect(body, ::CornerRadius::Overlay, ::CornerRadius::Overlay);
            if (invertible && path.contains(inverse.map(QPointF(pos))))
                return index;
        }
        return -1;
    }
    const int cardWidth = columnWidth();
    if (cardWidth <= 0)
        return -1;
    const int cols = columns();
    const int column = pos.x() / (cardWidth + kGridSpacing);
    if (column < 0 || column >= cols || pos.y() < 0)
        return -1;

    int row = -1;
    for (int candidate = 0; candidate < m_rowHeights.size(); ++candidate) {
        const int top = m_rowTops.at(candidate);
        if (pos.y() < top)
            break;
        if (pos.y() < top + m_rowHeights.at(candidate)) {
            row = candidate;
            break;
        }
    }
    if (row < 0)
        return -1;

    const int index = row * cols + column;
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
    resetDepthMotion();
    const int cols = columns();
    const int cardWidth = columnWidth();
    if (cols != m_lastColumns || cardWidth != m_lastColumnWidth) {
        m_lastColumns = cols;
        m_lastColumnWidth = cardWidth;
        recalculateRowLayout();
        // Width changes can alter both the row count and wrapped-text height. Tell the
        // page layout to re-read sizeHint so the scroll extent stays correct.
        // zh_CN: 宽度变化会同时影响行数和文本换行高度，通知页面布局重新读取 sizeHint。
        updateGeometry();
    }
    update();
}

bool GalleryEntryGrid::recalculateRowLayout()
{
    const int rows = rowCount();
    QVector<int> rowHeights(rows, kMinCardHeight);
    const int depthInset = depth::enabled(this) ? 12 : 0;
    const int textWidth = columnWidth() - depthInset - 2 * kCardPadding - kIconSize - kIconTextGap;
    if (textWidth > 0) {
        const QFontMetrics titleMetrics(themeFont(Typography::FontRole::BodyStrong).toQFont());
        const QFontMetrics descMetrics(themeFont(Typography::FontRole::Caption).toQFont());
        const int cols = columns();
        for (int index = 0; index < m_entries.size(); ++index) {
            const Entry& entry = m_entries.at(index);
            int textHeight = titleMetrics.height();
            if (!entry.description.isEmpty()) {
                const QRect descriptionBounds = descMetrics.boundingRect(
                    QRect(0, 0, textWidth, QWIDGETSIZE_MAX),
                    Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, entry.description);
                textHeight += kTitleDescGap + descriptionBounds.height();
            }
            const int row = index / cols;
            rowHeights[row] = qMax(rowHeights.at(row),
                                   depthInset + 2 * kCardPadding + qMax(kIconSize, textHeight));
        }
    }

    QVector<int> rowTops;
    rowTops.reserve(rows);
    int nextTop = 0;
    for (int height : rowHeights) {
        rowTops.append(nextTop);
        nextTop += height + kGridSpacing;
    }

    if (rowHeights == m_rowHeights && rowTops == m_rowTops) {
        return false;
    }
    m_rowHeights = rowHeights;
    m_rowTops = rowTops;
    return true;
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
    const int previous = m_animatedIndex >= 0 ? m_animatedIndex : m_hoveredIndex;
    m_hoveredIndex = index;
    if (depth::enabled(this)) {
        if (index >= 0) {
            m_depthMotion.stop();
            m_animatedIndex = index;
            m_hoverFrame = QPixmap();
            m_tilt = QPointF();
            m_lift = 0;
        } else {
            animateTilt(QPointF(), 0);
        }
    }
    if (previous >= 0 && previous < m_entries.size())
        update(cardRect(previous).adjusted(-16, -16, 16, 16));
    if (index >= 0 && index < m_entries.size())
        update(cardRect(index).adjusted(-16, -16, 16, 16));
}

void GalleryEntryGrid::mouseMoveEvent(QMouseEvent* event)
{
    setHoveredIndex(cardIndexAt(event->pos()));
    if (depth::enabled(this) && m_hoveredIndex >= 0) {
        const QRectF bounds = cardRect(m_hoveredIndex);
        const QPointF offset = QPointF(event->pos()) - bounds.center();
        animateTilt(QPointF(qBound(-1.0, offset.x() * 2 / bounds.width(), 1.0) * 5.0,
                            qBound(-1.0, -offset.y() * 2 / bounds.height(), 1.0) * 4.0),
                    3.5);
    }
    QWidget::mouseMoveEvent(event);
}

void GalleryEntryGrid::mouseReleaseEvent(QMouseEvent* event)
{
    QString activatedRoute;
    if (event->button() == Qt::LeftButton) {
        const int index = cardIndexAt(event->pos());
        if (index >= 0 && index == m_pressedIndex)
            activatedRoute = m_entries.at(index).routeId;
    }
    m_pressedIndex = -1;
    QWidget::mouseReleaseEvent(event);
    if (!activatedRoute.isEmpty())
        emit activated(activatedRoute);
}

void GalleryEntryGrid::onThemeUpdated()
{
    resetDepthMotion();
    if (recalculateRowLayout())
        updateGeometry();
    update();
}

bool GalleryEntryGrid::event(QEvent* event)
{
    if (event->type() == depth::changeEvent() || event->type() == QEvent::Show ||
        event->type() == QEvent::ParentChange) {
        resetDepthMotion();
        recalculateRowLayout();
        updateGeometry();
        update();
    } else if (event->type() == QEvent::Hide || event->type() == QEvent::WindowDeactivate) {
        resetDepthMotion();
    } else if (event->type() == QEvent::MouseButtonPress) {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton)
            m_pressedIndex = cardIndexAt(mouse->pos());
    }
    return QWidget::event(event);
}

void GalleryEntryGrid::resetDepthMotion()
{
    const int previous = m_animatedIndex;
    m_depthMotion.stop();
    m_hoveredIndex = m_animatedIndex = m_pressedIndex = -1;
    m_hoverFrame = QPixmap();
    m_tilt = QPointF();
    m_lift = 0;
    if (previous >= 0 && previous < m_entries.size())
        update(cardRect(previous).adjusted(-16, -16, 16, 16));
}

void GalleryEntryGrid::animateTilt(const QPointF& target, qreal lift)
{
    const QPointF from = m_tilt;
    const qreal fromLift = m_lift;
    m_depthMotion.stop();
    disconnect(&m_depthMotion, nullptr, this, nullptr);
    m_depthMotion.setStartValue(0.0);
    m_depthMotion.setEndValue(1.0);
    connect(&m_depthMotion, &QVariantAnimation::valueChanged, this,
            [this, from, fromLift, target, lift](const QVariant& value) {
                const qreal t = value.toReal();
                m_tilt = from + (target - from) * t;
                m_lift = fromLift + (lift - fromLift) * t;
                if (m_animatedIndex >= 0)
                    update(cardRect(m_animatedIndex).adjusted(-16, -16, 16, 16));
            });
    connect(&m_depthMotion, &QVariantAnimation::finished, this, [this, lift] {
        if (qFuzzyIsNull(lift)) {
            const int previous = m_animatedIndex;
            m_animatedIndex = -1;
            m_hoverFrame = QPixmap();
            if (previous >= 0)
                update(cardRect(previous).adjusted(-16, -16, 16, 16));
        }
    });
    m_depthMotion.start();
}

QTransform GalleryEntryGrid::cardTransform(int index) const
{
    return index == m_animatedIndex && depth::enabled(this)
               ? depth::projection(cardRect(index), m_tilt, m_lift)
               : QTransform();
}

void GalleryEntryGrid::paintEvent(QPaintEvent* event)
{
    if (m_entries.isEmpty())
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    const bool spatial = depth::enabled(this);
    const QRect exposed = event->rect().adjusted(-16, -16, 16, 16);
    for (int index = 0; index < m_entries.size(); ++index) {
        const QRect bounds = cardRect(index);
        if (bounds.bottom() < exposed.top())
            continue;
        if (bounds.top() > exposed.bottom())
            break;
        const QRect body = spatial ? bounds.adjusted(6, 6, -6, -6) : bounds;
        if (spatial && index == m_animatedIndex) {
            const qreal dpr = devicePixelRatioF();
            const QSize pixels(qCeil(bounds.width() * dpr), qCeil(bounds.height() * dpr));
            if (m_hoverFrame.isNull() || m_hoverFrame.size() != pixels ||
                m_hoverFrame.devicePixelRatioF() != dpr || m_frameTheme != themeGeneration()) {
                m_hoverFrame = QPixmap(pixels);
                m_hoverFrame.setDevicePixelRatio(dpr);
                m_hoverFrame.fill(Qt::transparent);
                QPainter cached(&m_hoverFrame);
                cached.setRenderHint(QPainter::Antialiasing);
                cached.setRenderHint(QPainter::TextAntialiasing);
                paintEntry(cached, index, QRect(QPoint(6, 6), bounds.size() - QSize(12, 12)), true);
                m_frameTheme = themeGeneration();
            }
            painter.save();
            painter.setTransform(cardTransform(index));
            painter.drawPixmap(bounds.topLeft(), m_hoverFrame);
            painter.restore();
        } else {
            paintEntry(painter, index, body, index == m_hoveredIndex);
        }
    }
}

void GalleryEntryGrid::paintEntry(QPainter& painter, int index, const QRect& rect,
                                  bool hovered) const
{
    const Colors colors = themeColors();
    const QFont titleFont = themeFont(Typography::FontRole::BodyStrong).toQFont();
    const QFont descFont = themeFont(Typography::FontRole::Caption).toQFont();
    const QFontMetrics titleMetrics(titleFont);
    const QFontMetrics descMetrics(descFont);
    const Entry& entry = m_entries.at(index);

    if (depth::enabled(this)) {
        depth::paintSurface(painter, QRectF(rect), colors, hovered ? 1.0 : 0.0);
    } else {
        painter.setPen(QPen(colors.strokeCard, 1.0));
        painter.setBrush(hovered ? colors.subtleSecondary : colors.bgLayer);
        const QRectF body = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.drawRoundedRect(body, ::CornerRadius::Overlay, ::CornerRadius::Overlay);
    }

    const QRect iconRect(rect.left() + kCardPadding, rect.top() + kCardPadding, kIconSize,
                         kIconSize);
    if (!entry.iconGlyph.isEmpty()) {
        // Glyph variant (used by category cards): a tinted tile with an icon-font glyph,
        // matching GalleryIconTile. zh_CN: 字形变体（分类卡片用）：着色圆角块 + 图标字体字形，对齐 GalleryIconTile。
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.subtleSecondary);
        painter.drawRoundedRect(iconRect, ::CornerRadius::Control, ::CornerRadius::Control);
        const int glyphSize = Typography::IconSize::Large;
        painter.setFont(Typography::Icons::font(glyphSize));
        painter.setPen(colors.textPrimary);
        painter.drawText(iconRect, Qt::AlignCenter,
                         Typography::Icons::glyphForSize(entry.iconGlyph, glyphSize));
    } else if (!entry.icon.isNull()) {
        const int inset = (kIconSize - kControlImageSize) / 2;
        fluentDrawPixmapInLogicalRect(painter, iconRect.adjusted(inset, inset, -inset, -inset),
                                      entry.icon);
    }

    const int textLeft = iconRect.right() + 1 + kIconTextGap;
    const int textWidth = rect.right() - kCardPadding - textLeft;
    if (textWidth <= 0)
        return;

    const int titleY = rect.top() + kCardPadding;
    painter.setFont(titleFont);
    painter.setPen(colors.textPrimary);
    painter.drawText(QRect(textLeft, titleY, textWidth, titleMetrics.height()),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     titleMetrics.elidedText(entry.title, Qt::ElideRight, textWidth));

    if (!entry.description.isEmpty()) {
        const int descY = titleY + titleMetrics.height() + kTitleDescGap;
        const int descBottom = rect.bottom() - kCardPadding;
        const QRect descRect(textLeft, descY, textWidth, qMax(0, descBottom - descY + 1));
        painter.setFont(descFont);
        painter.setPen(colors.textSecondary);
        painter.drawText(descRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                         entry.description);
    }
}

} // namespace fluent::gallery
