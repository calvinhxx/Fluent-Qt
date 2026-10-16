#ifndef FLUENTQT_SPATIAL_SPATIALPLUGIN_H
#define FLUENTQT_SPATIAL_SPATIALPLUGIN_H

#include <memory>
#include <QSurface>
#include "components/foundation/plugins/WidgetPlugin.h"

namespace fluent::layout {
class ParticleBackdrop;
}
namespace fluent::windowing {
class Window;
}

namespace fluent::spatial {
class ParticleLayer;
class SpatialView;

/**
 * @brief Provides Spatial factories and cleanup through the optional plugin layer.
 * zh_CN: 通过可选插件层提供 Spatial 工厂与停用清理。
 *
 * Register this caller-owned plugin with PluginManager and activate fluentqt.spatial.
 * Activation prepares native styles without a GPU context or a hardware verdict.
 * Deactivation suspends tracked views in flat/raster mode and releases particle
 * delegation, preserving caller-owned widgets and models. Reactivation restores
 * render modes but does not turn depth back on. All calls use the GUI thread.
 * zh_CN: 将调用方持有的插件注册到 PluginManager，再激活 fluentqt.spatial。
 * 激活只准备原生样式，不创建 GPU 上下文或判定硬件可用性。停用时，受追踪视图回到
 * 平面/栅格模式并释放粒子代理，不销毁调用方控件或模型；再次激活恢复渲染模式，
 * 不自动重启立体模式。所有调用均须在 GUI 线程执行。
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

    /** @brief Tracks a borrowed view, including application-defined subclasses.
     * zh_CN: 追踪借用视图，也支持应用定义的子类；不改变父对象或所有权。 */
    bool attachView(SpatialView* view);
    /** @brief Creates a caller-owned particle adapter only while active.
     * zh_CN: 仅在激活时创建调用方持有的粒子适配器；不接管源控件。 */
    ParticleLayer* createParticleLayer(layout::ParticleBackdrop* backdrop, QObject* parent = nullptr);
    /** @brief Prepares a hidden Fluent window while active, without creating a GPU context.
     * zh_CN: 激活时准备隐藏的 Fluent 窗口，不创建 GPU 上下文。 */
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
