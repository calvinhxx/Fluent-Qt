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

    static GallerySettings& instance();

    ThemeMode themeMode() const { return m_themeMode; }
    void setThemeMode(ThemeMode mode);

    NavigationStyle navigationStyle() const { return m_navigationStyle; }
    void setNavigationStyle(NavigationStyle style);

signals:
    void themeModeChanged(ThemeMode mode);
    void navigationStyleChanged(NavigationStyle style);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    explicit GallerySettings(QObject* parent = nullptr);
    void applyThemeMode();
    void load();

    ThemeMode m_themeMode = ThemeMode::System;
    NavigationStyle m_navigationStyle = NavigationStyle::Auto;
};

} // namespace fluent::gallery

#endif // GALLERYSETTINGS_H
