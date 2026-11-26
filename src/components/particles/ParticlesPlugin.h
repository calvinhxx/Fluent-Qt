#ifndef FLUENTQT_PARTICLES_PARTICLESPLUGIN_H
#define FLUENTQT_PARTICLES_PARTICLESPLUGIN_H

#include <memory>
#include "../foundation/plugins/WidgetPlugin.h"
#include "ParticleBackdrop.h"

namespace fluent::particles {

/**
 * @brief Plugin that manages pausing and resuming ParticleBackdrop widgets.
 * Deactivating pauses attached backdrops without deleting them; must be used on the GUI thread.
 * zh_CN: 管理粒子背景暂停与恢复的插件。停用时统一暂停关联的动画（不销毁控件），须在 GUI 线程使用。
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
    /** @brief Attaches an existing ParticleBackdrop while active (caller retains ownership). zh_CN: 激活状态下关联已有的 ParticleBackdrop（仍由应用管理，不会随插件销毁）。 */
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
