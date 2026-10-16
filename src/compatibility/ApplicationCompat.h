#pragma once

// Application startup, theme notifications, and display changes.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QEvent>
#include <utility>

enum class FluentSystemColorScheme { Unknown, Light, Dark };

/**
 * @brief Applies the supported Qt-version-specific High-DPI startup settings.
 * zh_CN: 应用当前 Qt 版本所需的 High-DPI 启动设置。
 */
inline void fluentPrepareHighDpiApplicationAttributes()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
}

/**
 * @brief Reports whether device-independent High-DPI scaling is active by contract.
 * zh_CN: 返回设备无关 High-DPI 缩放是否已按契约启用。
 */
inline bool fluentHighDpiScalingIsEnabled()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    return QCoreApplication::testAttribute(Qt::AA_EnableHighDpiScaling)
        && QCoreApplication::testAttribute(Qt::AA_UseHighDpiPixmaps);
#else
    return true;
#endif
}

inline FluentSystemColorScheme fluentSystemColorScheme()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (QGuiApplication::styleHints()) {
        const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
        if (scheme == Qt::ColorScheme::Dark)
            return FluentSystemColorScheme::Dark;
        if (scheme == Qt::ColorScheme::Light)
            return FluentSystemColorScheme::Light;
    }
#endif
    return FluentSystemColorScheme::Unknown;
}

template <typename Context, typename Functor>
QMetaObject::Connection fluentConnectSystemColorSchemeChanged(Context* context, Functor&& functor)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (!QGuiApplication::styleHints())
        return {};
    return QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, context,
        [slot = std::forward<Functor>(functor)](Qt::ColorScheme) mutable { slot(); });
#else
    Q_UNUSED(context);
    Q_UNUSED(functor);
    return {};
#endif
}

/**
 * @brief Returns true when an event can change the native window safe-area insets.
 * zh_CN: 判断事件是否可能改变原生窗口安全区域边距。
 */
inline bool fluentIsWindowInsetChangeEvent(const QEvent* event)
{
    if (!event)
        return false;

    if (event->type() == QEvent::WindowStateChange)
        return true;

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (event->type() == QEvent::SafeAreaMarginsChange)
        return true;
#endif

    return false;
}

/**
 * @brief Returns true when an event reports a device-pixel-ratio change.
 * zh_CN: 判断事件是否表示设备像素比发生变化。
 */
inline bool fluentIsDevicePixelRatioChangeEvent(const QEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    return event && event->type() == QEvent::DevicePixelRatioChange;
#else
    Q_UNUSED(event);
    return false;
#endif
}

/**
 * @brief Returns true when a widget should refresh display-scale-dependent caches.
 * zh_CN: 判断控件是否应刷新依赖显示缩放比例的缓存。
 *
 * ScreenChangeInternal is available on the full Qt 5.15+/6.2+ support range;
 * Qt 6.6 additionally exposes a dedicated DevicePixelRatioChange event.
 * zh_CN: 全部支持的 Qt 5.15+/6.2+ 均可使用 ScreenChangeInternal；Qt 6.6+
 * 还会提供独立的 DevicePixelRatioChange 事件。
 */
inline bool fluentIsDisplayScaleChangeEvent(const QEvent* event)
{
    return event && (event->type() == QEvent::ScreenChangeInternal || fluentIsDevicePixelRatioChangeEvent(event));
}
