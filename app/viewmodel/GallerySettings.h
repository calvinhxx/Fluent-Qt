#ifndef GALLERYSETTINGS_H
#define GALLERYSETTINGS_H

#include <QColor>
#include <QObject>
#include <QRect>
#include <QString>

#include "components/windowing/WindowBackdrop.h"

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

    fluent::windowing::BackdropEffect windowEffect() const { return m_windowEffect; }
    void setWindowEffect(fluent::windowing::BackdropEffect effect);

    CloseBehavior closeBehavior() const { return m_closeBehavior; }
    void setCloseBehavior(CloseBehavior behavior);

    /**
     * @brief Returns the Gallery scale relative to the operating-system scale.
     * zh_CN: 返回 Gallery 相对于操作系统缩放的应用缩放百分比。
     */
    int uiScalePercent() const { return m_uiScalePercent; }
    void setUiScalePercent(int percent);

    /**
     * @brief Applies the persisted UI scale before QApplication is constructed.
     * zh_CN: 在 QApplication 构造前应用已持久化的界面缩放。
     */
    static int applyStartupUiScalePreference();

    static int normalizeUiScalePercent(int percent);

    QRect windowNormalGeometry() const { return m_windowNormalGeometry; }
    QString windowScreenName() const { return m_windowScreenName; }
    bool windowMaximized() const { return m_windowMaximized; }
    int windowPlacementScalePercent() const { return m_windowPlacementScalePercent; }
    void setWindowPlacement(const QRect& normalGeometry,
                            const QString& screenName,
                            bool maximized,
                            int scalePercent);

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
    void windowEffectChanged(fluent::windowing::BackdropEffect effect);
    void closeBehaviorChanged(CloseBehavior behavior);
    void uiScalePercentChanged(int percent);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit GallerySettings(QObject* parent = nullptr);
    void applyThemeMode();
    void load();

    ThemeMode m_themeMode = ThemeMode::System;
    StyleTheme m_styleTheme = StyleTheme::Fluent;
    NavigationStyle m_navigationStyle = NavigationStyle::Auto;
    fluent::windowing::BackdropEffect m_windowEffect =
        fluent::windowing::BackdropEffect::Mica;
    CloseBehavior m_closeBehavior = CloseBehavior::Tray;
    int m_uiScalePercent = 100;
    QRect m_windowNormalGeometry;
    QString m_windowScreenName;
    bool m_windowMaximized = false;
    int m_windowPlacementScalePercent = 0;
    bool m_closeBehaviorConfirmed = false;
    bool m_introCompleted = false;
};

} // namespace fluent::gallery

#endif // GALLERYSETTINGS_H
