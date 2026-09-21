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
 * @brief OpenGL-backed host for a small number of perspective-transformed live widgets.
 * zh_CN: 使用 OpenGL 承载少量具有透视变换的真实控件。
 *
 * Compose a Card with normal Fluent controls, then add the whole Card with addWidget().
 * Its returned SpatialItem controls that surface and all of its children.
 * Scene composition remains application-owned. Flat mode restores the same widgets
 * to a native vertical layout. Tab/Escape from the scene, reduced motion and high
 * contrast select flat mode. Native-window/OpenGL child widgets are unsupported.
 * Use flat mode for text input, popups and platform accessibility. Intersecting
 * planes use center-depth ordering; this is a planar UI compositor, not a mesh engine.
 * zh_CN: 场景内容由应用决定。平面模式将同一批控件
 * 放回原生纵向布局。场景内 Tab/Escape、减弱动效、高对比度会进入平面模式。
 * 不支持内嵌原生窗口或 OpenGL 控件；文字输入、弹窗、平台无障碍使用平面模式。
 * 相交平面按中心深度排序；本组件合成平面 UI，不提供三维网格相交裁剪。
 */
class SpatialView : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(bool spatialEnabled READ isSpatialEnabled WRITE setSpatialEnabled NOTIFY
                   spatialEnabledChanged)
    Q_PROPERTY(qreal cameraDistance READ cameraDistance WRITE setCameraDistance NOTIFY
                   cameraDistanceChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(bool pointerTrackingEnabled READ isPointerTrackingEnabled WRITE
                   setPointerTrackingEnabled NOTIFY pointerTrackingEnabledChanged)
    Q_PROPERTY(QPointF maximumTilt READ maximumTilt WRITE setMaximumTilt NOTIFY maximumTiltChanged)
    Q_PROPERTY(int responseTime READ responseTime WRITE setResponseTime NOTIFY responseTimeChanged)
    Q_PROPERTY(int maximumFrameRate READ maximumFrameRate WRITE setMaximumFrameRate NOTIFY
                   maximumFrameRateChanged)
    Q_PROPERTY(
        bool cacheEnabled READ isCacheEnabled WRITE setCacheEnabled NOTIFY cacheEnabledChanged)
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
     * @brief Hosts a supported widget; null/unsupported/already-hosted content is rejected.
     * zh_CN: 承载可支持的控件；拒绝空指针、不支持的控件或已在其他代理中的内容。
     * Borrowed is detached at release; Reparented restores its original parent;
     * Owned is destroyed. Re-adding a widget already in this view returns its item.
     * zh_CN: 默认 Borrowed 释放时分离；Reparented 恢复原父对象；Owned 销毁。
     * 重复添加当前视图已有的控件返回原条目。
     */
    SpatialItem* addWidget(QWidget* widget, WidgetOwnership ownership = WidgetOwnership::Borrowed);
    /** @brief Detaches content and deletes the item, transferring ownership to the caller.
     * zh_CN: 分离内容并删除条目，将控件所有权转交调用方。 */
    QWidget* takeWidget(SpatialItem* item);
    /** @brief Deletes the item and applies its recorded widget ownership policy.
     * zh_CN: 删除条目并按记录的所有权策略释放控件。 */
    void releaseItem(SpatialItem* item);
    QList<SpatialItem*> items() const;
    /** @brief Number of hosted surfaces, including hidden ones; read-only, initially 0.
     * zh_CN: 承载的表面数量，包含隐藏条目；只读，初始为 0。 */
    int itemCount() const;

    /** @brief Enables perspective; default true when motion/theme policy permits it.
     * False restores the same widgets in a native vertical layout with their state intact.
     * zh_CN: 是否启用透视；动效和主题策略允许时默认 true。
     * 关闭后将同一批控件放回原生纵向布局，保留原有状态。 */
    bool isSpatialEnabled() const;
    void setSpatialEnabled(bool enabled);
    /** @brief Perspective distance in logical pixels [100, 10000]; default 1000.
     * A shorter distance increases the apparent size difference between near and far surfaces.
     * zh_CN: 透视距离，逻辑像素范围 [100, 10000]，默认 1000；越小，近大远小越明显。 */
    qreal cameraDistance() const;
    void setCameraDistance(qreal distance);
    /** @brief Whole-scene magnification [0.1, 4]; default 1. Item poses stay unchanged.
     * zh_CN: 整个场景的显示倍率 [0.1, 4]，默认 1；不改变条目的位置和姿态。 */
    qreal zoom() const;
    void setZoom(qreal zoom);
    /** @brief Tilts the whole scene toward the pointer; default true, active in 3D only.
     * zh_CN: 是否让整个场景随鼠标倾斜，默认 true，仅在 3D 中生效。 */
    bool isPointerTrackingEnabled() const;
    void setPointerTrackingEnabled(bool enabled);
    /** @brief Maximum pointer pitch/yaw, each [0, 45] degrees; default (10, 16).
     * zh_CN: 鼠标跟随的俯仰/偏航上限，各为 [0, 45] 度，默认 (10, 16)。 */
    QPointF maximumTilt() const;
    void setMaximumTilt(const QPointF& degrees);
    /** @brief Exponential pointer response time [0, 1000] ms; default 140, zero is immediate.
     * Larger values soften the response; this is a time constant, not an animation duration.
     * zh_CN: 指针跟随时间常数 [0, 1000] 毫秒，默认 140；越大越柔和，0 表示立即跟随。
     * 此值是指数响应的时间常数，不是固定动画时长。 */
    int responseTime() const;
    void setResponseTime(int milliseconds);
    /** @brief Pointer animation update cap [15, 120] fps; default 60, not measured display FPS.
     * zh_CN: 鼠标动画更新上限 [15, 120] 帧/秒，默认 60，不代表显示器实际帧率。 */
    int maximumFrameRate() const;
    void setMaximumFrameRate(int framesPerSecond);
    /** @brief Caches widget surfaces; default true. Changed content still repaints.
     * zh_CN: 是否缓存控件表面，默认 true；内容发生变化时仍会重绘。 */
    bool isCacheEnabled() const;
    void setCacheEnabled(bool enabled);
    /** @brief Requested renderer: Auto (default), Raster or OpenGL; OpenGL may fall back.
     * Switching waits until active pointer input finishes; inspect activeBackend for the result.
     * Clipped/hidden views release their GPU viewport. Ancestor graphics effects use Raster
     * so the outer compositor can capture the scene; the requested mode remains unchanged.
     * zh_CN: 请求的后端：Auto（默认）、Raster 或 OpenGL；OpenGL 不可用时回退。
     * 切换等待当前鼠标操作结束；使用 activeBackend 查询实际后端。隐藏或完全裁出时释放 GPU
     * 视口；祖先绘制特效启用时使用 Raster 供外层捕获，保留请求的渲染模式。 */
    RenderMode renderMode() const;
    void setRenderMode(RenderMode mode);
    /** @brief Actual renderer, Raster or OpenGL; read-only, initially Raster until exposed.
     * zh_CN: 实际渲染后端 Raster 或 OpenGL；只读，在视图显示并完成初始化前为 Raster。 */
    Backend activeBackend() const;
    /** @brief Active OpenGL device name; read-only, empty for Raster or before initialization.
     * zh_CN: 当前 OpenGL 设备名称；只读，Raster 或尚未初始化时为空。 */
    QString rendererName() const;
    /** @brief Explanation of automatic renderer fallback; read-only, empty when none applies.
     * zh_CN: 自动回退后端的原因；只读，没有回退原因时为空。 */
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
    void updateProjection();
    QWidget* detachItem(SpatialItem* item, bool applyOwnership);
    struct Private;
    std::unique_ptr<Private> d;
};
} // namespace fluent::spatial
#endif
