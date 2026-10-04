#include <gtest/gtest.h>
#include "compatibility/WindowChromeCompat.h"
#include "components/basicinput/Button.h"
#include "components/windowing/Window.h"
#include "components/windowing/WindowBackdrop.h"
#include "components/windowing/WindowBackdropMaterial.h"

#include <QApplication>
#include <QGuiApplication>
#include <QImage>
#include <QPalette>
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

// Use a real GDI surface so native erase behavior is checked independently of
// QWidget::grab(), which can repaint and conceal a wrong first background.
class NativeEraseSurface final {
public:
    explicit NativeEraseSurface(HWND hwnd)
    {
        if (!GetClientRect(hwnd, &m_rect) || m_rect.right <= 0 || m_rect.bottom <= 0)
            return;
        m_dc = CreateCompatibleDC(nullptr);
        if (!m_dc)
            return;

        BITMAPINFO info = {};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = m_rect.right;
        info.bmiHeader.biHeight = -m_rect.bottom;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        void* pixels = nullptr;
        m_bitmap = CreateDIBSection(m_dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if (!m_bitmap)
            return;
        const auto previous = SelectObject(m_dc, m_bitmap);
        if (previous && previous != HGDI_ERROR)
            m_previousBitmap = previous;
    }

    ~NativeEraseSurface()
    {
        if (m_previousBitmap)
            SelectObject(m_dc, m_previousBitmap);
        if (m_bitmap)
            DeleteObject(m_bitmap);
        if (m_dc)
            DeleteDC(m_dc);
    }

    bool isValid() const { return m_previousBitmap != nullptr; }
    HDC dc() const { return m_dc; }

    bool fill(COLORREF color)
    {
        const HBRUSH brush = CreateSolidBrush(color);
        if (!brush)
            return false;
        const bool filled = FillRect(m_dc, &m_rect, brush) != 0;
        DeleteObject(brush);
        return filled;
    }

    void expectColor(COLORREF color) const
    {
        for (const QPoint point : {QPoint(0, 0), QPoint(m_rect.right / 2, m_rect.bottom / 2),
                                   QPoint(m_rect.right - 1, m_rect.bottom - 1)}) {
            SCOPED_TRACE(::testing::Message() << "GDI pixel=" << point.x() << ',' << point.y());
            EXPECT_EQ(GetPixel(m_dc, point.x(), point.y()), color);
        }
    }

private:
    NativeEraseSurface(const NativeEraseSurface&) = delete;
    NativeEraseSurface& operator=(const NativeEraseSurface&) = delete;
    RECT m_rect = {};
    HDC m_dc = nullptr;
    HBITMAP m_bitmap = nullptr;
    HGDIOBJ m_previousBitmap = nullptr;
};

class NativeActivationProbeWindow final : public Window {
public:
    int activationCount = 0;
    int deactivationCount = 0;
    int shows = 0;
    int hides = 0;
    int nativeIdChanges = 0;
    int backgroundEraseCount = 0;
    bool lastBackgroundEraseHandled = false;
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
        if (nativeMessage && nativeMessage->message == WM_ERASEBKGND) {
            ++backgroundEraseCount;
            lastBackgroundEraseHandled = Window::nativeEvent(eventType, message, result);
            return lastBackgroundEraseHandled;
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

TEST(WindowsWindowBackdropTest, SolidFirstShowPreparesNativeThemeBackground)
{
    if (QGuiApplication::platformName() != QLatin1String("windows"))
        GTEST_SKIP() << "Native first-show background requires the Windows desktop platform";

    const auto globalTheme = fluent::FluentElement::currentTheme();
    for (const bool customChrome : {false, true}) {
        for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
            SCOPED_TRACE(::testing::Message()
                         << "customChrome=" << customChrome << ", theme=" << theme);
            NativeActivationProbeWindow window;
            window.setCustomWindowChromeEnabled(customChrome);
            window.setProperty("fluentThemeOverride", static_cast<int>(theme));
            window.setBackdropEffect(BackdropEffect::Solid);
            window.onThemeUpdated();
            window.resize(640, 480);
            window.move(100, 100);
            const WId firstId = window.winId();
            const auto hwnd = reinterpret_cast<HWND>(firstId);
            QPointer<QWindow> firstHandle = window.windowHandle();
            ASSERT_NE(firstHandle, nullptr);
            const QRect firstGeometry = window.geometry();
            const Qt::WindowFlags firstFlags = window.windowFlags();
            const bool firstAlpha = window.testAttribute(Qt::WA_TranslucentBackground);
            const auto classBrush = GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND);
            EXPECT_EQ(window.effectiveTheme(), theme);
            EXPECT_EQ(fluent::FluentElement::currentTheme(), globalTheme);
            ASSERT_TRUE(window.paintedStates.isEmpty());

            NativeEraseSurface surface(hwnd);
            ASSERT_TRUE(surface.isValid());
            ASSERT_TRUE(surface.fill(RGB(211, 23, 173)));
            const auto eraseCount = window.backgroundEraseCount;
            EXPECT_EQ(SendMessageW(hwnd, WM_ERASEBKGND, reinterpret_cast<WPARAM>(surface.dc()), 0),
                      1);
            EXPECT_EQ(window.backgroundEraseCount, eraseCount + 1);
            EXPECT_TRUE(window.lastBackgroundEraseHandled);
            const QColor expected = window.themeBackdrop(false);
            surface.expectColor(RGB(expected.red(), expected.green(), expected.blue()));
            EXPECT_TRUE(window.paintedStates.isEmpty())
                << "The native theme base must be ready before the first Qt paint";

            QSignalSpy visibilitySpy(firstHandle, &QWindow::visibleChanged);
            window.shows = window.hides = window.nativeIdChanges = 0;
            window.show();
            ASSERT_TRUE(QTest::qWaitForWindowExposed(&window, 2000));
            EXPECT_EQ(window.winId(), firstId);
            EXPECT_EQ(window.windowHandle(), firstHandle);
            EXPECT_EQ(window.geometry(), firstGeometry);
            EXPECT_EQ(window.windowFlags(), firstFlags);
            EXPECT_EQ(window.testAttribute(Qt::WA_TranslucentBackground), firstAlpha);
            EXPECT_EQ(window.shows, 1);
            EXPECT_EQ(window.hides, 0);
            EXPECT_EQ(window.nativeIdChanges, 0);
            ASSERT_EQ(visibilitySpy.count(), 1);
            EXPECT_TRUE(visibilitySpy.first().first().toBool());
            EXPECT_EQ(GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND), classBrush)
                << "Preparing one window must not replace Qt's shared window-class brush";
            window.close();
        }
    }
}

TEST(WindowsWindowBackdropTest, NativeErasePaintsFallbackAndPreservesCompositedPixels)
{
    if (QGuiApplication::platformName() != QLatin1String("windows"))
        GTEST_SKIP() << "Native erase behavior requires the Windows desktop platform";

    constexpr COLORREF sentinel = RGB(211, 23, 173);
    for (const bool customChrome : {false, true}) {
        for (const auto effect : {BackdropEffect::Mica, BackdropEffect::Acrylic}) {
            SCOPED_TRACE(::testing::Message() << "customChrome=" << customChrome
                                              << ", effect=" << static_cast<int>(effect));
            NativeActivationProbeWindow window;
            window.setCustomWindowChromeEnabled(customChrome);
            window.setBackdropEffect(effect);
            window.resize(640, 480);
            const auto hwnd = reinterpret_cast<HWND>(window.winId());
            ASSERT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::PaintedOpaque);
            NativeEraseSurface pendingSurface(hwnd);
            ASSERT_TRUE(pendingSurface.isValid());
            ASSERT_TRUE(pendingSurface.fill(sentinel));
            EXPECT_EQ(
                SendMessageW(hwnd, WM_ERASEBKGND, reinterpret_cast<WPARAM>(pendingSurface.dc()), 0),
                1);
            EXPECT_TRUE(window.lastBackgroundEraseHandled);
            const auto& colors = window.themeColors();
            auto material = fluent::windowing::WindowBackdropMaterialOptions::forTheme(
                window.effectiveThemeUsesDarkAppearance(), colors.bgCanvas, colors.accentDefault);
            material.effect = effect;
            material.active = false;
            const QColor fallback =
                fluent::windowing::WindowBackdropMaterial::opaqueBaseColor(material);
            pendingSurface.expectColor(RGB(fallback.red(), fallback.green(), fallback.blue()));
            EXPECT_TRUE(window.paintedStates.isEmpty());

            // A hidden cache render does not initialize the native surface.
            QImage cache(window.size(), QImage::Format_ARGB32_Premultiplied);
            cache.fill(Qt::transparent);
            window.render(&cache);
            EXPECT_EQ(cache.pixelColor(cache.rect().center()).alpha(), 255);
            ASSERT_TRUE(pendingSurface.fill(sentinel));
            EXPECT_EQ(
                SendMessageW(hwnd, WM_ERASEBKGND, reinterpret_cast<WPARAM>(pendingSurface.dc()), 0),
                1);
            pendingSurface.expectColor(RGB(fallback.red(), fallback.green(), fallback.blue()));
            window.paintedStates.clear();
            window.nativeTypesAtPaint.clear();

            window.show();
            ASSERT_TRUE(QTest::qWaitForWindowExposed(&window, 2000));
            ASSERT_TRUE(QTest::qWaitFor([&] { return !window.paintedStates.isEmpty(); }, 2000));
            NativeEraseSurface exposedSurface(hwnd);
            ASSERT_TRUE(exposedSurface.isValid());
            const auto expectWarmPixelsPreserved = [&] {
                ASSERT_TRUE(exposedSurface.fill(sentinel));
                const auto paintCount = window.paintedStates.size();
                const auto eraseCount = window.backgroundEraseCount;
                EXPECT_EQ(SendMessageW(hwnd, WM_ERASEBKGND,
                                       reinterpret_cast<WPARAM>(exposedSurface.dc()), 0),
                          1);
                EXPECT_EQ(window.backgroundEraseCount, eraseCount + 1);
                EXPECT_TRUE(window.lastBackgroundEraseHandled);
                exposedSurface.expectColor(sentinel);
                EXPECT_EQ(window.paintedStates.size(), paintCount)
                    << "WM_ERASEBKGND must not dispatch Qt paint or OpenGL composition";
                EXPECT_EQ(reinterpret_cast<HWND>(window.winId()), hwnd);
            };
            expectWarmPixelsPreserved();

            // Effect and local-theme changes must keep the submitted surface warm,
            // both while a repaint is queued and after its new pixels are submitted.
            for (const auto nextEffect : {BackdropEffect::Solid, effect}) {
                SCOPED_TRACE(static_cast<int>(nextEffect));
                const auto paintCount = window.paintedStates.size();
                window.setBackdropEffect(nextEffect);
                expectWarmPixelsPreserved();
                ASSERT_TRUE(QTest::qWaitFor(
                    [&] { return window.paintedStates.size() > paintCount; }, 2000));
                expectWarmPixelsPreserved();
            }
            for (const auto theme : {fluent::FluentElement::Dark, fluent::FluentElement::Light}) {
                SCOPED_TRACE(static_cast<int>(theme));
                const auto paintCount = window.paintedStates.size();
                window.setProperty("fluentThemeOverride", static_cast<int>(theme));
                window.onThemeUpdated();
                expectWarmPixelsPreserved();
                ASSERT_TRUE(QTest::qWaitFor(
                    [&] { return window.paintedStates.size() > paintCount; }, 2000));
                expectWarmPixelsPreserved();
            }
            window.close();
        }
    }
}

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
            const bool solid = effect == BackdropEffect::Solid;
            const int expectedNativeType = solid ? 1 : (effect == BackdropEffect::Mica ? 2 : 3);
            if (solid) {
                EXPECT_TRUE(window.paintedStates.isEmpty())
                    << "The setter must schedule painting without reentering native presentation";
                EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::SolidOpaque);
                EXPECT_EQ(nativeBackdropType(hwnd), previousNativeType)
                    << "The old material must remain until a real opaque frame is submitted";
            }
            ASSERT_TRUE(QTest::qWaitFor(
                [&] {
                    return nativeBackdropType(hwnd) == expectedNativeType &&
                           !window.paintedStates.isEmpty();
                },
                2000));
            if (solid) {
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
            EXPECT_EQ(state.surfaceMode, solid ? BackdropSurfaceMode::SolidOpaque
                                               : BackdropSurfaceMode::CompositedTransparent);
            EXPECT_EQ(state.platformApplied, !solid);
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
        EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::SolidOpaque);
        EXPECT_EQ(nativeBackdropType(hwnd), materialType);
        EXPECT_TRUE(window.paintedStates.isEmpty());
        QTest::qWait(30);
        EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::SolidOpaque);
        EXPECT_TRUE(window.paintedStates.isEmpty());
        EXPECT_EQ(nativeBackdropType(hwnd), materialType)
            << "No opaque frame was submitted while painting is disabled";

        window.setUpdatesEnabled(true);
        EXPECT_EQ(nativeBackdropType(hwnd), materialType);
        QImage cache(window.size(), QImage::Format_ARGB32_Premultiplied);
        cache.fill(Qt::transparent);
        window.render(&cache);
        EXPECT_EQ(cache.pixelColor(cache.rect().center()).alpha(), 255);
        EXPECT_EQ(nativeBackdropType(hwnd), materialType)
            << "A cache capture cannot release the native material";
        window.paintedStates.clear();
        window.nativeTypesAtPaint.clear();
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
    EXPECT_EQ(nativeBackdropType(hwnd), 3);
    window.setBackdropEffect(BackdropEffect::Mica);
    EXPECT_EQ(nativeBackdropType(hwnd), 2);
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
