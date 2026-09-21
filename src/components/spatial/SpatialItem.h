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
 * @brief One live widget surface positioned in a SpatialView.
 * zh_CN: SpatialView 中承载真实控件的一个空间表面。
 *
 * Position uses logical pixels; positive Z approaches the camera. Rotation is
 * X/Y/Z in degrees, applied in that order. Pivot is normalized within the widget.
 * Invalid values are ignored; scale is clamped to [0.05, 8], pivot to [0, 1].
 * Destroying the item releases its widget according to WidgetOwnership.
 * zh_CN: 位置使用逻辑像素，Z 正值朝向相机；旋转按 X/Y/Z 顺序使用角度值。
 * 轴心使用控件内归一化坐标。忽略非有限数值，缩放限制为 [0.05, 8]，轴心为 [0, 1]。
 * 删除条目时按 WidgetOwnership 释放内容。
 */
class SpatialItem final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D rotation READ rotation WRITE setRotation NOTIFY rotationChanged)
    Q_PROPERTY(qreal scale READ scale WRITE setScale NOTIFY scaleChanged)
    Q_PROPERTY(QPointF pivot READ pivot WRITE setPivot NOTIFY pivotChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(qreal surfaceIntensity READ surfaceIntensity WRITE setSurfaceIntensity NOTIFY
                   surfaceIntensityChanged)
    Q_PROPERTY(qreal hoverLift READ hoverLift WRITE setHoverLift NOTIFY hoverLiftChanged)

public:
    ~SpatialItem() override;
    QWidget* widget() const;
    /** @brief Pivot position relative to view center in logical pixels; default (0, 0, 0).
     * X points right, Y down and positive Z toward the camera.
     * zh_CN: 轴心相对视图中心的位置，使用逻辑像素，默认 (0, 0, 0)；X 向右、Y 向下、Z 正值朝向相机。 */
    QVector3D position() const;
    void setPosition(const QVector3D& position);
    /** @brief Rotation about the pivot in X/Y/Z degrees, applied in order; default (0, 0, 0).
     * zh_CN: 绕轴心依次应用的 X/Y/Z 旋转角度，默认 (0, 0, 0)；单位是度，不是弧度。 */
    QVector3D rotation() const;
    void setRotation(const QVector3D& rotation);
    /** @brief Uniform size multiplier for this surface [0.05, 8]; default 1.
     * zh_CN: 单个表面的整体缩放倍率 [0.05, 8]，默认 1。 */
    qreal scale() const;
    void setScale(qreal scale);
    /** @brief Normalized anchor and rotation origin, each axis [0, 1]; default center (0.5, 0.5).
     * zh_CN: 归一化的位置锚点和旋转轴心，各轴 [0, 1]，默认中心 (0.5, 0.5)。 */
    QPointF pivot() const;
    void setPivot(const QPointF& pivot);
    /** @brief Visibility in both spatial and native layouts; default true.
     * Use this property instead of hiding the hosted root widget directly.
     * zh_CN: 在 3D 和原生 2D 布局中的可见性，默认 true；使用此属性，而非直接隐藏承载的根控件。 */
    bool isVisible() const;
    void setVisible(bool visible);

    /** @brief Strength of the rounded surface shadow and rim, clamped to [0, 1]; default 0.
     * Intended for an intact rectangular Card; decoration does not change content or hit areas.
     * Only drawn in spatial mode.
     * zh_CN: 圆角卡片表面的阴影和边缘高光强度，限制为 [0, 1]，默认 0。
     * 用于完整矩形 Card，不改变内容或命中区域；仅空间模式绘制。 */
    qreal surfaceIntensity() const;
    void setSurfaceIntensity(qreal intensity);
    /** @brief Temporary upward pointer-hover offset in logical pixels [0, 16]; default 0.
     * Motion freezes during pointer input. Native mode removes it.
     * zh_CN: 悬停时临时向上抬升的逻辑像素数 [0, 16]，默认 0，不修改 position。
     * 鼠标按住时冻结位移，原生平面模式移除此效果。 */
    qreal hoverLift() const;
    void setHoverLift(qreal pixels);

    /**
     * @brief Current projected corners in SpatialView coordinates; empty when not projectable.
     * zh_CN: 当前投影在 SpatialView 坐标中的四个角；无法投影时返回空。
     */
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
