#include <gtest/gtest.h>

#include <QColor>
#include <QEvent>
#include <QGuiApplication>
#include <QImage>
#include <QLibrary>
#include <QPalette>
#include <QPixmap>
#include <QScreen>
#include <QTest>
#include <QVector>
#include <QWindow>

#include <functional>

#include "components/windowing/Window.h"
#include "components/windowing/WindowBackdrop.h"

using fluent::windowing::BackdropBackend;
using fluent::windowing::BackdropEffect;
using fluent::windowing::BackdropSurfaceMode;
using fluent::windowing::Window;

namespace {

bool isXcbPlatform()
{
    return QGuiApplication::platformName() == QStringLiteral("xcb");
}

struct BlurHintResult {
    bool valid = false;
    bool present = false;
};

// Inspect the server property independently of Window's published state. No
// Xlib struct replicas, development headers, or link dependency are needed.
class X11BlurHintReader final {
public:
    X11BlurHintReader()
    {
        if (!m_library.load())
            return;
        const auto openDisplay = reinterpret_cast<OpenDisplay>(m_library.resolve("XOpenDisplay"));
        m_closeDisplay = reinterpret_cast<CloseDisplay>(m_library.resolve("XCloseDisplay"));
        m_internAtom = reinterpret_cast<InternAtom>(m_library.resolve("XInternAtom"));
        m_getWindowProperty =
            reinterpret_cast<GetWindowProperty>(m_library.resolve("XGetWindowProperty"));
        m_freeData = reinterpret_cast<FreeData>(m_library.resolve("XFree"));
        if (openDisplay && m_closeDisplay && m_internAtom && m_getWindowProperty && m_freeData)
            m_display = openDisplay(nullptr);
    }

    ~X11BlurHintReader()
    {
        if (m_display)
            m_closeDisplay(m_display);
    }

    bool isAvailable() const { return m_display != nullptr; }

    BlurHintResult read(WId nativeId) const
    {
        if (!m_display || !nativeId)
            return {};
        const unsigned long atom = m_internAtom(m_display, "_KDE_NET_WM_BLUR_BEHIND_REGION", 1);
        if (!atom)
            return {true, false};
        unsigned long actualType = 0;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long bytesAfter = 0;
        unsigned char* data = nullptr;
        const int status =
            m_getWindowProperty(m_display, static_cast<unsigned long>(nativeId), atom, 0, 0, 0, 0,
                                &actualType, &actualFormat, &itemCount, &bytesAfter, &data);
        const bool freed = !data || m_freeData(data) != 0;
        constexpr unsigned long CardinalAtom = 6;
        const bool present = actualType != 0;
        const bool valid =
            status == 0 && freed &&
            (present ? actualType == CardinalAtom && actualFormat == 32 : actualFormat == 0);
        return {valid, present};
    }

private:
    using Display = struct XDisplay;
    using OpenDisplay = Display* (*)(const char*);
    using CloseDisplay = int (*)(Display*);
    using InternAtom = unsigned long (*)(Display*, const char*, int);
    using GetWindowProperty = int (*)(Display*, unsigned long, unsigned long, long, long, int,
                                      unsigned long, unsigned long*, int*, unsigned long*,
                                      unsigned long*, unsigned char**);
    using FreeData = int (*)(void*);

    QLibrary m_library{QStringLiteral("X11")};
    Display* m_display = nullptr;
    CloseDisplay m_closeDisplay = nullptr;
    InternAtom m_internAtom = nullptr;
    GetWindowProperty m_getWindowProperty = nullptr;
    FreeData m_freeData = nullptr;
};

class NativeBackdropProbeWindow final : public Window {
public:
    ~NativeBackdropProbeWindow() override { beforePaint = {}; }

    int paints = 0;
    int shows = 0;
    int hides = 0;
    int nativeIdChanges = 0;
    QVector<BackdropSurfaceMode> paintedModes;
    std::function<void()> beforePaint;

protected:
    void paintEvent(QPaintEvent* event) override
    {
        ++paints;
        paintedModes.append(backdropState().surfaceMode);
        if (beforePaint)
            beforePaint();
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
};

bool sampleMatchesThemeSeed(const QImage& sample, const QColor& expected)
{
    for (int y = 0; y < sample.height(); ++y) {
        for (int x = 0; x < sample.width(); ++x) {
            const QColor pixel = sample.pixelColor(x, y);
            // Allow the quantization of a real RGB565 display, while still
            // distinguishing the Light theme seed from an unthemed white frame.
            if (qAbs(pixel.red() - expected.red()) > 4 ||
                qAbs(pixel.green() - expected.green()) > 4 ||
                qAbs(pixel.blue() - expected.blue()) > 4 || pixel.alpha() != 255)
                return false;
        }
    }
    return true;
}

void expectThemeSeed(const QImage& sample, const QColor& prepared, const QColor& current)
{
    ASSERT_FALSE(sample.isNull());
    EXPECT_TRUE(sampleMatchesThemeSeed(sample, prepared) || sampleMatchesThemeSeed(sample, current))
        << "Native pixels must match the complete prepared or active theme seed; prepared="
        << prepared.name(QColor::HexArgb).toStdString()
        << " current=" << current.name(QColor::HexArgb).toStdString()
        << " first pixel=" << sample.pixelColor(0, 0).name(QColor::HexArgb).toStdString();
}

void runKWinOpaqueCommit(BackdropEffect target)
{
    if (!isXcbPlatform())
        GTEST_SKIP() << "Requires a real XCB desktop";
    X11BlurHintReader reader;
    if (!reader.isAvailable())
        GTEST_SKIP() << "Requires the optional X11 runtime adapter";
    NativeBackdropProbeWindow window;
    window.setBackdropEffect(BackdropEffect::Acrylic);
    window.resize(520, 360);
    window.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
    if (window.backdropState().backend != BackdropBackend::LinuxCompositor)
        GTEST_SKIP() << "Requires KWin's X11 Acrylic blur protocol and an alpha visual";
    ASSERT_TRUE(QTest::qWaitFor([&] { return window.paints > 0; }, 2000));
    const WId nativeId = window.winId();
    const QRect geometry = window.geometry();
    const int nativeIdChanges = window.nativeIdChanges;
    const auto initial = reader.read(nativeId);
    ASSERT_TRUE(initial.valid);
    ASSERT_TRUE(initial.present);

    window.setUpdatesEnabled(false);
    window.setBackdropEffect(target);
    ASSERT_NE(window.backdropState().surfaceMode, BackdropSurfaceMode::CompositedTransparent);
    EXPECT_FALSE(fluent::windowing::windowBackdropRequiresTransparentClear(&window));
    auto hint = reader.read(nativeId);
    ASSERT_TRUE(hint.valid);
    EXPECT_TRUE(hint.present) << "Suspended client updates must retain the last blur hint";
    window.reapplySystemBackdrop();
    hint = reader.read(nativeId);
    ASSERT_TRUE(hint.valid);
    EXPECT_TRUE(hint.present);

    QImage cache(window.size(), QImage::Format_ARGB32_Premultiplied);
    cache.fill(Qt::transparent);
    window.render(&cache);
    hint = reader.read(nativeId);
    ASSERT_TRUE(hint.valid);
    EXPECT_TRUE(hint.present) << "A QWidget cache render is not an opaque native submission";

    QVector<bool> blurHintsAtOpaquePaint;
    window.beforePaint = [&] {
        if (window.backdropState().surfaceMode == BackdropSurfaceMode::CompositedTransparent)
            return;
        const auto atPaint = reader.read(nativeId);
        EXPECT_TRUE(atPaint.valid);
        blurHintsAtOpaquePaint.append(atPaint.present);
    };
    const int beforeResume = window.paints;
    window.setUpdatesEnabled(true);
    window.update();
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            const auto current = reader.read(nativeId);
            return window.paints > beforeResume && current.valid && !current.present;
        },
        3000));
    ASSERT_FALSE(blurHintsAtOpaquePaint.isEmpty());
    EXPECT_TRUE(blurHintsAtOpaquePaint.first())
        << "The first opaque client paint must still be covered by the previous blur hint";
    EXPECT_EQ(window.backdropEffect(), target);
    EXPECT_EQ(window.backdropState().surfaceMode, target == BackdropEffect::Solid
                                                      ? BackdropSurfaceMode::SolidOpaque
                                                      : BackdropSurfaceMode::PaintedOpaque);
    EXPECT_EQ(window.winId(), nativeId);
    EXPECT_EQ(window.geometry(), geometry);
    EXPECT_EQ(window.nativeIdChanges, nativeIdChanges);
    EXPECT_EQ(window.shows, 1);
    EXPECT_EQ(window.hides, 0);
    window.beforePaint = {};
    window.close();
}

} // namespace

TEST(XcbWindowBackdropTest, SolidFirstExposureUsesNativeThemeBackground)
{
    if (!isXcbPlatform())
        GTEST_SKIP() << "Requires a real XCB desktop";
    X11BlurHintReader reader;
    if (!reader.isAvailable())
        GTEST_SKIP() << "Requires the optional X11 runtime adapter";
    for (const auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark}) {
        SCOPED_TRACE(static_cast<int>(theme));
        NativeBackdropProbeWindow window;
        window.setProperty("fluentThemeOverride", static_cast<int>(theme));
        window.setBackdropEffect(BackdropEffect::Solid);
        window.onThemeUpdated();
        window.resize(520, 360);
        const QSize expectedSize = window.size();
        window.setUpdatesEnabled(false);
        const WId nativeId = window.winId();
        const QColor preparedSeed = window.palette().color(QPalette::Window);
        EXPECT_EQ(preparedSeed, window.themeBackdrop(false));
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        ASSERT_EQ(window.paints, 0);
        ASSERT_NE(window.windowHandle(), nullptr);
        QScreen* screen = window.windowHandle()->screen();
        ASSERT_NE(screen, nullptr);
        // QScreen reads only this window's native pixels. QWidget::grab() would
        // repaint and could conceal an incorrect background before the first paint.
        const QPoint center = window.rect().center();
        const QImage sample =
            screen->grabWindow(nativeId, center.x() - 2, center.y() - 2, 4, 4).toImage();
        const QColor expected = window.themeBackdrop(window.isActiveWindow());
        EXPECT_EQ(window.palette().color(QPalette::Window), expected);
        // Mapping can precede WM activation. Updating XSetWindowBackground on
        // activation deliberately leaves the already-mapped pixels untouched,
        // so the disabled-paint sample may still contain the prepared inactive
        // seed. Accept only one complete, opaque seed from these two theme states.
        expectThemeSeed(sample, preparedSeed, expected);
        EXPECT_EQ(window.paints, 0);
        EXPECT_EQ(window.winId(), nativeId);
        EXPECT_EQ(window.size(), expectedSize);
        const QRect geometry = window.geometry();
        const int nativeIdChanges = window.nativeIdChanges;
        window.setUpdatesEnabled(true);
        window.update();
        ASSERT_TRUE(QTest::qWaitFor([&] { return window.paints > 0; }, 2000));
        EXPECT_EQ(window.winId(), nativeId);
        EXPECT_EQ(window.geometry(), geometry);
        EXPECT_EQ(window.nativeIdChanges, nativeIdChanges);
        EXPECT_EQ(window.shows, 1);
        EXPECT_EQ(window.hides, 0);
        EXPECT_EQ(window.backdropState().surfaceMode, BackdropSurfaceMode::SolidOpaque);
        window.close();
    }
}

TEST(XcbWindowBackdropTest, SuspendedUpdatesKeepBlurUntilSolidFrameCommits)
{
    runKWinOpaqueCommit(BackdropEffect::Solid);
}

TEST(XcbWindowBackdropTest, SuspendedUpdatesKeepBlurUntilPaintedMicaFrameCommits)
{
    runKWinOpaqueCommit(BackdropEffect::Mica);
}

TEST(WaylandWindowBackdropTest, FirstPaintUsesOpaqueFallback)
{
    if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland")))
        GTEST_SKIP() << "Requires a real Wayland compositor";
    for (const auto effect :
         {BackdropEffect::Solid, BackdropEffect::Mica, BackdropEffect::Acrylic}) {
        SCOPED_TRACE(static_cast<int>(effect));
        NativeBackdropProbeWindow window;
        window.setProperty("fluentThemeOverride", static_cast<int>(fluent::FluentElement::Dark));
        window.setBackdropEffect(effect);
        window.onThemeUpdated();
        window.resize(520, 360);
        const QSize expectedSize = window.size();
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        ASSERT_TRUE(QTest::qWaitFor([&] { return !window.paintedModes.isEmpty(); }, 2000));
        EXPECT_NE(window.paintedModes.first(), BackdropSurfaceMode::CompositedTransparent);
        EXPECT_EQ(window.backdropState().surfaceMode, effect == BackdropEffect::Solid
                                                          ? BackdropSurfaceMode::SolidOpaque
                                                          : BackdropSurfaceMode::PaintedOpaque);
        EXPECT_FALSE(fluent::windowing::windowBackdropRequiresTransparentClear(&window));
        EXPECT_EQ(window.palette().color(QPalette::Window).alpha(), 255);
        EXPECT_EQ(window.size(), expectedSize);
        EXPECT_EQ(window.shows, 1);
        EXPECT_EQ(window.hides, 0);
        window.close();
    }
}
