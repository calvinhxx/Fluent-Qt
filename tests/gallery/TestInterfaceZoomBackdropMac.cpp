#include <gtest/gtest.h>
#include <QApplication>
#include <QTest>
#include <QWindow>
#include <objc/message.h>
#include <objc/runtime.h>

#include "components/windowing/Window.h"
#include "support/interfacezoom/InterfaceZoom.h"

namespace {
id sendObject(id object, const char* selector)
{
    return reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(object, sel_registerName(selector));
}

id materialView(id view)
{
    id identifier = sendObject(view, "identifier");
    const char* name = identifier
        ? reinterpret_cast<const char* (*)(id, SEL)>(objc_msgSend)(identifier, sel_registerName("UTF8String"))
        : nullptr;
    if (name && QByteArray(name) == "fluentBackdropBase")
        return view;
    id children = sendObject(view, "subviews");
    const auto count = reinterpret_cast<unsigned long (*)(id, SEL)>(objc_msgSend)(children, sel_registerName("count"));
    for (unsigned long i = 0; i < count; ++i) {
        id child = reinterpret_cast<id (*)(id, SEL, unsigned long)>(
            objc_msgSend)(children, sel_registerName("objectAtIndex:"), i);
        if (id found = materialView(child))
            return found;
    }
    return nil;
}

bool hidden(id view)
{
    return reinterpret_cast<BOOL (*)(id, SEL)>(objc_msgSend)(view, sel_registerName("isHidden"));
}
} // namespace

TEST(InterfaceZoomBackdropTest, NativeHalfDprCommitsOpaqueMaterialRemoval)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        GTEST_SKIP() << "Requires a native Cocoa material hierarchy";
    namespace zoom = fluent::support::interfacezoom;
    using namespace fluent::windowing;
    ASSERT_TRUE(zoom::isSupported());
    ASSERT_TRUE(zoom::apply(1.0));
    struct Restore {
        ~Restore() { zoom::apply(1.0); }
    } restore;
    Window window;
    window.resize(520, 360);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    ASSERT_TRUE(zoom::apply(0.5 / window.devicePixelRatioF()));
    ASSERT_NEAR(window.devicePixelRatioF(), 0.5, 1e-6);
    for (auto effect : {BackdropEffect::Mica, BackdropEffect::Acrylic}) {
        SCOPED_TRACE(int(effect));
        window.setBackdropEffect(effect);
        ASSERT_TRUE(QTest::qWaitFor([&] { return window.backdropState().platformApplied; }));
        id nsWindow = sendObject(reinterpret_cast<id>(window.winId()), "window");
        id material = materialView(sendObject(sendObject(nsWindow, "contentView"), "superview"));
        ASSERT_NE(material, nil);
        ASSERT_FALSE(hidden(material));
        window.setUpdatesEnabled(false);
        window.setBackdropEffect(BackdropEffect::Solid);
        const auto cache = window.grab();
        ASSERT_FALSE(cache.isNull());
        EXPECT_FALSE(hidden(material)) << "Capturing an opaque cache cannot retire the native material";
        window.setUpdatesEnabled(true);
        ASSERT_TRUE(QTest::qWaitFor([&] { return hidden(material); }, 2000));
        EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::SolidOpaque);
        EXPECT_TRUE(window.windowHandle()->isExposed());
    }
}
