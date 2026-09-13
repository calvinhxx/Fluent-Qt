#include <gtest/gtest.h>

#include <QAbstractScrollArea>
#include <QAbstractButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QSlider>
#include <QTest>
#include <memory>

#include "components/foundation/MotionPolicy.h"
#include "components/layout/ParticleBackdrop.h"
#include "model/GalleryContentCatalog.h"
#include "view/pages/GalleryHomePage.h"
#include "view/widgets/GallerySampleCatalog.h"
#include "view/widgets/GallerySampleCard.h"
#include "viewmodel/GalleryNavigationViewModel.h"

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
    EXPECT_EQ(backdrop->particleCount(), 240);
    EXPECT_EQ(backdrop->maximumFrameRate(), 30);
    EXPECT_EQ(backdrop->backgroundMode(), ParticleBackdrop::Transparent);
    EXPECT_FALSE(backdrop->isInteractive());
    backdrop->setPauseWhenInactive(false);
    home.show();
    QTest::qWait(50);
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
}
