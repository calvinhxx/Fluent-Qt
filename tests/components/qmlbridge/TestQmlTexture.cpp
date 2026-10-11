#include "compatibility/private/QuickCompat_p.h"
#include <gtest/gtest.h>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOpenGLWidget>
#include <QPointer>
#include <QPalette>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTest>
#include <QtMath>
#include "QmlBridgeTestModel.h"
#include "components/qmlbridge/QmlSession.h"
#include "components/qmlbridge/QmlTextureLayer.h"
#include "components/qmlbridge/QmlView.h"

namespace {
using fluent::qmlbridge::QmlSession;
using fluent::qmlbridge::QmlView;

qreal textureDpr(const QWidget& window)
{
#if !FLUENT_QUICK_HAS_RENDER_TARGET_DPR
    // Older Quick render targets inherit the presenting window's density.
    return window.devicePixelRatioF();
#else
    return 2;
#endif
}

QSize texturePixels(const QSize& size, qreal dpr)
{
    return QSize(qCeil(size.width() * dpr), qCeil(size.height() * dpr));
}

QColor logicalPixel(const QImage& image, qreal dpr, int x, int y)
{
    return image.pixelColor(qRound(x * dpr), qRound(y * dpr));
}

QImage readTexture(QOpenGLWidget& viewport, unsigned int texture, const QSize& size)
{
    viewport.makeCurrent();
    auto* functions = viewport.context()->functions();
    GLint previous = 0;
    functions->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
    GLuint framebuffer = 0;
    functions->glGenFramebuffers(1, &framebuffer);
    functions->glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    functions->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture,
                                      0);
    QImage image;
    if (functions->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        image = QImage(size, QImage::Format_RGBA8888);
        functions->glReadPixels(0, 0, image.width(), image.height(), GL_RGBA, GL_UNSIGNED_BYTE,
                                image.bits());
        image = image.mirrored();
    }
    functions->glBindFramebuffer(GL_FRAMEBUFFER, GLuint(previous));
    functions->glDeleteFramebuffers(1, &framebuffer);
    viewport.doneCurrent();
    return image;
}

QImage readTexture(QOpenGLWidget& viewport, QmlView& view)
{
    return readTexture(viewport, view.textureId(), view.textureSize());
}

bool render(QOpenGLWidget& viewport, QmlView& view, qreal dpr)
{
    viewport.makeCurrent();
    const bool rendered = view.renderTexture(viewport.context(), dpr, 67108864);
    viewport.doneCurrent();
    return rendered;
}

class QmlTextureTest : public testing::Test {
protected:
    static void SetUpTestSuite() { QmlSession::prepareApplication(); }
};
} // namespace

TEST_F(QmlTextureTest, Contract_NativeTextureKeepsSceneAndReusesStaticFrames)
{
    QWidget window;
    window.resize(680, 240);
    QOpenGLWidget viewport(&window);
    viewport.setGeometry(340, 0, 320, 200);
    QmlSession session(&window);
    QmlBridgeTestModel model;
    ASSERT_TRUE(session.activate());
    auto* view = session.createView(&window);
    ASSERT_NE(view, nullptr);
    view->setGeometry(0, 0, 320, 200);
    ASSERT_TRUE(view->setContextObject(QStringLiteral("bridgeModel"), &model));
    ASSERT_TRUE(view->load(QUrl::fromLocalFile(QStringLiteral(FLUENT_QT_QML_TEST_SOURCE_DIR) +
                                               QStringLiteral("/Texture.qml"))));
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return viewport.isValid(); }));
    const qreal dpr = textureDpr(window);
    const QPointer<QObject> root(view->rootObject());
    auto* quick = view->findChild<QQuickWidget*>();
    ASSERT_NE(quick, nullptr);
    const QPointer<QQmlEngine> engine(quick->engine());
    ASSERT_FALSE(root.isNull());
    auto* rootItem = qobject_cast<QQuickItem*>(root.data());
    ASSERT_NE(rootItem, nullptr);
    ASSERT_TRUE(root->setProperty("localCounter", 41));
    QSignalSpy failures(view, &QmlView::textureRenderingFailed);
    int counter = 41;
    window.raise();
    window.activateWindow();
    ASSERT_TRUE(QTest::qWaitForWindowActive(&window));
    // Older Wayland helpers can report exposure before activation arrives.
    ASSERT_TRUE(QTest::qWaitFor([&] { return window.isActiveWindow(); }));

    for (int pass = 0; pass < 2; ++pass) {
        SCOPED_TRACE(pass);
        ASSERT_TRUE(view->setTextureRenderingEnabled(true));
        ASSERT_TRUE(render(viewport, *view, dpr));
        EXPECT_TRUE(quick->isHidden());
        EXPECT_EQ(view->rootObject(), root);
        EXPECT_EQ(quick->engine(), engine);
        EXPECT_EQ(root->property("localCounter").toInt(), counter);
        ASSERT_NE(view->textureId(), 0u);
        EXPECT_EQ(view->textureSize(), texturePixels(QSize(320, 200), dpr));
        const QImage image = readTexture(viewport, *view);
        ASSERT_FALSE(image.isNull());
        EXPECT_EQ(logicalPixel(image, dpr, 10, 10), QColor(Qt::red));
        EXPECT_EQ(logicalPixel(image, dpr, 310, 190), QColor(Qt::blue));
        EXPECT_EQ(logicalPixel(image, dpr, 60, 100), QColor(Qt::green));
        EXPECT_EQ(logicalPixel(image, dpr, 200, 100), QColor(QStringLiteral("#17212b")));
        // Qt can request a resource-initialization frame during the first sync.
        // Require bounded convergence before checking the steady idle contract.
        quint64 previousRevision = view->textureRevision();
        ASSERT_TRUE(QTest::qWaitFor(
            [&] {
                if (!render(viewport, *view, dpr))
                    return false;
                const quint64 current = view->textureRevision();
                const bool stable = current == previousRevision;
                previousRevision = current;
                return stable;
            },
            1000));
        const auto revision = view->textureRevision();
        const auto frames = view->textureStatistics()[QStringLiteral("renderedFrames")];
        EXPECT_GT(frames.toULongLong(), 0u);
        for (int idle = 0; idle < 4; ++idle) {
            ASSERT_TRUE(render(viewport, *view, dpr));
            EXPECT_EQ(view->textureRevision(), revision);
            EXPECT_EQ(view->textureStatistics()[QStringLiteral("renderedFrames")], frames);
        }
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, QPoint(100, 50));
        EXPECT_TRUE(rootItem->hasActiveFocus());
        QTest::keyClick(view, Qt::Key_Return);
        counter += 2;
        EXPECT_EQ(root->property("localCounter").toInt(), counter);
        ASSERT_TRUE(view->setTextureRenderingEnabled(false));
        EXPECT_EQ(view->rootObject(), root);
        EXPECT_EQ(quick->engine(), engine);
        EXPECT_EQ(root->property("localCounter").toInt(), counter);
        ASSERT_TRUE(QTest::qWaitFor([&] {
            return quick->isVisible() && quick->geometry() == view->contentsRect();
        })) << "Native presentation must return after the queued layout completes";
    }

    ASSERT_TRUE(view->setTextureRenderingEnabled(true));
    ASSERT_TRUE(render(viewport, *view, dpr));
    const auto revision = view->textureRevision();
    model.setProgress(75);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return render(viewport, *view, dpr) && view->textureRevision() > revision; }));
    const auto updated = readTexture(viewport, *view);
    ASSERT_FALSE(updated.isNull());
    EXPECT_EQ(logicalPixel(updated, dpr, 180, 100), QColor(Qt::green));
    view->resize(360, 220);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return render(viewport, *view, dpr) &&
               view->textureSize() == texturePixels(QSize(360, 220), dpr);
    }));
    const auto resized = readTexture(viewport, *view);
    ASSERT_FALSE(resized.isNull());
    EXPECT_EQ(logicalPixel(resized, dpr, 350, 210), QColor(Qt::blue));
    viewport.makeCurrent();
    view->releaseTexture();
    viewport.doneCurrent();
    EXPECT_EQ(view->textureId(), 0u);
    EXPECT_EQ(view->rootObject(), root);
    ASSERT_TRUE(render(viewport, *view, dpr));
    EXPECT_NE(view->textureId(), 0u);

    QObject::connect(&model, &QmlBridgeTestModel::stopRequested, &session, [&] {
        EXPECT_TRUE(session.deactivate());
        EXPECT_EQ(view->rootObject(), nullptr);
        EXPECT_EQ(view->textureId(), 0u);
        EXPECT_FALSE(root.isNull());
        EXPECT_FALSE(engine.isNull());
    });
    ASSERT_TRUE(QMetaObject::invokeMethod(root, "stopAndContinue", Qt::DirectConnection));
    ASSERT_FALSE(root.isNull());
    EXPECT_EQ(root->property("completedAfterStop").toInt(), 1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return root.isNull() && engine.isNull(); }));
    EXPECT_EQ(view->status(), QmlView::Unloaded);
    EXPECT_FALSE(view->isActive());
    EXPECT_EQ(model.progress(), 75);
    EXPECT_EQ(failures.count(), 0);
}

TEST_F(QmlTextureTest, Contract_NativeErrorsRespectTheBorrowedContextBoundary)
{
    QWidget window;
    window.resize(720, 260);
    QWidget panel(&window);
    panel.setGeometry(0, 0, 360, 240);
    QOpenGLWidget viewport(&window);
    viewport.setGeometry(380, 0, 320, 240);
    QmlBridgeTestModel model;
    QmlSession session(&panel);
    ASSERT_TRUE(session.activate());
    auto* view = session.createView(&panel);
    ASSERT_NE(view, nullptr);
    view->setGeometry(0, 0, 320, 200);
    ASSERT_TRUE(view->setContextObject(QStringLiteral("bridgeModel"), &model));
    ASSERT_TRUE(view->load(QUrl::fromLocalFile(QStringLiteral(FLUENT_QT_QML_TEST_SOURCE_DIR) +
                                               QStringLiteral("/Texture.qml"))));
    const QPointer<QObject> root(view->rootObject());
    ASSERT_FALSE(root.isNull());
    fluent::qmlbridge::QmlTextureLayer layer;
    layer.setRootWidget(&panel);
    ASSERT_TRUE(layer.synchronize());
    QSignalSpy failures(&layer, &fluent::qmlbridge::QmlTextureLayer::renderingFailed);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return viewport.isValid(); }));
    const QRectF source(panel.rect());
    const qreal dpr = view->devicePixelRatioF();
    const auto prepare = [&] {
        viewport.makeCurrent();
        const bool prepared = layer.prepareFrame(viewport.context(), source, dpr, 67108864);
        viewport.doneCurrent();
        return prepared;
    };
    ASSERT_TRUE(prepare());
    const auto revision = view->textureRevision();
    model.setProgress(75);
    viewport.makeCurrent();
    auto* gl = viewport.context()->functions();
    while (gl->glGetError() != GL_NO_ERROR) {
    }
    gl->glEnable(GLenum(0xffffffffu));
    ASSERT_EQ(gl->glGetError(), GLenum(GL_INVALID_ENUM));
    gl->glEnable(GLenum(0xffffffffu)); // Leave a real caller error pending.
    const bool inheritedErrorPrepared =
        layer.prepareFrame(viewport.context(), source, dpr, 67108864);
    EXPECT_EQ(gl->glGetError(), GLenum(GL_NO_ERROR));
    viewport.doneCurrent();
    ASSERT_TRUE(inheritedErrorPrepared);
    ASSERT_TRUE(QTest::qWaitFor([&] { return prepare() && view->textureRevision() > revision; }));
    EXPECT_EQ(failures.count(), 0);
    const QImage updated = readTexture(viewport, *view);
    ASSERT_FALSE(updated.isNull());
    EXPECT_EQ(updated.pixelColor(qRound(180 * dpr), qRound(100 * dpr)), QColor(Qt::green));

    auto* item = qobject_cast<QQuickItem*>(root.data());
    ASSERT_NE(item, nullptr);
    ASSERT_NE(item->window(), nullptr);
    bool injected = false;
    const auto inject = [&] {
        if (!injected) {
            injected = true;
            QOpenGLContext::currentContext()->functions()->glEnable(GLenum(0xffffffffu));
        }
    };
#if FLUENT_QUICK_USES_RHI
    const auto frameFinished = &QQuickWindow::afterFrameEnd;
#else
    const auto frameFinished = &QQuickWindow::afterRendering;
#endif
    const auto connection =
        QObject::connect(item->window(), frameFinished, &layer, inject, Qt::DirectConnection);
    model.setProgress(25);
    ASSERT_TRUE(QTest::qWaitFor([&] { return !prepare() && injected; }));
    QObject::disconnect(connection);
    EXPECT_EQ(failures.count(), 1);
    EXPECT_EQ(view->textureId(), 0u);
    EXPECT_FALSE(layer.synchronize());
    EXPECT_FALSE(prepare());
    EXPECT_EQ(failures.count(), 1);

    // The host leaves its render callback before restoring native presentation.
    layer.release();
    EXPECT_FALSE(view->isTextureRenderingEnabled());
    EXPECT_EQ(view->rootObject(), root);
    ASSERT_TRUE(layer.synchronize());
    ASSERT_TRUE(prepare());
    EXPECT_NE(view->textureId(), 0u);
    EXPECT_EQ(failures.count(), 1);
}

TEST_F(QmlTextureTest, Contract_NativeLayerClipsAndReusesBorrowedTextures)
{
    QWidget window;
    window.resize(720, 260);
    QWidget panel(&window);
    panel.setGeometry(0, 0, 360, 240);
    QWidget clip(&panel);
    clip.setGeometry(20, 20, 200, 140);
    QOpenGLWidget viewport(&window);
    viewport.setGeometry(380, 0, 320, 240);
    QmlBridgeTestModel model;
    QmlSession session(&panel);
    ASSERT_TRUE(session.activate());
    auto* view = session.createView(&clip);
    ASSERT_NE(view, nullptr);
    view->setGeometry(0, 0, 320, 200);
    ASSERT_TRUE(view->setContextObject(QStringLiteral("bridgeModel"), &model));
    ASSERT_TRUE(view->load(QUrl::fromLocalFile(QStringLiteral(FLUENT_QT_QML_TEST_SOURCE_DIR) +
                                               QStringLiteral("/Texture.qml"))));
    const QPointer<QObject> root(view->rootObject());
    fluent::qmlbridge::QmlTextureLayer layer;
    layer.setRootWidget(&panel);
    ASSERT_TRUE(layer.synchronize());
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return viewport.isValid(); }));
    const QRectF source(0, 0, 360, 240);
    const qreal dpr = textureDpr(window);
    const quint64 required = layer.requiredBytes(source, dpr);
    ASSERT_GT(required, 0u);
    ASSERT_LT(required, quint64(67108864));
    viewport.makeCurrent();
    ASSERT_TRUE(layer.prepareFrame(viewport.context(), source, dpr, 67108864));
    auto base = std::make_unique<QOpenGLFramebufferObject>(texturePixels(QSize(360, 240), dpr));
    ASSERT_TRUE(base->isValid());
    ASSERT_TRUE(base->bind());
    auto* functions = viewport.context()->functions();
    functions->glClearColor(0, 1, 1, 1);
    functions->glClear(GL_COLOR_BUFFER_BIT);
    base->release();
    const unsigned int texture = layer.compose(base->texture(), base->size(), 7);
    ASSERT_NE(texture, 0u);
    viewport.doneCurrent();
    const QImage composed = readTexture(viewport, texture, base->size());
    ASSERT_FALSE(composed.isNull());
    EXPECT_EQ(logicalPixel(composed, dpr, 30, 30), QColor(Qt::red));
    EXPECT_EQ(logicalPixel(composed, dpr, 80, 120), QColor(Qt::green));
    EXPECT_EQ(logicalPixel(composed, dpr, 200, 120), QColor(QStringLiteral("#17212b")));
    EXPECT_EQ(logicalPixel(composed, dpr, 230, 30), QColor(Qt::cyan))
        << "A QML texture must not paint outside its clipping ancestor";
    EXPECT_EQ(logicalPixel(composed, dpr, 330, 210), QColor(Qt::cyan));
    const auto frames = view->textureStatistics()[QStringLiteral("renderedFrames")];
    const auto compositions = layer.statistics()[QStringLiteral("compositions")];
    ASSERT_GT(compositions.toULongLong(), 0u);
    viewport.makeCurrent();
    ASSERT_TRUE(layer.prepareFrame(viewport.context(), source, dpr, 67108864));
    EXPECT_EQ(layer.compose(base->texture(), base->size(), 7), texture);
    EXPECT_EQ(layer.statistics()[QStringLiteral("compositions")], compositions);
    EXPECT_EQ(view->textureStatistics()[QStringLiteral("renderedFrames")], frames);
    EXPECT_NE(layer.compose(base->texture(), base->size(), 8), 0u);
    EXPECT_EQ(layer.statistics()[QStringLiteral("compositions")].toULongLong(),
              compositions.toULongLong() + 1);
    EXPECT_EQ(view->textureStatistics()[QStringLiteral("renderedFrames")], frames);
    viewport.doneCurrent();
    model.setProgress(75);
    ASSERT_TRUE(QTest::qWaitFor([&] {
        viewport.makeCurrent();
        const bool prepared = layer.prepareFrame(viewport.context(), source, dpr, 67108864);
        const auto refreshed = prepared ? layer.compose(base->texture(), base->size(), 8) : 0u;
        viewport.doneCurrent();
        const QImage image = refreshed ? readTexture(viewport, refreshed, base->size()) : QImage();
        return !image.isNull() && logicalPixel(image, dpr, 200, 120) == QColor(Qt::green);
    })) << "A new QML texture revision must refresh the composed panel without a Widget repaint";
    EXPECT_GT(view->textureStatistics()[QStringLiteral("renderedFrames")].toULongLong(),
              frames.toULongLong());

    const int count = root->property("localCounter").toInt();
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(230, 30));
    EXPECT_EQ(root->property("localCounter").toInt(), count);
    QTest::mouseClick(window.windowHandle(), Qt::LeftButton, Qt::NoModifier, QPoint(30, 30));
    EXPECT_EQ(root->property("localCounter").toInt(), count + 1);
    // Native event delivery can leave Qt's widget context current on Windows.
    // Establish the no-context cleanup case independently of that platform detail.
    if (auto* current = QOpenGLContext::currentContext())
        current->doneCurrent();
    ASSERT_EQ(QOpenGLContext::currentContext(), nullptr);
    layer.setRootWidget(nullptr);
    EXPECT_EQ(layer.textureId(), 0u);
    EXPECT_EQ(view->textureId(), 0u);
    EXPECT_EQ(layer.statistics()[QStringLiteral("allocatedBytes")].toULongLong(), 0u);
    EXPECT_EQ(QOpenGLContext::currentContext(), nullptr);
    EXPECT_EQ(view->rootObject(), root);
    EXPECT_TRUE(view->isTextureRenderingEnabled());
    layer.setRootWidget(&panel);
    ASSERT_TRUE(layer.synchronize());
    const auto rebuild = [&] {
        viewport.makeCurrent();
        const bool prepared = layer.prepareFrame(viewport.context(), source, dpr, 67108864);
        const unsigned int composed =
            prepared ? layer.compose(base->texture(), base->size(), 9) : 0u;
        const GLenum error = functions->glGetError();
        viewport.doneCurrent();
        EXPECT_EQ(error, GLenum(GL_NO_ERROR));
        return prepared && composed != 0;
    };
    ASSERT_TRUE(rebuild());
    QOpenGLContext foreign;
    foreign.setFormat(viewport.context()->format());
    ASSERT_TRUE(foreign.create());
    QOffscreenSurface foreignSurface;
    foreignSurface.setFormat(foreign.format());
    foreignSurface.create();
    ASSERT_TRUE(foreignSurface.isValid());
    ASSERT_TRUE(foreign.makeCurrent(&foreignSurface));
    layer.clearFrameCaches();
    EXPECT_EQ(QOpenGLContext::currentContext(), &foreign);
    EXPECT_EQ(foreign.surface(), &foreignSurface);
    EXPECT_EQ(foreign.functions()->glGetError(), GLenum(GL_NO_ERROR));
    EXPECT_EQ(layer.textureId(), 0u);
    EXPECT_EQ(view->textureId(), 0u);
    EXPECT_EQ(layer.statistics()[QStringLiteral("allocatedBytes")].toULongLong(), 0u);
    foreign.doneCurrent();
    ASSERT_TRUE(rebuild()) << "The original owner context must remain usable after foreign cleanup";
    ASSERT_TRUE(foreign.makeCurrent(&foreignSurface));
    layer.release();
    EXPECT_EQ(QOpenGLContext::currentContext(), &foreign);
    EXPECT_EQ(foreign.surface(), &foreignSurface);
    EXPECT_EQ(foreign.functions()->glGetError(), GLenum(GL_NO_ERROR));
    foreign.doneCurrent();
    viewport.makeCurrent();
    base.reset();
    EXPECT_EQ(functions->glGetError(), GLenum(GL_NO_ERROR));
    viewport.doneCurrent();
    EXPECT_FALSE(view->isTextureRenderingEnabled());
    EXPECT_EQ(view->rootObject(), root);
    EXPECT_EQ(root->property("localCounter").toInt(), count + 1);
}

TEST_F(QmlTextureTest, Contract_NativeLayerDefersReentrantCleanup)
{
    QWidget window;
    window.resize(900, 260);
    QWidget page(&window);
    page.setGeometry(0, 0, 360, 240);
    QWidget otherPage(&window);
    otherPage.setGeometry(370, 0, 160, 240);
    QOpenGLWidget viewport(&window);
    viewport.setGeometry(550, 0, 320, 240);
    QmlBridgeTestModel model;
    QmlSession session(&page);
    ASSERT_TRUE(session.activate());
    auto* view = session.createView(&page);
    ASSERT_NE(view, nullptr);
    view->setGeometry(0, 0, 320, 200);
    ASSERT_TRUE(view->setContextObject(QStringLiteral("bridgeModel"), &model));
    ASSERT_TRUE(view->load(QUrl::fromLocalFile(QStringLiteral(FLUENT_QT_QML_TEST_SOURCE_DIR) +
                                               QStringLiteral("/Texture.qml"))));
    const QPointer<QObject> root(view->rootObject());
    ASSERT_FALSE(root.isNull());
    ASSERT_TRUE(root->setProperty("localCounter", 41));
    auto* quick = view->findChild<QQuickWidget*>();
    ASSERT_NE(quick, nullptr);
    const QPointer<QQmlEngine> engine(quick->engine());
    fluent::qmlbridge::QmlTextureLayer layer;
    layer.setRootWidget(&page);
    ASSERT_TRUE(layer.synchronize());
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return viewport.isValid(); }));

    bool preparing = false;
    const qreal dpr = textureDpr(window);
    bool switched = false;
    bool released = false;
    QSignalSpy errors(&layer, &fluent::qmlbridge::QmlTextureLayer::renderingFailed);
    auto* item = qobject_cast<QQuickItem*>(root.data());
    ASSERT_NE(item, nullptr);
    ASSERT_NE(item->window(), nullptr);
    const auto switchConnection = QObject::connect(
        item->window(), &QQuickWindow::beforeSynchronizing, &layer,
        [&] {
            // Use the real sync boundary, independent of scene materials or
            // whether Qt needs a follow-up resource-initialization frame.
            if (preparing && !switched) {
                switched = true;
                layer.setRootWidget(&otherPage);
            }
        },
        Qt::DirectConnection);
    const QRectF source(0, 0, 360, 240);
    viewport.makeCurrent();
    preparing = true;
    const bool oldPagePrepared = layer.prepareFrame(viewport.context(), source, dpr, 67108864);
    preparing = false;
    QObject::disconnect(switchConnection);
    EXPECT_TRUE(switched);
    EXPECT_FALSE(oldPagePrepared);
    EXPECT_EQ(layer.rootWidget(), &otherPage);
    EXPECT_EQ(layer.textureId(), 0u);
    EXPECT_EQ(view->textureId(), 0u);
    EXPECT_EQ(layer.statistics()[QStringLiteral("visibleViews")].toInt(), 0);
    EXPECT_EQ(layer.statistics()[QStringLiteral("allocatedBytes")].toULongLong(), 0u);
    EXPECT_EQ(QOpenGLContext::currentContext(), viewport.context());
    ASSERT_TRUE(layer.synchronize());
    ASSERT_TRUE(layer.prepareFrame(viewport.context(), source, dpr, 67108864));
    EXPECT_EQ(layer.statistics()[QStringLiteral("visibleViews")].toInt(), 0);
    viewport.doneCurrent();

    layer.setRootWidget(&page);
    ASSERT_TRUE(layer.synchronize());
    ASSERT_NE(item->window(), nullptr);
    const auto releaseConnection = QObject::connect(
        item->window(), &QQuickWindow::beforeSynchronizing, &layer,
        [&] {
            if (preparing && !released) {
                released = true;
                layer.release();
            }
        },
        Qt::DirectConnection);
    viewport.makeCurrent();
    preparing = true;
    const bool releasedPagePrepared = layer.prepareFrame(viewport.context(), source, dpr, 67108864);
    preparing = false;
    QObject::disconnect(releaseConnection);
    EXPECT_TRUE(released);
    EXPECT_FALSE(releasedPagePrepared);
    EXPECT_EQ(layer.textureId(), 0u);
    EXPECT_EQ(view->textureId(), 0u);
    EXPECT_EQ(layer.statistics()[QStringLiteral("views")].toInt(), 0);
    EXPECT_EQ(layer.statistics()[QStringLiteral("visibleViews")].toInt(), 0);
    EXPECT_EQ(layer.statistics()[QStringLiteral("allocatedBytes")].toULongLong(), 0u);
    EXPECT_FALSE(view->isTextureRenderingEnabled());
    EXPECT_EQ(QOpenGLContext::currentContext(), viewport.context());
    EXPECT_EQ(viewport.context()->functions()->glGetError(), GLenum(GL_NO_ERROR));
    viewport.doneCurrent();
    ASSERT_TRUE(QTest::qWaitFor([&] {
        return quick->isVisible() && quick->geometry() == view->contentsRect();
    })) << "Reentrant release must restore native presentation after rendering returns";
    EXPECT_EQ(view->rootObject(), root);
    EXPECT_EQ(quick->engine(), engine);
    EXPECT_EQ(root->property("localCounter").toInt(), 41);

    ASSERT_TRUE(layer.synchronize());
    viewport.makeCurrent();
    ASSERT_TRUE(layer.prepareFrame(viewport.context(), source, dpr, 67108864));
    EXPECT_NE(view->textureId(), 0u);
    EXPECT_EQ(layer.statistics()[QStringLiteral("visibleViews")].toInt(), 1);
    EXPECT_EQ(viewport.context()->functions()->glGetError(), GLenum(GL_NO_ERROR));
    viewport.doneCurrent();
    EXPECT_EQ(view->rootObject(), root);
    EXPECT_EQ(quick->engine(), engine);
    EXPECT_EQ(errors.count(), 0);
}

TEST_F(QmlTextureTest, Contract_RoundedScenesPreserveAlpha)
{
    const auto oldTheme = fluent::FluentElement::currentTheme();
    const auto restoreTheme = qScopeGuard([&] { fluent::FluentElement::setTheme(oldTheme); });
    const QColor hostColor(QStringLiteral("#b64089"));
    QWidget window;
    window.resize(400, 160);
    QWidget host;
    host.resize(160, 100);
    QPalette palette = host.palette();
    palette.setColor(QPalette::Window, hostColor);
    host.setPalette(palette);
    host.setAutoFillBackground(true);
    QOpenGLWidget viewport(&window);
    viewport.setGeometry(200, 0, 160, 100);
    QmlSession session(&host);
    ASSERT_TRUE(session.activate());
    auto* view = session.createView(&host);
    ASSERT_NE(view, nullptr);
    view->setGeometry(host.rect());
    ASSERT_TRUE(view->load(QUrl::fromLocalFile(QStringLiteral(FLUENT_QT_QML_TEST_SOURCE_DIR) +
                                               QStringLiteral("/Rounded.qml"))));
    auto* quick = view->findChild<QQuickWidget*>();
    ASSERT_NE(quick, nullptr);
    const QPointer<QObject> root(view->rootObject());
    const QPointer<QQmlEngine> engine(quick->engine());
    ASSERT_FALSE(root.isNull());
    ASSERT_FALSE(engine.isNull());
    ASSERT_EQ(host.parentWidget(), nullptr);
    ASSERT_EQ(view->status(), QmlView::Ready);
    // Gallery adopts an already loaded preview into its card. Qt recreates the
    // Quick window during this step; alpha and scene ownership must survive it.
    host.setParent(&window);
    host.setGeometry(0, 0, 160, 100);
    host.show();
    EXPECT_EQ(view->rootObject(), root);
    EXPECT_EQ(quick->engine(), engine);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return viewport.isValid(); }));
    EXPECT_EQ(view->rootObject(), root);
    EXPECT_EQ(quick->engine(), engine);
    QSignalSpy failures(view, &QmlView::textureRenderingFailed);
    fluent::qmlbridge::QmlTextureLayer layer;
    layer.setRootWidget(&host);
    const qreal dpr = textureDpr(window);
    viewport.makeCurrent();
    auto base = std::make_unique<QOpenGLFramebufferObject>(texturePixels(host.size(), dpr));
    ASSERT_TRUE(base->isValid());
    ASSERT_TRUE(base->bind());
    auto* functions = viewport.context()->functions();
    functions->glClearColor(hostColor.redF(), hostColor.greenF(), hostColor.blueF(), 1);
    functions->glClear(GL_COLOR_BUFFER_BIT);
    base->release();
    viewport.doneCurrent();
    const auto releaseBase = qScopeGuard([&] {
        viewport.makeCurrent();
        base.reset();
        viewport.doneCurrent();
    });
    const auto transparentCorners = [](const QImage& image) {
        if (image.isNull())
            return false;
        for (const QPoint& point :
             {QPoint(1, 1), QPoint(image.width() - 2, 1), QPoint(1, image.height() - 2),
              QPoint(image.width() - 2, image.height() - 2)})
            if (image.pixelColor(point).alpha() != 0)
                return false;
        return true;
    };
    const auto hasAntialiasing = [](const QImage& image) {
        for (int y = 0; y < qMin(64, image.height() / 2); ++y)
            for (int x = 0; x < qMin(64, image.width() / 2); ++x) {
                const int alpha = image.pixelColor(x, y).alpha();
                if (alpha > 0 && alpha < 255)
                    return true;
            }
        return false;
    };
    ASSERT_TRUE(QTest::qWaitFor([&] { return transparentCorners(quick->grabFramebuffer()); }))
        << "Adopting a loaded preview must preserve alpha before any theme update";
    quint64 baseRevision = 0;
    for (auto mode : {fluent::FluentElement::Light, fluent::FluentElement::Dark,
                      fluent::FluentElement::Light}) {
        SCOPED_TRACE(int(mode));
        layer.release();
        fluent::FluentElement::setTheme(mode);
        EXPECT_EQ(view->themeObject()->property("dark").toBool(),
                  mode == fluent::FluentElement::Dark);
        const QColor surface = view->themeObject()->property("surface").value<QColor>();
        QImage native;
        ASSERT_TRUE(QTest::qWaitFor([&] {
            // Hidden Quick widgets can render a readback before their queued
            // native show. Require the host's actual presentation first.
            if (!quick->isVisible() || quick->geometry() != view->contentsRect())
                return false;
            native = quick->grabFramebuffer();
            return transparentCorners(native) &&
                   native.pixelColor(native.rect().center()) == surface;
        })) << "Native Quick must clear rounded corners to alpha zero";
        EXPECT_TRUE(hasAntialiasing(native));
        const QImage painted = host.grab().toImage();
        ASSERT_FALSE(painted.isNull());
        EXPECT_EQ(painted.pixelColor(1, 1), hostColor)
            << "The real QWidget host must remain visible behind the rounded corner";
        EXPECT_EQ(painted.pixelColor(painted.rect().center()), surface);

        ASSERT_TRUE(layer.synchronize());
        viewport.makeCurrent();
        const bool prepared = layer.prepareFrame(viewport.context(), host.rect(), dpr, 67108864);
        const unsigned int composed =
            prepared ? layer.compose(base->texture(), base->size(), ++baseRevision) : 0u;
        viewport.doneCurrent();
        ASSERT_TRUE(prepared);
        ASSERT_NE(composed, 0u);
        EXPECT_TRUE(quick->isHidden());
        const QImage texture = readTexture(viewport, *view);
        ASSERT_FALSE(texture.isNull());
        EXPECT_TRUE(transparentCorners(texture));
        EXPECT_TRUE(hasAntialiasing(texture));
        EXPECT_EQ(texture.pixelColor(texture.rect().center()), surface);
        const QImage composite = readTexture(viewport, composed, base->size());
        ASSERT_FALSE(composite.isNull());
        EXPECT_EQ(composite.pixelColor(2, 2), hostColor);
        EXPECT_EQ(composite.pixelColor(composite.width() - 3, composite.height() - 3), hostColor);
        EXPECT_EQ(composite.pixelColor(composite.rect().center()), surface);
        EXPECT_EQ(view->rootObject(), root);
        EXPECT_EQ(quick->engine(), engine);
    }
    layer.release();
    EXPECT_EQ(failures.count(), 0);
}
