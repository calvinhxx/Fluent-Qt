#pragma once

#include "QuickFeatures_p.h"
#include <QQuickWindow>
#include <QSGRendererInterface>

// Only the optional QmlBridge target consumes this Qt Quick adapter.
// zh_CN: 此 Qt Quick 适配器仅供可选 QmlBridge 目标使用。
namespace compatibility::detail {

enum class QuickGraphicsApi { Automatic, OpenGL, Direct3D11, Metal };

inline bool usesSoftwareQuick()
{
    QString backend = QQuickWindow::sceneGraphBackend();
    // Qt reads the environment lazily when its first Quick window is created.
    // zh_CN: Qt 在首次创建 Quick 窗口时才读取环境设置。
    if (backend.isEmpty()) {
        backend = qEnvironmentVariable("QT_QUICK_BACKEND");
        if (backend.isEmpty())
            backend = qEnvironmentVariable("QMLSCENE_DEVICE");
    }
    return backend == QStringLiteral("software");
}

#if FLUENT_QUICK_USES_RHI
inline QSGRendererInterface::GraphicsApi quickGraphicsApi(QuickGraphicsApi api)
{
    switch (api) {
    case QuickGraphicsApi::Automatic:
        return QSGRendererInterface::Unknown;
    case QuickGraphicsApi::OpenGL:
        return QSGRendererInterface::OpenGL;
    case QuickGraphicsApi::Direct3D11:
        return QSGRendererInterface::Direct3D11;
    case QuickGraphicsApi::Metal:
        return QSGRendererInterface::Metal;
    }
    return QSGRendererInterface::Unknown;
}
#endif

inline void prepareQuickGraphicsApi(QuickGraphicsApi api)
{
#if FLUENT_QUICK_USES_RHI
    QQuickWindow::setGraphicsApi(quickGraphicsApi(api));
#else
    Q_UNUSED(api);
#endif
}

inline bool quickGraphicsApiCompatible(QuickGraphicsApi api)
{
    if (api == QuickGraphicsApi::Automatic || usesSoftwareQuick())
        return true;
#if FLUENT_QUICK_USES_RHI
    const auto selected = QQuickWindow::graphicsApi();
    return selected == QSGRendererInterface::Software || selected == quickGraphicsApi(api);
#else
    return api == QuickGraphicsApi::OpenGL;
#endif
}

inline bool quickUsesOpenGL()
{
    if (usesSoftwareQuick())
        return false;
#if FLUENT_QUICK_USES_RHI
    return QQuickWindow::graphicsApi() == QSGRendererInterface::OpenGL;
#else
    return true;
#endif
}

constexpr bool quickSupportsIndependentTargetDpr()
{
    return FLUENT_QUICK_HAS_RENDER_TARGET_DPR;
}

} // namespace compatibility::detail
