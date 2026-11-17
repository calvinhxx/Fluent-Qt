#ifndef FLUENTQT_COMPONENTS_SPATIAL_SPATIALVIEW_H
#define FLUENTQT_COMPONENTS_SPATIAL_SPATIALVIEW_H

#include <QList>
#include <QPointF>
#include <QVector3D>
#include <QWidget>
#include <memory>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"
#include "SpatialItem.h"

namespace fluent::spatial {
/**
 * @brief Container for perspective-transformed QWidgets with OpenGL and raster backends.
 * Wrap Fluent controls inside a Card, add it via addWidget(), and transform the returned
 * SpatialItem. Switching to flat mode (via setSpatialEnabled(false), Tab/Escape, reduced motion,
 * or high contrast) moves the same widgets into a vertical scroll layout; use flat mode for
 * text input, popups, and accessibility. Native-window and OpenGL child widgets are unsupported.
 * Overlapping items sort by center depth (planar compositor without 3D intersection clipping).
 * zh_CN: 支持透视变换的控件容器，提供 OpenGL 与软件光栅两种绘制后端。
 * 建议将普通控件组合在 Card 中后通过 addWidget() 添加，再通过返回的 SpatialItem 控制姿态。
 * 关闭透视（或按下 Tab/Escape、开启减弱动效、高对比度主题）会将同一批控件移入原生纵向滚动布局，
 * 文字输入、弹窗与辅助功能建议使用平面模式。不支持内嵌原生窗口或 OpenGL 子控件；相交平面按中心深度
 * 排序，不提供三维网格裁剪。
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
     * @brief Adds a widget and returns its SpatialItem; rejects null, unsupported, or already-hosted widgets.
     * Re-adding a widget already in this view returns its existing item. Release behavior follows
     * ownership: Borrowed leaves it parentless, Reparented restores its prior parent, Owned deletes it.
     * zh_CN: 添加控件并返回对应的 SpatialItem；拒绝空指针、不支持的控件（原生窗口/OpenGL）或已在其他代理中的控件。
     * 重复添加本视图已有控件会返回原条目。释放策略由 ownership 决定：Borrowed 设为无父对象，Reparented 恢复原父对象，Owned 直接销毁。
     */
    SpatialItem* addWidget(QWidget* widget, WidgetOwnership ownership = WidgetOwnership::Borrowed);
    /** @brief Detaches the widget, deletes item, and transfers widget ownership to the caller. zh_CN: 分离控件并删除条目，将控件所有权交还调用方。 */
    QWidget* takeWidget(SpatialItem* item);
    /** @brief Removes and deletes item, handling its widget according to its WidgetOwnership policy. zh_CN: 移除并删除条目，按创建时指定的 WidgetOwnership 策略处理控件。 */
    void releaseItem(SpatialItem* item);
    QList<SpatialItem*> items() const;
    /**
     * @brief Number of hosted items (including hidden ones); initially 0.
     * Externally deleted widgets are detached immediately while itemCountChanged() coalesces after teardown completes.
     * zh_CN: 当前托管的条目数量（含隐藏项），初始为 0。外部销毁控件后会立即移除条目，并在控件析构完成后合并触发通知。
     */
    int itemCount() const;

    /**
     * @brief Enables 3D perspective; defaults to true when motion and theme policies allow it.
     * Setting false restores the same widgets to a flat vertical layout with their state preserved.
     * zh_CN: 是否启用 3D 透视；动效和主题策略允许时默认为 true。关闭后将控件放回原生纵向布局并保留其状态。
     */
    bool isSpatialEnabled() const;
    void setSpatialEnabled(bool enabled);
    /** @brief Perspective camera distance in logical pixels [100, 10000]; default 1000 (smaller values increase foreshortening). zh_CN: 透视相机距离，范围 [100, 10000] 逻辑像素，默认 1000；越小近大远小越明显。 */
    qreal cameraDistance() const;
    void setCameraDistance(qreal distance);
    /** @brief Whole-scene scale factor [0.1, 4]; default 1. Does not modify item positions. zh_CN: 整体场景缩放倍率 [0.1, 4]，默认 1；不改变各条目的位置和姿态。 */
    qreal zoom() const;
    void setZoom(qreal zoom);
    /** @brief Tilts the scene toward the mouse pointer in 3D mode; default true. zh_CN: 在 3D 模式下是否随鼠标位置倾斜整个场景，默认 true。 */
    bool isPointerTrackingEnabled() const;
    void setPointerTrackingEnabled(bool enabled);
    /** @brief Maximum pointer pitch/yaw tilt in degrees, each axis [0, 45]; default (10, 16). zh_CN: 鼠标跟随的最大俯仰/偏航角度，各轴范围 [0, 45] 度，默认 (10, 16)。 */
    QPointF maximumTilt() const;
    void setMaximumTilt(const QPointF& degrees);
    /** @brief Exponential pointer smoothing time constant in ms [0, 1000]; default 140 (0 is immediate). zh_CN: 鼠标跟随平滑时间常数 [0, 1000] 毫秒，默认 140（0 表示立即响应，非固定动画时长）。 */
    int responseTime() const;
    void setResponseTime(int milliseconds);
    /** @brief Pointer tilt animation update cap [15, 120] fps; default 60. zh_CN: 鼠标倾斜动画的更新帧率上限 [15, 120] fps，默认 60。 */
    int maximumFrameRate() const;
    void setMaximumFrameRate(int framesPerSecond);
    /** @brief Enables QGraphicsItem surface caching; default true. Dirty widgets still repaint. zh_CN: 是否缓存控件绘制结果，默认 true；控件内容更新时仍会重绘。 */
    bool isCacheEnabled() const;
    void setCacheEnabled(bool enabled);
    /**
     * @brief Requested backend: Auto (default), Raster, or OpenGL.
     * Mode changes wait for active pointer drags to finish; OpenGL falls back to Raster when hardware
     * rendering is unavailable or while an ancestor graphics effect is active, without altering renderMode().
     * Clipped or hidden views release their OpenGL viewport.
     * zh_CN: 请求的渲染模式：Auto（默认）、Raster 或 OpenGL。
     * 切换会等待当前鼠标操作结束；当硬件 OpenGL 不可用或祖先控件启用了图形特效时自动回退到 Raster
     * （不修改 renderMode() 的请求值）。视图隐藏或被完全裁剪时会释放 GPU 视口。
     */
    RenderMode renderMode() const;
    void setRenderMode(RenderMode mode);
    /** @brief Active rendering backend (Raster or OpenGL); stays Raster until exposed and verified. zh_CN: 实际生效的渲染后端（Raster 或 OpenGL）；在视图显示并完成探测前为 Raster。 */
    Backend activeBackend() const;
    /** @brief Active OpenGL GL_RENDERER string, or empty in Raster mode or before initialization. zh_CN: 当前 OpenGL 渲染器名称；Raster 模式或尚未初始化时为空。 */
    QString rendererName() const;
    /** @brief Reason OpenGL fell back to Raster, or empty when no fallback occurred. zh_CN: 自动回退到 Raster 的原因；未发生回退时为空。 */
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
