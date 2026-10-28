#ifndef FLUENTQT_PARTICLES_PARTICLESPLUGIN_H
#define FLUENTQT_PARTICLES_PARTICLESPLUGIN_H

#include <memory>
#include "components/foundation/plugins/WidgetPlugin.h"
#include "ParticleBackdrop.h"

namespace fluent::particles {

/**
 * @brief Creates and pauses caller-owned particle backdrops through the plugin lifecycle.
 * zh_CN: 通过插件生命周期创建和暂停调用方持有的粒子背景。
 *
 * Register with PluginManager and activate fluentqt.particles on the GUI thread.
 * Deactivation pauses tracked widgets without deleting them or changing their
 * settings, including animationEnabled. Reactivating or destroying the plugin
 * removes its pause. Keep the plugin outside the tracked widgets' QObject
 * ancestry. No GPU context is created.
 * zh_CN: 在 GUI 线程注册并激活 fluentqt.particles。停用时暂停受追踪控件，不销毁控件，
 * 也不修改 animationEnabled 等设置；再次激活或销毁插件即解除暂停。插件不可作为
 * 受追踪控件的后代；不创建 GPU 上下文。
 */
class ParticlesPlugin : public plugins::WidgetPlugin {
    Q_OBJECT
public:
    explicit ParticlesPlugin(QObject* parent = nullptr);
    ~ParticlesPlugin() override;
    static QString pluginId();
    plugins::PluginMetadata metadata() const override;
    QStringList widgetKeys() const override;
    QWidget* createWidget(const QString& key, QWidget* parent, QString* error) override;
    bool isActive() const;
    /** @brief Tracks a borrowed backdrop while active, preserving its parent and settings.
     * zh_CN: 激活时追踪借用的粒子背景，保留其父对象与设置。 */
    bool attachBackdrop(ParticleBackdrop* backdrop);

protected:
    bool onActivate(const plugins::PluginContext& context, QString* error) override;
    void onDeactivate() noexcept override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace fluent::particles
#endif // FLUENTQT_PARTICLES_PARTICLESPLUGIN_H
