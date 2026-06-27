#include "GallerySettings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>
#include <QTimer>
#include <QtGlobal>

#include "components/foundation/FluentElement.h"
#include "components/foundation/ThemeRegistry.h"
#include "utils/Log.h"
#include "viewmodel/ThemeCatalog.h"

namespace fluent::gallery {
namespace {

constexpr char kThemeModeKey[] = "settings/themeMode";
constexpr char kStyleThemeKey[] = "settings/styleTheme";
constexpr char kNavigationStyleKey[] = "settings/navigationStyle";
constexpr char kWindowEffectKey[] = "settings/windowEffect";
constexpr char kCloseBehaviorKey[] = "settings/closeBehavior";
constexpr char kCloseBehaviorConfirmedKey[] = "settings/closeBehaviorConfirmed";
constexpr char kIntroCompletedKey[] = "intro/completed";

bool persistenceAvailable()
{
    return QCoreApplication::organizationName() == QStringLiteral("Fluent-QT")
        && QCoreApplication::applicationName() == QStringLiteral("WinUI 3 Gallery");
}

// Single config file, in the same per-user app folder as the logs (AppLocalDataLocation), so all app
// data lives under one directory: %LOCALAPPDATA%\Fluent-QT\WinUI 3 Gallery\{config.ini, logs\}.
// zh_CN: 单一配置文件,与日志同处每用户 app 目录(AppLocalDataLocation),让所有 app 数据集中在一个目录下。
QString configFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/config.ini");
}

QSettings configSettings()
{
    return QSettings(configFilePath(), QSettings::IniFormat);
}

fluent::FluentElement::Theme systemTheme()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark)
            return fluent::FluentElement::Dark;
        if (scheme == Qt::ColorScheme::Light)
            return fluent::FluentElement::Light;
    }
#endif
#ifdef Q_OS_WIN
    const QSettings registry(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
        QSettings::NativeFormat);
    if (registry.contains(QStringLiteral("AppsUseLightTheme"))) {
        return registry.value(QStringLiteral("AppsUseLightTheme"), 1).toInt() == 0
            ? fluent::FluentElement::Dark
            : fluent::FluentElement::Light;
    }
#endif
    if (qApp) {
        const QPalette palette = qApp->palette();
        if (palette.color(QPalette::Window).lightness()
            < palette.color(QPalette::WindowText).lightness()) {
            return fluent::FluentElement::Dark;
        }
    }
    return fluent::FluentElement::Light;
}

} // namespace

GallerySettings& GallerySettings::instance()
{
    static auto* settings = new GallerySettings(qApp);
    return *settings;
}

GallerySettings::GallerySettings(QObject* parent)
    : QObject(parent)
{
    load();
    // Install the persisted brand style theme into the ThemeRegistry before any widget paints, so the
    // first frame already uses the right palette + radius. zh_CN: 在任何控件绘制前把持久化的品牌样式主题
    // 装入 ThemeRegistry,使首帧即用正确的调色板与圆角。
    ThemeCatalog::apply(m_styleTheme);
    if (qApp)
        qApp->installEventFilter(this);
    auto* systemThemePoll = new QTimer(this);
    systemThemePoll->setInterval(1000);
    connect(systemThemePoll, &QTimer::timeout, this, [this]() {
        if (m_themeMode == ThemeMode::System)
            applyThemeMode();
    });
    systemThemePoll->start();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
                this, [this](Qt::ColorScheme) {
                    if (m_themeMode == ThemeMode::System)
                        applyThemeMode();
                });
    }
#endif
    applyThemeMode();
}

void GallerySettings::setThemeMode(ThemeMode mode)
{
    if (m_themeMode == mode)
        return;

    m_themeMode = mode;
    if (persistenceAvailable()) {
        configSettings().setValue(QString::fromLatin1(kThemeModeKey),
                                  static_cast<int>(mode));
    }
    applyThemeMode();
    emit themeModeChanged(m_themeMode);
    LOG_INFO(QStringLiteral("GallerySettings themeModeChanged mode=%1")
                 .arg(static_cast<int>(mode)));
}

void GallerySettings::setStyleTheme(StyleTheme theme)
{
    if (m_styleTheme == theme)
        return;

    m_styleTheme = theme;
    if (persistenceAvailable()) {
        configSettings().setValue(QString::fromLatin1(kStyleThemeKey),
                                  static_cast<int>(theme));
    }
    // Install the new palette/radius preset, then force an atomic repaint (refreshTheme re-broadcasts
    // the current Light/Dark mode, which setThemeDeferred would skip since the mode is unchanged).
    // zh_CN: 装入新的调色板/圆角预设,再强制原子重绘(refreshTheme 重广播当前明暗模式;因模式未变,
    // setThemeDeferred 会跳过)。
    ThemeCatalog::apply(theme);
    fluent::FluentElement::refreshTheme();
    emit styleThemeChanged(m_styleTheme);
    // Switching presets changes the effective accent too, so any accent UI can resync. zh_CN: 切换预设也会
    // 改变生效强调色,便于强调色 UI 同步。
    emit accentColorChanged(accentColor());
    LOG_INFO(QStringLiteral("GallerySettings styleThemeChanged theme=%1")
                 .arg(static_cast<int>(theme)));
}

QColor GallerySettings::accentColor() const
{
    const bool dark = fluent::FluentElement::currentTheme() == fluent::FluentElement::Dark;
    return fluent::ThemeRegistry::instance().colors(dark).accentDefault;
}

void GallerySettings::setAccentColor(const QColor& accent)
{
    if (!accent.isValid())
        return;
    // Persist the override into the active style theme's JSON, then re-install + repaint atomically
    // (refreshTheme re-broadcasts the current mode, which setThemeDeferred would skip). zh_CN: 把覆盖
    // 持久化进当前样式主题的 JSON,再原子地重新安装并重绘(refreshTheme 重广播当前模式,setThemeDeferred 会跳过)。
    ThemeCatalog::setUserAccent(m_styleTheme, accent);
    ThemeCatalog::apply(m_styleTheme);
    fluent::FluentElement::refreshTheme();
    emit accentColorChanged(accentColor());
    LOG_INFO(QStringLiteral("GallerySettings setAccentColor theme=%1 accent=%2")
                 .arg(static_cast<int>(m_styleTheme))
                 .arg(accent.name(QColor::HexArgb)));
}

void GallerySettings::resetAccentColor()
{
    ThemeCatalog::clearUserAccent(m_styleTheme);
    ThemeCatalog::apply(m_styleTheme);
    fluent::FluentElement::refreshTheme();
    emit accentColorChanged(accentColor());
    LOG_INFO(QStringLiteral("GallerySettings resetAccentColor theme=%1")
                 .arg(static_cast<int>(m_styleTheme)));
}

void GallerySettings::setNavigationStyle(NavigationStyle style)
{
    if (m_navigationStyle == style)
        return;

    m_navigationStyle = style;
    if (persistenceAvailable()) {
        configSettings().setValue(QString::fromLatin1(kNavigationStyleKey),
                                  static_cast<int>(style));
    }
    emit navigationStyleChanged(m_navigationStyle);
    LOG_INFO(QStringLiteral("GallerySettings navigationStyleChanged style=%1")
                 .arg(static_cast<int>(style)));
}

void GallerySettings::setWindowEffect(WindowEffect effect)
{
    if (m_windowEffect == effect)
        return;

    m_windowEffect = effect;
    if (persistenceAvailable()) {
        configSettings().setValue(QString::fromLatin1(kWindowEffectKey),
                                  static_cast<int>(effect));
    }
    emit windowEffectChanged(m_windowEffect);
    LOG_INFO(QStringLiteral("GallerySettings windowEffectChanged effect=%1")
                 .arg(static_cast<int>(effect)));
}

void GallerySettings::setCloseBehavior(CloseBehavior behavior)
{
    if (m_closeBehavior == behavior)
        return;

    m_closeBehavior = behavior;
    if (persistenceAvailable()) {
        configSettings().setValue(QString::fromLatin1(kCloseBehaviorKey),
                                  static_cast<int>(behavior));
    }
    emit closeBehaviorChanged(m_closeBehavior);
    LOG_INFO(QStringLiteral("GallerySettings closeBehaviorChanged behavior=%1")
                 .arg(static_cast<int>(behavior)));
}

void GallerySettings::setCloseBehaviorConfirmed(bool confirmed)
{
    if (m_closeBehaviorConfirmed == confirmed)
        return;

    m_closeBehaviorConfirmed = confirmed;
    if (persistenceAvailable()) {
        QSettings settings = configSettings();
        settings.setValue(QString::fromLatin1(kCloseBehaviorConfirmedKey), confirmed);
        if (confirmed) {
            settings.setValue(QString::fromLatin1(kCloseBehaviorKey),
                              static_cast<int>(m_closeBehavior));
        }
    }
}

void GallerySettings::setIntroCompleted(bool completed)
{
    if (m_introCompleted == completed)
        return;

    m_introCompleted = completed;
    if (persistenceAvailable())
        configSettings().setValue(QString::fromLatin1(kIntroCompletedKey), completed);
}

bool GallerySettings::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == qApp && event && event->type() == QEvent::ApplicationPaletteChange
        && m_themeMode == ThemeMode::System) {
        applyThemeMode();
    }
    return QObject::eventFilter(watched, event);
}

void GallerySettings::applyThemeMode()
{
    fluent::FluentElement::Theme theme = systemTheme();
    if (m_themeMode == ThemeMode::Light)
        theme = fluent::FluentElement::Light;
    else if (m_themeMode == ThemeMode::Dark)
        theme = fluent::FluentElement::Dark;
    fluent::FluentElement::setThemeDeferred(theme);
}

void GallerySettings::load()
{
    if (!persistenceAvailable())
        return;

    const QSettings settings(configFilePath(), QSettings::IniFormat);
    const int theme = qBound(0, settings.value(QString::fromLatin1(kThemeModeKey), 0).toInt(), 2);
    const int styleTheme = qBound(0, settings.value(QString::fromLatin1(kStyleThemeKey), 0).toInt(), 2);
    const int navigation = qBound(0,
                                  settings.value(QString::fromLatin1(kNavigationStyleKey), 0).toInt(),
                                  4);
    // Default 1 = Mica, matching m_windowEffect's in-class initializer (the current shipping look).
    // zh_CN: 默认 1 = Mica，与 m_windowEffect 的类内初值一致（当前出厂观感）。
    const int windowEffect = qBound(0,
                                    settings.value(QString::fromLatin1(kWindowEffectKey), 1).toInt(),
                                    2);
    const int closeBehavior = qBound(
        0,
        settings.value(QString::fromLatin1(kCloseBehaviorKey), 1).toInt(),
        2);
    m_themeMode = static_cast<ThemeMode>(theme);
    m_styleTheme = static_cast<StyleTheme>(styleTheme);
    m_navigationStyle = static_cast<NavigationStyle>(navigation);
    m_windowEffect = static_cast<WindowEffect>(windowEffect);
    m_closeBehavior = static_cast<CloseBehavior>(closeBehavior);
    m_closeBehaviorConfirmed = settings.value(
        QString::fromLatin1(kCloseBehaviorConfirmedKey), false).toBool();
    m_introCompleted = settings.value(QString::fromLatin1(kIntroCompletedKey), false).toBool();
}

} // namespace fluent::gallery
