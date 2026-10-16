#ifndef FLUENTQT_SPATIAL_SPATIALSESSION_H
#define FLUENTQT_SPATIAL_SPATIALSESSION_H

#include <QObject>
#include <QSurface>
#include <memory>

class QWidget;
namespace fluent::layout {
class ParticleBackdrop;
}
namespace fluent::windowing {
class Window;
}
namespace fluent::spatial {
class SpatialView;
class ParticleLayer;

/**
 * @brief Owns a Spatial plugin registration for C++ and PySide6 application scopes.
 * zh_CN: 为 C++ 与 PySide6 应用作用域持有 Spatial 插件注册。
 *
 * Uses the common PluginManager internally. Host, views, particle sources and
 * created adapters remain caller-owned; the optional QObject parent owns only
 * this session. Activate before creating or attaching surfaces. Deactivate
 * before dismantling the host; a session must not be parented to a tracked view.
 * Activation does not enable Gallery preferences or create a GPU context.
 * All calls use QApplication's GUI thread.
 * zh_CN: 内部使用通用 PluginManager。宿主、视图、粒子源与已创建适配器仍由调用方持有；
 * 可选 QObject 父对象仅持有本会话。创建或关联表面前先激活；拆卸宿主前先停用，
 * 会话不得以受追踪视图作为父对象。激活不修改 Gallery 偏好或创建 GPU 上下文。
 * 所有调用均须在 QApplication 的 GUI 线程执行。
 */
class SpatialSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit SpatialSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~SpatialSession() override;
    /** @brief Activates the registered Spatial plugin; repeated calls are no-ops.
     * zh_CN: 激活已注册的 Spatial 插件；重复调用为空操作。 */
    bool activate(QString* error = nullptr);
    /** @brief Suspends tracked views and releases particle delegation without deleting outputs.
     * zh_CN: 暂停追踪视图并释放粒子代理，不销毁交付对象。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates a caller-owned view with the requested parent while active.
     * zh_CN: 激活时创建指定父对象的视图；父对象为空时由调用方直接持有。 */
    SpatialView* createView(QWidget* parent = nullptr);
    /** @brief Tracks an existing caller-owned view, including custom subclasses.
     * zh_CN: 追踪调用方已有视图，包括自定义子类，不改变父对象或所有权。 */
    bool attachView(SpatialView* view);
    /** @brief Creates a caller-owned adapter for a borrowed particle source while active.
     * zh_CN: 激活时为借用粒子源创建调用方持有的适配器。 */
    ParticleLayer* createParticleLayer(layout::ParticleBackdrop* backdrop, QObject* parent = nullptr);
    /** @brief Prepares a hidden window without a GPU context; false while inactive.
     * zh_CN: 不创建 GPU 上下文地准备隐藏窗口；停用时返回 false。 */
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
