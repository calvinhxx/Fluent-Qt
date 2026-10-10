#ifndef FLUENTQT_QMLBRIDGEEXAMPLE_PANEL_H
#define FLUENTQT_QMLBRIDGEEXAMPLE_PANEL_H

#include <QVariantMap>
#include <QWidget>
#include <memory>

namespace fluentqt::qml_bridge {

class QmlBridgePanel final : public QWidget {
    Q_OBJECT
public:
    explicit QmlBridgePanel(QWidget* parent = nullptr);
    ~QmlBridgePanel() override;
    bool reloadQml();
    bool deactivateQml();
    bool runSmokeTest();
    QVariantMap bridgeState() const;

signals:
    void stateChanged();

private:
    void updateState();
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace fluentqt::qml_bridge
#endif
