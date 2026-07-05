#ifndef GALLERYSETTINGS_H
#define GALLERYSETTINGS_H

#include <QColor>
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
    // system backdrop where the platform supports one (Win11 DWM, macOS); Windows 10 and other
    // unsupported platforms degrade to Normal. zh_CN: 窗口背景效果。Normal 为不透明表面；Mica/Acrylic
    // 在平台支持时（Win11 DWM、macOS）请求半透明系统背景；Windows 10 和其它不支持的平台退化为 Normal。
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

    // Brand style theme: the palette + corner-radius preset installed into the runtime ThemeRegistry.
    // Orthogonal to ThemeMode (each style has its own Light/Dark). zh_CN: 品牌样式主题:安装进运行时
    // ThemeRegistry 的「调色板 + 圆角」预设。与 ThemeMode 正交(每套样式各有明/暗)。
    enum class StyleTheme {
        Fluent,
        Material,
        MacOS
    };
    Q_ENUM(StyleTheme)

    static GallerySettings& instance();

    ThemeMode themeMode() const { return m_themeMode; }
    void setThemeMode(ThemeMode mode);

    StyleTheme styleTheme() const { return m_styleTheme; }
    void setStyleTheme(StyleTheme theme);

    // Accent color of the active style theme (the effective value for the current Light/Dark mode,
    // i.e. preset accent unless the user customized it). Customizing persists into the style theme's
    // themes/<key>.json override file, so it survives restarts and is orthogonal to the preset switch.
    // zh_CN: 当前样式主题的强调色(当前明暗模式下的生效值,即未自定义时为预设强调色)。自定义会持久化进该样式主题的
    // themes/<key>.json 覆盖文件,可跨重启保留,且与预设切换正交。
    QColor accentColor() const;
    void setAccentColor(const QColor& accent);
    void resetAccentColor();

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
    void styleThemeChanged(StyleTheme theme);
    void accentColorChanged(QColor accent);
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
    StyleTheme m_styleTheme = StyleTheme::Fluent;
    NavigationStyle m_navigationStyle = NavigationStyle::Auto;
    WindowEffect m_windowEffect = WindowEffect::Mica;
    CloseBehavior m_closeBehavior = CloseBehavior::Tray;
    bool m_closeBehaviorConfirmed = false;
    bool m_introCompleted = false;
};

} // namespace fluent::gallery

#endif // GALLERYSETTINGS_H
