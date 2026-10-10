#include "QmlBridgeWindow.h"
#include "QmlBridgePanel.h"

namespace fluentqt::qml_bridge {

QmlBridgeWindow::QmlBridgeWindow() : m_panel(new QmlBridgePanel)
{
    setObjectName(QStringLiteral("qmlBridgeWindow"));
    setWindowTitle(QStringLiteral("FluentQt QML Bridge"));
    setMinimumWidth(360);
    resize(760, 680);
    setContentWidget(m_panel);
}

} // namespace fluentqt::qml_bridge
