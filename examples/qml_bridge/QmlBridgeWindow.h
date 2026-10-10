#ifndef FLUENTQT_QMLBRIDGEEXAMPLE_WINDOW_H
#define FLUENTQT_QMLBRIDGEEXAMPLE_WINDOW_H

#include <FluentQt/FluentQt.h>

namespace fluentqt::qml_bridge {
class QmlBridgePanel;

class QmlBridgeWindow final : public fluent::windowing::Window {
    Q_OBJECT
public:
    QmlBridgeWindow();
    QmlBridgePanel* panel() const { return m_panel; }

private:
    QmlBridgePanel* m_panel;
};
} // namespace fluentqt::qml_bridge
#endif
