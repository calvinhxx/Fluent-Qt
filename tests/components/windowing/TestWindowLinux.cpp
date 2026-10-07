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

#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#include <QtGui/qguiapplication_platform.h>
#endif

#include <functional>
#include <cstring>

#include "components/windowing/Window.h"
#include "components/windowing/WindowBackdrop.h"
#include "compatibility/private/WindowBackdropXcb_p.h"

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

namespace xcb = compatibility::detail::xcb;

struct X11ImageReply {
    quint8 responseType;
    quint8 depth;
    quint16 sequence;
    quint32 length;
    quint32 visual;
    quint8 padding[20];
};

struct X11ImageSetup {
    quint8 status;
    quint8 padding0;
    quint16 protocolMajor;
    quint16 protocolMinor;
    quint16 length;
    quint32 release;
    quint32 resourceIdBase;
    quint32 resourceIdMask;
    quint32 motionBufferSize;
    quint16 vendorLength;
    quint16 maximumRequestLength;
    quint8 rootsLength;
    quint8 formatsLength;
    quint8 imageByteOrder;
    quint8 bitmapBitOrder;
    quint8 bitmapScanlineUnit;
    quint8 bitmapScanlinePad;
    quint8 minimumKeycode;
    quint8 maximumKeycode;
    quint8 padding1[4];
};

static_assert(sizeof(X11ImageReply) == 32 && offsetof(X11ImageReply, visual) == 8 &&
                  sizeof(X11ImageSetup) == 40 && offsetof(X11ImageSetup, imageByteOrder) == 30,
              "The native image probe must retain the core XCB wire ABI");

int x11ImageColorComponent(quint32 pixel, quint32 mask)
{
    if (!mask)
        return -1;
    int shift = 0;
    while ((mask & 1U) == 0) {
        mask >>= 1;
        ++shift;
    }
    if ((mask & (mask + 1U)) != 0)
        return -1;
    return static_cast<int>((quint64((pixel >> shift) & mask) * 255 + mask / 2) / mask);
}

// Inspect server properties and pixels independently of Window's published state.
// Checked XCB replies keep a disappearing/recreated native ID nonfatal.
class X11BackdropReader final {
public:
    X11BackdropReader()
    {
        if (m_api.load()) {
            m_connection = m_api.connect(nullptr, nullptr);
            if (!m_api.healthy(m_connection)) {
                if (m_connection)
                    m_api.disconnect(m_connection);
                m_connection = nullptr;
            }
        }
        if (m_imageLibrary.load()) {
            m_getImage = reinterpret_cast<GetImage>(m_imageLibrary.resolve("xcb_get_image"));
            m_getImageReply =
                reinterpret_cast<GetImageReply>(m_imageLibrary.resolve("xcb_get_image_reply"));
            m_imageData = reinterpret_cast<ImageData>(m_imageLibrary.resolve("xcb_get_image_data"));
            m_imageDataLength = reinterpret_cast<ImageDataLength>(
                m_imageLibrary.resolve("xcb_get_image_data_length"));
        }
    }

    ~X11BackdropReader()
    {
        if (m_connection)
            m_api.disconnect(m_connection);
    }

    bool isAvailable() const { return m_connection != nullptr; }

    bool canCaptureSamples() const
    {
        return m_api.healthy(m_connection) && m_getImage && m_getImageReply && m_imageData &&
               m_imageDataLength;
    }

    QImage captureSample(WId nativeId, const QPoint& position) const
    {
        if (!canCaptureSamples() || !nativeId)
            return {};
        constexpr int Extent = 4;
        constexpr quint8 ZPixmap = 2;
        const auto reply =
            m_api.reply(m_connection,
                        m_getImage(m_connection, ZPixmap, static_cast<quint32>(nativeId),
                                   position.x(), position.y(), Extent, Extent, ~quint32{0}),
                        m_getImageReply);
        if (!reply || reply->depth == 0 || reply->depth > 32)
            return {};
        const int length = m_imageDataLength(reply.get());
        const quint8* data = m_imageData(reply.get());
        // Four pixels align each row to every core X11 scanline pad (8/16/32),
        // so the payload size determines its 8/16/24/32-bit pixel storage.
        if (!data || length <= 0 || length % (Extent * Extent) != 0)
            return {};
        const int bytesPerPixel = length / (Extent * Extent);
        if (bytesPerPixel < 1 || bytesPerPixel > 4 || reply->depth > bytesPerPixel * 8)
            return {};
        xcb::Visual visual{};
        if (!findVisual(reply->visual, &visual) || visual.visualClass != 4)
            return {};
        const quint32 colorMask = visual.redMask | visual.greenMask | visual.blueMask;
        const quint32 depthMask = static_cast<quint32>((quint64{1} << reply->depth) - 1);
        if ((colorMask & ~depthMask) != 0 || (visual.redMask & visual.greenMask) != 0 ||
            (visual.redMask & visual.blueMask) != 0 || (visual.greenMask & visual.blueMask) != 0)
            return {};
        const quint32 alphaMask = depthMask & ~colorMask;
        const xcb::Setup* setup = m_api.getSetup(m_connection);
        if (!setup)
            return {};
        X11ImageSetup imageSetup{};
        std::memcpy(&imageSetup, setup, sizeof(imageSetup));
        if (imageSetup.imageByteOrder > 1)
            return {};
        QImage sample(Extent, Extent, QImage::Format_ARGB32);
        for (int y = 0; y < Extent; ++y) {
            for (int x = 0; x < Extent; ++x) {
                const int offset = (y * Extent + x) * bytesPerPixel;
                quint32 pixel = 0;
                for (int byte = 0; byte < bytesPerPixel; ++byte) {
                    const int shift =
                        imageSetup.imageByteOrder == 0 ? byte * 8 : (bytesPerPixel - byte - 1) * 8;
                    pixel |= quint32(data[offset + byte]) << shift;
                }
                const int red = x11ImageColorComponent(pixel, visual.redMask);
                const int green = x11ImageColorComponent(pixel, visual.greenMask);
                const int blue = x11ImageColorComponent(pixel, visual.blueMask);
                const int alpha = alphaMask ? x11ImageColorComponent(pixel, alphaMask) : 255;
                if (red < 0 || green < 0 || blue < 0 || alpha < 0)
                    return {};
                sample.setPixel(x, y, qRgba(red, green, blue, alpha));
            }
        }
        return sample;
    }

    BlurHintResult read(WId nativeId) const
    {
        if (!m_connection || !nativeId)
            return {};
        constexpr const char* AtomName = "_KDE_NET_WM_BLUR_BEHIND_REGION";
        const auto atom = m_api.reply(
            m_connection,
            m_api.internAtom(m_connection, 1, static_cast<quint16>(qstrlen(AtomName)), AtomName),
            m_api.internAtomReply);
        if (!atom)
            return {};
        if (!atom->id)
            return {true, false};
        const auto property = m_api.reply(
            m_connection,
            m_api.getProperty(m_connection, 0, static_cast<quint32>(nativeId), atom->id, 0, 0, 0),
            m_api.getPropertyReply);
        if (!property)
            return {};
        const bool present = property->type != 0;
        const bool valid =
            present ? property->type == 6 && property->format == 32 : property->format == 0;
        return {valid, present};
    }

private:
    bool findVisual(quint32 id, xcb::Visual* result) const
    {
        const xcb::Setup* setup = m_api.getSetup(m_connection);
        if (!setup)
            return false;
        for (auto screen = m_api.rootsIterator(setup); screen.remaining;
             m_api.screenNext(&screen)) {
            for (auto depth = m_api.depthsIterator(screen.data); depth.remaining;
                 m_api.depthNext(&depth)) {
                for (auto visual = m_api.visualsIterator(depth.data); visual.remaining;
                     m_api.visualNext(&visual)) {
                    if (visual.data->id == id) {
                        *result = *visual.data;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    using GetImage = xcb::Cookie (*)(xcb::Connection*, quint8, quint32, qint16, qint16, quint16,
                                     quint16, quint32);
    using GetImageReply = xcb::Api::ReplyFunction<X11ImageReply>;
    using ImageData = quint8* (*)(const X11ImageReply*);
    using ImageDataLength = int (*)(const X11ImageReply*);

    xcb::Api m_api;
    xcb::Connection* m_connection = nullptr;
    QLibrary m_imageLibrary{QStringLiteral("xcb"), 1};
    GetImage m_getImage = nullptr;
    GetImageReply m_getImageReply = nullptr;
    ImageData m_imageData = nullptr;
    ImageDataLength m_imageDataLength = nullptr;
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
    X11BackdropReader reader;
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
    X11BackdropReader reader;
    if (!reader.isAvailable())
        GTEST_SKIP() << "Requires the optional X11 runtime adapter";
    ASSERT_TRUE(reader.canCaptureSamples());
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
        const QColor preparedSeed = window.palette().color(QPalette::Inactive, QPalette::Window);
        EXPECT_EQ(preparedSeed, window.themeBackdrop(false));
        window.show();
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        ASSERT_EQ(window.paints, 0);
        ASSERT_NE(window.windowHandle(), nullptr);
        QScreen* screen = window.windowHandle()->screen();
        ASSERT_NE(screen, nullptr);
        // QScreen may capture the root for a same-depth window, which is black
        // under rootless Xwayland. Read this native drawable directly instead;
        // QWidget::grab() would repaint and hide a bad pre-paint background.
        const QPoint center = window.rect().center();
        const qreal scale = window.devicePixelRatioF();
        const QImage sample = reader.captureSample(
            nativeId, QPoint(qRound(center.x() * scale) - 2, qRound(center.y() * scale) - 2));
        const QColor expected = window.themeBackdrop(window.isActiveWindow());
        const auto group = window.isActiveWindow() ? QPalette::Active : QPalette::Inactive;
        EXPECT_EQ(window.palette().color(group, QPalette::Window), expected);
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

TEST(XcbWindowBackdropTest, DestroyedSurfaceRequestsReturnErrorsAndKeepConnectionAlive)
{
    if (!isXcbPlatform())
        GTEST_SKIP() << "Requires a real XCB desktop";
    namespace xcb = compatibility::detail::xcb;
    xcb::Api api;
    if (!api.load())
        GTEST_SKIP() << "Requires the optional XCB runtime adapter";
    xcb::Connection* connection = api.connect(nullptr, nullptr);
    ASSERT_TRUE(api.healthy(connection));
    const auto closeConnection = [&api](xcb::Connection* value) { api.disconnect(value); };
    std::unique_ptr<xcb::Connection, decltype(closeConnection)> ownedConnection(connection,
                                                                                closeConnection);
    QWindow nativeWindow;
    nativeWindow.create();
    const quint32 destroyedId = static_cast<quint32>(nativeWindow.winId());
    ASSERT_NE(destroyedId, 0U);
    QScreen* screen = nativeWindow.screen();
    ASSERT_NE(screen, nullptr);
    nativeWindow.destroy();
#if QT_VERSION >= QT_VERSION_CHECK(6, 2, 0) && QT_CONFIG(xcb)
    auto* native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    ASSERT_NE(native, nullptr);
    ASSERT_TRUE(api.roundTrip(reinterpret_cast<xcb::Connection*>(native->connection())));
#else
    ASSERT_NE(screen->handle(), nullptr);
    // Qt 5.15 always waits for root geometry first, even when the following
    // lookup of this intentionally destroyed client returns BadWindow.
    screen->grabWindow(destroyedId, 0, 0, 2, 2);
#endif
    EXPECT_FALSE(api.reply(connection, api.getWindowAttributes(connection, destroyedId),
                           api.getWindowAttributesReply));
    EXPECT_FALSE(
        api.reply(connection, api.getGeometry(connection, destroyedId), api.getGeometryReply));
    EXPECT_FALSE(api.reply(connection, api.getProperty(connection, 0, destroyedId, 6, 0, 0, 0),
                           api.getPropertyReply));
    constexpr quint32 pixel = 0xff202020;
    EXPECT_FALSE(
        api.check(connection, api.changeAttributesChecked(connection, destroyedId, 2, &pixel)));
    EXPECT_FALSE(api.check(
        connection, api.changePropertyChecked(connection, 0, destroyedId, 6, 6, 32, 1, &pixel)));
    EXPECT_FALSE(api.check(connection, api.deletePropertyChecked(connection, destroyedId, 6)));
    EXPECT_FALSE(api.check(connection, api.mapWindowChecked(connection, destroyedId)));
    constexpr quint32 above = 0;
    EXPECT_FALSE(
        api.check(connection, api.configureWindowChecked(connection, destroyedId, 64, &above)));
    EXPECT_FALSE(api.check(connection, api.setInputFocusChecked(connection, 2, destroyedId, 0)));
    EXPECT_TRUE(api.roundTrip(connection)) << "BadWindow must not terminate or poison the client";
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
