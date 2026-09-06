#include <gtest/gtest.h>

#include <QApplication>
#include <QGuiApplication>
#include <QTest>
#include <QWindow>
#include <QWindowStateChangeEvent>

#include "compatibility/WindowChromeCompat.h"
#include "components/windowing/Window.h"
#include "components/windowing/WindowBackdrop.h"

// Windows headers define small as a macro; include them after FluentQt declarations.
// zh_CN: Windows 头文件将 small 定义为宏，应在 FluentQt 声明之后包含。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using fluent::windowing::BackdropBackend;
using fluent::windowing::BackdropEffect;
using fluent::windowing::Window;

namespace {

class NativeActivationProbeWindow final : public Window {
public:
    int activationCount = 0;
    int deactivationCount = 0;

    void resetActivationCounts()
    {
        activationCount = 0;
        deactivationCount = 0;
    }

protected:
    bool nativeEvent(const QByteArray& eventType, void* message,
                     compatibility::FluentNativeEventResult* result) override
    {
        const auto* nativeMessage = static_cast<MSG*>(message);
        if (nativeMessage && nativeMessage->message == WM_NCACTIVATE) {
            if (nativeMessage->wParam)
                ++activationCount;
            else
                ++deactivationCount;
        }
        return Window::nativeEvent(eventType, message, result);
    }
};

} // namespace

TEST(WindowsWindowBackdropTest, StateChangesDoNotRepeatActivationCompensation)
{
    if (QGuiApplication::platformName() != QLatin1String("windows"))
        GTEST_SKIP() << "DWM activation regression requires the Windows desktop platform";

    NativeActivationProbeWindow window;
    if (!window.backdropCapabilities().nativeMica || !window.backdropCapabilities().nativeAcrylic)
        GTEST_SKIP() << "This Windows session does not support DWM Mica and Acrylic";

    window.setBackdropEffect(BackdropEffect::Mica);
    window.resize(520, 500);
    window.show();
    window.requestForegroundActivation();
    if (!QTest::qWaitForWindowActive(&window, 2000))
        GTEST_SKIP() << "The desktop did not activate the DWM probe window";

    // Let first-show backdrop priming and native chrome repair finish before observing runtime work.
    QTest::qWait(100);
    if (window.backdropState().backend != BackdropBackend::DwmSystemBackdrop)
        GTEST_SKIP() << "DWM did not accept a native backdrop in this session";

    ASSERT_NE(window.windowHandle(), nullptr);
    const auto hwnd = reinterpret_cast<HWND>(window.windowHandle()->winId());
    ASSERT_NE(hwnd, nullptr);
    ASSERT_EQ(GetActiveWindow(), hwnd);

    // Positive control: observe the same synchronous messages as DWM's first-show compensation.
    // These change non-client painting only; they do not transfer keyboard focus.
    window.resetActivationCounts();
    SendMessageW(hwnd, WM_NCACTIVATE, FALSE, 0);
    SendMessageW(hwnd, WM_NCACTIVATE, TRUE, 0);
    ASSERT_GT(window.deactivationCount, 0);
    ASSERT_GT(window.activationCount, 0);
    ASSERT_EQ(GetActiveWindow(), hwnd);

    for (BackdropEffect effect : {BackdropEffect::Mica, BackdropEffect::Acrylic}) {
        SCOPED_TRACE(static_cast<int>(effect));
        window.setBackdropEffect(effect);
        QTest::qWait(50);
        ASSERT_EQ(window.backdropState().backend, BackdropBackend::DwmSystemBackdrop);

        // Inject only the Qt notification, so real desktop activation cannot mask the regression.
        for (Qt::WindowStates oldState :
             {Qt::WindowStates(Qt::WindowNoState), Qt::WindowStates(Qt::WindowMaximized),
              Qt::WindowStates(Qt::WindowFullScreen)}) {
            SCOPED_TRACE(static_cast<int>(oldState));
            ASSERT_EQ(GetActiveWindow(), hwnd);
            window.resetActivationCounts();
            QWindowStateChangeEvent stateChange(oldState);
            QApplication::sendEvent(&window, &stateChange);
            QTest::qWait(50);

            EXPECT_EQ(window.deactivationCount, 0)
                << "Runtime window-state changes must not repeat first-show DWM compensation";
            EXPECT_EQ(GetActiveWindow(), hwnd);
            EXPECT_EQ(window.backdropState().backend, BackdropBackend::DwmSystemBackdrop);
        }
    }
    window.close();
}
