#ifndef FLUENTQT_COMPONENTS_SPATIAL_SPATIALVIEW_H
#define FLUENTQT_COMPONENTS_SPATIAL_SPATIALVIEW_H

#include <QList>
#include <QPointF>
#include <QVector3D>
#include <QWidget>
#include <memory>
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"
#include "../foundation/WidgetOwnership.h"
#include "SpatialItem.h"

namespace fluent::spatial {
/**
 * @brief Widget container supporting 3D perspective transforms with OpenGL and CPU raster backends.
 * Falls back to a flat 2D vertical layout when perspective is disabled or Tab / Esc is pressed for input and accessibility.
 * zh_CN: 支持 3D 透视变换的控件容器，提供 OpenGL 与 CPU 两种绘制方式；关闭透视或按下 Tab / Esc 时切回普通 2D 纵向布局以保证输入与无障碍正常。
 */
class SpatialView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(bool spatialEnabled READ isSpatialEnabled WRITE setSpatialEnabled NOTIFY spatialEnabledChanged)
    Q_PROPERTY(qreal cameraDistance READ cameraDistance WRITE setCameraDistance NOTIFY cameraDistanceChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(bool pointerTrackingEnabled
        READ isPointerTrackingEnabled
        WRITE setPointerTrackingEnabled
        NOTIFY pointerTrackingEnabledChanged)
    Q_PROPERTY(QPointF maximumTilt READ maximumTilt WRITE setMaximumTilt NOTIFY maximumTiltChanged)
    Q_PROPERTY(int responseTime READ responseTime WRITE setResponseTime NOTIFY responseTimeChanged)
    Q_PROPERTY(int maximumFrameRate READ maximumFrameRate WRITE setMaximumFrameRate NOTIFY maximumFrameRateChanged)
    Q_PROPERTY(bool cacheEnabled READ isCacheEnabled WRITE setCacheEnabled NOTIFY cacheEnabledChanged)
    Q_PROPERTY(RenderMode renderMode READ renderMode WRITE setRenderMode NOTIFY renderModeChanged)
    Q_PROPERTY(Backend activeBackend READ activeBackend NOTIFY rendererChanged)
    Q_PROPERTY(QString rendererName READ rendererName NOTIFY rendererChanged)
    Q_PROPERTY(QString fallbackReason READ fallbackReason NOTIFY rendererChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)

public:
    enum class RenderMode { Auto, Raster, OpenGL };
    Q_ENUM(RenderMode)
    enum class Backend { Raster, OpenGL };
    Q_ENUM(Backend)
    explicit SpatialView(QWidget* parent = nullptr);
    ~SpatialView() override;

    /**
     * @brief Adds a widget to the 3D view and returns its SpatialItem; ownership controls cleanup on removal.
     * zh_CN: 将控件放入 3D 视图并返回对应的 SpatialItem；可通过 ownership 指定移除时的销毁方式。
     */
    SpatialItem* addWidget(QWidget* widget, WidgetOwnership ownership = WidgetOwnership::Borrowed);
    /** @brief Detaches the widget, deletes item, and returns widget ownership to the caller. zh_CN: 移除条目并返回控件，由调用方继续管理其销毁。 */
    QWidget* takeWidget(SpatialItem* item);
    /** @brief Removes item and handles its widget using its WidgetOwnership setting. zh_CN: 移除条目，并按添加时指定的销毁方式处理控件。 */
    void releaseItem(SpatialItem* item);
    QList<SpatialItem*> items() const;
    /** @brief Number of items currently in the view. zh_CN: 当前容器中的条目数量。 */
    int itemCount() const;

    /** @brief Enables 3D perspective (default true when allowed); disabling switches to a 2D vertical layout. zh_CN: 是否启用 3D 透视（允许时默认开启）；关闭后切回普通 2D 纵向布局。 */
    bool isSpatialEnabled() const;
    void setSpatialEnabled(bool enabled);
    /** @brief Perspective camera distance in logical pixels [100, 10000]; default 1000. zh_CN: 透视相机距离（逻辑像素 [100, 10000]，默认 1000）。 */
    qreal cameraDistance() const;
    void setCameraDistance(qreal distance);
    /** @brief Whole-scene zoom factor [0.1, 4]; default 1. zh_CN: 整体场景缩放倍数 [0.1, 4]，默认 1。 */
    qreal zoom() const;
    void setZoom(qreal zoom);
    /** @brief Tilts the scene toward the pointer in 3D mode; default true. zh_CN: 3D 模式下是否随鼠标位置倾斜场景，默认 true。 */
    bool isPointerTrackingEnabled() const;
    void setPointerTrackingEnabled(bool enabled);
    /** @brief Maximum pointer tilt in degrees per axis [0, 45]; default (10, 16). zh_CN: 鼠标跟随的最大倾斜角度，各轴范围 [0, 45] 度，默认 (10, 16)。 */
    QPointF maximumTilt() const;
    void setMaximumTilt(const QPointF& degrees);
    /** @brief Pointer smoothing time constant in ms [0, 1000]; default 140. zh_CN: 鼠标跟随平滑时间常数 [0, 1000] 毫秒，默认 140（0 表示立即响应）。 */
    int responseTime() const;
    void setResponseTime(int milliseconds);
    /** @brief Pointer tilt update rate cap [15, 120] fps; default 60. zh_CN: 鼠标倾斜动画的帧率上限 [15, 120] fps，默认 60。 */
    int maximumFrameRate() const;
    void setMaximumFrameRate(int framesPerSecond);
    /** @brief Enables item offscreen caching; default true. zh_CN: 是否开启子控件绘制缓存，默认 true。 */
    bool isCacheEnabled() const;
    void setCacheEnabled(bool enabled);
    /** @brief Render backend: Auto (default), Raster (CPU), or OpenGL (falls back to CPU if hardware is unavailable). zh_CN: 绘制后端：Auto（默认）、Raster（CPU）或 OpenGL（硬件不可用时自动切为 CPU）。 */
    RenderMode renderMode() const;
    void setRenderMode(RenderMode mode);
    /** @brief Currently active rendering backend (Raster or OpenGL). zh_CN: 当前实际使用的绘制后端（Raster 或 OpenGL）。 */
    Backend activeBackend() const;
    /** @brief Active OpenGL renderer name, or empty when using CPU raster. zh_CN: 当前 OpenGL 渲染器名称，使用 CPU 绘制或未初始化时为空。 */
    QString rendererName() const;
    /** @brief Reason OpenGL fell back to CPU rendering, or empty otherwise. zh_CN: OpenGL 自动切回 CPU 绘制的原因（正常时为空）。 */
    QString fallbackReason() const;
    void onThemeUpdated() override;
    QSize sizeHint() const override;

signals:
    void spatialEnabledChanged(bool enabled);
    void cameraDistanceChanged(qreal distance);
    void zoomChanged(qreal zoom);
    void pointerTrackingEnabledChanged(bool enabled);
    void maximumTiltChanged(const QPointF& degrees);
    void responseTimeChanged(int milliseconds);
    void maximumFrameRateChanged(int framesPerSecond);
    void cacheEnabledChanged(bool enabled);
    void renderModeChanged(RenderMode mode);
    void rendererChanged();
    void itemCountChanged(int count);

protected:
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    friend class SpatialItem;
    friend class SpatialPlugin;
    bool isHostTearingDown() const;
    void updateProjection();
    QWidget* detachItem(SpatialItem* item, bool applyOwnership);
    struct Private;
    std::unique_ptr<Private> d;
};
} // namespace fluent::spatial
#endif
