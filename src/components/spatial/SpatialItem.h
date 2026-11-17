#ifndef FLUENTQT_COMPONENTS_SPATIAL_SPATIALITEM_H
#define FLUENTQT_COMPONENTS_SPATIAL_SPATIALITEM_H

#include <QObject>
#include <QPointF>
#include <QPolygonF>
#include <QVector3D>
#include <memory>

class QWidget;
namespace fluent::spatial {
class SpatialView;

/**
 * @brief Controls the 3D transform and surface styling of a widget inside SpatialView.
 * Position uses logical pixels with +Z toward the camera; rotation is in X/Y/Z degrees applied in
 * that order; pivot is normalized within the widget. Non-finite inputs are ignored, scale is
 * clamped to [0.05, 8], and pivot to [0, 1]. Removing or destroying the item handles its widget
 * according to WidgetOwnership.
 * zh_CN: 控制 SpatialView 中单个控件的 3D 变换与表面样式。
 * 位置采用逻辑像素（Z 正方向朝向相机），旋转按 X/Y/Z 顺序使用角度制，轴心为控件内归一化坐标。
 * 忽略非有限数值，缩放限制在 [0.05, 8]，轴心限制在 [0, 1]；移除或销毁条目时按 WidgetOwnership 处理控件。
 */
class SpatialItem final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(QPointF pivot READ pivot WRITE setPivot NOTIFY pivotChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(qreal surfaceIntensity READ surfaceIntensity WRITE setSurfaceIntensity NOTIFY surfaceIntensityChanged)
    Q_PROPERTY(qreal hoverLift READ hoverLift WRITE setHoverLift NOTIFY hoverLiftChanged)

public:
    ~SpatialItem() override;
    QWidget* widget() const;
    /** @brief Pivot offset from the view center in logical pixels (+X right, +Y down, +Z toward camera); default (0, 0, 0). zh_CN: 轴心相对视图中心的偏移（逻辑像素，+X 向右、+Y 向下、+Z 朝向相机），默认 (0, 0, 0)。 */
    QVector3D position() const;
    void setPosition(const QVector3D& position);
    /** @brief Rotation around the pivot in X/Y/Z degrees applied in order; default (0, 0, 0). zh_CN: 绕轴心依次应用的 X/Y/Z 角度制旋转，默认 (0, 0, 0)。 */
    QVector3D rotation() const;
    void setRotation(const QVector3D& rotation);
    /** @brief Uniform scale multiplier [0.05, 8]; default 1. zh_CN: 整体缩放倍率 [0.05, 8]，默认 1。 */
    qreal scale() const;
    void setScale(qreal scale);
    /** @brief Normalized anchor and rotation origin [0, 1] per axis; default (0.5, 0.5). zh_CN: 归一化定位与旋转轴心，各轴范围 [0, 1]，默认中心 (0.5, 0.5)。 */
    QPointF pivot() const;
    void setPivot(const QPointF& pivot);
    /** @brief Visibility in both 3D and flat layouts; default true. Prefer this over hiding the widget directly. zh_CN: 在 3D 与平面布局中的可见性，默认 true；请通过此属性控制而非直接隐藏内部控件。 */
    bool isVisible() const;
    void setVisible(bool visible);

    /** @brief Shadow and rim highlight strength [0, 1] for rounded cards in 3D mode; default 0. Does not affect hit testing. zh_CN: 3D 模式下圆角卡片的阴影与边缘高光强度 [0, 1]，默认 0；仅作装饰，不影响命中区域。 */
    qreal surfaceIntensity() const;
    void setSurfaceIntensity(qreal intensity);
    /** @brief Upward hover offset in logical pixels [0, 16]; default 0. Pauses during pointer press and is omitted in flat mode. zh_CN: 悬停时向上抬升的逻辑像素偏移 [0, 16]，默认 0；鼠标按住期间冻结，平面模式下不生效。 */
    qreal hoverLift() const;
    void setHoverLift(qreal pixels);

    /** @brief Projected corners in SpatialView coordinates, or empty if not projectable. zh_CN: 当前投影到 SpatialView 坐标系中的四个顶点；无法投影时返回空。 */
    QPolygonF projectedPolygon() const;

signals:
    void positionChanged(const QVector3D& position);
    void rotationChanged(const QVector3D& rotation);
    void scaleChanged(qreal scale);
    void pivotChanged(const QPointF& pivot);
    void visibleChanged(bool visible);
    void surfaceIntensityChanged(qreal intensity);
    void hoverLiftChanged(qreal pixels);

private:
    friend class SpatialView;
    explicit SpatialItem(SpatialView* view);
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace fluent::spatial
#endif // FLUENTQT_COMPONENTS_SPATIAL_SPATIALITEM_H
