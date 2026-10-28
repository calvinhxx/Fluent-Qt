#ifndef FLUENTQT_PARTICLES_PARTICLESSESSION_H
#define FLUENTQT_PARTICLES_PARTICLESSESSION_H

#include <QObject>
#include <memory>
#include "ParticleBackdrop.h"

namespace fluent::particles {

/**
 * @brief Owns a Particles plugin registration for an application scope.
 * zh_CN: 为应用作用域持有 Particles 插件注册。
 *
 * Host and backdrops remain borrowed. Activate before creating or attaching
 * backdrops; deactivation pauses them without deleting widgets, child content,
 * or settings such as animationEnabled. Reactivating or destroying the session
 * removes the pause. Keep this session outside tracked widget ancestry. All
 * operations use QApplication's GUI thread.
 * zh_CN: 宿主和背景均为借用对象。先激活再创建或关联背景；停用仅暂停动画，不销毁
 * 控件或子内容，也不修改 animationEnabled 等设置；再次激活或销毁会话即解除暂停。
 * 本会话不可作为受追踪控件的后代，所有操作均须在 QApplication 的 GUI 线程执行。
 */
class ParticlesSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ isActive NOTIFY activeChanged)
public:
    explicit ParticlesSession(QObject* host = nullptr, QObject* parent = nullptr);
    ~ParticlesSession() override;
    /** @brief Activates the plugin; repeated activation is a no-op.
     * zh_CN: 激活插件；重复激活为空操作。 */
    bool activate(QString* error = nullptr);
    /** @brief Pauses borrowed backdrops until reactivation or destruction; settings are preserved.
     * zh_CN: 暂停借用背景，直到再次激活或会话销毁；保留所有权和粒子设置。 */
    bool deactivate(QString* error = nullptr);
    bool isActive() const;
    /** @brief Creates a caller-owned backdrop with the requested parent while active.
     * zh_CN: 激活时创建指定父对象的背景，由调用方持有。 */
    ParticleBackdrop* createBackdrop(QWidget* parent = nullptr);
    /** @brief Tracks a borrowed backdrop while active without reparenting it.
     * zh_CN: 激活时追踪借用背景，不改变其父对象。 */
    bool attachBackdrop(ParticleBackdrop* backdrop);

signals:
    void activeChanged(bool active);

private:
    struct Private;
    std::shared_ptr<Private> d;
};
} // namespace fluent::particles
#endif // FLUENTQT_PARTICLES_PARTICLESSESSION_H
