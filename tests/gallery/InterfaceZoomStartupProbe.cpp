#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWidget>
#include <QWindow>
#include <cstdio>

#include "compatibility/ApplicationCompat.h"
#include "support/interfacezoom/InterfaceZoom.h"

int main(int argc, char** argv)
{
    if (argc != 2)
        return 2;
    const qreal zoom = QByteArray(argv[1]).toDouble();
    if (!fluent::support::interfacezoom::prepareStartup(zoom))
        return 3;
    fluentPrepareHighDpiApplicationAttributes();
    QApplication app(argc, argv);
    QWidget window;
    window.resize(400, 300);
    window.show();
    app.processEvents();
    const QJsonObject result{{QStringLiteral("zoom"), fluent::support::interfacezoom::appliedZoom()},
        {QStringLiteral("dpr"), window.windowHandle()->devicePixelRatio()},
        {QStringLiteral("live"), fluent::support::interfacezoom::isSupported()},
        {QStringLiteral("latePrepare"), fluent::support::interfacezoom::prepareStartup(2.0)}};
    std::puts(QJsonDocument(result).toJson(QJsonDocument::Compact).constData());
    return 0;
}
