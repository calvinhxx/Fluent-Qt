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
 * @brief Scoped facade owning a SpatialPlugin registration for C++ and PySide6 apps.
 * Caller retains ownership of the host, views, particle backdrops, and created ParticleLayers;
 * parent owns only this session and must not be a tracked view or its descendant. Deactivate or
 * destroy the session before dismantling the host. Activation does not create a GPU context or
 * change user 3D preferences. All calls must run on the GUI thread.
 * zh_CN: 管理 SpatialPlugin 注册与生命周期的会话封装（面向 C++ 与 PySide6 应用）。
 * 调用方负责宿主、视图、粒子背景及所创建 ParticleLayer 的生命周期；parent 仅管理本会话，且不能是
 * 已关联视图或其子对象。销毁宿主前须先停用或销毁会话。激活不会创建 GPU 上下文或更改用户 3D 偏好，
 * 所有调用须在 GUI 线程执行。
 */
class SpatialSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit SpatialSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~SpatialSession() override;
    /** @brief Registers and activates SpatialPlugin; returns true immediately if already active. zh_CN: 注册并激活 SpatialPlugin；已激活时直接返回 true。 */
    bool activate(QString* error = nullptr);
    /** @brief Suspends tracked views to flat raster mode and releases particle GPU layers without deleting caller objects. zh_CN: 将关联视图暂停为平面光栅模式并释放粒子 GPU 代理，不销毁外部对象。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates an attached SpatialView while active (owned by parent or caller); returns nullptr when inactive. zh_CN: 激活时创建并关联一个 SpatialView（由 parent 或调用方管理）；未激活时返回 nullptr。 */
    SpatialView* createView(QWidget* parent = nullptr);
    /** @brief Attaches an existing SpatialView (including subclasses) without reparenting or taking ownership. zh_CN: 关联已有的 SpatialView（含子类），不改变父对象或所有权。 */
    bool attachView(SpatialView* view);
    /** @brief Creates a ParticleLayer for backdrop while active (without taking ownership of backdrop); returns nullptr when inactive. zh_CN: 激活时为粒子背景创建 ParticleLayer（不接管背景所有权）；未激活时返回 nullptr。 */
    ParticleLayer* createParticleLayer(particles::ParticleBackdrop* backdrop, QObject* parent = nullptr);
    /** @brief Prepares a hidden Fluent window without creating a GPU context; returns false while inactive. zh_CN: 为隐藏窗口预配置合成属性（不创建 GPU 上下文）；未激活时返回 false。 */
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
