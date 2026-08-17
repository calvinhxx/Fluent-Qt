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

    static GallerySettings& instance();

    ThemeMode themeMode() const { return m_themeMode; }
    void setThemeMode(ThemeMode mode);

    // Effective Fluent accent for the current Light/Dark mode. Custom values
    // persist through the selected platform backend.
    // zh_CN: 当前明暗模式下的 Fluent 生效强调色；自定义值会持久化。
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
    void setWindowPlacement(const QRect& normalGeometry,
                            const QString& screenName,
                            bool maximized);

    bool closeBehaviorConfirmed() const { return m_closeBehaviorConfirmed; }
    void setCloseBehaviorConfirmed(bool confirmed);

    /// First-launch intro tour seen flag. zh_CN: 首启引导是否已看过。
    bool introCompleted() const { return m_introCompleted; }
    void setIntroCompleted(bool completed);

signals:
    void themeModeChanged(ThemeMode mode);
    void accentColorChanged(QColor accent);
    void navigationStyleChanged(NavigationStyle style);
    void windowEffectChanged(fluent::windowing::BackdropEffect effect);
    void closeBehaviorChanged(CloseBehavior behavior);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit GallerySettings(QObject* parent = nullptr);
    void applyHostThemeMode(ThemeMode mode);
    void applyThemeMode();
    void load();

    ThemeMode m_themeMode = ThemeMode::System;
    NavigationStyle m_navigationStyle = NavigationStyle::Auto;
    fluent::windowing::BackdropEffect m_windowEffect =
        fluent::windowing::BackdropEffect::Mica;
    CloseBehavior m_closeBehavior = CloseBehavior::Tray;
    QRect m_windowNormalGeometry;
    QString m_windowScreenName;
    bool m_windowMaximized = false;
    bool m_closeBehaviorConfirmed = false;
    bool m_introCompleted = false;
};

} // namespace fluent::gallery

#endif // GALLERYSETTINGS_H
