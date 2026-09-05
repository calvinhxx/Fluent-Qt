#include <gtest/gtest.h>

#include <QApplication>
#include <QGuiApplication>
#include <QTest>
#include <QWindow>

#include <cstring>

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>

#include "components/windowing/Window.h"
#include "components/windowing/WindowBackdrop.h"

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
    QTRY_VERIFY_WITH_TIMEOUT(window.isFullScreen(), 8000);
    QTRY_COMPARE_WITH_TIMEOUT(window.backdropState().backend, BackdropBackend::MacVibrancy, 8000);
    EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::CompositedTransparent);
    EXPECT_TRUE(window.backdropState().platformApplied);
    expectBackdropHierarchy(resolveBackdropHierarchy(&window), NSVisualEffectMaterialSidebar);
    nativeWindow = nativeWindowFor(&window);
    ASSERT_NE(nativeWindow, nil);
    if (respondsTo(nativeWindow, "titlebarSeparatorStyle")) {
        EXPECT_EQ(sendLong(nativeWindow, "titlebarSeparatorStyle"), NSTitlebarSeparatorStyleNone);
    }

    window.showNormal();
    QTRY_VERIFY_WITH_TIMEOUT(!window.isFullScreen(), 8000);
    QTRY_COMPARE_WITH_TIMEOUT(window.backdropState().backend, BackdropBackend::MacVibrancy, 8000);
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
