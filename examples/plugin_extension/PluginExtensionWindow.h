#ifndef FLUENTQT_PLUGINEXTENSIONWINDOW_H
#define FLUENTQT_PLUGINEXTENSIONWINDOW_H

#include <FluentQt/FluentQt.h>
#include <QVariantMap>
#include <memory>

namespace fluentqt::plugin_extension {

class PluginExtensionWindow final : public fluent::windowing::Window {
    Q_OBJECT
public:
    PluginExtensionWindow();
    ~PluginExtensionWindow() override;
    bool isReady() const;
    bool setExtensionActive(bool active);
    QWidget* createGreeting();
    QVariantMap pluginState() const;
    bool runSmokeTest();

signals:
    void stateChanged();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void updateState();
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace fluentqt::plugin_extension
#endif
