#pragma once

#include <QRectF>
#include <Qt>

class QPainter;

namespace fluent::basicinput {
class Button;

class ButtonContentPainter final {
public:
    static void paintButtonIcon(const Button& button, QPainter& painter, const QRectF& bounds);
    static QRectF paintHorizontalButtonContent(const Button& button,
        QPainter& painter,
        const QRectF& bounds,
        int gap,
        Qt::Alignment alignment = Qt::AlignHCenter);
};

} // namespace fluent::basicinput
