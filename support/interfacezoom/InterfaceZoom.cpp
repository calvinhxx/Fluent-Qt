#include "InterfaceZoom.h"

#include <QCoreApplication>
#include <QScopedValueRollback>
#include <QThread>
#include <cmath>

#include "src/compatibility/private/InterfaceZoomRepaintCompat_p.h"

#ifdef FLUENT_QT_HAS_INTERFACE_ZOOM
#include "src/compatibility/private/InterfaceZoomCompat_p.h"
#endif

namespace fluent::support::interfacezoom {
namespace {
qreal currentZoom = 1.0;
bool applying = false;

// Installed only by processes that use zoom, never by merely linking this
// support code (the PySide6 extension links it for every fluentqt user).
// zh_CN: 仅在实际使用缩放的进程中安装；仅链接此支持代码（PySide6 扩展会为所有
// fluentqt 用户链接）不会安装。
void installRepaintGuard()
{
    ::compatibility::detail::installInterfaceZoomRepaintGuard();
}

qreal baseScaleFactor()
{
    static const qreal base = [] {
        bool ok = false;
        const qreal value = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble(&ok);
        return ok && std::isfinite(value) && value > 0.0 ? value : 1.0;
    }();
    return base;
}

bool validZoom(qreal zoom)
{
    const qreal factor = baseScaleFactor() * zoom;
    return std::isfinite(zoom) && zoom > 0.0 && std::isfinite(factor) && factor > 0.0;
}
} // namespace

bool isSupported()
{
#ifdef FLUENT_QT_HAS_INTERFACE_ZOOM
    return true;
#else
    return false;
#endif
}

bool prepareStartup(qreal zoom)
{
    if (QCoreApplication::instance() || !validZoom(zoom))
        return false;
    // Runs from the QCoreApplication constructor. zh_CN: 在 QCoreApplication 构造时运行。
    qAddPreRoutine(installRepaintGuard);
    const qreal factor = baseScaleFactor() * zoom;
    if (!qFuzzyCompare(factor, baseScaleFactor()) || !qFuzzyCompare(currentZoom, zoom))
        qputenv("QT_SCALE_FACTOR", QByteArray::number(factor, 'g', 16));
    currentZoom = zoom;
    return true;
}

qreal appliedZoom()
{
    return currentZoom;
}

bool apply(qreal zoom)
{
#ifdef FLUENT_QT_HAS_INTERFACE_ZOOM
    const auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app || QThread::currentThread() != app->thread() || !validZoom(zoom) || applying)
        return false;
    installRepaintGuard();
    if (qFuzzyCompare(currentZoom, zoom))
        return true;
    QScopedValueRollback<bool> guard(applying, true);
    // Publish the applied level before synchronous screen/resize callbacks run.
    // zh_CN: 同步屏幕与尺寸回调执行前先公布生效倍率。
    currentZoom = zoom;
    ::compatibility::detail::applyInterfaceZoomFactor(baseScaleFactor() * zoom);
    return true;
#else
    Q_UNUSED(zoom);
    return false;
#endif
}
} // namespace fluent::support::interfacezoom
