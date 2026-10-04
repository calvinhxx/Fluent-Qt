#include <gtest/gtest.h>

#include <QApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTest>
#include <QWindow>
#ifdef FLUENT_QT_HAS_SPATIAL
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#endif
#include <QVBoxLayout>

#include <cstring>

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>

#include "components/windowing/Window.h"
#include "components/windowing/WindowBackdrop.h"
#include "components/windowing/TitleBar.h"
#include "compatibility/QtCompat.h"

using fluent::windowing::BackdropBackend;
using fluent::windowing::BackdropEffect;
using fluent::windowing::BackdropSurfaceMode;
using fluent::windowing::Window;

namespace {

constexpr char kBackdropBaseIdentifier[] = "fluentBackdropBase";
constexpr char kBackdropTintIdentifier[] = "fluentBackdropTint";
constexpr char kLegacyBackdropWindowIdentifier[] = "fluentBackdropWindow";

SEL selector(const char* name)
{
    return sel_registerName(name);
}

bool respondsTo(id receiver, const char* name)
{
    if (!receiver)
        return false;
    using Send = BOOL (*)(id, SEL, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector("respondsToSelector:"),
                                                selector(name));
}

id sendId(id receiver, const char* name)
{
    using Send = id (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

id sendUnsignedLongReturnsId(id receiver, const char* name, unsigned long value)
{
    using Send = id (*)(id, SEL, unsigned long);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

unsigned long sendUnsignedLong(id receiver, const char* name)
{
    using Send = unsigned long (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

long sendLong(id receiver, const char* name)
{
    using Send = long (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

bool sendBool(id receiver, const char* name)
{
    using Send = BOOL (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

QColor nativeBackgroundColor(id nativeWindow)
{
    id color = sendId(nativeWindow, "backgroundColor");
    if (!color)
        return QColor();
    const auto cgColor = reinterpret_cast<CGColorRef>(sendId(color, "CGColor"));
    const auto space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    const auto converted =
        CGColorCreateCopyByMatchingToColorSpace(space, kCGRenderingIntentDefault, cgColor, nullptr);
    CGColorSpaceRelease(space);
    if (!converted)
        return QColor();
    const auto* components = CGColorGetComponents(converted);
    const auto result =
        QColor::fromRgbF(components[0], components[1], components[2], components[3]);
    CGColorRelease(converted);
    return result;
}

#ifdef FLUENT_QT_HAS_SPATIAL
class BackdropCommitOpenGLWidget final : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    explicit BackdropCommitOpenGLWidget(Window* window, QWidget* parent)
        : QOpenGLWidget(parent), m_window(window)
    {
        auto alphaFormat = format();
        alphaFormat.setAlphaBufferSize(8);
        setFormat(alphaFormat);
    }

    bool opaqueFramePainted = false;

protected:
    void initializeGL() override { initializeOpenGLFunctions(); }
    void paintGL() override
    {
        opaqueFramePainted =
            m_window->backdropState().surfaceMode != BackdropSurfaceMode::CompositedTransparent;
        glClearColor(0.15f, 0.15f, 0.15f, opaqueFramePainted ? 1.0f : 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

private:
    Window* m_window;
};
#endif

CGRect sendRect(id receiver, const char* name)
{
#if defined(__x86_64__)
    CGRect rect = CGRectNull;
    using Send = void (*)(CGRect*, id, SEL);
    reinterpret_cast<Send>(objc_msgSend_stret)(&rect, receiver, selector(name));
    return rect;
#else
    using Send = CGRect (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
#endif
}

bool identifierEquals(id object, const char* expected)
{
    if (!object || !respondsTo(object, "identifier"))
        return false;
    id identifier = sendId(object, "identifier");
    if (!identifier || !respondsTo(identifier, "UTF8String"))
        return false;
    using Send = const char* (*)(id, SEL);
    const char* value = reinterpret_cast<Send>(objc_msgSend)(identifier, selector("UTF8String"));
    return value && std::strcmp(value, expected) == 0;
}

id nativeWindowFor(QWidget* window)
{
    if (!window)
        return nil;
    id nativeView = reinterpret_cast<id>(window->winId());
    return nativeView && respondsTo(nativeView, "window") ? sendId(nativeView, "window") : nil;
}

id directSubviewWithIdentifier(id superview, const char* identifier, int* count = nullptr)
{
    if (count)
        *count = 0;
    if (!superview || !respondsTo(superview, "subviews"))
        return nil;

    id subviews = sendId(superview, "subviews");
    if (!subviews || !respondsTo(subviews, "count") || !respondsTo(subviews, "objectAtIndex:")) {
        return nil;
    }

    id found = nil;
    const unsigned long subviewCount = sendUnsignedLong(subviews, "count");
    for (unsigned long index = 0; index < subviewCount; ++index) {
        id view = sendUnsignedLongReturnsId(subviews, "objectAtIndex:", index);
        if (!identifierEquals(view, identifier))
            continue;
        if (count)
            ++(*count);
        if (!found)
            found = view;
    }
    return found;
}

bool isSubviewBelow(id superview, id lowerView, id upperView)
{
    if (!superview || !lowerView || !upperView || !respondsTo(superview, "subviews"))
        return false;

    id subviews = sendId(superview, "subviews");
    const unsigned long count = subviews ? sendUnsignedLong(subviews, "count") : 0;
    unsigned long lowerIndex = count;
    unsigned long upperIndex = count;
    for (unsigned long index = 0; index < count; ++index) {
        id view = sendUnsignedLongReturnsId(subviews, "objectAtIndex:", index);
        if (view == lowerView)
            lowerIndex = index;
        if (view == upperView)
            upperIndex = index;
    }
    return lowerIndex < upperIndex;
}

int legacyBackdropWindowCount(id nativeWindow)
{
    if (!nativeWindow || !respondsTo(nativeWindow, "childWindows"))
        return 0;

    id children = sendId(nativeWindow, "childWindows");
    const unsigned long count = children ? sendUnsignedLong(children, "count") : 0;
    int matches = 0;
    for (unsigned long index = 0; index < count; ++index) {
        id child = sendUnsignedLongReturnsId(children, "objectAtIndex:", index);
        if (identifierEquals(child, kLegacyBackdropWindowIdentifier))
            ++matches;
    }
    return matches;
}

struct BackdropHierarchy {
    id nativeView = nil;
    id nativeWindow = nil;
    id contentView = nil;
    id hostView = nil;
    id effectView = nil;
    id tintView = nil;
    int effectCount = 0;
    int tintCount = 0;
};

BackdropHierarchy resolveBackdropHierarchy(QWidget* window)
{
    BackdropHierarchy hierarchy;
    hierarchy.nativeView = reinterpret_cast<id>(window->winId());
    hierarchy.nativeWindow = nativeWindowFor(window);
    if (!hierarchy.nativeWindow || !respondsTo(hierarchy.nativeWindow, "contentView"))
        return hierarchy;

    hierarchy.contentView = sendId(hierarchy.nativeWindow, "contentView");
    if (!hierarchy.contentView || !respondsTo(hierarchy.contentView, "superview"))
        return hierarchy;

    hierarchy.hostView = sendId(hierarchy.contentView, "superview");
    hierarchy.effectView = directSubviewWithIdentifier(hierarchy.hostView, kBackdropBaseIdentifier,
                                                       &hierarchy.effectCount);
    hierarchy.tintView = directSubviewWithIdentifier(hierarchy.effectView, kBackdropTintIdentifier,
                                                     &hierarchy.tintCount);
    return hierarchy;
}

void expectBackdropHierarchy(const BackdropHierarchy& hierarchy, long expectedMaterial)
{
    constexpr long NSVisualEffectBlendingModeBehindWindow = 0;
    constexpr long NSVisualEffectStateFollowsWindowActiveState = 0;

    ASSERT_NE(hierarchy.nativeView, nil);
    ASSERT_NE(hierarchy.nativeWindow, nil);
    ASSERT_NE(hierarchy.contentView, nil);
    ASSERT_NE(hierarchy.hostView, nil);
    ASSERT_NE(hierarchy.effectView, nil);
    ASSERT_NE(hierarchy.tintView, nil);
    EXPECT_EQ(hierarchy.contentView, hierarchy.nativeView)
        << "FluentQt must preserve Qt's QNSView as NSWindow.contentView";
    EXPECT_EQ(hierarchy.effectCount, 1) << "Material re-application must not stack effect views";
    EXPECT_EQ(hierarchy.tintCount, 1) << "Material re-application must not stack tint views";
    EXPECT_EQ(sendId(hierarchy.effectView, "superview"), hierarchy.hostView);
    EXPECT_EQ(sendId(hierarchy.effectView, "window"), hierarchy.nativeWindow);
    EXPECT_TRUE(isSubviewBelow(hierarchy.hostView, hierarchy.effectView, hierarchy.contentView))
        << "The native material must remain below Qt's sole foreground backing store";
    EXPECT_FALSE(sendBool(hierarchy.effectView, "isHidden"));
    EXPECT_EQ(sendLong(hierarchy.effectView, "material"), expectedMaterial);
    EXPECT_EQ(sendLong(hierarchy.effectView, "blendingMode"),
              NSVisualEffectBlendingModeBehindWindow);
    EXPECT_EQ(sendLong(hierarchy.effectView, "state"), NSVisualEffectStateFollowsWindowActiveState);
    EXPECT_TRUE(CGRectEqualToRect(sendRect(hierarchy.effectView, "frame"),
                                  sendRect(hierarchy.contentView, "frame")))
        << "The backdrop must follow the Qt content frame, not the full title-frame bounds";
    EXPECT_TRUE(CGRectEqualToRect(sendRect(hierarchy.tintView, "frame"),
                                  sendRect(hierarchy.effectView, "bounds")));
    EXPECT_EQ(legacyBackdropWindowCount(hierarchy.nativeWindow), 0)
        << "A companion NSWindow can sample stale foreground frames in WindowServer";
}

} // namespace

#ifdef FLUENT_QT_HAS_SPATIAL
TEST(CocoaWindowBackdropTest, TrafficLightsStayCenteredAfterOpenGLSurfaceCreation)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "Requires the native macOS window system";
    Window window;
    window.resize(640, 480);
    auto* content = new QWidget;
    auto* column = new QVBoxLayout(content);
    window.setContentWidget(content);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const auto expectCentered = [&] {
        const id native = nativeWindowFor(&window);
        ASSERT_NE(native, nil);
        for (unsigned long type : {0UL, 1UL, 2UL}) {
            const id button = sendUnsignedLongReturnsId(native, "standardWindowButton:", type);
            ASSERT_NE(button, nil);
            const CGRect frame = sendRect(button, "frame");
            const id host = sendId(button, "superview");
            const CGRect bounds = sendRect(host, "bounds");
            const qreal centerFromTop = sendBool(host, "isFlipped")
                                            ? CGRectGetMidY(frame)
                                            : bounds.size.height - CGRectGetMidY(frame);
            const auto* titleBar = window.titleBar();
            const qreal expected =
                titleBar->mapTo(&window, QPoint()).y() + titleBar->height() / 2.0;
            EXPECT_NEAR(centerFromTop, expected, 1.0) << "native button " << type;
        }
    };
    QTest::qWait(100);
    expectCentered();
    for (int cycle = 0; cycle < 2; ++cycle) {
        auto* viewport = new QOpenGLWidget(content);
        column->addWidget(viewport);
        viewport->show();
        QTest::qWait(150);
        ASSERT_TRUE(viewport->isValid());
        expectCentered();
        delete viewport;
        QTest::qWait(50);
        expectCentered();
    }
}

#endif // FLUENT_QT_HAS_SPATIAL

TEST(CocoaWindowBackdropTest, PreservesQtContentAndReusesInWindowMaterial)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "The native vibrancy contract requires the Cocoa platform plugin";

    constexpr long NSVisualEffectMaterialSidebar = 7;
    constexpr long NSVisualEffectMaterialHUDWindow = 13;
    constexpr long NSTitlebarSeparatorStyleNone = 1;

    Window window;
    window.resize(520, 360);
    window.setBackdropEffect(BackdropEffect::Solid);
    window.show();
    QApplication::processEvents();

    id nativeWindow = nativeWindowFor(&window);
    ASSERT_NE(nativeWindow, nil);
    ASSERT_NE(window.windowHandle(), nullptr);
    EXPECT_GT(window.windowHandle()->format().alphaBufferSize(), 0)
        << "The alpha-capable surface format must be fixed before Cocoa creates QNSWindow";
    EXPECT_FALSE(sendBool(nativeWindow, "isOpaque"))
        << "The alpha-capable native surface remains sticky while opaque modes paint every pixel";
    if (respondsTo(nativeWindow, "titlebarSeparatorStyle")) {
        EXPECT_EQ(sendLong(nativeWindow, "titlebarSeparatorStyle"), NSTitlebarSeparatorStyleNone);
    }

    window.setBackdropEffect(BackdropEffect::Mica);
    QApplication::processEvents();
    window.reapplySystemBackdrop();
    QApplication::processEvents();

    EXPECT_EQ(window.backdropState().backend, BackdropBackend::MacVibrancy);
    EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::CompositedTransparent);
    EXPECT_TRUE(window.backdropState().platformApplied);
    BackdropHierarchy mica = resolveBackdropHierarchy(&window);
    expectBackdropHierarchy(mica, NSVisualEffectMaterialSidebar);

    window.reapplySystemBackdrop();
    window.reapplySystemBackdrop();
    QApplication::processEvents();
    BackdropHierarchy reapplied = resolveBackdropHierarchy(&window);
    expectBackdropHierarchy(reapplied, NSVisualEffectMaterialSidebar);
    EXPECT_EQ(reapplied.effectView, mica.effectView);
    EXPECT_EQ(reapplied.tintView, mica.tintView);

    window.resize(640, 420);
    QApplication::processEvents();
    BackdropHierarchy resized = resolveBackdropHierarchy(&window);
    expectBackdropHierarchy(resized, NSVisualEffectMaterialSidebar);
    EXPECT_EQ(resized.effectView, mica.effectView);

    window.setBackdropEffect(BackdropEffect::Acrylic);
    QApplication::processEvents();
    BackdropHierarchy acrylic = resolveBackdropHierarchy(&window);
    expectBackdropHierarchy(acrylic, NSVisualEffectMaterialHUDWindow);
    EXPECT_EQ(acrylic.effectView, mica.effectView);

    window.setBackdropEffect(BackdropEffect::Solid);
    QApplication::processEvents();
    BackdropHierarchy solid = resolveBackdropHierarchy(&window);
    ASSERT_NE(solid.effectView, nil);
    EXPECT_TRUE(sendBool(solid.effectView, "isHidden"));
    EXPECT_EQ(solid.effectCount, 1);
    EXPECT_EQ(solid.contentView, solid.nativeView);
    EXPECT_FALSE(sendBool(solid.nativeWindow, "isOpaque"));
    EXPECT_EQ(window.backdropState().backend, BackdropBackend::Solid);
    EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::SolidOpaque);

    for (int cycle = 0; cycle < 20; ++cycle) {
        window.setBackdropEffect(BackdropEffect::Mica);
        QApplication::processEvents();
        BackdropHierarchy cycleMica = resolveBackdropHierarchy(&window);
        expectBackdropHierarchy(cycleMica, NSVisualEffectMaterialSidebar);
        EXPECT_EQ(cycleMica.effectView, mica.effectView);

        window.setBackdropEffect(BackdropEffect::Acrylic);
        QApplication::processEvents();
        BackdropHierarchy cycleAcrylic = resolveBackdropHierarchy(&window);
        expectBackdropHierarchy(cycleAcrylic, NSVisualEffectMaterialHUDWindow);
        EXPECT_EQ(cycleAcrylic.effectView, mica.effectView);

        window.setBackdropEffect(BackdropEffect::Solid);
        QApplication::processEvents();
        BackdropHierarchy cycleSolid = resolveBackdropHierarchy(&window);
        ASSERT_NE(cycleSolid.effectView, nil);
        EXPECT_TRUE(sendBool(cycleSolid.effectView, "isHidden"));
        EXPECT_EQ(cycleSolid.effectCount, 1);
        EXPECT_EQ(cycleSolid.contentView, cycleSolid.nativeView);
        EXPECT_EQ(legacyBackdropWindowCount(cycleSolid.nativeWindow), 0);
    }

    window.setBackdropEffect(BackdropEffect::Mica);
    window.showFullScreen();
    ASSERT_TRUE(QTest::qWaitFor([&] { return window.isFullScreen(); }, 8000));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (window.backdropState().backend) == (BackdropBackend::MacVibrancy); }, 8000));
    EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::CompositedTransparent);
    EXPECT_TRUE(window.backdropState().platformApplied);
    expectBackdropHierarchy(resolveBackdropHierarchy(&window), NSVisualEffectMaterialSidebar);
    nativeWindow = nativeWindowFor(&window);
    ASSERT_NE(nativeWindow, nil);
    if (respondsTo(nativeWindow, "titlebarSeparatorStyle")) {
        EXPECT_EQ(sendLong(nativeWindow, "titlebarSeparatorStyle"), NSTitlebarSeparatorStyleNone);
    }

    window.showNormal();
    ASSERT_TRUE(QTest::qWaitFor([&] { return !window.isFullScreen(); }, 8000));
    ASSERT_TRUE(QTest::qWaitFor(
        [&] { return (window.backdropState().backend) == (BackdropBackend::MacVibrancy); }, 8000));
    EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::CompositedTransparent);
    EXPECT_TRUE(window.backdropState().platformApplied);
    expectBackdropHierarchy(resolveBackdropHierarchy(&window), NSVisualEffectMaterialSidebar);
    nativeWindow = nativeWindowFor(&window);
    ASSERT_NE(nativeWindow, nil);
    if (respondsTo(nativeWindow, "titlebarSeparatorStyle")) {
        EXPECT_EQ(sendLong(nativeWindow, "titlebarSeparatorStyle"), NSTitlebarSeparatorStyleNone);
    }

    window.close();
}

TEST(CocoaWindowBackdropTest, SolidFirstShowPreparesNativeThemeBackground)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "Requires the Cocoa platform plugin";
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        SCOPED_TRACE(static_cast<int>(theme));
        Window window;
        window.setProperty("fluentThemeOverride", static_cast<int>(theme));
        window.setBackdropEffect(BackdropEffect::Solid);
        window.onThemeUpdated();
        window.resize(520, 360);
        const QSize expectedSize = window.size();
        const WId nativeId = window.winId();
        const QColor initial = nativeBackgroundColor(nativeWindowFor(&window));
        EXPECT_NEAR(initial.redF(), window.themeBackdrop(false).redF(), 0.005);
        EXPECT_NEAR(initial.greenF(), window.themeBackdrop(false).greenF(), 0.005);
        EXPECT_NEAR(initial.blueF(), window.themeBackdrop(false).blueF(), 0.005);
        EXPECT_EQ(initial.alpha(), 255) << "The native base must be ready before show(), not later";
        window.show();
        EXPECT_EQ(window.winId(), nativeId);
        EXPECT_EQ(window.size(), expectedSize);
        QApplication::processEvents();
        window.close();
    }
}

TEST(CocoaWindowBackdropTest, SuspendedUpdatesKeepMaterialUntilOpaqueFrameCommits)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "Requires the Cocoa platform plugin";
    Window window;
    window.resize(520, 360);
    const QSize expectedSize = window.size();
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_EQ(window.backdropState().backend, BackdropBackend::MacVibrancy);
    const auto material = resolveBackdropHierarchy(&window);
    ASSERT_NE(material.effectView, nil);
    const WId nativeId = window.winId();
    window.setUpdatesEnabled(false);
    window.setBackdropEffect(BackdropEffect::Solid);
    EXPECT_FALSE(sendBool(material.effectView, "isHidden"));
    window.reapplySystemBackdrop();
    EXPECT_FALSE(sendBool(material.effectView, "isHidden"));
    window.setUpdatesEnabled(true);
    QImage cache(window.size(), QImage::Format_ARGB32_Premultiplied);
    cache.fill(Qt::transparent);
    window.render(&cache);
    EXPECT_FALSE(sendBool(material.effectView, "isHidden"))
        << "A cache capture cannot release the native material";
    ASSERT_TRUE(QTest::qWaitFor([&] { return sendBool(material.effectView, "isHidden"); }, 2000));
    EXPECT_EQ(window.winId(), nativeId);
    EXPECT_EQ(window.size(), expectedSize);
    window.close();
}

#ifdef FLUENT_QT_HAS_SPATIAL
TEST(CocoaWindowBackdropTest, BackdropCommitWaitsForOpenGLComposition)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "Requires native Cocoa/OpenGL composition";
    Window window;
    auto* content = new QWidget;
    auto* layout = new QVBoxLayout(content);
    auto* gl = new BackdropCommitOpenGLWidget(&window, content);
    layout->addWidget(gl);
    window.setContentWidget(content);
    window.resize(640, 520);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(QTest::qWaitFor([&] { return gl->isValid(); }, 2000));
    const auto material = resolveBackdropHierarchy(&window);
    ASSERT_NE(material.effectView, nil);
    const WId nativeId = window.winId();
    int opaqueCompositions = 0;
    QObject::connect(gl, &QOpenGLWidget::frameSwapped, &window, [&] {
        if (!gl->opaqueFramePainted || opaqueCompositions != 0)
            return;
        ++opaqueCompositions;
        EXPECT_FALSE(sendBool(material.effectView, "isHidden"))
            << "The first opaque GL composition must still be covered by the old material";
    });
    window.setBackdropEffect(BackdropEffect::Solid);
    EXPECT_FALSE(sendBool(material.effectView, "isHidden"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return sendBool(material.effectView, "isHidden"); }, 2000));
    EXPECT_EQ(opaqueCompositions, 1);
    EXPECT_EQ(window.winId(), nativeId);
    EXPECT_EQ(nativeBackgroundColor(nativeWindowFor(&window)).alpha(), 255);
    window.close();
}

TEST(CocoaWindowBackdropTest, RecreatedSurfaceDiscardsPendingBackdropCommit)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "Requires native Cocoa/OpenGL surfaces";
    if (!FLUENT_OPENGL_WIDGET_CAN_REPLACE_NATIVE_WINDOW)
        GTEST_SKIP() << "Dynamic top-level OpenGL surface recreation begins with Qt 6.4";
    Window window;
    window.setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));
    window.onThemeUpdated();
    window.resize(640, 520);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    const WId originalId = window.winId();
    window.setUpdatesEnabled(false);
    window.setBackdropEffect(BackdropEffect::Solid);
    auto* gl = new BackdropCommitOpenGLWidget(&window, nullptr);
    window.setContentWidget(gl);
    gl->show();
    const WId replacementId = window.winId();
    if (originalId == replacementId)
        GTEST_SKIP() << "This Qt backend retained the existing native surface";
    const QColor base = nativeBackgroundColor(nativeWindowFor(&window));
    EXPECT_EQ(base.alpha(), 255);
    EXPECT_LT(base.lightness(), 80)
        << "A replacement must not inherit clear pixels or the old surface's paint evidence";
    window.setUpdatesEnabled(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return gl->isValid(); }, 2000));
    EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::SolidOpaque);
    window.close();
}
#endif
