#ifndef FLUENTQT_SPATIAL_SPATIALSESSION_H
#define FLUENTQT_SPATIAL_SPATIALSESSION_H

#include <QObject>
#include <QSurface>
#include <memory>

class QWidget;
namespace fluent::particles {
class ParticleBackdrop;
}
namespace fluent::windowing {
class Window;
}
namespace fluent::spatial {
class SpatialView;
class ParticleLayer;

/**
 * @brief Session for managing the 3D perspective module lifecycle.
 * Activate before creating or attaching SpatialView, and deactivate before destroying the host; must be called on the GUI thread.
 * zh_CN: 管理 3D 透视模块启停的会话类；在创建或关联 SpatialView 前激活，宿主销毁前先停用，须在 GUI 线程调用。
 */
class SpatialSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit SpatialSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~SpatialSession() override;
    /** @brief Registers and activates SpatialPlugin; returns true if already active. zh_CN: 注册并激活 Spatial 插件；已激活时直接返回 true。 */
    bool activate(QString* error = nullptr);
    /** @brief Switches attached views to 2D raster mode and releases particle GPU layers without deleting caller objects. zh_CN: 将关联视图切回 2D 绘制并释放粒子 GPU 资源，不销毁外部控件。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates and attaches a SpatialView while active; returns nullptr when inactive. zh_CN: 激活时创建并关联 SpatialView（由 parent 或调用方管理）；未激活时返回 nullptr。 */
    SpatialView* createView(QWidget* parent = nullptr);
    /** @brief Attaches an existing SpatialView while active (caller retains ownership). zh_CN: 激活状态下关联已有的 SpatialView（仍由应用管理，不会随会话销毁）。 */
    bool attachView(SpatialView* view);
    /** @brief Creates a ParticleLayer for backdrop while active (backdrop remains caller-owned); returns nullptr when inactive. zh_CN: 激活时为粒子背景创建 ParticleLayer（背景控件仍由应用管理）；未激活时返回 nullptr。 */
    ParticleLayer* createParticleLayer(particles::ParticleBackdrop* backdrop, QObject* parent = nullptr);
    /** @brief Pre-configures a hidden Fluent window without creating a GPU context; returns false while inactive. zh_CN: 激活时为隐藏窗口预置所需的图形表面属性（不创建 GPU 上下文）；未激活时返回 false。 */
    bool prepareWindow(windowing::Window* window);
    bool prepareWindow(windowing::Window* window, QSurface::SurfaceType surfaceType);

signals:
    void activeChanged(bool active);

private:
    struct Private;
    std::shared_ptr<Private> d;
};
} // namespace fluent::spatial
#endif // FLUENTQT_SPATIAL_SPATIALSESSION_H
