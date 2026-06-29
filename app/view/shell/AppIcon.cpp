#include "AppIcon.h"

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include "utils/Log.h"

namespace fluent::gallery::appicon {
namespace {

constexpr auto kIconResourcePath = ":/app/assets/app-icon.png";

QPixmap fallbackPixmap(int logicalSize, qreal devicePixelRatio)
{
    const int size = qRound(logicalSize * devicePixelRatio);
    QPixmap pixmap(size, size);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(devicePixelRatio, devicePixelRatio);

    QPainterPath background;
    background.addRoundedRect(QRectF(0, 0, logicalSize, logicalSize), 3.0, 3.0);
    painter.fillPath(background, QColor(QStringLiteral("#0078D4")));

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(QRectF(logicalSize * 0.30, logicalSize * 0.34,
                                   logicalSize * 0.40, logicalSize * 0.32),
                            2.0, 2.0);
    return pixmap;
}

} // namespace

QIcon icon()
{
    QPixmap source(QString::fromLatin1(kIconResourcePath));
    if (source.isNull()) {
        LOG_WARN(QStringLiteral("AppIcon fallback caller=icon reason=missing-resource path=%1")
                     .arg(QString::fromLatin1(kIconResourcePath)));
        return QIcon(fallbackPixmap(256, 1.0));
    }

#ifdef Q_OS_MAC
    // macOS Dock icon spec: content should occupy ~80% of the canvas (~10% padding each side).
    // The raw PNG has only ~5% padding, making it appear larger than system icons in the Dock.
    // Add extra padding here so the visual footprint matches the platform standard.
    constexpr int kCanvasSize = 256;
    constexpr double kContentFraction = 0.88; // 88% content, 6% pad each side
    const int innerSize = qRound(kCanvasSize * kContentFraction);
    const int offset = (kCanvasSize - innerSize) / 2;
    QPixmap padded(kCanvasSize, kCanvasSize);
    padded.fill(Qt::transparent);
    {
        QPainter p(&padded);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(QRect(offset, offset, innerSize, innerSize),
                     source, source.rect());
    }
    return QIcon(padded);
#else
    return QIcon(source);
#endif
}

QPixmap pixmap(int logicalSize, qreal devicePixelRatio)
{
    const int normalizedLogicalSize = qMax(1, logicalSize);
    const qreal normalizedDevicePixelRatio = qMax<qreal>(1.0, devicePixelRatio);
    const int targetSize = qRound(normalizedLogicalSize * normalizedDevicePixelRatio);

    QPixmap source(QString::fromLatin1(kIconResourcePath));
    if (source.isNull()) {
        LOG_WARN(QStringLiteral("AppIcon fallback caller=pixmap reason=missing-resource path=%1 logicalSize=%2 devicePixelRatio=%3")
                     .arg(QString::fromLatin1(kIconResourcePath))
                     .arg(normalizedLogicalSize)
                     .arg(normalizedDevicePixelRatio));
        return fallbackPixmap(normalizedLogicalSize, normalizedDevicePixelRatio);
    }

    QPixmap scaled = source.scaled(QSize(targetSize, targetSize),
                                   Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(normalizedDevicePixelRatio);
    return scaled;
}

} // namespace fluent::gallery::appicon
