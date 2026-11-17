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
 * @brief Controls the 3D position, rotation (degrees), and scale (0.05-8) of a widget in SpatialView, with +Z toward the viewer.
 * Removing or destroying the item handles its widget according to the chosen WidgetOwnership mode.
 * zh_CN: 控制 SpatialView 中单个子控件的 3D 位移、旋转（角度制）与缩放（0.05–8），Z 正方向朝向屏幕外。
 * 移除或销毁条目时会按创建时指定的销毁策略处理该控件。
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
    /** @brief Pivot offset from the view center in logical pixels (+Z toward viewer); default (0, 0, 0). zh_CN: 相对视图中心的位移（逻辑像素，+Z 朝向屏幕外），默认 (0, 0, 0)。 */
    QVector3D position() const;
    void setPosition(const QVector3D& position);
    /** @brief X/Y/Z rotation around the pivot in degrees; default (0, 0, 0). zh_CN: 绕轴心的 X/Y/Z 旋转角度（角度制），默认 (0, 0, 0)。 */
    QVector3D rotation() const;
    void setRotation(const QVector3D& rotation);
    /** @brief Uniform scale factor clamped to [0.05, 8]; default 1. zh_CN: 整体缩放倍数 [0.05, 8]，默认 1。 */
    qreal scale() const;
    void setScale(qreal scale);
    /** @brief Normalized transform origin in [0, 1]; default center (0.5, 0.5). zh_CN: 归一化变换中心 [0, 1]，默认中心 (0.5, 0.5)。 */
    QPointF pivot() const;
    void setPivot(const QPointF& pivot);
    /** @brief Item visibility in both 3D and 2D layouts; default true. Use this instead of hiding the widget directly. zh_CN: 条目在 3D 与 2D 布局中的可见性，默认 true；请通过此属性而非直接隐藏子控件来控制。 */
    bool isVisible() const;
    void setVisible(bool visible);

    /** @brief Shadow and rim highlight strength [0, 1] for cards in 3D mode; default 0. zh_CN: 3D 模式下卡片阴影与边缘高光强度 [0, 1]，默认 0。 */
    qreal surfaceIntensity() const;
    void setSurfaceIntensity(qreal intensity);
    /** @brief Upward hover offset in logical pixels [0, 16] in 3D mode; default 0. zh_CN: 3D 模式下鼠标悬停抬升高度（逻辑像素 [0, 16]），默认 0。 */
    qreal hoverLift() const;
    void setHoverLift(qreal pixels);

    /** @brief Projected corners in SpatialView coordinates, or empty if unavailable. zh_CN: 投影到 SpatialView 坐标系中的四个顶点，无法投影时返回空。 */
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
