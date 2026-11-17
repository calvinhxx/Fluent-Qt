#ifndef FLUENTQT_PARTICLES_PARTICLESPLUGIN_H
#define FLUENTQT_PARTICLES_PARTICLESPLUGIN_H

#include <memory>
#include "components/foundation/plugins/WidgetPlugin.h"
#include "ParticleBackdrop.h"

namespace fluent::particles {

/**
 * @brief Plugin adapter that creates and pauses ParticleBackdrop widgets on deactivation.
 * Register with PluginManager and activate "fluentqt.particles" on the GUI thread. Deactivating
 * pauses tracked backdrops without deleting them or changing properties like animationEnabled;
 * reactivating or destroying the plugin lifts the pause. Must not be parented under a tracked
 * backdrop, and creates no GPU context.
 * zh_CN: 负责创建 ParticleBackdrop 并在停用时统一暂停的插件适配器。
 * 在 GUI 线程向 PluginManager 注册并激活 fluentqt.particles。停用时仅暂停已关联的背景动画，
 * 不销毁控件，也不更改 animationEnabled 等属性；重新激活或销毁插件即恢复运行。插件不能作为
 * 受管理控件的子对象，且不创建 GPU 上下文。
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
    /** @brief Attaches an existing ParticleBackdrop while active without reparenting or taking ownership. zh_CN: 在激活状态下关联已有的 ParticleBackdrop（不改变父对象或所有权，保留原设置）。 */
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
