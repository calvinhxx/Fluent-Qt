#include <gtest/gtest.h>
#include <QOpenGLContext>
#include <QPointer>
#include <QSignalSpy>
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
    auto* owner = new QWidget;
    auto* session = new spatial::SpatialSession(owner, owner);
    ASSERT_TRUE(session->activate());
    QPointer<spatial::SpatialView> view = session->createView(owner);
    ASSERT_FALSE(view.isNull());
    delete owner;
    EXPECT_TRUE(view.isNull());
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
