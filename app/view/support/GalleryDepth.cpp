#include "GalleryDepth.h"

#include <QLinearGradient>
#include <QMatrix4x4>
#include <QPainter>
#include <QPainterPath>
#include <QWidget>
#include <QCoreApplication>

#include "components/foundation/MotionPolicy.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "design/CornerRadius.h"

namespace fluent::gallery::depth {
namespace {
constexpr char kModeProperty[] = "gallerySpatialEnabled";
QColor alpha(QColor color, qreal opacity)
{
    color.setAlphaF(qBound(0.0, opacity, 1.0));
    return color;
}
} // namespace

bool enabled(const QWidget* widget)
{
    if (MotionPolicy::instance().mode() != MotionPolicy::Mode::Full ||
        FluentElement::currentTheme() == FluentElement::HighContrast)
        return false;
    for (const QWidget* ancestor = widget; ancestor; ancestor = ancestor->parentWidget()) {
        const QVariant mode = ancestor->property(kModeProperty);
        if (mode.isValid())
            return mode.toBool();
    }
    return false;
}

QEvent::Type changeEvent()
{
    static const auto type = static_cast<QEvent::Type>(QEvent::registerEventType());
    return type;
}

void setEnabled(QWidget* root, bool value)
{
    root->setProperty(kModeProperty, value);
    // Only the visible page needs new styles now. Prewarmed sample pages refresh on Show;
    // repolishing the entire cached catalog here can block the first animation frame.
    // zh_CN: 当前只刷新可见页面，预热的隐藏示例在 Show 时同步；避免重设整个目录样式阻塞首帧。
    const auto notify = [](auto&& self, QWidget* widget) -> void {
        QEvent changed(changeEvent());
        QCoreApplication::sendEvent(widget, &changed);
        const auto children = widget->children();
        for (auto* child : children) {
            if (auto* childWidget = qobject_cast<QWidget*>(child)) {
                if (!childWidget->isHidden())
                    self(self, childWidget);
            }
        }
    };
    notify(notify, root);
    root->update();
}

void paintSurface(QPainter& painter, const QRectF& body, const FluentElement::Colors& colors,
                  qreal emphasis)
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal radius = ::CornerRadius::Overlay;
    // The shell supplies the main elevation. Content cards need only a quiet contact shadow.
    // zh_CN: 主要层次由外壳承担，内容卡片只保留轻微的接触阴影。
    const bool dark = FluentElement::currentTheme() == FluentElement::Dark;
    overlay::paintLayeredShadow(painter, body.toAlignedRect(), radius,
                                Elevation::getShadow(Elevation::Low, dark), .08 + emphasis * .06, 4,
                                1);
    QLinearGradient fill(body.topLeft(), body.bottomLeft());
    fill.setColorAt(0, colors.bgLayer);
    fill.setColorAt(1, alpha(colors.bgLayer, .92));
    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(body, radius, radius);

    QPainterPath clip;
    clip.addRoundedRect(body, radius, radius);
    painter.setClipPath(clip);
    QLinearGradient light(body.topLeft(), body.bottomRight());
    light.setColorAt(0, alpha(colors.grey10, 0.06 + emphasis * 0.06));
    light.setColorAt(0.45, alpha(colors.grey10, 0.0));
    light.setColorAt(1, alpha(colors.accentDefault, emphasis * 0.025));
    painter.fillRect(body, light);
    QLinearGradient edge(body.topLeft(), body.bottomLeft());
    edge.setColorAt(0, alpha(colors.grey10, dark ? .08 : .40));
    edge.setColorAt(1, alpha(colors.strokeSurface, .04));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(edge, 1.0));
    painter.drawRoundedRect(body.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    painter.restore();
}

void paintNavigationSurface(QPainter& painter, const QRectF& body,
                            const FluentElement::Colors& colors, bool selected, bool pressed)
{
    painter.save();
    painter.setPen(Qt::NoPen);
    // Selection is a tinted inlay on the navigation pane, not another raised panel.
    // zh_CN: 选中项是导航面板上的浅色衬底，避免再次叠一块凸起面板。
    const bool dark = FluentElement::currentTheme() == FluentElement::Dark;
    painter.setBrush(alpha(colors.bgLayer, selected ? (dark ? .46 : .70) : .26));
    painter.drawRoundedRect(body, 6, 6);
    painter.setBrush(alpha(colors.accentDefault, pressed ? .12 : (selected ? .065 : .025)));
    painter.drawRoundedRect(body, 6, 6);
    painter.restore();
}

QTransform projection(const QRectF& rect, const QPointF& tilt, qreal lift, qreal cameraDistance)
{
    QMatrix4x4 rotation;
    rotation.rotate(float(tilt.y()), 1, 0, 0);
    rotation.rotate(float(tilt.x()), 0, 1, 0);
    const QPointF center = rect.center();
    QPolygonF source;
    source << rect.topLeft() << rect.topRight() << rect.bottomRight() << rect.bottomLeft();
    QPolygonF target;
    for (const QPointF& corner : source) {
        const QVector3D point =
            rotation.map(QVector3D(corner.x() - center.x(), corner.y() - center.y(), 0));
        const qreal scale = cameraDistance / (cameraDistance - point.z());
        target << center + QPointF(point.x() * scale, point.y() * scale - lift);
    }
    QTransform transform;
    QTransform::quadToQuad(source, target, transform);
    return transform;
}

} // namespace fluent::gallery::depth
