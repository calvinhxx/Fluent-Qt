#ifndef FLUENTQT_SPATIALNATIVESTYLE_P_H
#define FLUENTQT_SPATIALNATIVESTYLE_P_H

#include <QApplication>
#include <QFrame>
#include <QImage>
#include <QPainter>
#include <QPaintEngine>
#include <QProxyStyle>
#include <QStyleOption>
#include <QtMath>

namespace compatibility::detail {
// Cocoa draws some native controls through CGContext, which cannot target an OpenGL
// painter. Rasterize only those style primitives; Fluent painting and text stay on GPU.
// zh_CN: Cocoa 的原生控件需要 CGContext；只为原生样式图元提供小位图，Fluent 绘制仍走 GPU。
class GpuCompatibleNativeStyle final : public QProxyStyle {
public:
    explicit GpuCompatibleNativeStyle(QStyle* base) : QProxyStyle(base)
    {
        setObjectName(base->objectName());
        setProperty("galleryGpuCompatibleStyle", true);
    }

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override
    {
        // QMacStyle delegates PE_Widget to QCommonStyle, where it paints nothing.
        if (isGpu(painter) && element == PE_Widget)
            return;
        paintNative(option, painter, [=](const QStyleOption* local, QPainter* target) {
            QProxyStyle::drawPrimitive(element, local, target, widget);
        });
    }

    void drawControl(ControlElement element, const QStyleOption* option, QPainter* painter,
                     const QWidget* widget = nullptr) const override
    {
        if (isGpu(painter) && element == CE_ShapedFrame) {
            const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option);
            if (frame && frame->frameShape == QFrame::NoFrame)
                return;
        }
        paintNative(option, painter, [=](const QStyleOption* local, QPainter* target) {
            QProxyStyle::drawControl(element, local, target, widget);
        });
    }

    void drawComplexControl(ComplexControl control, const QStyleOptionComplex* option,
                            QPainter* painter, const QWidget* widget = nullptr) const override
    {
        paintNative(option, painter, [=](const QStyleOption* local, QPainter* target) {
            QProxyStyle::drawComplexControl(control, static_cast<const QStyleOptionComplex*>(local),
                                            target, widget);
        });
    }

private:
    static bool isGpu(QPainter* painter)
    {
        return painter && painter->paintEngine()->type() == QPaintEngine::OpenGL2;
    }

    template <typename Paint>
    static void paintNative(const QStyleOption* option, QPainter* painter, Paint paint)
    {
        if (!isGpu(painter)) {
            paint(option, painter);
            return;
        }
        // Cocoa's NSView drawing ignores painter translations. Localize the option
        // itself for common primitives, preserving the concrete option's fields.
        // Other option types retain their original origin and native behavior.
        if (const auto* frame = qstyleoption_cast<const QStyleOptionFrame*>(option))
            paintLocal(*frame, painter, paint);
        else if (const auto* button = qstyleoption_cast<const QStyleOptionButton*>(option))
            paintLocal(*button, painter, paint);
        else if (option->type == QStyleOption::SO_Default)
            paintLocal(*option, painter, paint);
        else
            paintImage(
                option, painter,
                QRect(0, 0, qMax(1, option->rect.right() + 5), qMax(1, option->rect.bottom() + 5)),
                paint);
    }

    template <typename Option, typename Paint>
    static void paintLocal(Option option, QPainter* painter, Paint paint)
    {
        const QRect bounds = option.rect.adjusted(-4, -4, 4, 4);
        option.rect.translate(-bounds.topLeft());
        paintImage(&option, painter, bounds, paint);
    }

    template <typename Paint>
    static void paintImage(const QStyleOption* option, QPainter* painter, const QRect& bounds,
                           Paint paint)
    {
        const qreal dpr = painter->device()->devicePixelRatioF();
        QImage image(QSize(qCeil(bounds.width() * dpr), qCeil(bounds.height() * dpr)),
                     QImage::Format_ARGB32_Premultiplied);
        if (image.isNull())
            return;
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);
        if (qEnvironmentVariableIntValue("FLUENT_QT_SPATIAL_BENCHMARK"))
            qApp->style()->setProperty("galleryLastNativeRasterPixels",
                                       qint64(image.width()) * image.height());
        QPainter native(&image);
        native.setFont(painter->font());
        native.setPen(painter->pen());
        native.setRenderHints(painter->renderHints());
        paint(option, &native);
        native.end();
        painter->drawImage(bounds.topLeft(), image);
    }
};

inline void prepareNativeStyle()
{
    auto* style = qApp->style();
    if (QGuiApplication::platformName() == QLatin1String("cocoa") &&
        style->objectName().contains(QLatin1String("mac"), Qt::CaseInsensitive) &&
        !style->property("galleryGpuCompatibleStyle").toBool())
        qApp->setStyle(new GpuCompatibleNativeStyle(style));
}

} // namespace compatibility::detail
#endif
