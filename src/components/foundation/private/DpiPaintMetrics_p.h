#ifndef FLUENTDPIPAINTMETRICS_P_H
#define FLUENTDPIPAINTMETRICS_P_H

#include <QPainter>
#include <QRectF>
#include <QTransform>
#include <QtMath>

namespace fluent::painting {

struct DeviceAlignedStroke {
    QRectF rect;
    qreal width = 0.0;
};

// Private paint-time metrics. Qt continues to own logical widget geometry;
// this helper only snaps custom-painted edges to the current paint device.
// The painter transform is deliberately the source of truth because it also
// covers redirected widgets, off-screen surfaces, and fractional translations.
class DpiPaintMetrics final {
public:
    explicit DpiPaintMetrics(const QPainter& painter)
        : m_deviceTransform(painter.deviceTransform())
        , m_axisAligned(qAbs(m_deviceTransform.m12()) < 0.000001
                        && qAbs(m_deviceTransform.m21()) < 0.000001
                        && qAbs(m_deviceTransform.m11()) > 0.000001
                        && qAbs(m_deviceTransform.m22()) > 0.000001
                        && m_deviceTransform.isInvertible())
        , m_scaleX(qAbs(m_deviceTransform.m11()))
        , m_scaleY(qAbs(m_deviceTransform.m22()))
    {
    }

    QRectF alignedOuterRect(const QRectF& logicalRect) const
    {
        if (!m_axisAligned || !logicalRect.isValid() || logicalRect.isEmpty())
            return logicalRect;

        QRectF deviceRect = m_deviceTransform.mapRect(logicalRect).normalized();
        const qreal left = qRound(deviceRect.left());
        const qreal top = qRound(deviceRect.top());
        const qreal right = qMax(left, static_cast<qreal>(qRound(deviceRect.right())));
        const qreal bottom = qMax(top, static_cast<qreal>(qRound(deviceRect.bottom())));
        deviceRect = QRectF(QPointF(left, top), QPointF(right, bottom));

        bool invertible = false;
        const QTransform inverse = m_deviceTransform.inverted(&invertible);
        return invertible ? inverse.mapRect(deviceRect).normalized() : logicalRect;
    }

    DeviceAlignedStroke alignedStroke(const QRectF& outerLogicalRect,
                                      qreal logicalWidth) const
    {
        DeviceAlignedStroke result;
        if (logicalWidth <= 0.0 || !outerLogicalRect.isValid()
            || outerLogicalRect.isEmpty()) {
            return result;
        }

        if (!m_axisAligned || !qFuzzyCompare(m_scaleX + 1.0, m_scaleY + 1.0)) {
            const qreal inset = logicalWidth / 2.0;
            result.rect = outerLogicalRect.adjusted(inset, inset, -inset, -inset);
            result.width = logicalWidth;
            return result;
        }

        QRectF deviceRect = m_deviceTransform.mapRect(
            alignedOuterRect(outerLogicalRect)).normalized();
        const int physicalWidth = qMax(1, qRound(logicalWidth * m_scaleX));
        const qreal inset = physicalWidth / 2.0;
        deviceRect.adjust(inset, inset, -inset, -inset);

        bool invertible = false;
        const QTransform inverse = m_deviceTransform.inverted(&invertible);
        result.rect = invertible ? inverse.mapRect(deviceRect).normalized()
                                 : outerLogicalRect.adjusted(logicalWidth / 2.0,
                                                             logicalWidth / 2.0,
                                                             -logicalWidth / 2.0,
                                                             -logicalWidth / 2.0);
        result.width = physicalWidth / m_scaleX;
        return result;
    }

private:
    QTransform m_deviceTransform;
    bool m_axisAligned = false;
    qreal m_scaleX = 1.0;
    qreal m_scaleY = 1.0;
};

} // namespace fluent::painting

#endif // FLUENTDPIPAINTMETRICS_P_H
