#include <gtest/gtest.h>

#include <QApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <QVBoxLayout>
#include <QWindow>
#include <QWindowStateChangeEvent>
#ifdef FLUENT_QT_HAS_SPATIAL
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#endif

#include "compatibility/WindowChromeCompat.h"
#include "components/basicinput/Button.h"
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
using fluent::windowing::BackdropSurfaceMode;
using fluent::windowing::Window;

namespace {

int nativeBackdropType(HWND hwnd)
{
    using GetAttribute = HRESULT(WINAPI*)(HWND, DWORD, PVOID, DWORD);
    const auto getAttribute = reinterpret_cast<GetAttribute>(
        GetProcAddress(GetModuleHandleW(L"dwmapi.dll"), "DwmGetWindowAttribute"));
    int type = -1;
    if (getAttribute)
        getAttribute(hwnd, 38, &type, sizeof(type));
    return type;
}

class NativeActivationProbeWindow final : public Window {
public:
    int activationCount = 0;
    int deactivationCount = 0;
    int shows = 0;
    int hides = 0;
    int nativeIdChanges = 0;
    QVector<fluent::windowing::BackdropState> paintedStates;
    QVector<int> nativeTypesAtPaint;

    void resetActivationCounts()
    {
        activationCount = 0;
        deactivationCount = 0;
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        paintedStates.append(backdropState());
        nativeTypesAtPaint.append(nativeBackdropType(reinterpret_cast<HWND>(winId())));
        Window::paintEvent(event);
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Show)
            ++shows;
        else if (event->type() == QEvent::Hide)
            ++hides;
        else if (event->type() == QEvent::WinIdChange)
            ++nativeIdChanges;
        return Window::event(event);
    }

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

#ifdef FLUENT_QT_HAS_SPATIAL
class BackdropProbeOpenGLWidget final : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    explicit BackdropProbeOpenGLWidget(Window* window, QWidget* parent)
        : QOpenGLWidget(parent), m_window(window)
    {
        auto alphaFormat = format();
        alphaFormat.setAlphaBufferSize(8);
        setFormat(alphaFormat);
        connect(this, &QOpenGLWidget::frameSwapped, this, [this] {
            if (m_lastFrameOpaque)
                nativeTypesAtOpaqueSwap.append(
                    nativeBackdropType(reinterpret_cast<HWND>(m_window->winId())));
        });
    }

    QVector<int> nativeTypesAtOpaqueSwap;

protected:
    void initializeGL() override { initializeOpenGLFunctions(); }

    void paintGL() override
    {
        m_lastFrameOpaque =
            m_window->backdropState().surfaceMode == BackdropSurfaceMode::SolidOpaque;
        const float channel = m_lastFrameOpaque ? 0.2f : 0.0f;
        glClearColor(channel, channel, channel, m_lastFrameOpaque ? 1.0f : 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

private:
    Window* m_window;
    bool m_lastFrameOpaque = false;
};
#endif

} // namespace

TEST(WindowsWindowBackdropTest, EffectChangesPreserveNativeWindowWithoutActivationCompensation)
{
    if (QGuiApplication::platformName() != QLatin1String("windows"))
        GTEST_SKIP() << "DWM effect transitions require the Windows desktop platform";

    for (const bool withOpenGL : {false, true}) {
#ifndef FLUENT_QT_HAS_SPATIAL
        if (withOpenGL)
            break;
#endif
        SCOPED_TRACE(::testing::Message() << "withOpenGL=" << withOpenGL);
        NativeActivationProbeWindow window;
        if (!window.backdropCapabilities().nativeMica ||
            !window.backdropCapabilities().nativeAcrylic)
            GTEST_SKIP() << "This Windows session does not support DWM Mica and Acrylic";
        auto* content = new QWidget;
        auto* layout = new QVBoxLayout(content);
        auto* focusButton = new fluent::basicinput::Button(QStringLiteral("Keep focus"), content);
        layout->addWidget(focusButton);
#ifdef FLUENT_QT_HAS_SPATIAL
        BackdropProbeOpenGLWidget* glSurface = nullptr;
        if (withOpenGL) {
            glSurface = new BackdropProbeOpenGLWidget(&window, content);
            glSurface->setFixedHeight(96);
            layout->addWidget(glSurface);
        }
#endif
        layout->addStretch();
        window.setContentWidget(content);
        window.setBackdropEffect(BackdropEffect::Mica);
        window.resize(640, 500);
        window.show();
        window.requestForegroundActivation();
        ASSERT_TRUE(QTest::qWaitForWindowActive(&window, 2000));
        QTest::qWait(100);
        ASSERT_EQ(window.backdropState().backend, BackdropBackend::DwmSystemBackdrop);
#ifdef FLUENT_QT_HAS_SPATIAL
        if (glSurface)
            ASSERT_TRUE(QTest::qWaitFor([&] { return glSurface->isValid(); }, 3000));
#endif
        focusButton->setFocus(Qt::OtherFocusReason);
        ASSERT_TRUE(QTest::qWaitFor([&] { return focusButton->hasFocus(); }, 1000));
        const auto firstId = window.winId();
        QPointer<QWindow> firstHandle = window.windowHandle();
        const QRect firstGeometry = window.geometry();
        const Qt::WindowFlags firstFlags = window.windowFlags();
        const bool firstAlpha = window.testAttribute(Qt::WA_TranslucentBackground);
        const auto hwnd = reinterpret_cast<HWND>(firstId);
        QSignalSpy visibilitySpy(firstHandle, &QWindow::visibleChanged);
        window.shows = window.hides = window.nativeIdChanges = 0;

        for (BackdropEffect effect :
             {BackdropEffect::Acrylic, BackdropEffect::Mica, BackdropEffect::Solid,
              BackdropEffect::Mica, BackdropEffect::Solid, BackdropEffect::Acrylic}) {
            SCOPED_TRACE(static_cast<int>(effect));
            window.resetActivationCounts();
            window.paintedStates.clear();
            window.nativeTypesAtPaint.clear();
#ifdef FLUENT_QT_HAS_SPATIAL
            if (glSurface)
                glSurface->nativeTypesAtOpaqueSwap.clear();
#endif
            const int previousNativeType = nativeBackdropType(hwnd);
            window.setBackdropEffect(effect);
            // Check the setter synchronously: a fake NCACTIVATE round-trip may
            // flash the material even though real keyboard focus never changes.
            EXPECT_EQ(window.deactivationCount, 0);
            EXPECT_EQ(window.activationCount, 0);
            if (effect == BackdropEffect::Solid) {
                ASSERT_FALSE(window.paintedStates.isEmpty())
                    << "The opaque replacement must paint before native teardown returns";
                EXPECT_EQ(window.paintedStates.first().surfaceMode,
                          BackdropSurfaceMode::SolidOpaque);
                EXPECT_EQ(window.nativeTypesAtPaint.first(), previousNativeType)
                    << "The old material must protect the last transparent frame until the "
                       "opaque replacement is submitted";
#ifdef FLUENT_QT_HAS_SPATIAL
                if (glSurface)
                    EXPECT_TRUE(glSurface->nativeTypesAtOpaqueSwap.contains(previousNativeType))
                        << "Opaque GL composition must swap before native material removal";
#endif
            }
            QTest::qWait(50);
            ASSERT_FALSE(window.paintedStates.isEmpty());
            EXPECT_EQ(window.deactivationCount, 0);
            EXPECT_EQ(window.activationCount, 0);
            EXPECT_EQ(window.winId(), firstId);
            EXPECT_EQ(window.windowHandle(), firstHandle);
            EXPECT_EQ(window.geometry(), firstGeometry);
            EXPECT_EQ(window.windowFlags(), firstFlags);
            EXPECT_EQ(window.testAttribute(Qt::WA_TranslucentBackground), firstAlpha);
            EXPECT_EQ(GetActiveWindow(), hwnd);
            EXPECT_EQ(QApplication::focusWidget(), focusButton);
            EXPECT_EQ(window.shows, 0);
            EXPECT_EQ(window.hides, 0);
            EXPECT_EQ(window.nativeIdChanges, 0);
            EXPECT_TRUE(visibilitySpy.isEmpty());

            const auto state = window.backdropState();
            EXPECT_EQ(state.requestedEffect, effect);
            EXPECT_EQ(state.effectiveEffect, effect);
            const bool solid = effect == BackdropEffect::Solid;
            EXPECT_EQ(state.surfaceMode, solid ? BackdropSurfaceMode::SolidOpaque
                                               : BackdropSurfaceMode::CompositedTransparent);
            EXPECT_EQ(state.platformApplied, !solid);
            const int expectedNativeType = solid ? 1 : (effect == BackdropEffect::Mica ? 2 : 3);
            EXPECT_EQ(nativeBackdropType(hwnd), expectedNativeType);
            for (int i = 0; i < window.paintedStates.size(); ++i) {
                EXPECT_EQ(window.paintedStates.at(i), state);
                if (solid) {
                    EXPECT_TRUE(window.nativeTypesAtPaint.at(i) == previousNativeType ||
                                window.nativeTypesAtPaint.at(i) == expectedNativeType);
                } else {
                    EXPECT_EQ(window.nativeTypesAtPaint.at(i), expectedNativeType)
                        << "The first transparent paint must already have its native material";
                }
            }

            // The first requested backing-store render must match the typed
            // surface contract, including Solid after a transparent material.
            const QImage rendered = window.grab().toImage();
            ASSERT_FALSE(rendered.isNull());
            const QColor pixel = rendered.pixelColor(rendered.width() / 2, rendered.height() - 20);
            EXPECT_EQ(pixel.alpha(), solid ? 255 : 0);
        }
        window.close();
    }
}

TEST(WindowsWindowBackdropTest, SuspendedUpdatesKeepMaterialUntilOpaqueFrameCommits)
{
    if (QGuiApplication::platformName() != QLatin1String("windows"))
        GTEST_SKIP() << "DWM presentation ordering requires the Windows desktop platform";

    NativeActivationProbeWindow window;
    if (!window.backdropCapabilities().nativeMica || !window.backdropCapabilities().nativeAcrylic)
        GTEST_SKIP() << "This Windows session does not support DWM Mica and Acrylic";
    window.resize(520, 420);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window, 2000));
    QTest::qWait(100);
    const auto hwnd = reinterpret_cast<HWND>(window.winId());

    for (BackdropEffect material : {BackdropEffect::Mica, BackdropEffect::Acrylic}) {
        SCOPED_TRACE(static_cast<int>(material));
        window.setBackdropEffect(material);
        QTest::qWait(50);
        const int materialType = nativeBackdropType(hwnd);
        ASSERT_EQ(materialType, material == BackdropEffect::Mica ? 2 : 3);
        window.setUpdatesEnabled(false);
        window.paintedStates.clear();
        window.nativeTypesAtPaint.clear();
        window.setBackdropEffect(BackdropEffect::Solid);
        QTest::qWait(30);
        EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::SolidOpaque);
        EXPECT_TRUE(window.paintedStates.isEmpty());
        EXPECT_EQ(nativeBackdropType(hwnd), materialType)
            << "No opaque frame was submitted while painting is disabled";

        window.setUpdatesEnabled(true);
        ASSERT_TRUE(QTest::qWaitFor([&] { return nativeBackdropType(hwnd) == 1; }, 1000));
        ASSERT_FALSE(window.paintedStates.isEmpty());
        EXPECT_EQ(window.paintedStates.first().surfaceMode, BackdropSurfaceMode::SolidOpaque);
        EXPECT_EQ(window.nativeTypesAtPaint.first(), materialType);
    }

    // Replacing a deferred Solid request must not remove the newer material.
    window.setBackdropEffect(BackdropEffect::Acrylic);
    QTest::qWait(30);
    window.setUpdatesEnabled(false);
    window.setBackdropEffect(BackdropEffect::Solid);
    window.setBackdropEffect(BackdropEffect::Mica);
    window.setUpdatesEnabled(true);
    QTest::qWait(50);
    EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::CompositedTransparent);
    EXPECT_EQ(nativeBackdropType(hwnd), 2);

    // A hidden surface cannot submit a replacement until it is exposed again.
    window.hide();
    window.setBackdropEffect(BackdropEffect::Solid);
    EXPECT_EQ(nativeBackdropType(hwnd), 2);
    window.paintedStates.clear();
    window.nativeTypesAtPaint.clear();
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window, 2000));
    ASSERT_TRUE(QTest::qWaitFor([&] { return nativeBackdropType(hwnd) == 1; }, 1000));
    ASSERT_FALSE(window.paintedStates.isEmpty());
    EXPECT_EQ(window.nativeTypesAtPaint.first(), 2);

    // Synchronous observers may supersede the effect while the safe state is published.
    window.setBackdropEffect(BackdropEffect::Acrylic);
    QTest::qWait(30);
    const auto connection = QObject::connect(
        &window, &Window::backdropStateChanged, &window,
        [&](const fluent::windowing::BackdropState& state) {
            if (state.surfaceMode == BackdropSurfaceMode::SolidOpaque) {
                window.reapplySystemBackdrop();
                EXPECT_EQ(nativeBackdropType(hwnd), 3)
                    << "Reentrant native repair must honor the pending opaque commit";
                window.setBackdropEffect(BackdropEffect::Mica);
            }
        });
    window.setBackdropEffect(BackdropEffect::Solid);
    QObject::disconnect(connection);
    QTest::qWait(30);
    EXPECT_EQ(window.backdropEffect(), BackdropEffect::Mica);
    EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::CompositedTransparent);
    EXPECT_EQ(nativeBackdropType(hwnd), 2);
    window.close();
}

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
