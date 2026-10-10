#include "QmlBridgeApplication.h"
#include "QmlBridgePanel.h"
#include "QmlBridgeWindow.h"

#include <FluentQt/QmlBridge.h>
#include <QApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QPainter>
#include <QQuickWidget>
#include <QRegularExpression>
#include <QTimer>
#include <cstdlib>

namespace fluentqt::qml_bridge {
namespace {
bool captureWindow(QmlBridgeWindow& window, const QString& path)
{
    QPixmap image = window.grab();
    auto* quick = window.findChild<QQuickWidget*>(QStringLiteral("qmlQuickWidget"));
    if (quick && quick->isVisible()) {
        const QImage frame = quick->grabFramebuffer();
        if (frame.isNull())
            return false;
        QPainter painter(&image);
        painter.drawImage(QRect(quick->mapTo(&window, QPoint()), quick->size()), frame);
    }
    return image.save(path);
}
} // namespace

int runApplication(int argc, char** argv)
{
    fluent::prepareHighDpiApplication();
    fluent::qmlbridge::QmlSession::prepareApplication();
    QApplication application(argc, argv);
    fluent::initializeResources();
    application.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());
    const QStringList arguments = application.arguments();
    const int themeIndex = arguments.indexOf(QStringLiteral("--theme"));
    if (themeIndex >= 0 && arguments.value(themeIndex + 1) == QStringLiteral("dark"))
        fluent::FluentElement::setTheme(fluent::FluentElement::Dark);
    QmlBridgeWindow window;
    const int sizeIndex = arguments.indexOf(QStringLiteral("--size"));
    if (sizeIndex >= 0) {
        const auto size = QRegularExpression(QStringLiteral("^(\\d+)x(\\d+)$"))
                              .match(arguments.value(sizeIndex + 1));
        if (size.hasMatch())
            window.resize(size.captured(1).toInt(), size.captured(2).toInt());
    }
    window.show();
    const bool smoke = arguments.contains(QStringLiteral("--smoke-test"));
    const int captureIndex = arguments.indexOf(QStringLiteral("--capture"));
    const QString capture = captureIndex < 0 ? QString() : arguments.value(captureIndex + 1);
    QTimer poll;
    QElapsedTimer elapsed;
    if (smoke || !capture.isEmpty()) {
        elapsed.start();
        poll.setInterval(50);
        QObject::connect(&poll, &QTimer::timeout, &application, [&] {
            const auto state = window.panel()->bridgeState();
            if (!state.value(QStringLiteral("ready")).toBool() &&
                state.value(QStringLiteral("error")).toString().isEmpty() &&
                elapsed.elapsed() < 5000)
                return;
            poll.stop();
            const bool passed = state.value(QStringLiteral("ready")).toBool() &&
                                (!smoke || window.panel()->runSmokeTest());
            QTimer::singleShot(200, &application, [&, passed] {
                const bool captured = capture.isEmpty() || captureWindow(window, capture);
                auto result = window.panel()->bridgeState();
                if (smoke)
                    result.insert(QStringLiteral("smokePassed"), passed);
                if (!capture.isEmpty())
                    result.insert(QStringLiteral("captureSaved"), captured);
                qInfo().noquote() << QJsonDocument::fromVariant(result).toJson(
                    QJsonDocument::Compact);
                application.exit(passed && captured ? EXIT_SUCCESS : EXIT_FAILURE);
            });
        });
        poll.start();
    }
    return application.exec();
}

} // namespace fluentqt::qml_bridge
