#ifndef FLUENTQT_PARTICLES_PARTICLESSESSION_H
#define FLUENTQT_PARTICLES_PARTICLESSESSION_H

#include <QObject>
#include <memory>
#include "ParticleBackdrop.h"

namespace fluent::particles {

/**
 * @brief Scoped session for managing the particles plugin lifecycle.
 * Call activate() before use; deactivating pauses attached backdrops. Must be used on the GUI thread.
 * zh_CN: 管理粒子插件生命周期的会话工具；使用前先调用 activate()，停用时暂停已关联的背景，须在 GUI 线程操作。
 */
class ParticlesSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit ParticlesSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~ParticlesSession() override;
    /** @brief Registers and activates ParticlesPlugin; returns true if already active. zh_CN: 注册并激活粒子插件；已激活时直接返回 true。 */
    bool activate(QString* error = nullptr);
    /** @brief Pauses attached backdrops without deleting widgets. zh_CN: 暂停已关联的粒子背景，不销毁控件。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates and attaches a ParticleBackdrop while active; returns nullptr when inactive. zh_CN: 激活时创建并关联 ParticleBackdrop（由 parent 或调用方管理）；未激活时返回 nullptr。 */
    ParticleBackdrop* createBackdrop(QWidget* parent = nullptr);
    /** @brief Attaches an existing ParticleBackdrop while active (caller retains ownership). zh_CN: 激活状态下关联已有的 ParticleBackdrop（仍由应用管理，不会随会话销毁）。 */
    bool attachBackdrop(ParticleBackdrop* backdrop);

signals:
    void activeChanged(bool active);

private:
    struct Private;
    std::shared_ptr<Private> d;
};
} // namespace fluent::particles
#endif // FLUENTQT_PARTICLES_PARTICLESSESSION_H
