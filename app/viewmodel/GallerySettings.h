#ifndef GALLERYSETTINGS_H
#define GALLERYSETTINGS_H

#include <QColor>
#include <QObject>
#include <QRect>
#include <QString>

#include "components/foundation/MotionPolicy.h"
#include "components/windowing/WindowBackdrop.h"

class QEvent;

namespace fluent::gallery {

class GallerySettings final : public QObject {
    Q_OBJECT

public:
    enum class ThemeMode { System, Light, Dark, HighContrast };
    Q_ENUM(ThemeMode)

    enum class NavigationStyle { Auto, Left, LeftCompact, LeftMinimal, Top };
    Q_ENUM(NavigationStyle)

    enum class CloseBehavior { Minimize, Tray, Quit };
    Q_ENUM(CloseBehavior)

    using MotionMode = fluent::MotionPolicy::Mode;

    static GallerySettings& instance();

    ThemeMode themeMode() const { return m_themeMode; }
    void setThemeMode(ThemeMode mode);

    MotionMode motionMode() const { return m_motionMode; }
    void setMotionMode(MotionMode mode);

    // Effective Fluent accent for the current visual mode. Custom values
    // persist through the selected platform backend.
    // zh_CN: 当前视觉模式下的 Fluent 生效强调色；自定义值会持久化。
    QColor accentColor() const;
    void setAccentColor(const QColor& accent);
    void resetAccentColor();

    NavigationStyle navigationStyle() const { return m_navigationStyle; }
    void setNavigationStyle(NavigationStyle style);

    fluent::windowing::BackdropEffect windowEffect() const { return m_windowEffect; }
    void setWindowEffect(fluent::windowing::BackdropEffect effect);

    CloseBehavior closeBehavior() const { return m_closeBehavior; }
    void setCloseBehavior(CloseBehavior behavior);

    QRect windowNormalGeometry() const { return m_windowNormalGeometry; }
    QString windowScreenName() const { return m_windowScreenName; }
    bool windowMaximized() const { return m_windowMaximized; }
    void setWindowPlacement(const QRect& normalGeometry, const QString& screenName, bool maximized);

    bool closeBehaviorConfirmed() const { return m_closeBehaviorConfirmed; }
    void setCloseBehaviorConfirmed(bool confirmed);

    /// First-launch intro tour seen flag. zh_CN: 首启引导是否已看过。
    bool introCompleted() const { return m_introCompleted; }
    void setIntroCompleted(bool completed);

    // Stable preset name from the previous launch. zh_CN: 上次启动使用的特效预设名称。
    QString lastHomeParticleEffect() const { return m_lastHomeParticleEffect; }
    void setLastHomeParticleEffect(const QString& effect);

    // Show home decoration; other particle demos keep their own controls.
    // zh_CN: 显示首页粒子装饰，其他粒子示例仍由各自控件控制。
    bool homeParticlesEnabled() const { return m_homeParticlesEnabled; }
    void setHomeParticlesEnabled(bool enabled);

signals:
    void themeModeChanged(ThemeMode mode);
    void motionModeChanged(fluent::MotionPolicy::Mode mode);
    void accentColorChanged(QColor accent);
    void navigationStyleChanged(NavigationStyle style);
    void windowEffectChanged(fluent::windowing::BackdropEffect effect);
    void closeBehaviorChanged(CloseBehavior behavior);
    void homeParticlesEnabledChanged(bool enabled);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit GallerySettings(QObject* parent = nullptr);
    void applyHostThemeMode(ThemeMode mode);
    void applyThemeMode();
    void load();

    ThemeMode m_themeMode = ThemeMode::System;
    MotionMode m_motionMode = MotionMode::Full;
    NavigationStyle m_navigationStyle = NavigationStyle::Auto;
    fluent::windowing::BackdropEffect m_windowEffect = fluent::windowing::BackdropEffect::Mica;
    CloseBehavior m_closeBehavior = CloseBehavior::Tray;
    QRect m_windowNormalGeometry;
    QString m_windowScreenName;
    bool m_windowMaximized = false;
    bool m_closeBehaviorConfirmed = false;
    bool m_introCompleted = false;
    QString m_lastHomeParticleEffect;
    bool m_homeParticlesEnabled = true;
};

} // namespace fluent::gallery

#endif // GALLERYSETTINGS_H
