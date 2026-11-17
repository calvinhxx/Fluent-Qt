#ifndef FLUENTQT_PARTICLES_PARTICLESSESSION_H
#define FLUENTQT_PARTICLES_PARTICLESSESSION_H

#include <QObject>
#include <memory>
#include "ParticleBackdrop.h"

namespace fluent::particles {

/**
 * @brief Scoped facade owning a ParticlesPlugin registration for an application host.
 * Caller retains ownership of the host and backdrops. Activate before creating or attaching
 * backdrops; deactivating pauses tracked backdrops without deleting widgets or altering settings
 * such as animationEnabled, and reactivating or destroying the session lifts the pause. Must not
 * be parented under a tracked widget, and all calls must run on the GUI thread.
 * zh_CN: 为应用作用域管理 ParticlesPlugin 注册的会话封装。
 * 调用方负责宿主与粒子背景的生命周期。创建或关联背景前须先激活；停用时仅暂停已关联背景的动画，
 * 不销毁控件或子内容，也不修改 animationEnabled 等属性，重新激活或销毁会话即恢复。本会话不能
 * 作为受管理控件的子对象，所有调用须在 GUI 线程执行。
 */
class ParticlesSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit ParticlesSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~ParticlesSession() override;
    /** @brief Registers and activates ParticlesPlugin; returns true immediately if already active. zh_CN: 注册并激活 ParticlesPlugin；已激活时直接返回 true。 */
    bool activate(QString* error = nullptr);
    /** @brief Pauses tracked backdrops until reactivated or destroyed without deleting widgets or changing settings. zh_CN: 暂停已关联背景直至重新激活或销毁会话，不销毁控件或更改其设置。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates an attached ParticleBackdrop while active (owned by parent or caller); returns nullptr when inactive. zh_CN: 激活时创建并关联一个 ParticleBackdrop（由 parent 或调用方管理）；未激活时返回 nullptr。 */
    ParticleBackdrop* createBackdrop(QWidget* parent = nullptr);
    /** @brief Attaches an existing ParticleBackdrop while active without reparenting or taking ownership. zh_CN: 在激活状态下关联已有的 ParticleBackdrop（不改变父对象或所有权）。 */
    bool attachBackdrop(ParticleBackdrop* backdrop);

signals:
    void activeChanged(bool active);

private:
    struct Private;
    std::shared_ptr<Private> d;
};
} // namespace fluent::particles
#endif // FLUENTQT_PARTICLES_PARTICLESSESSION_H
