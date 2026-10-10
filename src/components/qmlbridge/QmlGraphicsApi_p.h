#ifndef FLUENTQT_QMLBRIDGE_QMLGRAPHICSAPI_P_H
#define FLUENTQT_QMLBRIDGE_QMLGRAPHICSAPI_P_H

#include <QQuickWindow>

namespace fluent::qmlbridge::detail {
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
} // namespace fluent::qmlbridge::detail

#endif
