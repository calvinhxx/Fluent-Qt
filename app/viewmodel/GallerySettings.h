#ifndef GALLERYSETTINGS_H
#define GALLERYSETTINGS_H

#include <QObject>

class QEvent;

namespace fluent::gallery {

class GallerySettings final : public QObject {
    Q_OBJECT

public:
    enum class ThemeMode {
        System,
        Light,
        Dark
    };
    Q_ENUM(ThemeMode)

    enum class NavigationStyle {
        Auto,
        Left,
        LeftCompact,
        LeftMinimal,
        Top
    };
    Q_ENUM(NavigationStyle)

    // Window background effect. Normal is an opaque surface; Mica/Acrylic request a translucent
    // system backdrop where the platform supports one (Win11 / macOS), else they degrade to Normal.
    // zh_CN: 窗口背景效果。Normal 为不透明表面；Mica/Acrylic 在平台支持时（Win11 / macOS）请求半透明系统
    // 背景，否则退化为 Normal。
    enum class WindowEffect {
        Normal,
        Mica,
        Acrylic
    };
    Q_ENUM(WindowEffect)

    enum class CloseBehavior {
        Minimize,
        Tray,
        Quit
    };
    Q_ENUM(CloseBehavior)

    static GallerySettings& instance();

    ThemeMode themeMode() const { return m_themeMode; }
    void setThemeMode(ThemeMode mode);

    NavigationStyle navigationStyle() const { return m_navigationStyle; }
    void setNavigationStyle(NavigationStyle style);

    WindowEffect windowEffect() const { return m_windowEffect; }
    void setWindowEffect(WindowEffect effect);

    CloseBehavior closeBehavior() const { return m_closeBehavior; }
    void setCloseBehavior(CloseBehavior behavior);

    bool closeBehaviorConfirmed() const { return m_closeBehaviorConfirmed; }
    void setCloseBehaviorConfirmed(bool confirmed);

    /// First-launch intro tour seen flag. zh_CN: 首启引导是否已看过。
    bool introCompleted() const { return m_introCompleted; }
    void setIntroCompleted(bool completed);

signals:
    void themeModeChanged(ThemeMode mode);
    void navigationStyleChanged(NavigationStyle style);
    void windowEffectChanged(WindowEffect effect);
    void closeBehaviorChanged(CloseBehavior behavior);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit GallerySettings(QObject* parent = nullptr);
    void applyThemeMode();
    void load();

    ThemeMode m_themeMode = ThemeMode::System;
    NavigationStyle m_navigationStyle = NavigationStyle::Auto;
    WindowEffect m_windowEffect = WindowEffect::Mica;
    CloseBehavior m_closeBehavior = CloseBehavior::Tray;
    bool m_closeBehaviorConfirmed = false;
    bool m_introCompleted = false;
};

} // namespace fluent::gallery

#endif // GALLERYSETTINGS_H
