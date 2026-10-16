#include "compatibility/private/quick/QuickFeatures_p.h"
#include <gtest/gtest.h>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQmlEngine>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTest>
#include <QVBoxLayout>
#include "QmlBridgeTestModel.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/plugins/PluginManager.h"
#include "components/qmlbridge/QmlPlugin.h"
#include "components/qmlbridge/QmlSession.h"
#include "components/qmlbridge/QmlTheme.h"
#include "components/qmlbridge/QmlView.h"

using namespace fluent;
namespace {
QUrl source(const char* filename = "Bridge.qml")
{
    return QUrl::fromLocalFile(
        QStringLiteral(FLUENT_QT_QML_TEST_SOURCE_DIR) + QLatin1Char('/') + QString::fromLatin1(filename));
}
} // namespace

TEST(QmlGraphicsApiTest, StartupSelectionProbe)
{
    const auto scenario = qgetenv("FLUENTQT_QML_GRAPHICS_PROBE");
    if (scenario.isEmpty())
        GTEST_SKIP() << "Run by Contract_BackendSelectionUsesFreshProcesses.";
    using Session = qmlbridge::QmlSession;
    using Api = Session::GraphicsApi;
    Api nativeApi = Api::OpenGL;
#if FLUENT_QUICK_USES_RHI && defined(Q_OS_MACOS)
    nativeApi = Api::Metal;
#elif FLUENT_QUICK_USES_RHI && defined(Q_OS_WIN)
    nativeApi = Api::Direct3D11;
#endif
    const bool software = scenario == "software" || scenario == "software-alias";
    const bool native = scenario == "native" || scenario == "environment";
    const auto expected = native ? nativeApi : Api::OpenGL;
    if (scenario == "legacy")
        Session::prepareApplication();
    else if (scenario == "native" || scenario == "opengl")
        Session::prepareApplication(expected);
    else if (scenario == "environment")
        Session::prepareApplication(Api::Automatic);

    EXPECT_TRUE(Session::isGraphicsApiCompatible(Api::Automatic));
    EXPECT_TRUE(Session::isGraphicsApiCompatible(expected));
    EXPECT_EQ(Session::isGraphicsApiCompatible(Api::OpenGL), software || expected == Api::OpenGL);
    if (software) {
        EXPECT_TRUE(Session::isGraphicsApiCompatible(Api::Metal));
        EXPECT_TRUE(Session::isGraphicsApiCompatible(Api::Direct3D11));
    }
    const auto sceneGraphBackend = QQuickWindow::sceneGraphBackend();
#if FLUENT_QUICK_USES_RHI
    const auto selectedApi = QQuickWindow::graphicsApi();
#endif
    for (int invalid : {-1, 99}) {
        const auto api = static_cast<Api>(invalid);
        EXPECT_FALSE(Session::isGraphicsApiCompatible(api));
        Session::prepareApplication(api);
        EXPECT_EQ(QQuickWindow::sceneGraphBackend(), sceneGraphBackend);
#if FLUENT_QUICK_USES_RHI
        EXPECT_EQ(QQuickWindow::graphicsApi(), selectedApi);
#endif
    }
    // No Quick surface exists yet: unsupported texture mode must fail before
    // a root can be moved away from its native presentation.
    // zh_CN: 尚无 Quick 表面；须在根对象离开原生显示前拒绝不支持的纹理模式。
    Session session;
    ASSERT_TRUE(session.activate());
    QWidget owner;
    auto* view = session.createView(&owner);
    ASSERT_NE(view, nullptr);
    QSignalSpy changed(view, &qmlbridge::QmlView::textureRenderingEnabledChanged);
    const bool textureSupported = !software && expected == Api::OpenGL;
    EXPECT_EQ(view->setTextureRenderingEnabled(true), textureSupported);
    EXPECT_EQ(view->isTextureRenderingEnabled(), textureSupported);
    EXPECT_EQ(changed.count(), textureSupported ? 1 : 0);
    EXPECT_EQ(view->findChild<QQuickWidget*>(), nullptr);
    EXPECT_EQ(view->rootObject(), nullptr);
    EXPECT_EQ(view->status(), qmlbridge::QmlView::Unloaded);
}

TEST(QmlGraphicsApiTest, Contract_BackendSelectionUsesFreshProcesses)
{
    for (const QString& scenario : {"legacy", "opengl", "native", "environment", "software", "software-alias"}) {
        SCOPED_TRACE(scenario.toStdString());
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.remove("QT_QUICK_BACKEND");
        environment.remove("QMLSCENE_DEVICE");
        environment.remove("QSG_RHI_BACKEND");
        environment.insert("FLUENTQT_QML_GRAPHICS_PROBE", scenario);
        if (scenario == "software")
            environment.insert("QT_QUICK_BACKEND", "software");
        else if (scenario == "software-alias")
            environment.insert("QMLSCENE_DEVICE", "software");
        else if (scenario == "environment") {
#ifdef Q_OS_MACOS
            environment.insert("QSG_RHI_BACKEND", "metal");
#elif defined(Q_OS_WIN)
            environment.insert("QSG_RHI_BACKEND", "d3d11");
#else
            environment.insert("QSG_RHI_BACKEND", "opengl");
#endif
        }
        QProcess process;
        process.setProcessEnvironment(environment);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(QCoreApplication::applicationFilePath(),
            {"--gtest_filter=QmlGraphicsApiTest.StartupSelectionProbe"});
        ASSERT_TRUE(process.waitForStarted(10000));
        ASSERT_TRUE(process.waitForFinished(30000));
        EXPECT_EQ(process.exitStatus(), QProcess::NormalExit);
        EXPECT_EQ(process.exitCode(), 0) << process.readAll().toStdString();
    }
}

TEST(QmlBridgeTest, Contract_GenericFactoryCreatesOnlyActiveCallerOwnedHosts)
{
    qmlbridge::QmlPlugin plugin;
    plugins::PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&plugin));
    EXPECT_EQ(manager.createWidget(plugin.pluginId(), QStringLiteral("qml-view")), nullptr);
    ASSERT_TRUE(manager.activate(plugin.pluginId()));
    QWidget owner;
    auto* view = qobject_cast<qmlbridge::QmlView*>(
        manager.createWidget(plugin.pluginId(), QStringLiteral("qml-view"), &owner));
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(view->parentWidget(), &owner);
    EXPECT_TRUE(view->isActive());
    EXPECT_EQ(view->status(), qmlbridge::QmlView::Unloaded);
    EXPECT_EQ(view->findChild<QQuickWidget*>(), nullptr);
    ASSERT_TRUE(manager.deactivateAll());
    EXPECT_FALSE(view->isActive());
    EXPECT_EQ(view->parentWidget(), &owner);
}

TEST(QmlBridgeTest, Contract_RapidReloadOnlyPresentsCurrentSceneAfterLayout)
{
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    qmlbridge::QmlSession session(&host, &host);
    ASSERT_TRUE(session.activate());
    auto* view = session.createView(&host);
    ASSERT_NE(view, nullptr);
    layout->addWidget(view);
    host.resize(620, 380);
    host.show();
    QApplication::processEvents();
    QPointer<QQuickWidget> latest;
    for (int iteration = 0; iteration < 12; ++iteration) {
        view->unload();
        ASSERT_TRUE(view->load(source()));
        const auto scenes = view->findChildren<QQuickWidget*>();
        ASSERT_FALSE(scenes.isEmpty());
        latest = scenes.last();
        EXPECT_TRUE(latest->isHidden());
        for (auto* scene : scenes)
            EXPECT_TRUE(scene->isHidden());
    }
    ASSERT_TRUE(QTest::qWaitFor([&] { return latest && latest->isVisible(); }));
    EXPECT_EQ(latest->geometry(), view->contentsRect());
    EXPECT_EQ(view->status(), qmlbridge::QmlView::Ready);
    for (auto* scene : view->findChildren<QQuickWidget*>()) {
        if (scene != latest)
            EXPECT_TRUE(scene->isHidden());
    }
}

TEST(QmlBridgeTest, Contract_SharedModelUpdatesBothDirectionsWithoutChangingOwnership)
{
    QObject modelOwner;
    QmlBridgeTestModel model(&modelOwner);
    QWidget owner;
    qmlbridge::QmlSession session(&owner, &owner);
    ASSERT_TRUE(session.activate());
    auto* view = session.createView(&owner);
    ASSERT_NE(view, nullptr);
    ASSERT_TRUE(view->setContextObject(QStringLiteral("bridgeModel"), &model));
    QString error;
    ASSERT_TRUE(view->load(source(), &error)) << error.toStdString();
    ASSERT_TRUE(QTest::qWaitFor([&] { return view->status() == qmlbridge::QmlView::Ready; }));
    auto* root = view->rootObject();
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->property("observedTitle").toString(), model.title());
    model.setTitle(QStringLiteral("Changed in Widgets"));
    EXPECT_EQ(root->property("observedTitle").toString(), model.title());
    ASSERT_TRUE(QMetaObject::invokeMethod(root, "advance"));
    EXPECT_EQ(model.progress(), 35);
    ASSERT_TRUE(QMetaObject::invokeMethod(root, "rename"));
    EXPECT_EQ(model.title(), QStringLiteral("Changed in QML"));
    EXPECT_EQ(model.parent(), &modelOwner);
    EXPECT_EQ(QQmlEngine::objectOwnership(&model), QQmlEngine::CppOwnership);
    const QPointer<QObject> originalRoot(root);
    QSignalSpy changed(view, &qmlbridge::QmlView::sourceChanged);
    ASSERT_TRUE(view->load(source()));
    EXPECT_EQ(view->rootObject(), originalRoot);
    EXPECT_EQ(changed.count(), 0);
}

TEST(QmlBridgeTest, Contract_DeactivationDestroysSceneAndEngineButRetainsViewAndModel)
{
    QmlBridgeTestModel model;
    qmlbridge::QmlView view;
    auto session = std::make_unique<qmlbridge::QmlSession>();
    QSignalSpy active(session.get(), &qmlbridge::QmlSession::activeChanged);
    ASSERT_TRUE(session->activate());
    ASSERT_TRUE(session->activate());
    EXPECT_EQ(active.count(), 1);
    ASSERT_TRUE(session->attachView(&view));
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), &model));
    ASSERT_TRUE(view.load(source()));
    QPointer<QObject> root(view.rootObject());
    auto* quick = view.findChild<QQuickWidget*>();
    ASSERT_NE(quick, nullptr);
    QPointer<QQmlEngine> engine(quick->engine());
    ASSERT_TRUE(session->deactivate());
    ASSERT_TRUE(QTest::qWaitFor([&] { return root.isNull(); }));
    EXPECT_TRUE(engine.isNull());
    EXPECT_EQ(view.findChild<QQuickWidget*>(), nullptr);
    EXPECT_TRUE(view.source().isEmpty());
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Unloaded);
    EXPECT_FALSE(view.load(source()));
    ASSERT_TRUE(session->activate());
    EXPECT_EQ(view.rootObject(), nullptr);
    ASSERT_TRUE(view.load(source()));
    root = view.rootObject();
    session.reset();
    ASSERT_TRUE(QTest::qWaitFor([&] { return root.isNull(); }));
    EXPECT_FALSE(view.isActive());
    EXPECT_EQ(model.progress(), 25);
    EXPECT_EQ(model.parent(), nullptr);
}

TEST(QmlBridgeTest, Contract_DestroyingBorrowedModelUnloadsItsScene)
{
    qmlbridge::QmlView view;
    qmlbridge::QmlSession session;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    auto model = std::make_unique<QmlBridgeTestModel>();
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), model.get()));
    ASSERT_TRUE(view.load(source()));
    QPointer<QObject> root(view.rootObject());
    model.reset();
    ASSERT_TRUE(QTest::qWaitFor([&] { return root.isNull(); }));
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Unloaded);
    EXPECT_TRUE(view.isActive());
    QmlBridgeTestModel replacement;
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), &replacement));
    ASSERT_TRUE(view.load(source()));
}

TEST(QmlBridgeTest, Contract_FailedLoadAndContextChangesHaveActionableErrorsAndRecover)
{
    qmlbridge::QmlView view;
    QmlBridgeTestModel model;
    EXPECT_FALSE(view.load(source()));
    qmlbridge::QmlSession session;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    EXPECT_FALSE(view.setContextObject(QStringLiteral("fluentTheme"), &model));
    EXPECT_FALSE(view.setContextObject(QStringLiteral("invalid-name"), &model));
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), &model));
    EXPECT_FALSE(view.load(QUrl(QStringLiteral("relative.qml"))));
    QSignalSpy errors(&view, &qmlbridge::QmlView::errorOccurred);
    QString error;
    EXPECT_FALSE(view.load(source("Broken.qml"), &error));
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Error);
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(view.errors().isEmpty());
    EXPECT_GE(errors.count(), 1);
    view.unload();
    ASSERT_TRUE(view.load(source()));
    EXPECT_TRUE(view.errors().isEmpty());
    EXPECT_FALSE(view.setContextObject(QStringLiteral("bridgeModel"), nullptr));
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Ready);
    view.unload();
    EXPECT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), nullptr));
}

TEST(QmlBridgeTest, Contract_AViewCannotJoinTwoPluginsOrOwnItsSession)
{
    qmlbridge::QmlView view;
    qmlbridge::QmlSession first;
    qmlbridge::QmlSession second;
    ASSERT_TRUE(first.activate());
    ASSERT_TRUE(second.activate());
    ASSERT_TRUE(first.attachView(&view));
    EXPECT_FALSE(second.attachView(&view));
    qmlbridge::QmlView independent;
    qmlbridge::QmlSession child(nullptr, &independent);
    ASSERT_TRUE(child.activate());
    EXPECT_FALSE(child.attachView(&independent));
}

TEST(QmlBridgeTest, Contract_ThemeAndMotionTokensFollowHostPolicy)
{
    qmlbridge::QmlView view;
    qmlbridge::QmlSession session;
    QmlBridgeTestModel model;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), &model));
    view.setProperty("fluentThemeOverride", int(FluentElement::Light));
    view.onThemeUpdated();
    ASSERT_TRUE(view.load(source()));
    const QPointer<QObject> root(view.rootObject());
    ASSERT_FALSE(root.isNull());
    const QColor initialSurface = root->property("observedSurfaceAlt").value<QColor>();
    EXPECT_EQ(initialSurface, view.themeColorsRef().bgLayerAlt);
    auto* theme = qobject_cast<qmlbridge::QmlTheme*>(view.themeObject());
    ASSERT_NE(theme, nullptr);
    EXPECT_EQ(theme->accent(), view.themeColorsRef().accentDefault);
    view.setProperty("fluentThemeOverride", int(FluentElement::Dark));
    QSignalSpy changed(theme, &qmlbridge::QmlTheme::changed);
    view.onThemeUpdated();
    EXPECT_TRUE(theme->isDark());
    EXPECT_EQ(theme->background(), view.themeColorsRef().bgCanvas);
    EXPECT_EQ(root->property("observedSurfaceAlt").value<QColor>(), view.themeColorsRef().bgLayerAlt);
    EXPECT_NE(root->property("observedSurfaceAlt").value<QColor>(), initialSurface);
    EXPECT_EQ(view.rootObject(), root);
    EXPECT_EQ(changed.count(), 1);
    auto& motion = MotionPolicy::instance();
    const auto original = motion.mode();
    motion.setMode(MotionPolicy::Mode::Disabled);
    EXPECT_FALSE(theme->areAnimationsEnabled());
    EXPECT_EQ(theme->transitionDuration(), 0);
    motion.setMode(MotionPolicy::Mode::Reduced);
    EXPECT_EQ(theme->transitionDuration(), 50);
    motion.setMode(original);
}

TEST(QmlBridgeTest, Contract_StopDuringQmlCompletionDefersSafeSceneDestruction)
{
    qmlbridge::QmlView view;
    qmlbridge::QmlSession session;
    QmlBridgeTestModel model;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), &model));
    QObject::connect(&model, &QmlBridgeTestModel::stopRequested, &session, [&] { EXPECT_TRUE(session.deactivate()); });
    EXPECT_FALSE(view.load(source("Stop.qml")));
    EXPECT_FALSE(session.isActive());
    EXPECT_FALSE(view.isActive());
    ASSERT_TRUE(QTest::qWaitFor([&] { return view.rootObject() == nullptr; }));
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Unloaded);
}

TEST(QmlBridgeTest, Contract_ItemModelsRemainSharedAndBorrowed)
{
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("First")));
    qmlbridge::QmlView view;
    qmlbridge::QmlSession session;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    ASSERT_TRUE(view.setContextObject(QStringLiteral("listModel"), &model));
    view.resize(320, 200);
    view.show();
    ASSERT_TRUE(view.load(source("List.qml")));
    ASSERT_NE(view.rootObject(), nullptr);
    EXPECT_EQ(view.rootObject()->property("observedCount").toInt(), 1);
    model.appendRow(new QStandardItem(QStringLiteral("Second")));
    ASSERT_TRUE(QTest::qWaitFor([&] { return view.rootObject()->property("observedCount").toInt() == 2; }));
    ASSERT_TRUE(session.deactivate());
    EXPECT_EQ(model.rowCount(), 2);
    EXPECT_EQ(model.parent(), nullptr);
}

TEST(QmlBridgeTest, Contract_GraphicsErrorsPersistUntilExplicitReload)
{
    qmlbridge::QmlView view;
    qmlbridge::QmlSession session;
    QmlBridgeTestModel model;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), &model));
    ASSERT_TRUE(view.load(source()));
    auto* quick = view.findChild<QQuickWidget*>();
    ASSERT_NE(quick, nullptr);
    quick->sceneGraphError(QQuickWindow::ContextNotAvailable, QStringLiteral("Graphics unavailable"));
    quick->statusChanged(QQuickWidget::Ready);
    ASSERT_TRUE(QTest::qWaitFor([&] { return view.status() == qmlbridge::QmlView::Error; }));
    QCoreApplication::processEvents();
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Error);
    EXPECT_TRUE(view.lastError().contains(QStringLiteral("Graphics unavailable")));
    ASSERT_TRUE(view.load(source()));
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Ready);
    EXPECT_TRUE(view.errors().isEmpty());
}

TEST(QmlBridgeTest, Contract_ReplacingAnErrorSceneDoesNotEmitItsStaleDiagnostic)
{
    qmlbridge::QmlView view;
    qmlbridge::QmlSession session;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    QStringList received;
    QObject::connect(&view, &qmlbridge::QmlView::errorOccurred, &view, [&](const QString& message) {
        received.append(message);
    });
    bool replaced = false;
    QObject::connect(&view, &qmlbridge::QmlView::errorsChanged, &view, [&] {
        if (!replaced && view.status() == qmlbridge::QmlView::Error) {
            replaced = true;
            EXPECT_FALSE(view.load(source("OtherBroken.qml")));
        }
    });
    EXPECT_FALSE(view.load(source("Broken.qml")));
    ASSERT_TRUE(replaced);
    ASSERT_EQ(received.size(), 1);
    EXPECT_TRUE(received.first().contains(QStringLiteral("anotherInvalidProperty")));
    EXPECT_EQ(view.source(), source("OtherBroken.qml"));
}

TEST(QmlBridgeTest, Contract_GraphicsErrorCallbackCanRecoverWithoutAStaleErrorStatus)
{
    qmlbridge::QmlView view;
    qmlbridge::QmlSession session;
    QmlBridgeTestModel model;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), &model));
    ASSERT_TRUE(view.load(source()));
    // Qt 5's spy resolves the unqualified parameter name written by moc.
    qRegisterMetaType<qmlbridge::QmlView::Status>("Status");
    QSignalSpy states(&view, &qmlbridge::QmlView::statusChanged);
    ASSERT_TRUE(states.isValid());
    QObject::connect(&view, &qmlbridge::QmlView::errorOccurred, &view, [&](const QString&) {
        ASSERT_TRUE(view.load(source()));
    });
    auto* quick = view.findChild<QQuickWidget*>();
    ASSERT_NE(quick, nullptr);
    quick->sceneGraphError(QQuickWindow::ContextNotAvailable, QStringLiteral("Recover graphics"));
    QCoreApplication::processEvents();
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Ready);
    ASSERT_FALSE(states.isEmpty());
    EXPECT_EQ(states.last().first().toInt(), int(qmlbridge::QmlView::Ready));
}

TEST(QmlBridgeTest, Contract_StopFromRunningQmlCallbackKeepsTheEngineAliveUntilItReturns)
{
    qmlbridge::QmlView view;
    qmlbridge::QmlSession session;
    QmlBridgeTestModel model;
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachView(&view));
    ASSERT_TRUE(view.setContextObject(QStringLiteral("bridgeModel"), &model));
    ASSERT_TRUE(view.load(source()));
    QPointer<QObject> root(view.rootObject());
    QPointer<QQmlEngine> engine(view.findChild<QQuickWidget*>()->engine());
    QObject::connect(&model, &QmlBridgeTestModel::stopRequested, &session, [&] {
        EXPECT_TRUE(session.deactivate());
        EXPECT_FALSE(view.isActive());
        EXPECT_EQ(view.rootObject(), nullptr);
        EXPECT_FALSE(root.isNull());
        EXPECT_FALSE(engine.isNull());
    });
    ASSERT_TRUE(QMetaObject::invokeMethod(root, "stopBridge"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return root.isNull() && engine.isNull(); }));
    EXPECT_EQ(view.status(), qmlbridge::QmlView::Unloaded);
}
