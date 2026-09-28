#include <gtest/gtest.h>
#include "components/foundation/MotionPolicy.h"
#include "components/layout/ParticleBackdrop.h"
#include "components/layout/ParticleBackdrop_p.h"
#include "components/spatial/ParticleLayer.h"
#include <QApplication>
#include <QImage>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>

using fluent::layout::ParticleBackdrop;
using fluent::spatial::ParticleLayer;

namespace {
QImage cpuFrame(ParticleBackdrop& backdrop)
{
    QImage frame(backdrop.size(), QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::transparent);
    QPainter painter(&frame);
    backdrop.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    return frame;
}

QImage textureFrame(const ParticleLayer& layer)
{
    auto* gl = QOpenGLContext::currentContext()->functions();
    GLint previous = 0;
    gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    GLuint framebuffer = 0;
    gl->glGenFramebuffers(1, &framebuffer);
    gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               layer.textureId(), 0);
    QImage result(layer.textureSize(), QImage::Format_RGBA8888_Premultiplied);
    gl->glReadPixels(0, 0, result.width(), result.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                     result.bits());
    gl->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(previous));
    gl->glDeleteFramebuffers(1, &framebuffer);
    return result.mirrored();
}

double alphaEnergy(const QImage& image)
{
    double sum = 0;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            sum += image.pixelColor(x, y).alphaF();
    return sum;
}
} // namespace

TEST(ParticleLayerTest, Contract_NoCurrentContextKeepsCpuDefault)
{
    ParticleBackdrop backdrop;
    backdrop.resize(240, 160);
    backdrop.setAnimationEnabled(false);
    const QImage before = cpuFrame(backdrop);
    ParticleLayer layer(&backdrop);
    EXPECT_FALSE(layer.isActive());
    EXPECT_FALSE(layer.render(1));
    EXPECT_EQ(layer.textureId(), 0u);
    EXPECT_EQ(layer.allocatedBytes(), 0u);
    EXPECT_EQ(cpuFrame(backdrop), before);
}

TEST(ParticleLayerTest, Contract_NativeGpuPreservesStateReusesFramesAndReleasesOnFallback)
{
    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("offscreen") || platform == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires the native Gallery OpenGL context path";
    QOpenGLWidget contextHost;
    contextHost.resize(420, 280);
    contextHost.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&contextHost));
    ASSERT_TRUE(QTest::qWaitFor([&] { return contextHost.isValid(); }));
    QWidget parent;
    parent.resize(320, 180);
    ParticleBackdrop backdrop(&parent);
    backdrop.resize(parent.size());
    backdrop.setAnimationEnabled(false);
    backdrop.setParticleCount(240);
    backdrop.setSpeed(.6);
    backdrop.setFadeMargins(QMarginsF(90, 0, 0, 70));
    parent.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&parent));
    ParticleLayer layer(&backdrop);
    QSignalSpy active(&layer, &ParticleLayer::activeChanged);
    contextHost.makeCurrent();
    EXPECT_FALSE(layer.render(1));
    EXPECT_EQ(layer.allocatedBytes(), 0u);
    backdrop.setGpuAccelerationEnabled(true);
    contextHost.doneCurrent();

    for (auto effect : {ParticleBackdrop::FlowingRibbons, ParticleBackdrop::FloatingDots,
                        ParticleBackdrop::Starfield}) {
        layer.release();
        backdrop.setEffect(effect);
        const QImage before = cpuFrame(backdrop);
        contextHost.makeCurrent();
        const quint64 estimate = ParticleLayer::estimatedBytes(backdrop.size(), 1);
        ASSERT_GT(estimate, 0u);
        ASSERT_TRUE(layer.render(1));
        EXPECT_TRUE(layer.isActive());
        EXPECT_NE(layer.textureId(), 0u);
        EXPECT_EQ(layer.textureSize(), backdrop.size());
        EXPECT_GT(layer.allocatedBytes(), 0u);
        EXPECT_EQ(layer.allocatedBytes(), estimate);
        const QImage gpu = textureFrame(layer);
        const double cpuEnergy = alphaEnergy(before);
        ASSERT_GT(cpuEnergy, 0);
        EXPECT_GT(alphaEnergy(gpu), cpuEnergy * .65);
        EXPECT_LT(alphaEnergy(gpu), cpuEnergy * 1.4);
        EXPECT_LE(gpu.pixelColor(0, 0).alpha(), 1);
        const quint64 count = layer.renderedFrameCount();
        ASSERT_TRUE(layer.render(1));
        EXPECT_EQ(layer.renderedFrameCount(), count);
        EXPECT_EQ(QOpenGLContext::currentContext()->functions()->glGetError(), GLenum(GL_NO_ERROR));
        EXPECT_FALSE(layer.render(1, 1)); // Fail closed before over-budget allocation.
        EXPECT_FALSE(layer.isActive());
        EXPECT_EQ(layer.textureId(), 0u);
        EXPECT_EQ(layer.allocatedBytes(), 0u);
        contextHost.doneCurrent();
        EXPECT_EQ(cpuFrame(backdrop), before);
        EXPECT_EQ(backdrop.effect(), effect);
        EXPECT_EQ(backdrop.particleCount(), 240);
        EXPECT_EQ(backdrop.speed(), .6);
        EXPECT_FALSE(backdrop.isAnimationEnabled());
    }
    EXPECT_EQ(active.count(), 6);
}

TEST(ParticleLayerTest, Contract_SourceDestructionWithoutCurrentContextIsSafe)
{
    for (bool child : {false, true}) {
        QWidget parent;
        auto* source = new ParticleBackdrop(child ? &parent : nullptr);
        auto* layer = new ParticleLayer(source);
        delete source;
        EXPECT_EQ(layer->backdrop(), nullptr);
        EXPECT_FALSE(layer->isActive());
        EXPECT_FALSE(layer->render(1));
        layer->release();
        delete layer;

        source = new ParticleBackdrop(child ? &parent : nullptr);
        QPointer<ParticleLayer> owned = new ParticleLayer(source, source);
        delete source;
        EXPECT_TRUE(owned.isNull());
    }
}

TEST(ParticleLayerTest, Contract_NativeActiveSourceDestructionReleasesExactlyOnce)
{
    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("offscreen") || platform == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires native GPU ownership";
    QOpenGLWidget contextHost;
    contextHost.resize(320, 200);
    contextHost.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&contextHost));
    ASSERT_TRUE(QTest::qWaitFor([&] { return contextHost.isValid(); }));
    for (bool child : {false, true}) {
        QWidget parent;
        parent.resize(240, 160);
        parent.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&parent));
        for (bool owned : {false, true}) {
            auto* source = new ParticleBackdrop(child ? &parent : nullptr);
            source->setGpuAccelerationEnabled(true);
            source->resize(240, 160);
            source->setAnimationEnabled(false);
            source->show();
            ASSERT_TRUE(QTest::qWaitForWindowExposed(source->window()));
            QPointer<ParticleLayer> layer = new ParticleLayer(source, owned ? source : nullptr);
            int activations = 0, releases = 0;
            QObject observer;
            QObject::connect(layer, &ParticleLayer::activeChanged, &observer,
                             [&](bool active) { active ? ++activations : ++releases; });
            contextHost.makeCurrent();
            ASSERT_TRUE(layer->render(1));
            ASSERT_TRUE(layer->isActive());
            EXPECT_EQ(activations, 1);
            delete source;
            EXPECT_EQ(releases, 1);
            if (owned) {
                EXPECT_TRUE(layer.isNull());
            } else {
                ASSERT_FALSE(layer.isNull());
                EXPECT_EQ(layer->backdrop(), nullptr);
                EXPECT_FALSE(layer->isActive());
                EXPECT_EQ(layer->allocatedBytes(), 0u);
                layer->release();
                EXPECT_EQ(releases, 1);
                delete layer;
            }
            contextHost.doneCurrent();
        }
    }
}

TEST(ParticleLayerTest, Contract_NativeReleaseNotificationMayDestroyLayerOrSource)
{
    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("offscreen") || platform == QStringLiteral("minimal"))
        GTEST_SKIP() << "Requires native GPU ownership";
    QOpenGLWidget contextHost;
    contextHost.resize(320, 200);
    contextHost.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&contextHost));
    ASSERT_TRUE(QTest::qWaitFor([&] { return contextHost.isValid(); }));
    QWidget parent;
    parent.resize(240, 160);
    parent.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&parent));
    for (bool destroySource : {false, true}) {
        QPointer<ParticleBackdrop> source = new ParticleBackdrop(&parent);
        source->setGpuAccelerationEnabled(true);
        source->resize(parent.size());
        source->setAnimationEnabled(false);
        source->show();
        QPointer<ParticleLayer> layer = new ParticleLayer(source);
        contextHost.makeCurrent();
        ASSERT_TRUE(layer->render(1));
        QObject::connect(layer, &ParticleLayer::activeChanged, &parent, [&](bool active) {
            if (!active) {
                if (destroySource)
                    delete source;
                else
                    delete layer;
            }
        });
        QEvent hide(QEvent::Hide);
        QApplication::sendEvent(source, &hide);
        EXPECT_EQ(source.isNull(), destroySource);
        EXPECT_EQ(layer.isNull(), !destroySource);
        delete layer;
        delete source;
        contextHost.doneCurrent();
    }
}
