#pragma once

// Application startup, theme notifications, and display changes.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QEvent>
#include <utility>

/**
 * @brief OS color scheme reported by Qt style hints when supported (Qt 6.5+).
 * zh_CN: Qt 样式提示上报的系统深浅色主题（不支持时返回 Unknown）。
 */
enum class FluentSystemColorScheme { Unknown, Light, Dark };

/**
 * @brief Applies Qt-version-specific High-DPI startup attributes before QApplication creation.
 * zh_CN: 在创建 QApplication 前应用当前 Qt 版本所需的 High-DPI 启动配置。
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
 * @brief Reports whether device-independent High-DPI scaling is active.
 * zh_CN: 返回设备无关的 High-DPI 缩放是否已启用。
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
 * @brief Returns true when an event requires refreshing display-scale-dependent caches (ScreenChangeInternal on all versions, plus DevicePixelRatioChange on Qt 6.6+).
 * zh_CN: 判断事件是否需要刷新缩放相关缓存（所有版本检查 ScreenChangeInternal，Qt 6.6+ 额外检查 DevicePixelRatioChange）。
 */
inline bool fluentIsDisplayScaleChangeEvent(const QEvent* event)
{
    return event && (event->type() == QEvent::ScreenChangeInternal || fluentIsDevicePixelRatioChangeEvent(event));
}
