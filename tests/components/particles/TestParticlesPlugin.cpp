#include <gtest/gtest.h>
#include <QPointer>
#include <QSignalSpy>
#include <FluentQt/Particles.h>
#include "components/foundation/MotionPolicy.h"
#include "components/foundation/plugins/PluginManager.h"

using namespace fluent;

class ParticlesPluginTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        oldMode = MotionPolicy::instance().mode();
        MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
        host.resize(240, 180);
        host.show();
    }
    void TearDown() override { MotionPolicy::instance().setMode(oldMode); }

    // A visible surface lets the tests observe the timer instead of the preference.
    void showInHost(particles::ParticleBackdrop* backdrop)
    {
        backdrop->setParent(&host);
        backdrop->setPauseWhenInactive(false);
        backdrop->setGeometry(0, 0, 200, 140);
        backdrop->show();
    }

    MotionPolicy::Mode oldMode;
    QWidget host;
};

TEST_F(ParticlesPluginTest, Contract_GenericFactoryPreservesCallerOwnership)
{
    particles::ParticlesPlugin plugin;
    plugins::PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&plugin));
    EXPECT_EQ(manager.createWidget(plugin.pluginId(), QStringLiteral("particle-backdrop")), nullptr);
    ASSERT_TRUE(manager.activate(plugin.pluginId()));
    QPointer<particles::ParticleBackdrop> backdrop = qobject_cast<particles::ParticleBackdrop*>(
        manager.createWidget(plugin.pluginId(), QStringLiteral("particle-backdrop"), &host));
    ASSERT_FALSE(backdrop.isNull());
    EXPECT_EQ(backdrop->parentWidget(), &host);
    EXPECT_FALSE(backdrop->isGpuAccelerationEnabled());
    EXPECT_EQ(manager.createWidget(plugin.pluginId(), QStringLiteral("unknown")), nullptr);
    showInHost(backdrop);
    EXPECT_TRUE(backdrop->isAnimating());
    ASSERT_TRUE(manager.deactivateAll());
    ASSERT_FALSE(backdrop.isNull());
    EXPECT_FALSE(backdrop->isAnimating());
    EXPECT_TRUE(backdrop->isAnimationEnabled());
    EXPECT_EQ(backdrop->parentWidget(), &host);
}

TEST_F(ParticlesPluginTest, Contract_PauseKeepsSettingsAndLaterPreferenceChanges)
{
    particles::ParticleBackdrop running;
    particles::ParticleBackdrop paused;
    showInHost(&running);
    showInHost(&paused);
    paused.setAnimationEnabled(false);
    running.setEffect(particles::ParticleBackdrop::Starfield);
    running.setSpeed(2);
    particles::ParticlesSession session;
    QSignalSpy state(&session, &particles::ParticlesSession::activeChanged);
    EXPECT_EQ(session.createBackdrop(), nullptr);
    EXPECT_FALSE(session.attachBackdrop(&running));
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.activate());
    EXPECT_EQ(state.count(), 1);
    ASSERT_TRUE(session.attachBackdrop(&running));
    ASSERT_TRUE(session.attachBackdrop(&running));
    ASSERT_TRUE(session.attachBackdrop(&paused));
    QSignalSpy preference(&running, &particles::ParticleBackdrop::animationEnabledChanged);
    ASSERT_TRUE(session.deactivate());
    ASSERT_TRUE(session.deactivate());
    EXPECT_FALSE(running.isAnimating());
    EXPECT_TRUE(running.isAnimationEnabled());
    EXPECT_EQ(preference.count(), 0);

    // A preference changed while paused is kept, not overwritten on reactivation.
    paused.setAnimationEnabled(true);
    EXPECT_FALSE(paused.isAnimating());
    ASSERT_TRUE(session.activate());
    EXPECT_TRUE(running.isAnimating());
    EXPECT_TRUE(paused.isAnimating());
    EXPECT_TRUE(paused.isAnimationEnabled());
    EXPECT_EQ(running.effect(), particles::ParticleBackdrop::Starfield);
    EXPECT_EQ(running.speed(), 2);
    EXPECT_EQ(running.parentWidget(), &host);
    EXPECT_EQ(state.count(), 3);
}

TEST_F(ParticlesPluginTest, Contract_SessionDestructionLiftsItsPause)
{
    QPointer<particles::ParticleBackdrop> borrowed = new particles::ParticleBackdrop;
    showInHost(borrowed);
    {
        particles::ParticlesSession session;
        ASSERT_TRUE(session.activate());
        ASSERT_TRUE(session.attachBackdrop(borrowed));
        auto* temporary = session.createBackdrop();
        ASSERT_NE(temporary, nullptr);
        delete temporary;
        ASSERT_TRUE(session.deactivate());
        EXPECT_FALSE(borrowed->isAnimating());
    }
    ASSERT_FALSE(borrowed.isNull());
    EXPECT_TRUE(borrowed->isAnimationEnabled());
    EXPECT_TRUE(borrowed->isAnimating());
    delete borrowed.data();
}

TEST_F(ParticlesPluginTest, Contract_EachScopeLiftsOnlyItsOwnPause)
{
    particles::ParticleBackdrop shared;
    showInHost(&shared);
    particles::ParticlesSession first;
    particles::ParticlesSession second;
    ASSERT_TRUE(first.activate());
    ASSERT_TRUE(second.activate());
    ASSERT_TRUE(first.attachBackdrop(&shared));
    ASSERT_TRUE(second.attachBackdrop(&shared));
    ASSERT_TRUE(first.deactivate());
    ASSERT_TRUE(second.deactivate());
    ASSERT_TRUE(first.activate());
    EXPECT_FALSE(shared.isAnimating());
    ASSERT_TRUE(second.activate());
    EXPECT_TRUE(shared.isAnimating());
}

TEST_F(ParticlesPluginTest, Contract_SessionCannotBelongToTrackedWidget)
{
    particles::ParticleBackdrop backdrop;
    QObject nested(&backdrop);
    particles::ParticlesSession session(nullptr, &nested);
    ASSERT_TRUE(session.activate());
    EXPECT_FALSE(session.attachBackdrop(&backdrop));
}

TEST_F(ParticlesPluginTest, Contract_BorrowedHostCanDisappearBeforeExplicitCleanup)
{
    particles::ParticleBackdrop backdrop;
    showInHost(&backdrop);
    auto* sessionHost = new QObject;
    particles::ParticlesSession session(sessionHost);
    ASSERT_TRUE(session.activate());
    ASSERT_TRUE(session.attachBackdrop(&backdrop));
    delete sessionHost;
    ASSERT_TRUE(session.deactivate());
    EXPECT_FALSE(session.isActive());
    EXPECT_FALSE(backdrop.isAnimating());
    EXPECT_TRUE(backdrop.isAnimationEnabled());
}

TEST_F(ParticlesPluginTest, Contract_DirectSignalsMayDeleteSessionAndOtherSources)
{
    particles::ParticleBackdrop first;
    showInHost(&first);
    QPointer<particles::ParticleBackdrop> second = new particles::ParticleBackdrop;
    showInHost(second);
    QPointer<particles::ParticlesSession> session = new particles::ParticlesSession;
    ASSERT_TRUE(session->activate());
    ASSERT_TRUE(session->attachBackdrop(&first));
    ASSERT_TRUE(session->attachBackdrop(second));
    ASSERT_TRUE(first.isAnimating());
    QObject::connect(&first, &particles::ParticleBackdrop::animatingChanged, &first, [&](bool running) {
        if (running)
            return;
        delete second.data();
        delete session.data();
    });
    EXPECT_TRUE(session->deactivate());
    EXPECT_TRUE(session.isNull());
    EXPECT_TRUE(second.isNull());
    // The destroyed session no longer pauses the remaining backdrop.
    EXPECT_TRUE(first.isAnimating());
}
