#include <gtest/gtest.h>
#include <QOpenGLContext>
#include <QPointer>
#include <QSignalSpy>
#include <QApplication>
#include <QScopeGuard>
#include "components/basicinput/Button.h"
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/plugins/PluginManager.h"
#include "components/spatial/SpatialPlugin.h"
#include "components/spatial/SpatialSession.h"
#include "components/spatial/SpatialView.h"
#include "components/spatial/ParticleLayer.h"
#include "components/layout/Card.h"
#include "components/layout/ParticleBackdrop.h"
#include "components/windowing/Window.h"

using namespace fluent;

TEST(SpatialPluginTest, Contract_RegistrationUsesGenericFactoryWithoutCreatingAContext)
{
    spatial::SpatialPlugin extension;
    plugins::PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&extension));
    EXPECT_EQ(manager.metadata(spatial::SpatialPlugin::pluginId()).id,
              QStringLiteral("fluentqt.spatial"));
    EXPECT_EQ(
        manager.createWidget(spatial::SpatialPlugin::pluginId(), QStringLiteral("spatial-view")),
        nullptr);
    ASSERT_TRUE(manager.activate(spatial::SpatialPlugin::pluginId()));
    EXPECT_TRUE(extension.isActive());
    EXPECT_EQ(QOpenGLContext::currentContext(), nullptr);
    QWidget owner;
    auto* view = qobject_cast<spatial::SpatialView*>(manager.createWidget(
        spatial::SpatialPlugin::pluginId(), QStringLiteral("spatial-view"), &owner));
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(view->parentWidget(), &owner);
    EXPECT_EQ(QOpenGLContext::currentContext(), nullptr);
    ASSERT_TRUE(manager.deactivateAll());
    EXPECT_FALSE(view->isSpatialEnabled());
    EXPECT_EQ(view->renderMode(), spatial::SpatialView::RenderMode::Raster);
    EXPECT_EQ(view->parentWidget(), &owner);
}

TEST(SpatialPluginTest, Contract_SessionStopsViewsAndRestoresRenderOptionsWithoutEnablingDepth)
{
    spatial::SpatialView view;
    layout::Card card;
    spatial::SpatialSession session;
    QSignalSpy active(&session, &spatial::SpatialSession::activeChanged);
    EXPECT_FALSE(session.attachView(&view));
    EXPECT_EQ(session.createView(), nullptr);
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.activate());
    EXPECT_EQ(active.count(), 1);
    ASSERT_TRUE(session.attachView(&view));
    ASSERT_TRUE(session.attachView(&view));
    ASSERT_NE(view.addWidget(&card), nullptr);
    view.setRenderMode(spatial::SpatialView::RenderMode::Auto);
    ASSERT_TRUE(session.deactivate());
    EXPECT_FALSE(view.isSpatialEnabled());
    EXPECT_EQ(view.renderMode(), spatial::SpatialView::RenderMode::Raster);
    EXPECT_EQ(view.itemCount(), 1);
    ASSERT_TRUE(session.activate());
    EXPECT_EQ(view.renderMode(), spatial::SpatialView::RenderMode::Auto);
    EXPECT_FALSE(view.isSpatialEnabled());
    EXPECT_EQ(view.itemCount(), 1);
    ASSERT_TRUE(session.deactivate());
    ASSERT_TRUE(session.deactivate());
    EXPECT_EQ(active.count(), 4);
}

TEST(SpatialPluginTest, Contract_DestroyedOutputsAreGuardedAndAdaptersRemainCallerOwned)
{
    layout::ParticleBackdrop source;
    QPointer<spatial::ParticleLayer> layer;
    {
        spatial::SpatialSession session;
        EXPECT_EQ(session.createParticleLayer(&source), nullptr);
        ASSERT_TRUE(session.activate());
        auto* view = session.createView();
        ASSERT_NE(view, nullptr);
        delete view;
        layer = session.createParticleLayer(&source);
        ASSERT_FALSE(layer.isNull());
        EXPECT_EQ(layer->backdrop(), &source);
        EXPECT_EQ(layer->parent(), nullptr);
        EXPECT_EQ(QOpenGLContext::currentContext(), nullptr);
        ASSERT_TRUE(session.deactivate());
        EXPECT_FALSE(layer->isActive());
    }
    ASSERT_FALSE(layer.isNull());
    EXPECT_EQ(layer->backdrop(), &source);
    delete layer.data();
}

TEST(SpatialPluginTest, Contract_ParentedSessionAndBorrowedProductsTeardownSafely)
{
    const auto previousMotion = MotionPolicy::instance().mode();
    const auto restoreMotion =
        qScopeGuard([&] { MotionPolicy::instance().setMode(previousMotion); });
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    for (bool nested : {false, true}) {
        SCOPED_TRACE(nested);
        layout::Card borrowed;
        basicinput::Button borrowedButton(QStringLiteral("Borrowed"), &borrowed);
        QPointer<layout::Card> borrowedLifetime(&borrowed);
        auto* owner = new QWidget;
        auto* host = nested ? new QWidget(owner) : owner;
        auto* session = new spatial::SpatialSession(host, host);
        ASSERT_TRUE(session->activate());
        QPointer<spatial::SpatialView> view = session->createView(host);
        ASSERT_FALSE(view.isNull());
        view->setRenderMode(spatial::SpatialView::RenderMode::Raster);
        QPointer<layout::Card> owned = new layout::Card;
        new basicinput::Button(QStringLiteral("Owned"), owned);
        auto* originalParent = new QWidget(host);
        QPointer<layout::Card> reparented = new layout::Card(originalParent);
        new basicinput::Button(QStringLiteral("Reparented"), reparented);
        QWidget externalParent;
        QPointer<layout::Card> external = new layout::Card(&externalParent);
        new basicinput::Button(QStringLiteral("External owner"), external);
        ASSERT_NE(view->addWidget(owned, WidgetOwnership::Owned), nullptr);
        ASSERT_NE(view->addWidget(&borrowed), nullptr);
        ASSERT_NE(view->addWidget(reparented, WidgetOwnership::Reparented), nullptr);
        ASSERT_NE(view->addWidget(external, WidgetOwnership::Reparented), nullptr);
        view->setSpatialEnabled(true);
        ASSERT_TRUE(view->isSpatialEnabled());
        owner->show();
        QApplication::processEvents();
        // Unexpected parent deletion must not restore cards into a dying focus chain.
        delete owner;
        EXPECT_TRUE(view.isNull());
        EXPECT_TRUE(owned.isNull());
        EXPECT_TRUE(reparented.isNull());
        ASSERT_FALSE(external.isNull());
        EXPECT_EQ(external->parentWidget(), &externalParent);
        EXPECT_FALSE(borrowedLifetime.isNull());
        EXPECT_EQ(borrowed.parentWidget(), nullptr);
        EXPECT_EQ(borrowed.graphicsProxyWidget(), nullptr);
    }
}

TEST(SpatialPluginTest, Contract_SessionDeletionRestoresSurvivingFocusedContent)
{
    QWidget host;
    auto* session = new spatial::SpatialSession(&host);
    ASSERT_TRUE(session->activate());
    auto* view = session->createView(&host);
    ASSERT_NE(view, nullptr);
    view->setRenderMode(spatial::SpatialView::RenderMode::Raster);
    layout::Card borrowed;
    basicinput::Button button(QStringLiteral("Keep using"), &borrowed);
    ASSERT_NE(view->addWidget(&borrowed), nullptr);
    host.show();
    // This lifetime contract tests Qt's focus chain, independent of whether a
    // window manager allows the test process to request native activation.
    QT_WARNING_PUSH
    QT_WARNING_DISABLE_DEPRECATED
    QApplication::setActiveWindow(&host);
    QT_WARNING_POP
    ASSERT_EQ(QApplication::activeWindow(), &host);
    delete session;
    EXPECT_FALSE(view->isSpatialEnabled());
    EXPECT_EQ(view->renderMode(), spatial::SpatialView::RenderMode::Raster);
    EXPECT_EQ(borrowed.graphicsProxyWidget(), nullptr);
    EXPECT_TRUE(view->isAncestorOf(&borrowed));
    button.setFocus();
    EXPECT_TRUE(button.hasFocus());
    delete view;
    EXPECT_EQ(borrowed.parentWidget(), nullptr);
}

TEST(SpatialPluginTest, Contract_ReparentedAncestorsDoNotSuppressSurvivingViewCleanup)
{
    auto* oldHost = new QWidget;
    QWidget survivingHost;
    auto* session = new spatial::SpatialSession(oldHost, oldHost);
    ASSERT_TRUE(session->activate());
    auto* panel = new QWidget(oldHost);
    auto* view = session->createView(panel);
    ASSERT_NE(view, nullptr);
    view->setRenderMode(spatial::SpatialView::RenderMode::Raster);
    QPointer<layout::Card> card = new layout::Card;
    new basicinput::Button(QStringLiteral("Moved"), card);
    ASSERT_NE(view->addWidget(card, WidgetOwnership::Owned), nullptr);
    panel->setParent(&survivingHost);
    // ParentChange must disconnect the old ancestor even without an event-loop turn.
    delete oldHost;
    EXPECT_FALSE(card.isNull());
    EXPECT_FALSE(view->isSpatialEnabled());
    EXPECT_TRUE(view->isAncestorOf(card));
    EXPECT_EQ(card->graphicsProxyWidget(), nullptr);
}

TEST(SpatialPluginTest, Contract_TrackingRejectsAncestorOwnersAndWindowPreparationIsGated)
{
    spatial::SpatialView view;
    QObject intermediate(&view);
    spatial::SpatialSession invalid(nullptr, &intermediate);
    ASSERT_TRUE(invalid.activate());
    EXPECT_FALSE(invalid.attachView(&view));
    windowing::Window window;
    spatial::SpatialSession session(&window);
    EXPECT_FALSE(session.prepareWindow(&window));
    ASSERT_TRUE(session.activate());
    EXPECT_EQ(QOpenGLContext::currentContext(), nullptr);
    EXPECT_EQ(session.prepareWindow(&window), session.prepareWindow(&window));
    window.show();
    EXPECT_FALSE(session.prepareWindow(&window));
    EXPECT_EQ(QOpenGLContext::currentContext(), nullptr);
}

TEST(SpatialPluginTest, Contract_DuplicateSpatialPluginsRespectManagerIdentity)
{
    spatial::SpatialPlugin first;
    spatial::SpatialPlugin second;
    plugins::PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&first));
    QString error;
    EXPECT_FALSE(manager.registerPlugin(&second, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_EQ(manager.plugin(spatial::SpatialPlugin::pluginId()), &first);
}

TEST(SpatialPluginTest, Contract_SessionDeletionInLifecycleSignalsKeepsManagerAlive)
{
    for (bool deleteWhenActive : {true, false}) {
        QPointer<spatial::SpatialSession> session = new spatial::SpatialSession;
        QObject receiver;
        if (!deleteWhenActive)
            ASSERT_TRUE(session->activate());
        QObject::connect(session, &spatial::SpatialSession::activeChanged, &receiver,
                         [&](bool active) {
                             if (active == deleteWhenActive)
                                 delete session.data();
                         });
        if (deleteWhenActive)
            EXPECT_TRUE(session->activate());
        else
            EXPECT_TRUE(session->deactivate());
        EXPECT_TRUE(session.isNull());
    }
}

TEST(SpatialPluginTest, Contract_ControlSignalsCanDeleteSessionWithoutDeletingViews)
{
    spatial::SpatialView view;
    QPointer<spatial::SpatialSession> session = new spatial::SpatialSession;
    ASSERT_TRUE(session->activate());
    ASSERT_TRUE(session->attachView(&view));
    QObject receiver;
    QObject::connect(&view, &spatial::SpatialView::renderModeChanged, &receiver,
                     [&](spatial::SpatialView::RenderMode) { delete session.data(); });
    EXPECT_TRUE(session->deactivate());
    EXPECT_TRUE(session.isNull());
    EXPECT_FALSE(view.isSpatialEnabled());
    EXPECT_EQ(view.renderMode(), spatial::SpatialView::RenderMode::Raster);
}
