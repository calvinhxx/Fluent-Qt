#include "SampleBuilders.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include "design/Typography.h"

namespace fluent::gallery::samples {
namespace {

/**
 * @brief Retina-aware pixmap canvas shared by the decorative painters.
 * zh_CN: 各装饰绘制器共用的高分屏感知画布。
 */
QPixmap makeCanvas(const QSize& size)
{
    constexpr qreal dpr = 2.0;
    QPixmap pixmap(size * dpr);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

} // namespace

GallerySample makeSample(const QString& id,
                         const QString& title,
                         const QString& description,
                         const QString& codeSnippet,
                         std::function<QWidget*(QWidget*)> createPreview)
{
    GallerySample sample;
    sample.id = id;
    sample.title = title;
    sample.description = description;
    sample.codeSnippet = codeSnippet;
    sample.createPreview = std::move(createPreview);
    return sample;
}

QWidget* verticalGroup(QWidget* parent, int spacing)
{
    auto* group = new QWidget(parent);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(spacing);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    return group;
}

QWidget* horizontalGroup(QWidget* parent, int spacing)
{
    auto* group = new QWidget(parent);
    auto* layout = new QHBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(spacing);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return group;
}

QPixmap glyphPixmap(const QString& glyph, const QColor& background, int size)
{
    QPixmap pixmap = makeCanvas(QSize(size, size));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF tile(0, 0, size, size);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(tile, size / 4.0, size / 4.0);

    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(qRound(size * 0.55));
    painter.setFont(iconFont);
    painter.setPen(Qt::white);
    painter.drawText(tile, Qt::AlignCenter, glyph);
    return pixmap;
}

QPixmap initialsAvatar(const QString& name, const QColor& background, int size)
{
    QString initials;
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& word : words.mid(0, 2))
        initials.append(word.front().toUpper());

    QPixmap pixmap = makeCanvas(QSize(size, size));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF circle(0, 0, size, size);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawEllipse(circle);

    QFont font;
    font.setPixelSize(qRound(size * 0.42));
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(circle, Qt::AlignCenter, initials);
    return pixmap;
}

QPixmap gradientPixmap(const QSize& size,
                       const QColor& from,
                       const QColor& to,
                       const QString& caption)
{
    QPixmap pixmap = makeCanvas(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF surface(0, 0, size.width(), size.height());
    QLinearGradient gradient(surface.topLeft(), surface.bottomRight());
    gradient.setColorAt(0.0, from);
    gradient.setColorAt(1.0, to);

    QPainterPath path;
    path.addRoundedRect(surface, 8, 8);
    painter.fillPath(path, gradient);

    if (!caption.isEmpty()) {
        QFont font;
        font.setPixelSize(15);
        font.setWeight(QFont::DemiBold);
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255, 230));
        painter.drawText(surface.adjusted(16, 12, -16, -12),
                         Qt::AlignLeft | Qt::AlignBottom, caption);
    }
    return pixmap;
}

} // namespace fluent::gallery::samples
