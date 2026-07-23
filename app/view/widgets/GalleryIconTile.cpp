#include "GalleryIconTile.h"

#include <QPainter>

#include "compatibility/QtCompat.h"
#include "design/CornerRadius.h"
#include "design/Typography.h"
#include "model/GalleryComponentCatalog.h"

namespace fluent::gallery {
namespace {

constexpr int kTileSize = 40;
constexpr int kGlyphSize = Typography::IconSize::Large;

} // namespace

GalleryIconTile::GalleryIconTile(const QString& controlName, QWidget* parent)
    : QWidget(parent)
    , m_pixmap(galleryControlImageResource(controlName))
{
    setObjectName(QStringLiteral("galleryIconTile"));
    setFixedSize(kTileSize, kTileSize);
    // Let clicks fall through to the owning card.
    // zh_CN: 让点击穿透到所属卡片。
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void GalleryIconTile::setIconGlyph(const QString& glyph)
{
    m_iconGlyph = glyph;
    update();
}

void GalleryIconTile::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    if (!m_iconGlyph.isEmpty()) {
        const Colors colors = themeColors();
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.subtleSecondary);
        painter.drawRoundedRect(rect(), ::CornerRadius::Control, ::CornerRadius::Control);

        painter.setFont(Typography::Icons::font(kGlyphSize));
        painter.setPen(colors.textPrimary);
        painter.drawText(rect(), Qt::AlignCenter,
                         Typography::Icons::glyphForSize(m_iconGlyph, kGlyphSize));
        return;
    }

    if (!m_pixmap.isNull())
        fluentDrawPixmapInLogicalRect(painter, rect(), m_pixmap);
}

} // namespace fluent::gallery
