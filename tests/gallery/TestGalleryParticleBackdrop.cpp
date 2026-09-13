#include <gtest/gtest.h>

#include <QAbstractScrollArea>
#include <QAbstractButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QHBoxLayout>
#include <QLockFile>
#include <QMetaEnum>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QTest>
#include <memory>

#include "components/foundation/MotionPolicy.h"
#include "components/layout/ParticleBackdrop.h"
#include "model/GalleryContentCatalog.h"
#include "platform/GalleryPlatform.h"
#include "view/pages/GalleryHomePage.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "view/widgets/GallerySampleCard.h"
#include "viewmodel/GalleryNavigationViewModel.h"
#include "viewmodel/GallerySettings.h"

using fluent::layout::ParticleBackdrop;

TEST(GalleryParticleBackdropTest, SamplesReusePublicComponentAndStartIdle)
{
    const auto samples =
        fluent::gallery::gallerySamplesForRoute(QStringLiteral("particle-backdrop"));
    ASSERT_EQ(samples.size(), 3);
    for (const auto& sample : samples) {
        std::unique_ptr<QWidget> preview(sample.createPreview(nullptr));
        auto* backdrop = qobject_cast<ParticleBackdrop*>(preview.get());
        if (!backdrop)
            backdrop = preview->findChild<ParticleBackdrop*>();
        ASSERT_NE(backdrop, nullptr);
        EXPECT_FALSE(backdrop->isAnimating());
        EXPECT_TRUE(sample.codeSnippet.contains(QStringLiteral("new ParticleBackdrop")));
        const auto expected = sample.id == QStringLiteral("particle-backdrop-basic")
                                  ? ParticleBackdrop::FlowingRibbons
                              : sample.id == QStringLiteral("particle-backdrop-content")
                                  ? ParticleBackdrop::FloatingDots
                                  : ParticleBackdrop::Starfield;
        EXPECT_EQ(backdrop->effect(), expected);
    }
}

TEST(GalleryParticleBackdropTest, SamplesFillAvailableWidthAndControlsChangeMotion)
{
    const auto samples =
        fluent::gallery::gallerySamplesForRoute(QStringLiteral("particle-backdrop"));
    for (const auto& sample : samples) {
        EXPECT_TRUE(sample.fillAvailableWidth);
        fluent::gallery::GallerySampleCard card(sample);
        card.resize(880, 650);
        card.show();
        QTest::qWait(30);
        auto* preview = card.previewWidget();
        ASSERT_NE(preview, nullptr);
        EXPECT_EQ(preview->width(), preview->parentWidget()->width() - 40);
        card.resize(400, 650);
        QTest::qWait(30);
        EXPECT_EQ(preview->width(), preview->parentWidget()->width() - 40);
        EXPECT_LE(preview->parentWidget()->width(), card.width());
        if (sample.id == QStringLiteral("particle-backdrop-interaction")) {
            auto* backdrop = preview->findChild<ParticleBackdrop*>();
            auto* pause = preview->findChild<QAbstractButton*>(QStringLiteral("particlePause"));
            auto* speed = preview->findChild<QSlider*>(QStringLiteral("particleSpeed"));
            auto* effect = preview->findChild<QComboBox*>(QStringLiteral("particleEffect"));
            ASSERT_NE(backdrop, nullptr);
            ASSERT_NE(pause, nullptr);
            ASSERT_NE(speed, nullptr);
            ASSERT_NE(effect, nullptr);
            EXPECT_EQ(effect->count(), 3);
            EXPECT_FALSE(effect->accessibleName().isEmpty());
            QTest::mouseClick(pause, Qt::LeftButton);
            EXPECT_FALSE(backdrop->isAnimationEnabled());
            EXPECT_EQ(pause->text(), QStringLiteral("Resume motion"));
            for (int index = 0; index < effect->count(); ++index) {
                effect->setCurrentIndex(index);
                EXPECT_EQ(int(backdrop->effect()), effect->currentData().toInt());
                EXPECT_FALSE(backdrop->isAnimationEnabled());
                EXPECT_FALSE(backdrop->isAnimating());
            }
            effect->setFocus();
            QTest::keyClick(effect, Qt::Key_Up);
            EXPECT_EQ(backdrop->effect(), ParticleBackdrop::FloatingDots);
            speed->setValue(150);
            EXPECT_DOUBLE_EQ(backdrop->speed(), 1.5);
            QTest::mouseClick(pause, Qt::LeftButton);
            EXPECT_TRUE(backdrop->isAnimationEnabled());
        }
    }
}

TEST(GalleryParticleBackdropTest, ExistingExpandingPreviewsKeepTheirRightSpacer)
{
    for (const auto& route : {QStringLiteral("tab-view"), QStringLiteral("navigation-view"),
                              QStringLiteral("button"), QStringLiteral("slider")}) {
        const auto samples = fluent::gallery::gallerySamplesForRoute(route);
        ASSERT_FALSE(samples.isEmpty()) << route.toStdString();
        for (const auto& sample : samples) {
            EXPECT_FALSE(sample.fillAvailableWidth);
            fluent::gallery::GallerySampleCard card(sample);
            auto* preview = card.previewWidget();
            ASSERT_NE(preview, nullptr);
            auto* row = qobject_cast<QHBoxLayout*>(preview->parentWidget()->layout());
            ASSERT_NE(row, nullptr);
            EXPECT_EQ(row->stretch(0), 0);
            ASSERT_GE(row->count(), 2);
            EXPECT_NE(row->itemAt(1)->spacerItem(), nullptr);
        }
    }
}

TEST(GalleryParticleBackdropTest, HomeUsesBoundedMotionAndStopsOutsideViewport)
{
    auto& settings = fluent::gallery::GallerySettings::instance();
    struct RestoreParticles {
        bool enabled;
        ~RestoreParticles()
        {
            fluent::gallery::GallerySettings::instance().setHomeParticlesEnabled(enabled);
        }
    } restoreParticles{settings.homeParticlesEnabled()};
    settings.setHomeParticlesEnabled(true);
    auto& policy = fluent::MotionPolicy::instance();
    const auto oldMode = policy.mode();
    struct Restore {
        fluent::MotionPolicy::Mode mode;
        ~Restore() { fluent::MotionPolicy::instance().setMode(mode); }
    } restore{oldMode};
    policy.setMode(fluent::MotionPolicy::Mode::Full);
    fluent::gallery::GalleryNavigationViewModel navigation;
    const auto* entry = fluent::gallery::galleryContentEntry(QStringLiteral("home"));
    ASSERT_NE(entry, nullptr);
    fluent::gallery::GalleryHomePage home(*entry, navigation);
    home.resize(920, 560);
    auto* backdrop = home.findChild<ParticleBackdrop*>(QStringLiteral("galleryHomeParticles"));
    ASSERT_NE(backdrop, nullptr);
    const auto launchEffect = backdrop->effect();
    EXPECT_TRUE(launchEffect == ParticleBackdrop::FlowingRibbons ||
                launchEffect == ParticleBackdrop::FloatingDots ||
                launchEffect == ParticleBackdrop::Starfield);
    EXPECT_EQ(backdrop->particleCount(), 240);
    EXPECT_EQ(backdrop->maximumFrameRate(), 30);
    EXPECT_EQ(backdrop->backgroundMode(), ParticleBackdrop::Transparent);
    EXPECT_FALSE(backdrop->isInteractive());
    backdrop->setPauseWhenInactive(false);
    home.show();
    QTest::qWait(50);
    EXPECT_TRUE(backdrop->isAnimating());
    settings.setHomeParticlesEnabled(false);
    EXPECT_TRUE(backdrop->isHidden());
    EXPECT_FALSE(backdrop->isAnimationEnabled());
    EXPECT_FALSE(backdrop->isAnimating());
    home.onThemeUpdated();
    EXPECT_TRUE(backdrop->isHidden());
    settings.setHomeParticlesEnabled(true);
    EXPECT_FALSE(backdrop->isHidden());
    EXPECT_EQ(backdrop->effect(), launchEffect);
    EXPECT_TRUE(backdrop->isAnimating());
    auto* scroll = home.findChild<QAbstractScrollArea*>();
    ASSERT_NE(scroll, nullptr);
    ASSERT_GT(scroll->verticalScrollBar()->maximum(), 390);
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    QTest::qWait(50);
    EXPECT_FALSE(backdrop->isAnimating());
    scroll->verticalScrollBar()->setValue(0);
    QTest::qWait(50);
    EXPECT_TRUE(backdrop->isAnimating());
    home.hide();
    QTest::qWait(25);
    EXPECT_FALSE(backdrop->isAnimating());
    home.onThemeUpdated();
    EXPECT_EQ(backdrop->effect(), launchEffect);
    fluent::gallery::GalleryHomePage recreatedHome(*entry, navigation);
    auto* recreatedBackdrop =
        recreatedHome.findChild<ParticleBackdrop*>(QStringLiteral("galleryHomeParticles"));
    ASSERT_NE(recreatedBackdrop, nullptr);
    EXPECT_EQ(recreatedBackdrop->effect(), launchEffect);
}

TEST(GalleryParticleBackdropTest, HomeEffectDoesNotRepeatAcrossLaunches)
{
    // Enable real persistence only inside Qt's isolated test-data scope.
    ASSERT_TRUE(QStandardPaths::isTestModeEnabled());
    struct RestoreIdentity {
        QString organization = QCoreApplication::organizationName();
        QString application = QCoreApplication::applicationName();
        ~RestoreIdentity()
        {
            QCoreApplication::setOrganizationName(organization);
            QCoreApplication::setApplicationName(application);
        }
    } restoreIdentity;
    QCoreApplication::setOrganizationName(QStringLiteral("Fluent-Qt"));
    QCoreApplication::setApplicationName(fluent::gallery::platform::capabilities().applicationName);
    const QString key = QStringLiteral("home/lastParticleEffect");
    const QString enabledKey = QStringLiteral("home/particlesEnabled");
    QSettings storage = fluent::gallery::platform::createSettings();
    const QStringList validEffects{QStringLiteral("FlowingRibbons"), QStringLiteral("FloatingDots"),
                                   QStringLiteral("Starfield")};

    if (qEnvironmentVariableIsSet("FLUENTQT_GALLERY_HERO_COLD_LOAD")) {
        const QString previous = storage.value(key).toString();
        const bool enabled = storage.value(enabledKey, true).toBool();
        fluent::gallery::GalleryNavigationViewModel navigation;
        const auto* entry = fluent::gallery::galleryContentEntry(QStringLiteral("home"));
        ASSERT_NE(entry, nullptr);
        fluent::gallery::GalleryHomePage home(*entry, navigation);
        auto* backdrop = home.findChild<ParticleBackdrop*>(QStringLiteral("galleryHomeParticles"));
        ASSERT_NE(backdrop, nullptr);
        auto& settings = fluent::gallery::GallerySettings::instance();
        EXPECT_EQ(settings.homeParticlesEnabled(), enabled);
        EXPECT_EQ(backdrop->isAnimationEnabled(), enabled);
        EXPECT_EQ(backdrop->isHidden(), !enabled);
        if (!enabled) {
            EXPECT_FALSE(backdrop->isAnimating());
            storage.sync();
            EXPECT_EQ(storage.value(key).toString(), previous);
            return;
        }
        const auto chosenEffect = backdrop->effect();
        const QString chosen = QString::fromLatin1(
            QMetaEnum::fromType<ParticleBackdrop::Effect>().valueToKey(chosenEffect));
        EXPECT_TRUE(validEffects.contains(chosen));
        EXPECT_NE(chosen, previous);
        storage.sync();
        EXPECT_EQ(storage.value(key).toString(), chosen);
        settings.setHomeParticlesEnabled(false);
        storage.sync();
        EXPECT_FALSE(storage.value(enabledKey, true).toBool());
        settings.setHomeParticlesEnabled(true);
        EXPECT_EQ(backdrop->effect(), chosenEffect);
        return;
    }

    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    ASSERT_TRUE(QDir().mkpath(dataPath));
    QLockFile lock(QDir(dataPath).filePath(QStringLiteral("hero-effect-cold-load.lock")));
    ASSERT_TRUE(lock.tryLock(10000));
    storage.sync();
    struct RestoreSetting {
        QSettings& storage;
        QString key;
        bool existed;
        QVariant value;
        ~RestoreSetting()
        {
            if (existed)
                storage.setValue(key, value);
            else
                storage.remove(key);
            storage.sync();
        }
    } restoreSetting{storage, key, storage.contains(key), storage.value(key)};
    RestoreSetting restoreEnabled{storage, enabledKey, storage.contains(enabledKey),
                                  storage.value(enabledKey)};
    storage.remove(key);
    storage.remove(enabledKey);
    for (int launch = 0; launch < 8; ++launch) {
        if (launch >= 1 && launch <= 3)
            storage.setValue(key, validEffects[launch - 1]);
        else if (launch == 4)
            storage.setValue(key, QStringLiteral("retired-effect"));
        if (launch > 0)
            storage.setValue(enabledKey, launch != 6);
        storage.sync();
        const QString previous = storage.value(key).toString();
        QProcess probe;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("FLUENTQT_GALLERY_HERO_COLD_LOAD"), QStringLiteral("1"));
        probe.setProcessEnvironment(environment);
        probe.setProcessChannelMode(QProcess::MergedChannels);
        probe.start(QCoreApplication::applicationFilePath(),
                    {QStringLiteral("--gtest_filter=GalleryParticleBackdropTest."
                                    "HomeEffectDoesNotRepeatAcrossLaunches")});
        ASSERT_TRUE(probe.waitForStarted(10000));
        ASSERT_TRUE(probe.waitForFinished(30000));
        ASSERT_EQ(probe.exitStatus(), QProcess::NormalExit);
        ASSERT_EQ(probe.exitCode(), 0) << probe.readAll().toStdString();
        storage.sync();
        const QString chosen = storage.value(key).toString();
        EXPECT_TRUE(validEffects.contains(chosen));
        if (launch == 6)
            EXPECT_EQ(chosen, previous);
        else
            EXPECT_NE(chosen, previous);
    }
}
