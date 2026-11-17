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
 * @brief Plugin adapter providing SpatialView and ParticleLayer factories with deactivation cleanup.
 * Register with PluginManager and activate "fluentqt.spatial" on the GUI thread. Activation
 * initializes native styling without creating a GPU context. Deactivation switches tracked views
 * to flat raster mode and releases particle GPU layers while keeping widgets intact; reactivation
 * restores render modes without re-enabling spatial depth automatically.
 * zh_CN: 提供 SpatialView 与 ParticleLayer 工厂及停用清理的插件适配器。
 * 在 GUI 线程向 PluginManager 注册并激活 fluentqt.spatial。激活仅初始化原生样式，不创建 GPU 上下文；
 * 停用时将关联视图切回平面光栅模式并释放粒子 GPU 代理，同时保留外部控件；重新激活会恢复先前的
 * 渲染模式，但不会自动重新开启 3D 透视。
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

    /** @brief Attaches an existing SpatialView (including subclasses) without reparenting or taking ownership. zh_CN: 关联已有的 SpatialView（含自定义子类），不改变父对象或所有权。 */
    bool attachView(SpatialView* view);
    /** @brief Creates a ParticleLayer for backdrop while active (without taking ownership of backdrop); returns nullptr when inactive. zh_CN: 激活时为指定粒子背景创建 ParticleLayer（不接管背景所有权）；未激活时返回 nullptr。 */
    ParticleLayer* createParticleLayer(particles::ParticleBackdrop* backdrop, QObject* parent = nullptr);
    /** @brief Configures a hidden Fluent window for future GPU composition while active without creating a context. zh_CN: 激活时为隐藏的 Fluent 窗口预配置 GPU 合成属性（不创建上下文）。 */
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
