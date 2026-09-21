#ifndef GALLERYDEPTH_H
#define GALLERYDEPTH_H

#include <QEvent>
#include <QTransform>
#include "components/foundation/FluentElement.h"

class QPainter;
class QWidget;

namespace fluent::gallery::depth {

// Gallery presentation is inherited from its shell, never from a Spatial sample.
// zh_CN: Gallery 外观继承自窗口外壳，与 Spatial 示例的状态独立。
bool enabled(const QWidget* widget);
QEvent::Type changeEvent();
void setEnabled(QWidget* root, bool enabled);
void paintSurface(QPainter& painter, const QRectF& body, const FluentElement::Colors& colors,
                  qreal emphasis = 0.0);
void paintNavigationSurface(QPainter& painter, const QRectF& body,
                            const FluentElement::Colors& colors, bool selected, bool pressed);
QTransform projection(const QRectF& rect, const QPointF& tilt, qreal lift = 0.0,
                      qreal cameraDistance = 900.0);

} // namespace fluent::gallery::depth
#endif
