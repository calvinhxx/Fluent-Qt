#pragma once

#include <QImage>
#include <QPainter>
#include <QWidget>
#include <QtMath>
#include <gtest/gtest.h>

namespace fluent::testutils {

inline QImage renderControlSurface(QWidget& widget, qreal scale, const QColor& background)
{
    QImage image(QSize(qCeil(widget.width() * scale), qCeil(widget.height() * scale)),
        QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(scale);
    image.fill(background);
    QPainter painter(&image);
    widget.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();
    return image;
}

inline void expectClearSurfacePerimeter(const QImage& image, const QColor& background)
{
    for (int x = 0; x < image.width(); ++x) {
        EXPECT_EQ(image.pixelColor(x, 0), background);
        EXPECT_EQ(image.pixelColor(x, image.height() - 1), background);
    }
    for (int y = 0; y < image.height(); ++y) {
        EXPECT_EQ(image.pixelColor(0, y), background);
        EXPECT_EQ(image.pixelColor(image.width() - 1, y), background);
    }
}

inline int surfaceColorPixels(const QImage& image, const QColor& color)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            count += image.pixelColor(x, y) == color;
    return count;
}

} // namespace fluent::testutils
