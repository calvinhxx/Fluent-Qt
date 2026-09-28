#include <gtest/gtest.h>
#include <QApplication>
#include <QImage>
#include <QGraphicsOpacityEffect>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QOpenGLPaintDevice>
#include <QOpenGLWidget>
#include <QPainter>
#include <QSignalSpy>
#include <QTest>
#include <QtMath>

#include "components/layout/ParticleBackdrop.h"
#include "view/shell/GalleryGlyphPaintDevice.h"
#include "view/shell/GalleryParticleCompositor.h"
#include "view/shell/GallerySpatialRenderPolicy.h"

namespace {
using fluent::layout::ParticleBackdrop;
using fluent::gallery::spatial_render::GalleryParticleCompositor;
using fluent::gallery::spatial_render::GalleryGlyphPaintDevice;

class ForegroundProbe : public QWidget {
public:
    QColor color;
    explicit ForegroundProbe(const QColor& fill, QWidget* parent) : QWidget(parent), color(fill) {}
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), color);
        painter.setPen(QColor(255, 255, 255, 176));
        painter.drawText(rect().adjusted(12, 8, -12, -8), Qt::AlignCenter,
                         QStringLiteral("Foreground Aa 123"));
    }
};

std::unique_ptr<QOpenGLFramebufferObject> capture(QWidget& root, qreal cacheDpr = 1)
{
    const QSize pixels(qCeil(root.width() * cacheDpr), qCeil(root.height() * cacheDpr));
    QOpenGLFramebufferObjectFormat format;
    format.setInternalTextureFormat(GL_RGBA8);
    format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    format.setSamples(2);
    QOpenGLFramebufferObject paintTarget(pixels, format);
    auto texture = std::make_unique<QOpenGLFramebufferObject>(pixels);
    paintTarget.bind();
    auto* gl = QOpenGLContext::currentContext()->functions();
    gl->glDisable(GL_SCISSOR_TEST);
    gl->glColorMask(true, true, true, true);
    gl->glStencilMask(0xFFFFFFFF);
    gl->glClearColor(0, 0, 0, 0);
    gl->glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    QOpenGLPaintDevice device(pixels);
    device.setDevicePixelRatio(cacheDpr);
    GalleryGlyphPaintDevice glyph(device, 1);
    QPainter painter(&glyph);
    root.render(&painter, QPoint(), QRegion(), QWidget::DrawChildren);
    painter.end();
    QOpenGLFramebufferObject::blitFramebuffer(texture.get(), &paintTarget);
    return texture;
}

QImage textureImage(GLuint texture, const QSize& size)
{
    auto* gl = QOpenGLContext::currentContext()->functions();
    GLint previous = 0;
    gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    GLuint framebuffer = 0;
    gl->glGenFramebuffers(1, &framebuffer);
    gl->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    gl->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    QImage image(size, QImage::Format_RGBA8888_Premultiplied);
    gl->glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
    gl->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(previous));
    gl->glDeleteFramebuffers(1, &framebuffer);
    return image.mirrored();
}

double difference(const QImage& actual, const QImage& expected, const QRect& region)
{
    double sum = 0;
    for (int y = region.top(); y <= region.bottom(); ++y) {
        for (int x = region.left(); x <= region.right(); ++x) {
            const QColor a = actual.pixelColor(x, y), e = expected.pixelColor(x, y);
            sum += qAbs(a.red() - e.red()) + qAbs(a.green() - e.green()) +
                   qAbs(a.blue() - e.blue()) + qAbs(a.alpha() - e.alpha());
        }
    }
    return sum / (region.width() * region.height() * 4);
}

bool nativePlatform()
{
    const auto platform = QGuiApplication::platformName();
    return platform != QStringLiteral("offscreen") && platform != QStringLiteral("minimal");
}

const auto renderForeground = [](QPainter& painter, QWidget* widget, const QRegion& region) {
    widget->render(&painter, region.boundingRect().topLeft(), region, QWidget::DrawChildren);
};

class GalleryParticleCompositionTest : public testing::TestWithParam<std::pair<int, bool>> {};
} // namespace

TEST_P(GalleryParticleCompositionTest, Contract_NativeInsertionPreservesStacking)
{
    if (!nativePlatform())
        GTEST_SKIP() << "Requires native GPU composition";
    QOpenGLWidget context;
    context.resize(420, 280);
    context.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&context));
    ASSERT_TRUE(QTest::qWaitFor([&] { return context.isValid(); }));
    QWidget root;
    root.resize(420, 240);
    const qreal cacheDpr = GetParam().first;
    const bool clipped = GetParam().second;
    const QSize panelPixels(root.width() * cacheDpr, root.height() * cacheDpr);
    const auto pixelRect = [cacheDpr](const QRect& rect) {
        return QRect(rect.x() * cacheDpr, rect.y() * cacheDpr, rect.width() * cacheDpr,
                     rect.height() * cacheDpr);
    };
    root.setAutoFillBackground(true);
    auto palette = root.palette();
    palette.setColor(QPalette::Window, QColor(24, 35, 47));
    root.setPalette(palette);
    QWidget viewport(&root);
    viewport.setGeometry(clipped ? QRect(15, 18, 380, 205) : root.rect());
    ParticleBackdrop lower(&viewport);
    lower.setGeometry(clipped ? QRect(-19, -23, 420, 240) : root.rect());
    lower.setEffect(ParticleBackdrop::FloatingDots);
    lower.setParticleCount(240);
    lower.setAnimationEnabled(false);
    lower.setFadeMargins(QMarginsF(80, 0, 0, 60));
    ParticleBackdrop upper(&viewport);
    upper.setGeometry(45, 30, 330, 180);
    upper.setEffect(ParticleBackdrop::Starfield);
    upper.setParticleCount(240);
    upper.setAnimationEnabled(false);
    ForegroundProbe translucent(QColor(210, 60, 25, 128), &root);
    translucent.setGeometry(35, 40, 235, 110);
    ForegroundProbe opaque(QColor(0, 90, 170), &root);
    opaque.setGeometry(280, 80, 105, 105);
    // Native child windows do not belong in the QWidget panel paint order.
    ForegroundProbe nativeWindow(QColor(255, 0, 255), &root);
    nativeWindow.setWindowFlag(Qt::Tool);
    nativeWindow.resize(100, 60);
    nativeWindow.show();
    root.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&root));
    context.makeCurrent();
    auto original = capture(root, cacheDpr);
    const QImage expected = textureImage(original->texture(), panelPixels);
    GalleryParticleCompositor compositor;
    quint64 revision = 1;
    QObject::connect(&compositor, &GalleryParticleCompositor::staticContentInvalidated,
                     [&] { ++revision; });
    ASSERT_TRUE(
        compositor.prepare(&root, revision, panelPixels, 1, cacheDpr, 32 * 1024 * 1024, true));
    EXPECT_EQ(compositor.activeLayerCount(), 2);
    auto base = capture(root, cacheDpr);
    QOpenGLFramebufferObjectFormat sharedFormat;
    sharedFormat.setInternalTextureFormat(GL_RGBA8);
    sharedFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    sharedFormat.setSamples(2);
    const QSize sharedSize(panelPixels.width(), clipped ? 47 : panelPixels.height());
    QOpenGLFramebufferObject sharedPaint(sharedSize, sharedFormat);
    QOpenGLFramebufferObject sharedResolve(sharedSize);
    const GLuint composed = compositor.compose(base->texture(), revision, &sharedPaint,
                                               &sharedResolve, renderForeground);
    EXPECT_NE(composed, base->texture());
    const QImage actual = textureImage(composed, panelPixels);
    EXPECT_LT(difference(actual, expected, pixelRect(root.rect())), 2.0);
    EXPECT_LT(difference(actual, expected, pixelRect(translucent.geometry())), 1.0);
    EXPECT_LT(difference(actual, expected, pixelRect(opaque.geometry().adjusted(2, 2, -2, -2))),
              .1);
    EXPECT_EQ(compositor.foregroundCaptureCount(), 2u);
    EXPECT_EQ(compositor.compositionCount(), 1u);
    EXPECT_EQ(compositor.compose(base->texture(), revision, &sharedPaint, &sharedResolve,
                                 renderForeground),
              composed);
    EXPECT_EQ(compositor.foregroundCaptureCount(), 2u);
    EXPECT_EQ(compositor.compositionCount(), 1u);
    EXPECT_EQ(context.context()->functions()->glGetError(), GLenum(GL_NO_ERROR));

    // A genuine foreground change invalidates once; particle time is unchanged.
    translucent.color = QColor(40, 170, 90, 128);
    ++revision;
    base = capture(root, cacheDpr);
    compositor.compose(base->texture(), revision, &sharedPaint, &sharedResolve, renderForeground);
    EXPECT_EQ(compositor.foregroundCaptureCount(), 4u);
    compositor.release();
    EXPECT_EQ(compositor.activeLayerCount(), 0);
    EXPECT_EQ(compositor.allocatedBytes(), 0u);
    EXPECT_EQ(lower.effect(), ParticleBackdrop::FloatingDots);
    EXPECT_EQ(upper.effect(), ParticleBackdrop::Starfield);
    ++revision;
    EXPECT_FALSE(compositor.prepare(&root, revision, panelPixels, 1, cacheDpr, 1, true));
    const quint64 scans = compositor.sourceScanCount();
    for (int i = 0; i < 12; ++i)
        EXPECT_FALSE(compositor.prepare(&root, ++revision, panelPixels, 1, cacheDpr, 1, true));
    EXPECT_EQ(compositor.sourceScanCount(), scans);
    EXPECT_EQ(compositor.allocatedBytes(), 0u);
    EXPECT_TRUE(
        compositor.prepare(&root, revision, panelPixels, 1, cacheDpr, 32 * 1024 * 1024, true));
    EXPECT_EQ(compositor.activeLayerCount(), 2);
    compositor.release();
    context.doneCurrent();
}

INSTANTIATE_TEST_SUITE_P(ScaleAndClip, GalleryParticleCompositionTest,
                         testing::Values(std::make_pair(1, false), std::make_pair(2, false),
                                         std::make_pair(2, true)));

TEST(GalleryParticleCompositorTest, Contract_NativeHiDpiBudgetKeepsAnimatedAndStaticCaches)
{
    if (!nativePlatform())
        GTEST_SKIP() << "Requires native GPU composition";
    using namespace fluent::gallery::spatial_render;
    QOpenGLWidget context;
    context.resize(320, 220);
    context.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&context));
    ASSERT_TRUE(QTest::qWaitFor([&] { return context.isValid(); }));
    QWidget root;
    root.resize(960, 700);
    ParticleBackdrop source(&root);
    source.setGeometry(20, 20, 600, 240);
    source.setEffect(ParticleBackdrop::FloatingDots);
    source.setPauseWhenInactive(false);
    ForegroundProbe foreground(QColor(20, 90, 150, 128), &root);
    foreground.setGeometry(40, 40, 300, 100);
    root.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&root));
    context.makeCurrent();
    {
        GalleryParticleCompositor compositor;
        const std::array<QSizeF, 2> panels = {QSizeF(240, 700), QSizeF(root.size())};
        QOpenGLFramebufferObjectFormat paintFormat;
        paintFormat.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        paintFormat.setInternalTextureFormat(GL_RGBA8);
        paintFormat.setSamples(kPaintSamples);
        QOpenGLFramebufferObject sampleProbe(QSize(1, 1), paintFormat);
        ASSERT_TRUE(sampleProbe.isValid());
        const int samples = sampleProbe.format().samples();
        paintFormat.setSamples(samples);
        const auto basePlan = planCaches(panels, 2, 16384, 2, kCacheBudgetBytes, samples);
        ASSERT_TRUE(basePlan.valid());
        ASSERT_EQ(basePlan.dpr, 4);
        const auto required = compositor.requiredBytes(&root, 1, basePlan.sizes[1], 2, 4, true);
        ASSERT_GT(required, quint64(kCacheBudgetBytes - basePlan.estimatedBytes));
        const auto plan =
            planCaches(panels, 2, 16384, 2, kCacheBudgetBytes, samples, 0, 0, qint64(required));
        ASSERT_TRUE(plan.valid());
        ASSERT_EQ(plan.dpr, basePlan.dpr);
        ASSERT_LT(plan.paintSize.height(), basePlan.paintSize.height());
        const quint64 allowance = kCacheBudgetBytes - plan.estimatedBytes;
        QOpenGLFramebufferObject navigation(plan.sizes[0]), content(plan.sizes[1]);
        QOpenGLFramebufferObject paint(plan.paintSize, paintFormat), resolve(plan.paintSize);
        ASSERT_TRUE(navigation.isValid() && content.isValid() && paint.isValid() &&
                    resolve.isValid());
        content.bind();
        auto* gl = context.context()->functions();
        gl->glClearColor(.1f, .15f, .2f, 1.f);
        gl->glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_TRUE(compositor.prepare(&root, 1, plan.sizes[1], 2, 4, allowance, true));
        ASSERT_EQ(compositor.activeLayerCount(), 1);
        ASSERT_NE(compositor.compose(content.texture(), 2, &paint, &resolve, renderForeground),
                  content.texture());
        const auto frames = compositor.particleFrameCount();
        const auto captures = compositor.foregroundCaptureCount();
        const auto bytes = compositor.allocatedBytes();
        const auto texture = content.texture();
        QSignalSpy requested(&compositor, &GalleryParticleCompositor::frameRequested);
        QSignalSpy invalidated(&compositor, &GalleryParticleCompositor::staticContentInvalidated);
        ASSERT_TRUE(QTest::qWaitFor([&] { return requested.count() >= 2; }, 1500));
        context.makeCurrent();
        // Trying a different candidate is a cost query, not an allocation change.
        EXPECT_GT(compositor.requiredBytes(&root, 2, QSize(3360, 2450), 2, 3.5, true), 0u);
        EXPECT_EQ(compositor.activeLayerCount(), 1);
        EXPECT_EQ(compositor.allocatedBytes(), bytes);
        ASSERT_TRUE(compositor.prepare(&root, 2, plan.sizes[1], 2, 4, allowance, true));
        EXPECT_GT(compositor.particleFrameCount(), frames);
        EXPECT_NE(compositor.compose(content.texture(), 2, &paint, &resolve, renderForeground),
                  content.texture());
        EXPECT_EQ(compositor.foregroundCaptureCount(), captures);
        EXPECT_EQ(invalidated.count(), 0);
        EXPECT_EQ(content.texture(), texture);
        EXPECT_EQ(compositor.allocatedBytes(), bytes);
        EXPECT_LE(plan.estimatedBytes + qint64(bytes), kCacheBudgetBytes);
        EXPECT_EQ(gl->glGetError(), GLenum(GL_NO_ERROR));

        // A stable failed budget remains cheap even while CPU pixels change.
        EXPECT_FALSE(compositor.prepare(&root, 3, plan.sizes[1], 2, 4, 1, true));
        const auto scans = compositor.sourceScanCount();
        for (quint64 revision = 4; revision < 20; ++revision)
            EXPECT_FALSE(compositor.prepare(&root, revision, plan.sizes[1], 2, 4, 1, true));
        EXPECT_EQ(compositor.sourceScanCount(), scans);
        EXPECT_EQ(compositor.allocatedBytes(), 0u);
        EXPECT_TRUE(compositor.prepare(&root, 20, plan.sizes[1], 2, 4, allowance, true));
        EXPECT_EQ(compositor.activeLayerCount(), 1);
        compositor.release();
    }
    context.doneCurrent();
}

TEST(GalleryParticleCompositorTest, Contract_NativeMaskedAncestorFallsBackWithoutLosingCpuPixels)
{
    if (!nativePlatform())
        GTEST_SKIP() << "Requires native GPU composition";
    QOpenGLWidget context;
    context.resize(320, 220);
    context.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&context));
    ASSERT_TRUE(QTest::qWaitFor([&] { return context.isValid(); }));
    QWidget root;
    root.resize(300, 180);
    QWidget masked(&root);
    masked.resize(root.size());
    masked.setMask(QRegion(QRect(0, 0, 250, 150), QRegion::Ellipse));
    ParticleBackdrop backdrop(&masked);
    backdrop.resize(masked.size());
    backdrop.setAnimationEnabled(false);
    ForegroundProbe foreground(QColor(200, 30, 20, 180), &masked);
    foreground.setGeometry(20, 20, 160, 70);
    root.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&root));
    context.makeCurrent();
    const QImage before = capture(root)->toImage();
    GalleryParticleCompositor compositor;
    EXPECT_FALSE(compositor.prepare(&root, 1, root.size(), 1, 1, 32 * 1024 * 1024, true));
    EXPECT_EQ(compositor.activeLayerCount(), 0);
    EXPECT_EQ(compositor.allocatedBytes(), 0u);
    EXPECT_EQ(capture(root)->toImage(), before);
    // Unrelated foreground effects are not silently omitted from occlusion.
    masked.clearMask();
    auto* opacity = new QGraphicsOpacityEffect(&foreground);
    opacity->setOpacity(.5);
    foreground.setGraphicsEffect(opacity);
    const QImage effectBefore = capture(root)->toImage();
    EXPECT_FALSE(compositor.prepare(&root, 2, root.size(), 1, 1, 32 * 1024 * 1024, true));
    EXPECT_EQ(compositor.activeLayerCount(), 0);
    EXPECT_EQ(capture(root)->toImage(), effectBefore);
    foreground.setGraphicsEffect(nullptr);
    EXPECT_TRUE(compositor.prepare(&root, 3, root.size(), 1, 1, 32 * 1024 * 1024, true));
    EXPECT_EQ(compositor.activeLayerCount(), 1);
    masked.move(10, -30);
    EXPECT_TRUE(compositor.prepare(&root, 4, root.size(), 1, 1, 32 * 1024 * 1024, true));
    masked.hide();
    EXPECT_FALSE(compositor.prepare(&root, 5, root.size(), 1, 1, 32 * 1024 * 1024, true));
    masked.show();
    EXPECT_TRUE(compositor.prepare(&root, 6, root.size(), 1, 1, 32 * 1024 * 1024, true));
    compositor.release();
    context.doneCurrent();
}

TEST(GalleryParticleCompositorTest, Contract_NativeNoParticlePageCachesTheNegativeScan)
{
    if (!nativePlatform())
        GTEST_SKIP() << "Requires native GPU composition";
    QOpenGLWidget context;
    context.resize(320, 220);
    context.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&context));
    ASSERT_TRUE(QTest::qWaitFor([&] { return context.isValid(); }));
    QWidget root;
    root.resize(300, 180);
    QWidget hiddenPage(&root);
    ParticleBackdrop hiddenParticle(&hiddenPage);
    hiddenPage.hide();
    root.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&root));
    context.makeCurrent();
    GalleryParticleCompositor compositor;
    for (int i = 0; i < 60; ++i)
        EXPECT_FALSE(compositor.prepare(&root, 1, root.size(), 1, 1, 32 * 1024 * 1024, true));
    EXPECT_EQ(compositor.sourceScanCount(), 1u);
    EXPECT_EQ(compositor.activeLayerCount(), 0);
    EXPECT_EQ(compositor.allocatedBytes(), 0u);
    context.doneCurrent();
}
