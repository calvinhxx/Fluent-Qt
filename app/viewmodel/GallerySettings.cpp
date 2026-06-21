#include "GallerySettings.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>
#include <QTimer>
#include <QtGlobal>

#include "components/foundation/FluentElement.h"
#include "utils/Log.h"

namespace fluent::gallery {
namespace {

constexpr char kThemeModeKey[] = "settings/themeMode";
constexpr char kNavigationStyleKey[] = "settings/navigationStyle";

bool persistenceAvailable()
{
    return QCoreApplication::organizationName() == QStringLiteral("Fluent-QT")
        && QCoreApplication::applicationName() == QStringLiteral("WinUI 3 Gallery");
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
        QSettings().setValue(QString::fromLatin1(kThemeModeKey),
                             static_cast<int>(mode));
    }
    applyThemeMode();
    emit themeModeChanged(m_themeMode);
    LOG_INFO(QStringLiteral("GallerySettings themeModeChanged mode=%1")
                 .arg(static_cast<int>(mode)));
}

void GallerySettings::setNavigationStyle(NavigationStyle style)
{
    if (m_navigationStyle == style)
        return;

    m_navigationStyle = style;
    if (persistenceAvailable()) {
        QSettings().setValue(QString::fromLatin1(kNavigationStyleKey),
                             static_cast<int>(style));
    }
    emit navigationStyleChanged(m_navigationStyle);
    LOG_INFO(QStringLiteral("GallerySettings navigationStyleChanged style=%1")
                 .arg(static_cast<int>(style)));
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

    const QSettings settings;
    const int theme = qBound(0, settings.value(QString::fromLatin1(kThemeModeKey), 0).toInt(), 2);
    const int navigation = qBound(0,
                                  settings.value(QString::fromLatin1(kNavigationStyleKey), 0).toInt(),
                                  4);
    m_themeMode = static_cast<ThemeMode>(theme);
    m_navigationStyle = static_cast<NavigationStyle>(navigation);
}

} // namespace fluent::gallery
