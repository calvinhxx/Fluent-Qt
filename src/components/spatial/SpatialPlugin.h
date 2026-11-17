#ifndef FLUENTQT_SPATIAL_SPATIALPLUGIN_H
#define FLUENTQT_SPATIAL_SPATIALPLUGIN_H

#include <memory>
#include <QSurface>
#include "components/foundation/plugins/WidgetPlugin.h"

namespace fluent::particles {
class ParticleBackdrop;
}
namespace fluent::windowing {
class Window;
}

namespace fluent::spatial {
class ParticleLayer;
class SpatialView;

/**
 * @brief Plugin providing SpatialView and ParticleLayer factories for the 3D perspective module.
 * Deactivating switches attached views to 2D raster mode and releases GPU particle layers without deleting widgets; must run on the GUI thread.
 * zh_CN: 管理 3D 透视视图与粒子纹理层的插件。停用时将关联视图切回 2D 绘制并释放粒子 GPU 资源（不销毁控件），须在 GUI 线程使用。
 */
class SpatialPlugin : public plugins::WidgetPlugin {
    Q_OBJECT
    Q_INTERFACES(fluent::plugins::WidgetPluginInterface)
public:
    explicit SpatialPlugin(QObject* parent = nullptr);
    ~SpatialPlugin() override;
    static QString pluginId();
    plugins::PluginMetadata metadata() const override;
    QStringList widgetKeys() const override;
    QWidget* createWidget(const QString& key, QWidget* parent, QString* error) override;
    bool isActive() const;

    /** @brief Attaches an existing SpatialView while active (caller retains ownership). zh_CN: 激活状态下关联已有的 SpatialView（仍由应用管理，不会随插件销毁）。 */
    bool attachView(SpatialView* view);
    /** @brief Creates a ParticleLayer for backdrop while active (backdrop remains caller-owned); returns nullptr when inactive. zh_CN: 激活时为粒子背景创建 ParticleLayer（背景控件仍由应用管理）；未激活时返回 nullptr。 */
    ParticleLayer* createParticleLayer(particles::ParticleBackdrop* backdrop, QObject* parent = nullptr);
    /** @brief Configures a hidden Fluent window for GPU compositing while active without creating a context. zh_CN: 激活时为隐藏的 Fluent 窗口预置所需的图形表面属性（不创建上下文）。 */
    bool prepareWindow(windowing::Window* window);
    bool prepareWindow(windowing::Window* window, QSurface::SurfaceType surfaceType);

protected:
    bool onActivate(const plugins::PluginContext& context, QString* error) override;
    void onDeactivate() noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace fluent::spatial
#endif // FLUENTQT_SPATIAL_SPATIALPLUGIN_H
