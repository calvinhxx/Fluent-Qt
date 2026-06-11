#include "WindowingSamples.h"

#include <QPointer>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "components/windowing/TitleBar.h"
#include "components/windowing/Window.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::textfields::Label;
using fluent::windowing::TitleBar;
using fluent::windowing::Window;
using samples::makeSample;

QVector<GallerySample> titleBarSamples()
{
    return {
        makeSample(QStringLiteral("title-bar-basic"),
                   QStringLiteral("TitleBar with custom content"),
                   QStringLiteral("The bar hosts custom widgets and exposes drag regions."),
                   QStringLiteral("auto* titleBar = new TitleBar(this);\n"
                                  "titleBar->setContentWidget(customContent);"),
                   [](QWidget* parent) {
                       auto* titleBar = new TitleBar(parent);
                       titleBar->setFixedWidth(420);
                       auto* content = new Label(QStringLiteral("Custom title bar content"), titleBar);
                       content->setFluentTypography(Typography::FontRole::Caption);
                       content->setAlignment(Qt::AlignCenter);
                       titleBar->setContentWidget(content);
                       return titleBar;
                   })
    };
}

QVector<GallerySample> windowSamples()
{
    return {
        makeSample(QStringLiteral("window-basic"),
                   QStringLiteral("Fluent window"),
                   QStringLiteral("Opens a top-level window with Fluent chrome and theming."),
                   QStringLiteral("auto* window = new Window();\n"
                                  "window->setWindowTitle(\"Fluent window\");\n"
                                  "window->setContentWidget(content);\n"
                                  "window->resize(420, 280);\n"
                                  "window->show();"),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Open new window"), parent);
                       QObject::connect(button, &Button::clicked, button, [button]() {
                           // One demo window at a time; re-clicking refocuses it.
                           // zh_CN: 同时只开一个演示窗口；再次点击改为聚焦它。
                           static QPointer<Window> demoWindow;
                           if (demoWindow) {
                               demoWindow->raise();
                               demoWindow->activateWindow();
                               return;
                           }
                           auto* window = new Window();
                           demoWindow = window;
                           window->setAttribute(Qt::WA_DeleteOnClose);
                           window->setWindowTitle(QStringLiteral("Fluent window"));
                           auto* content = new Label(
                               QStringLiteral("A Fluent window with custom chrome."), window);
                           content->setFluentTypography(Typography::FontRole::Body);
                           content->setAlignment(Qt::AlignCenter);
                           window->setContentWidget(content);
                           window->resize(420, 280);
                           window->show();
                       });
                       return button;
                   })
    };
}

} // namespace

QVector<GallerySample> windowingSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("title-bar"))
        return titleBarSamples();
    if (routeId == QStringLiteral("window"))
        return windowSamples();
    return {};
}

} // namespace fluent::gallery
