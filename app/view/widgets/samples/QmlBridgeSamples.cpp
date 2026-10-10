#include "QmlBridgeSamples.h"

#include <FluentQt/QmlBridge.h>
#include <QFile>
#include <QVBoxLayout>

#include "SampleBuilders.h"
#include "components/basicinput/Button.h"
#include "components/status_info/ProgressBar.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "support/logging/Log.h"

namespace fluent::gallery {
namespace {
using fluent::basicinput::Button;
using fluent::qmlbridge::QmlSession;
using fluent::qmlbridge::QmlView;
using fluent::status_info::ProgressBar;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;

QUrl qmlSource(const QString& name)
{
    return QUrl(QStringLiteral("qrc:/app/qml-bridge/") + name);
}

QString qmlAsset(const QString& name)
{
    QFile file(QStringLiteral(":/app/qml-bridge/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARN(QStringLiteral("Missing QmlBridge teaching asset: %1").arg(name));
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString withQml(const QString& cpp, const QString& name)
{
    const auto example = QString::fromUtf8(
        R"example(// main.cpp; build with CMakeLists.txt and keep the QML page in its own file.
#include <FluentQt/FluentQt.h>
#include <FluentQt/QmlBridge.h>
#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

using fluent::basicinput::Button;
using fluent::qmlbridge::QmlSession;
using fluent::qmlbridge::QmlView;
using fluent::status_info::ProgressBar;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;
using fluent::windowing::Window;

QWidget* makePanel(const QUrl& source, QWidget* parent)
{
    auto* panel = new QWidget(parent);
    auto* column = new QVBoxLayout(panel);
    column->setContentsMargins(16, 16, 16, 16);
    column->setSpacing(12);
    column->setAlignment(Qt::AlignTop);
    auto* session = new QmlSession(panel, panel);
    QString error;
    QmlView* view = session->activate(&error) ? session->createView(panel) : nullptr;
    if (!view) {
        column->addWidget(new Label(error.isEmpty() ? "QML is unavailable" : error, panel));
        return panel;
    }
@BODY@
    return panel;
}

int main(int argc, char* argv[])
{
    fluent::prepareHighDpiApplication();
    QmlSession::prepareApplication();
    QApplication application(argc, argv);
    fluent::initializeResources();
    application.setFont(Typography::fontStyle(Typography::FontRole::Body).toQFont());

    Window window;
    window.setWindowTitle("QmlView");
    window.resize(640, 480);
    window.setContentWidget(makePanel(QUrl("qrc:/@NAME@"), &window));
    window.show();
    return application.exec();
}
)example");
    return QString(example)
        .replace(QStringLiteral("@NAME@"), name)
        .replace(QStringLiteral("@BODY@"), cpp);
}

QString qmlResourceSource(const QString& name)
{
    const auto resource = QString::fromUtf8(R"resource(<RCC>
    <qresource prefix="/">
        <file>@NAME@</file>
    </qresource>
</RCC>
)resource");
    return QString(resource).replace(QStringLiteral("@NAME@"), name);
}

QString qmlCmakeSource()
{
    return QString::fromUtf8(R"cmake(cmake_minimum_required(VERSION 3.16)
project(QmlViewExample LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTORCC ON)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
find_package(FluentQt CONFIG REQUIRED COMPONENTS QmlBridge)
add_executable(qml_view_example main.cpp qml.qrc)
target_link_libraries(qml_view_example PRIVATE FluentQt::QmlBridge)
)cmake");
}

QString qmlFileName(const QString& id)
{
    if (id == QStringLiteral("qml-view-theme"))
        return QStringLiteral("Theme.qml");
    if (id == QStringLiteral("qml-view-context"))
        return QStringLiteral("Context.qml");
    if (id == QStringLiteral("qml-view-action"))
        return QStringLiteral("Action.qml");
    return QStringLiteral("Basic.qml");
}

// Preview layout and accessibility names belong to the host.
// zh_CN: 预览布局和无障碍名称由宿主负责，不混入特性教学代码。
class QmlExample final : public QWidget {
public:
    QmlExample(QWidget* parent, const QString& name, int height = 100) : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        body = new QVBoxLayout(this);
        body->setContentsMargins(0, 0, 0, 0);
        body->setSpacing(12);
        body->setAlignment(Qt::AlignTop);
        session = new QmlSession(this, this);
        QString error;
        if (session->activate(&error))
            view = session->createView(this);
        if (!view) {
            body->addWidget(
                new Label(error.isEmpty() ? QStringLiteral("QML is unavailable") : error, this));
            return;
        }
        view->setObjectName(name);
        view->setFixedHeight(height);
        view->setAccessibleName(QStringLiteral("QML preview"));
        body->addWidget(view);
    }

    QVBoxLayout* body = nullptr;
    QmlSession* session = nullptr;
    QmlView* view = nullptr;
};

QString loadSource()
{
    return withQml(QString::fromUtf8(R"(    view->setFixedHeight(100);
    column->addWidget(view);
    view->load(source);)"),
                   QStringLiteral("Basic.qml"));
}

QString themeSource()
{
    return withQml(QString::fromUtf8(R"(    view->setFixedHeight(100);
    column->addWidget(view);
    view->load(source);)"),
                   QStringLiteral("Theme.qml"));
}

QString contextSource()
{
    return withQml(QString::fromUtf8(R"(    auto* input = new LineEdit(panel);
    input->setText("A shared title");
    column->addWidget(input);
    view->setFixedHeight(100);
    column->addWidget(view);
    if (view->setContextObject("widgetInput", input))
        view->load(source);)"),
                   QStringLiteral("Context.qml"));
}

QString actionSource()
{
    return withQml(QString::fromUtf8(R"(    auto* progress = new ProgressBar(panel);
    progress->setRange(0, 100);
    progress->setValue(40);
    column->addWidget(progress);
    view->setFixedHeight(180);
    column->addWidget(view);
    if (view->setContextObject("widgetProgress", progress))
        view->load(source);)"),
                   QStringLiteral("Action.qml"));
}

QString lifecycleSource()
{
    return withQml(QString::fromUtf8(R"(    view->setFixedHeight(100);
    column->addWidget(view);
    auto* actions = new QWidget(panel);
    auto* row = new QHBoxLayout(actions);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(12);
    row->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto* deactivate = new Button("Deactivate", actions);
    auto* reload = new Button("Reload QML", actions);
    row->addWidget(deactivate);
    row->addWidget(reload);
    column->addWidget(actions);
    auto* status = new Label("QML stopped", panel);
    column->addWidget(status);
    QObject::connect(view, &QmlView::statusChanged, status, [status](QmlView::Status state) {
        status->setText((state == QmlView::Ready) ? "QML loaded" : "QML stopped");
    });
    QObject::connect(deactivate, &Button::clicked, session, [session] { session->deactivate(); });
    QObject::connect(reload, &Button::clicked, view, [session, view, source] {
        if (session->activate()) {
            view->unload();
            view->load(source);
        }
    });
    view->load(source);)"),
                   QStringLiteral("Basic.qml"));
}

QString errorSource()
{
    return withQml(QString::fromUtf8(R"(    view->setFixedHeight(100);
    column->addWidget(view);
    auto* actions = new QWidget(panel);
    auto* row = new QHBoxLayout(actions);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(12);
    row->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    auto* missing = new Button("Load missing QML", actions);
    auto* restore = new Button("Restore QML", actions);
    row->addWidget(missing);
    row->addWidget(restore);
    column->addWidget(actions);
    auto* status = new Label("QML stopped", panel);
    status->setWordWrap(true);
    column->addWidget(status);
    QObject::connect(view, &QmlView::statusChanged, status, [view, status](QmlView::Status state) {
        if (state == QmlView::Ready)
            status->setText("QML loaded");
        else if (state == QmlView::Error)
            status->setText(view->lastError());
        else
            status->setText("QML stopped");
    });
    QObject::connect(missing, &Button::clicked, view, [view, source] {
        view->load(source.resolved(QUrl("Missing.qml")));
    });
    QObject::connect(restore, &Button::clicked, view, [view, source] {
        view->load(source);
    });
    view->load(source);)"),
                   QStringLiteral("Basic.qml"));
}

QString keyUsage(const QString& id)
{
    if (id == QStringLiteral("qml-view-load")) {
        return QString::fromUtf8(
            R"(// main() calls QmlSession::prepareApplication() before QApplication.
// panel and the absolute QML URL source are defined in Full example.
auto* session = new QmlSession(panel, panel);
if (!session->activate())
    return panel;
QmlView* view = session->createView(panel);
if (!view)
    return panel;
view->setFixedHeight(100);
view->load(source);)");
    }
    if (id == QStringLiteral("qml-view-theme")) {
        return QString::fromUtf8(R"(// panel, view and source are defined in main.cpp.
// Theme.qml binds its colors and text to fluentTheme.
view->setFixedHeight(100);
view->load(source);)");
    }
    if (id == QStringLiteral("qml-view-context")) {
        return QString::fromUtf8(R"(// panel, view and source are defined in main.cpp.
auto* input = new LineEdit(panel);
input->setText("A shared title");
if (view->setContextObject("widgetInput", input))
    view->load(source);)");
    }
    if (id == QStringLiteral("qml-view-action")) {
        return QString::fromUtf8(R"(// panel, view and source are defined in main.cpp.
auto* progress = new ProgressBar(panel);
progress->setRange(0, 100);
progress->setValue(40);
if (view->setContextObject("widgetProgress", progress))
    view->load(source);)");
    }
    if (id == QStringLiteral("qml-view-lifecycle")) {
        return QString::fromUtf8(
            R"(// session, view, buttons, status and source are defined in Full example.
QObject::connect(view, &QmlView::statusChanged, status, [status](QmlView::Status state) {
    status->setText((state == QmlView::Ready) ? "QML loaded" : "QML stopped");
});
QObject::connect(deactivate, &Button::clicked, session, [session] { session->deactivate(); });
QObject::connect(reload, &Button::clicked, view, [session, view, source] {
    if (session->activate()) {
        view->unload();
        view->load(source);
    }
});
view->load(source);)");
    }
    if (id == QStringLiteral("qml-view-error")) {
        return QString::fromUtf8(R"(// view, buttons, status and source are defined in Full example.
QObject::connect(view, &QmlView::statusChanged, status, [view, status](QmlView::Status state) {
    if (state == QmlView::Ready)
        status->setText("QML loaded");
    else if (state == QmlView::Error)
        status->setText(view->lastError());
    else
        status->setText("QML stopped");
});
QObject::connect(missing, &Button::clicked, view, [view, source] {
    view->load(source.resolved(QUrl("Missing.qml")));
});
QObject::connect(restore, &Button::clicked, view, [view, source] { view->load(source); });
view->load(source);)");
    }
    return {};
}

QVector<GallerySample> qmlViewSamples()
{
    using samples::makeSample;
    QVector<GallerySample> result{
        makeSample(
            QStringLiteral("qml-view-load"), QStringLiteral("Load a QML page"),
            QStringLiteral("Create a QmlView and load one QML file."), loadSource(),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlExample(parent, QStringLiteral("qmlBridgeLoadView"));
                if (example->view)
                    example->view->load(qmlSource(QStringLiteral("Basic.qml")));
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qml-view-theme"), QStringLiteral("Fluent theme tokens"),
            QStringLiteral("Change the preview theme. The QML text and colors follow it."),
            themeSource(),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlExample(parent, QStringLiteral("qmlBridgeThemeView"));
                if (example->view)
                    example->view->load(qmlSource(QStringLiteral("Theme.qml")));
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qml-view-context"), QStringLiteral("Widgets to QML"),
            QStringLiteral("Edit the Widgets text field. A QML binding reads its text property."),
            contextSource(),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlExample(parent, QStringLiteral("qmlBridgeContextView"));
                if (!example->view)
                    return example;
                auto* input = new LineEdit(example);
                input->setObjectName(QStringLiteral("qmlBridgeTitleEdit"));
                input->setAccessibleName(QStringLiteral("Widgets text"));
                input->setText(QStringLiteral("A shared title"));
                example->body->insertWidget(0, input);
                if (example->view->setContextObject(QStringLiteral("widgetInput"), input))
                    example->view->load(qmlSource(QStringLiteral("Context.qml")));
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qml-view-action"), QStringLiteral("QML to Widgets"),
            QStringLiteral("Press the QML button. It updates the Widgets progress bar."),
            actionSource(),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlExample(parent, QStringLiteral("qmlBridgeActionView"), 180);
                if (!example->view)
                    return example;
                auto* progress = new ProgressBar(example);
                progress->setObjectName(QStringLiteral("qmlBridgeProgressIndicator"));
                progress->setAccessibleName(QStringLiteral("Widgets progress"));
                progress->setRange(0, 100);
                progress->setValue(40);
                example->body->insertWidget(0, progress);
                if (example->view->setContextObject(QStringLiteral("widgetProgress"), progress))
                    example->view->load(qmlSource(QStringLiteral("Action.qml")));
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qml-view-lifecycle"), QStringLiteral("Deactivate and reload"),
            QStringLiteral(
                "Deactivate stops this QML page. Reload activates its session and loads it again."),
            lifecycleSource(),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlExample(parent, QStringLiteral("qmlBridgeLifecycleView"));
                if (!example->view)
                    return example;
                auto* actions = samples::horizontalGroup(example);
                auto* deactivate = new Button(QStringLiteral("Deactivate"), actions);
                deactivate->setObjectName(QStringLiteral("qmlBridgeDeactivate"));
                auto* reload = new Button(QStringLiteral("Reload QML"), actions);
                reload->setObjectName(QStringLiteral("qmlBridgeReload"));
                actions->layout()->addWidget(deactivate);
                actions->layout()->addWidget(reload);
                example->body->addWidget(actions);
                auto* status = new Label(QStringLiteral("QML stopped"), example);
                status->setObjectName(QStringLiteral("qmlBridgeLifecycleStatus"));
                example->body->addWidget(status);
                auto* view = example->view;
                auto* session = example->session;
                QObject::connect(
                    view, &QmlView::statusChanged, status, [status](QmlView::Status state) {
                        status->setText((state == QmlView::Ready) ? QStringLiteral("QML loaded")
                                                                  : QStringLiteral("QML stopped"));
                    });
                QObject::connect(deactivate, &Button::clicked, session,
                                 [session] { session->deactivate(); });
                QObject::connect(reload, &Button::clicked, view, [session, view] {
                    if (session->activate()) {
                        view->unload();
                        view->load(qmlSource(QStringLiteral("Basic.qml")));
                    }
                });
                view->load(qmlSource(QStringLiteral("Basic.qml")));
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qml-view-error"), QStringLiteral("Handle a load error"),
            QStringLiteral(
                "Load a missing file to see the error, then restore the valid QML page."),
            errorSource(),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlExample(parent, QStringLiteral("qmlBridgeErrorView"));
                if (!example->view)
                    return example;
                auto* actions = samples::horizontalGroup(example);
                auto* missing = new Button(QStringLiteral("Load missing QML"), actions);
                missing->setObjectName(QStringLiteral("qmlBridgeLoadMissing"));
                auto* restore = new Button(QStringLiteral("Restore QML"), actions);
                restore->setObjectName(QStringLiteral("qmlBridgeRestore"));
                actions->layout()->addWidget(missing);
                actions->layout()->addWidget(restore);
                example->body->addWidget(actions);
                auto* status = new Label(QStringLiteral("QML stopped"), example);
                status->setObjectName(QStringLiteral("qmlBridgeErrorStatus"));
                status->setWordWrap(true);
                example->body->addWidget(status);
                auto* view = example->view;
                QObject::connect(view, &QmlView::statusChanged, status,
                                 [view, status](QmlView::Status state) {
                                     if (state == QmlView::Ready)
                                         status->setText(QStringLiteral("QML loaded"));
                                     else if (state == QmlView::Error)
                                         status->setText(view->lastError());
                                     else
                                         status->setText(QStringLiteral("QML stopped"));
                                 });
                QObject::connect(missing, &Button::clicked, view,
                                 [view] { view->load(qmlSource(QStringLiteral("Missing.qml"))); });
                QObject::connect(restore, &Button::clicked, view,
                                 [view] { view->load(qmlSource(QStringLiteral("Basic.qml"))); });
                view->load(qmlSource(QStringLiteral("Basic.qml")));
                return example;
            },
            true)};
    for (auto& sample : result) {
        const auto name = qmlFileName(sample.id);
        sample.usageSnippet = keyUsage(sample.id);
        sample.sourceFiles = {
            {QStringLiteral("main.cpp"), QStringLiteral("cpp"), sample.codeSnippet,
             sample.usageSnippet},
            {name, QStringLiteral("qml"), qmlAsset(name), {}},
            {QStringLiteral("qml.qrc"), QStringLiteral("xml"), qmlResourceSource(name), {}},
            {QStringLiteral("CMakeLists.txt"), QStringLiteral("cmake"), qmlCmakeSource(), {}}};
    }
    return result;
}
} // namespace

QVector<GallerySample> qmlBridgeSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("qml-view"))
        return qmlViewSamples();
    return {};
}

} // namespace fluent::gallery
