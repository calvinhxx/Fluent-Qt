#ifndef SELECTIONINDICATORTESTUTILS_H
#define SELECTIONINDICATORTESTUTILS_H

#include <QImage>
#include <QPalette>
#include <QWidget>
#include <cmath>
#include <gtest/gtest.h>

namespace fluent::testutils {

inline qreal indicatorLuminance(const QColor& color)
{
    const auto linear = [](qreal channel) {
        return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
}

inline void
expectVisibleUncheckedOutline(QWidget& control, const QRect& indicator, const QColor& background, bool highContrast)
{
    QPalette palette = control.palette();
    palette.setColor(control.backgroundRole(), background);
    control.setPalette(palette);
    control.setAutoFillBackground(true);

    for (const int scale : {1, 2}) {
        qreal restingContrast = 1.0;
        for (const bool hovered : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "scale=" << scale << " hovered=" << hovered);
            // Resting Light/Dark outlines are deliberately softer; hover and High Contrast stay strong.
            // zh_CN: 浅深色常态描边有意减淡，悬停和高对比度仍保留强描边。
            const qreal minimumContrast =
                hovered || highContrast ? (scale == 1 ? 2.95 : 3.0) : (scale == 1 ? 1.8 : 2.0);
            control.setAttribute(Qt::WA_UnderMouse, hovered);
            QImage image(control.size() * scale, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(scale);
            image.fill(background);
            control.render(&image);

            const QRect pixels(indicator.topLeft() * scale, indicator.size() * scale);
            const qreal fillLuminance = indicatorLuminance(image.pixelColor(pixels.center()));
            qreal minimumQuadrantContrast = 100.0;
            // Every quadrant needs a readable edge; ignore antialiased fringe pixels.
            // zh_CN: 每个象限都应有清晰边缘，不要求抗锯齿过渡像素满足同样的对比度。
            for (int quadrant = 0; quadrant < 4; ++quadrant) {
                const QRect quarter(pixels.x() + (quadrant % 2) * pixels.width() / 2,
                    pixels.y() + (quadrant / 2) * pixels.height() / 2, pixels.width() / 2, pixels.height() / 2);
                qreal maximumContrast = 1.0;
                for (int y = quarter.top(); y <= quarter.bottom(); ++y) {
                    for (int x = quarter.left(); x <= quarter.right(); ++x) {
                        const qreal edgeLuminance = indicatorLuminance(image.pixelColor(x, y));
                        const qreal contrast = (qMax(fillLuminance, edgeLuminance) + 0.05)
                            / (qMin(fillLuminance, edgeLuminance) + 0.05);
                        maximumContrast = qMax(maximumContrast, contrast);
                    }
                }
                EXPECT_GE(maximumContrast, minimumContrast) << "quadrant=" << quadrant;
                minimumQuadrantContrast = qMin(minimumQuadrantContrast, maximumContrast);
            }
            if (!hovered)
                restingContrast = minimumQuadrantContrast;
            else if (!highContrast)
                EXPECT_GT(minimumQuadrantContrast, restingContrast * 1.25);
        }
    }
    control.setAttribute(Qt::WA_UnderMouse, false);
}

} // namespace fluent::testutils

#endif // SELECTIONINDICATORTESTUTILS_H
