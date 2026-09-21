#pragma once

#include <QGraphicsProxyWidget>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QVariantAnimation>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <functional>
#include "components/foundation/FluentElement.h"
#include "components/foundation/MotionPolicy.h"

namespace fluent::spatial {

// Decorations are siblings of the widget's painted contents, never QWidget effects.
// The proxy's bounds, input mapping and native child hierarchy stay unchanged.
class SpatialSurfaceDecoration final : public QGraphicsItem {
public:
    explicit SpatialSurfaceDecoration(QGraphicsItem* parent) : QGraphicsItem(parent)
    {
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
    }

    QRectF boundingRect() const override { return m_rect.adjusted(-32, -32, 32, 40); }
    QPainterPath shape() const override { return {}; }

    void configure(const QSizeF& size, qreal intensity, qreal radius,
                   const FluentElement::Colors& colors, bool dark)
    {
        if (m_rect.size() != size || m_radius != radius) {
            prepareGeometryChange();
            m_rect = QRectF(QPointF(), size);
            m_radius = radius;
            m_rest = m_raised = {};
        }
        const bool changed = m_intensity != intensity || m_dark != dark ||
                             m_light != colors.grey10 || m_shade != colors.grey190;
        m_intensity = intensity;
        m_dark = dark;
        m_light = colors.grey10;
        m_shade = colors.grey190;
        setVisible(intensity > 0);
        if (changed)
            update();
    }

    void setRaised(qreal raised)
    {
        if (raised == m_raisedAmount)
            return;
        m_raisedAmount = raised;
        update();
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override
    {
        if (m_intensity <= 0 || m_rect.isEmpty())
            return;
        const QRectF bounds = boundingRect();
        // Bound cache memory for unusually large hosted widgets. Typical cards retain DPR.
        const qreal dpr = std::min(painter->device()->devicePixelRatioF(),
                                   1536.0 / std::max(bounds.width(), bounds.height()));
        if (m_rest.isNull() || !qFuzzyCompare(m_rest.devicePixelRatio(), dpr)) {
            m_rest = shadow(dpr, false);
            m_raised = shadow(dpr, true);
        }
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const qreal opacity = m_intensity * (m_dark ? 1.7 : 1.0);
        painter->setOpacity(std::min(1.0, opacity * (1 - m_raisedAmount)));
        painter->drawImage(bounds.topLeft(), m_rest);
        painter->setOpacity(std::min(1.0, opacity * m_raisedAmount));
        painter->drawImage(bounds.topLeft(), m_raised);
        painter->restore();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF rim = m_rect.adjusted(.75, .75, -.75, -.75);
        QLinearGradient edge(rim.topLeft(), rim.bottomLeft());
        QColor light = m_light;
        light.setAlphaF((m_dark ? .28 : .9) * m_intensity);
        QColor shade = m_shade;
        shade.setAlphaF((m_dark ? .22 : .13) * m_intensity);
        edge.setColorAt(0, light);
        edge.setColorAt(.35, Qt::transparent);
        edge.setColorAt(1, shade);
        painter->setPen(QPen(QBrush(edge), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(rim, m_radius, m_radius);
        painter->restore();
    }

private:
    QImage shadow(qreal dpr, bool raised) const
    {
        const QRectF bounds = boundingRect();
        QImage image(QSize(qCeil(bounds.width() * dpr), qCeil(bounds.height() * dpr)),
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        const auto distance = [this](qreal x, qreal y, qreal offset) {
            const qreal dx = std::abs(x - m_rect.center().x()) - (m_rect.width() / 2 - m_radius);
            const qreal dy =
                std::abs(y - m_rect.center().y() - offset) - (m_rect.height() / 2 - m_radius);
            return std::hypot(std::max(dx, 0.0), std::max(dy, 0.0)) +
                   std::min(std::max(dx, dy), 0.0) - m_radius;
        };
        for (int y = 0; y < image.height(); ++y) {
            auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const qreal px = (x + .5) / dpr + bounds.left();
                const qreal py = (y + .5) / dpr + bounds.top();
                const qreal cast =
                    .125 * std::erfc(distance(px, py, raised ? 12 : 5) / (raised ? 18 : 10));
                const qreal contact = .08 * std::erfc(distance(px, py, 1.5) / 2.4);
                // Raster painting can lose a projective clip. Cut the content hole into
                // the shadow's alpha before transforming it, so no shadow covers the card.
                // zh_CN: 透视下的软件裁剪可能失效；在阴影纹理中预先镂空内容区域。
                const qreal outside = std::clamp(distance(px, py, 0) * dpr, 0.0, 1.0);
                row[x] = qRgba(0, 0, 0, qRound(255 * outside * (1 - (1 - cast) * (1 - contact))));
            }
        }
        return image;
    }

    QRectF m_rect;
    qreal m_radius = 4, m_intensity = 0, m_raisedAmount = 0;
    bool m_dark = false;
    QColor m_light, m_shade;
    QImage m_rest, m_raised;
};

class SpatialSurfaceProxy final : public QGraphicsProxyWidget {
public:
    SpatialSurfaceProxy() : m_decoration(new SpatialSurfaceDecoration(this))
    {
        m_motion.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_motion, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_feedback = value.toReal();
            m_decoration->setRaised(m_feedback);
            if (moved)
                moved();
        });
    }

    std::function<void()> moved;
    qreal liftOffset() const { return m_feedback * m_lift; }
    void configure(qreal intensity, qreal lift, const FluentElement& theme)
    {
        m_lift = lift;
        m_intensity = intensity;
        m_decoration->configure(size(), intensity, theme.themeRadius().control,
                                theme.themeColorsRef(), theme.effectiveThemeUsesDarkAppearance());
    }
    void resetInteraction()
    {
        m_motion.stop();
        m_feedback = 0;
        m_hovered = m_pressed = false;
        m_decoration->setRaised(0);
    }

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override
    {
        QGraphicsProxyWidget::hoverEnterEvent(event);
        m_hovered = true;
        animate();
    }
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override
    {
        QGraphicsProxyWidget::hoverLeaveEvent(event);
        m_hovered = false;
        animate();
    }
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
        // Freeze before dispatch, preserving the coordinates of switches, sliders and drags.
        m_pressed = true;
        m_motion.stop();
        QGraphicsProxyWidget::mousePressEvent(event);
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
        QPointer<SpatialSurfaceProxy> alive(this);
        QGraphicsProxyWidget::mouseReleaseEvent(event);
        if (!alive)
            return;
        m_pressed = false;
        animate();
    }

private:
    void animate()
    {
        if (m_pressed)
            return;
        const qreal target = m_hovered && (m_intensity > 0 || m_lift > 0) ? 1 : 0;
        if (target == m_feedback)
            return;
        m_motion.stop();
        const int duration = MotionPolicy::instance().resolvedDuration(180);
        if (!duration) {
            m_feedback = target;
            m_decoration->setRaised(target);
            if (moved)
                moved();
            return;
        }
        m_motion.setDuration(duration);
        m_motion.setStartValue(m_feedback);
        m_motion.setEndValue(target);
        m_motion.start();
    }

    SpatialSurfaceDecoration* m_decoration;
    QVariantAnimation m_motion;
    qreal m_lift = 0, m_intensity = 0, m_feedback = 0;
    bool m_hovered = false, m_pressed = false;
};
} // namespace fluent::spatial
