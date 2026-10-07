#include "WindowChromeCompat.h"

#ifdef Q_OS_LINUX

#include <QByteArray>
#include <QColor>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QPointer>
#include <QPixmap>
#include <QRegion>
#include <QScreen>
#include <QTimer>
#include <QVariant>
#include <QWindow>
#include <QtMath>

#include "compatibility/private/WindowBackdropEvents_p.h"
#include "compatibility/private/WindowBackdropXcb_p.h"

#include <limits>

namespace compatibility {
namespace {

constexpr int LinuxClientFrameMargin = 16;
constexpr quint32 XcbNone = 0;
constexpr quint32 XcbCardinalAtom = 6;
constexpr quint8 XcbTrueColor = 4;
constexpr const char* KWinBlurAtomName = "_KDE_NET_WM_BLUR_BEHIND_REGION";
constexpr const char* BackdropSurfaceRectPropertyName = "fluentOverlaySurfaceRect";
constexpr const char* BackdropSurfaceRadiusPropertyName = "fluentClientSideFrameRadius";
constexpr const char* KWinBlurUpdaterObjectName = "_fluentKWinBlurRegionUpdater";
constexpr const char* X11PreparationObjectName = "_fluentX11BackdropSurfacePreparation";

namespace xcb = compatibility::detail::xcb;

bool isXcbPlatform()
{
    return QGuiApplication::platformName().compare(QStringLiteral("xcb"), Qt::CaseInsensitive) == 0;
}

bool isWaylandPlatform()
{
    return QGuiApplication::platformName().startsWith(QStringLiteral("wayland"),
                                                      Qt::CaseInsensitive);
}

class XcbClient final : public QObject {
public:
    explicit XcbClient(QObject* parent) : QObject(parent) {}
    ~XcbClient() override
    {
        if (m_connection)
            api.disconnect(m_connection);
    }

    xcb::Connection* connection()
    {
        if (m_connection && !api.healthy(m_connection)) {
            api.disconnect(m_connection);
            m_connection = nullptr;
        }
        if (!m_connection && api.load()) {
            m_connection = api.connect(nullptr, &m_screenNumber);
            ++m_generation;
            if (!api.healthy(m_connection)) {
                if (m_connection)
                    api.disconnect(m_connection);
                m_connection = nullptr;
            }
        }
        return m_connection;
    }

    quint64 generation() const { return m_generation; }

    xcb::Screen* screen(xcb::Connection* native)
    {
        if (!native)
            return nullptr;
        const xcb::Setup* setup = api.getSetup(native);
        if (!setup)
            return nullptr;
        auto screens = api.rootsIterator(setup);
        for (int index = 0; screens.remaining > 0; ++index, api.screenNext(&screens)) {
            if (index == m_screenNumber)
                return screens.data;
        }
        return nullptr;
    }

    int screenNumber() const { return m_screenNumber; }

    quint32 internAtom(xcb::Connection* native, const QByteArray& name, bool onlyIfExists)
    {
        if (!native || name.isEmpty() || name.size() > std::numeric_limits<quint16>::max())
            return XcbNone;
        const auto cookie = api.internAtom(native, onlyIfExists ? 1 : 0,
                                           static_cast<quint16>(name.size()), name.constData());
        const auto reply = api.reply(native, cookie, api.internAtomReply);
        return reply ? reply->id : XcbNone;
    }

    bool visualForWindow(xcb::Connection* native, quint32 nativeId, xcb::Visual* visual,
                         quint8* depth)
    {
        if (!native || !nativeId || !visual || !depth)
            return false;
        const auto attributes = api.reply(native, api.getWindowAttributes(native, nativeId),
                                          api.getWindowAttributesReply);
        const xcb::Setup* setup = api.getSetup(native);
        if (!attributes || !setup)
            return false;
        auto screens = api.rootsIterator(setup);
        for (; screens.remaining > 0; api.screenNext(&screens)) {
            auto depths = api.depthsIterator(screens.data);
            for (; depths.remaining > 0; api.depthNext(&depths)) {
                auto visuals = api.visualsIterator(depths.data);
                for (; visuals.remaining > 0; api.visualNext(&visuals)) {
                    if (visuals.data->id == attributes->visual) {
                        *visual = *visuals.data;
                        *depth = depths.data->depth;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    xcb::Api api;

private:
    xcb::Connection* m_connection = nullptr;
    int m_screenNumber = 0;
    quint64 m_generation = 0;
};

XcbClient* xcbClient()
{
    // One private connection per application, owned and torn down with it.
    // Checked XCB requests return protocol errors instead of invoking Xlib's
    // process-global handler; no application/Qt error handler is replaced.
    static QPointer<XcbClient> client;
    if (!qGuiApp || !isXcbPlatform())
        return nullptr;
    if (!client)
        client = new XcbClient(qGuiApp);
    return client;
}

bool qtXcbRoundTrip(QWidget* window)
{
    if (!window || !window->internalWinId() || !isXcbPlatform())
        return false;
    XcbClient* client = xcbClient();
    if (!client || !client->api.load())
        return false;
    // Qt 6 exposes its XCB connection publicly. A reply or protocol error on
    // this connection orders all earlier Qt backing-store requests.
    if (xcb::Connection* native = xcb::qtConnection())
        return client->api.roundTrip(native);
    QWindow* handle = window->windowHandle();
    QScreen* screen = handle ? handle->screen() : nullptr;
    if (!screen || !screen->handle())
        return false;
    // Qt 5.15's public QScreen path enters QXcbScreen::grabWindow only after
    // checking the platform-screen handle and nonzero native sample size.
    // Its first request waits for GetGeometry(root) on Qt's own connection;
    // a later GetImage failure therefore does not invalidate that round trip.
    // Use enough logical pixels to remain nonzero at fractional scale factors.
    // No pixels are kept and no events are dispatched. Connection health is
    // rechecked on our private client, rather than treating an empty image as
    // evidence that Qt's ordered request failed.
    const int extent = qMax(1, qCeil(1.0 / qMax<qreal>(0.01, screen->devicePixelRatio())));
    screen->grabWindow(window->internalWinId(), 0, 0, extent, extent);
    xcb::Connection* native = client->connection();
    return native && client->api.roundTrip(native);
}

bool x11ColorComponentPixel(int component, quint32 mask, quint32* pixel)
{
    if (!mask || !pixel)
        return false;
    quint32 maximum = mask;
    unsigned int shift = 0;
    while ((maximum & 1U) == 0) {
        maximum >>= 1;
        ++shift;
    }
    if ((maximum & (maximum + 1U)) != 0)
        return false;
    const quint64 scaled = (static_cast<quint64>(component) * maximum + 127) / 255;
    *pixel |= (static_cast<quint32>(scaled) << shift) & mask;
    return true;
}

bool x11OpaqueBackgroundPixel(const xcb::Visual& visual, quint8 depth, const QColor& color,
                              quint32* pixel)
{
    if (!pixel || visual.visualClass != XcbTrueColor || depth == 0 || depth > 32)
        return false;
    const quint32 colorMask = visual.redMask | visual.greenMask | visual.blueMask;
    const quint32 depthMask = static_cast<quint32>((quint64{1} << depth) - 1);
    if ((colorMask & ~depthMask) != 0 || (visual.redMask & visual.greenMask) != 0 ||
        (visual.redMask & visual.blueMask) != 0 || (visual.greenMask & visual.blueMask) != 0)
        return false;
    *pixel = depthMask & ~colorMask;
    return x11ColorComponentPixel(color.red(), visual.redMask, pixel) &&
           x11ColorComponentPixel(color.green(), visual.greenMask, pixel) &&
           x11ColorComponentPixel(color.blue(), visual.blueMask, pixel);
}

class X11BackdropSurfacePreparation final : public QObject {
public:
    explicit X11BackdropSurfacePreparation(QWidget* window) : QObject(window), m_window(window)
    {
        setObjectName(QString::fromLatin1(X11PreparationObjectName));
        window->installEventFilter(this);
    }

    void prepare(XcbClient* client, const QColor& color, bool transparent)
    {
        const quint32 nativeId = static_cast<quint32>(m_window->internalWinId());
        if (m_nativeId != nativeId || m_connectionGeneration != client->generation()) {
            invalidate();
            m_nativeId = nativeId;
            m_connectionGeneration = client->generation();
        }
        m_state.prepare(
            nativeId, color.rgb(), transparent, [&] { return qtXcbRoundTrip(m_window); },
            [&](quint32 rgb, bool clear) {
                xcb::Connection* connection = client->connection();
                if (!connection)
                    return false;
                if (!m_visualAvailable) {
                    m_visualAvailable =
                        client->visualForWindow(connection, nativeId, &m_visual, &m_depth);
                    if (!m_visualAvailable)
                        return false;
                }
                quint32 pixel = 0;
                if (!clear &&
                    !x11OpaqueBackgroundPixel(m_visual, m_depth, QColor::fromRgb(rgb), &pixel))
                    return false;
                // Change only future map/expose pixels, never clear a painted
                // backing store. A checked request completes before Qt maps it.
                constexpr quint32 BackgroundPixelMask = 2;
                const auto cookie = client->api.changeAttributesChecked(
                    connection, nativeId, BackgroundPixelMask, &pixel);
                return client->api.check(connection, cookie);
            });
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_window && event && event->type() == QEvent::WinIdChange)
            invalidate();
        return QObject::eventFilter(watched, event);
    }

private:
    void invalidate()
    {
        m_state.invalidate();
        m_nativeId = 0;
        m_visualAvailable = false;
    }

    QWidget* m_window;
    compatibility::detail::X11BackdropBackgroundPreparation m_state;
    quint32 m_nativeId = 0;
    quint64 m_connectionGeneration = 0;
    xcb::Visual m_visual{};
    quint8 m_depth = 0;
    bool m_visualAvailable = false;
};

X11BackdropSurfacePreparation* ensureX11BackdropSurfacePreparation(QWidget* window)
{
    if (!window)
        return nullptr;
    for (QObject* child : window->children()) {
        if (child->objectName() == QString::fromLatin1(X11PreparationObjectName)) {
            if (auto* preparation = dynamic_cast<X11BackdropSurfacePreparation*>(child))
                return preparation;
        }
    }
    return new X11BackdropSurfacePreparation(window);
}

struct LinuxBackdropCapabilities {
    bool xcbPlatform = false;
    bool x11LibraryAvailable = false;
    bool compositorActive = false;
    bool alphaCompositionAvailable = false;
    bool blurProtocolAdvertised = false;

    bool nativeBlurAvailable() const
    {
        return xcbPlatform && x11LibraryAvailable && compositorActive &&
               alphaCompositionAvailable && blurProtocolAdvertised;
    }
};

LinuxBackdropCapabilities queryLinuxBackdropCapabilities()
{
    LinuxBackdropCapabilities capabilities;
    capabilities.xcbPlatform = isXcbPlatform();
    if (!capabilities.xcbPlatform)
        return capabilities;
    XcbClient* client = xcbClient();
    xcb::Connection* connection = client ? client->connection() : nullptr;
    capabilities.x11LibraryAvailable = connection != nullptr;
    if (!connection)
        return capabilities;
    xcb::Screen* screen = client->screen(connection);
    if (!screen)
        return capabilities;
    const QByteArray selectionName =
        QByteArrayLiteral("_NET_WM_CM_S") + QByteArray::number(client->screenNumber());
    const quint32 compositorSelection = client->internAtom(connection, selectionName, true);
    const quint32 blurAtom = client->internAtom(connection, QByteArray(KWinBlurAtomName), true);
    if (compositorSelection) {
        const auto owner = client->api.reply(
            connection, client->api.getSelectionOwner(connection, compositorSelection),
            client->api.getSelectionOwnerReply);
        capabilities.compositorActive = owner && owner->id != XcbNone;
    }
    capabilities.alphaCompositionAvailable = capabilities.compositorActive;
    if (capabilities.compositorActive && screen->root && blurAtom) {
        const auto properties =
            client->api.reply(connection, client->api.listProperties(connection, screen->root),
                              client->api.listPropertiesReply);
        if (properties) {
            const quint32* atoms = client->api.listPropertyAtoms(properties.get());
            for (quint16 i = 0; atoms && i < properties->atomsLength; ++i) {
                if (atoms[i] == blurAtom) {
                    capabilities.blurProtocolAdvertised = true;
                    break;
                }
            }
        }
    }
    return capabilities;
}

bool x11WindowHasAlphaSurface(QWidget* window)
{
    if (!window || !isXcbPlatform() || !window->testAttribute(Qt::WA_TranslucentBackground))
        return false;
    const quint32 nativeId = static_cast<quint32>(window->internalWinId());
    XcbClient* client = xcbClient();
    xcb::Connection* connection = client ? client->connection() : nullptr;
    if (!nativeId || !connection)
        return false;
    const auto geometry = client->api.reply(
        connection, client->api.getGeometry(connection, nativeId), client->api.getGeometryReply);
    return geometry && geometry->root && geometry->width > 0 && geometry->height > 0 &&
           geometry->depth == 32;
}

QRect backdropSurfaceRect(QWidget* window)
{
    if (!window)
        return QRect();

    const QRect windowRect = window->rect();
    const QVariant configured = window->property(BackdropSurfaceRectPropertyName);
    if (configured.isValid() && configured.canConvert<QRect>()) {
        const QRect surface = configured.toRect().intersected(windowRect);
        if (!surface.isEmpty())
            return surface;
    }

    // Fluent's Linux custom chrome reserves a 16-DIP transparent shadow ring.
    // Preserve a safe fallback for direct WindowChromeCompat users that have
    // not published the richer surface-geometry property yet.
    if (window->windowFlags().testFlag(Qt::FramelessWindowHint)) {
        const QRect inset = windowRect.adjusted(LinuxClientFrameMargin, LinuxClientFrameMargin,
                                                -LinuxClientFrameMargin, -LinuxClientFrameMargin);
        if (!inset.isEmpty())
            return inset;
    }
    return windowRect;
}

int backdropSurfaceRadius(QWidget* window, const QRect& surface)
{
    if (!window || surface.isEmpty())
        return 0;
    const QVariant configured = window->property(BackdropSurfaceRadiusPropertyName);
    if (configured.isValid())
        return qBound(0, qCeil(configured.toDouble()), qMin(surface.width(), surface.height()) / 2);
    return surface == window->rect() ? 0 : 8;
}

QRegion roundedRectRegion(const QRect& rect, int radius)
{
    if (rect.isEmpty() || radius <= 0)
        return QRegion(rect);

    radius = qMin(radius, qMin(rect.width(), rect.height()) / 2);
    if (radius <= 0)
        return QRegion(rect);

    const int diameter = radius * 2;
    QRegion region(rect.adjusted(radius, 0, -radius, 0));
    region += QRegion(rect.adjusted(0, radius, 0, -radius));
    region += QRegion(QRect(rect.topLeft(), QSize(diameter, diameter)), QRegion::Ellipse);
    region +=
        QRegion(QRect(QPoint(rect.right() - diameter + 1, rect.top()), QSize(diameter, diameter)),
                QRegion::Ellipse);
    region +=
        QRegion(QRect(QPoint(rect.left(), rect.bottom() - diameter + 1), QSize(diameter, diameter)),
                QRegion::Ellipse);
    region += QRegion(QRect(QPoint(rect.right() - diameter + 1, rect.bottom() - diameter + 1),
                            QSize(diameter, diameter)),
                      QRegion::Ellipse);
    return region;
}

QVector<quint32> kwinBlurRegionValues(QWidget* window)
{
    QVector<quint32> values;
    const QRect surface = backdropSurfaceRect(window);
    if (surface.isEmpty())
        return values;

    const QRegion region = roundedRectRegion(surface, backdropSurfaceRadius(window, surface));
    const qreal dpr = qMax<qreal>(1.0, window->devicePixelRatioF());
    for (const QRect& rect : region) {
        if (rect.isEmpty() || rect.x() < 0 || rect.y() < 0)
            continue;
        // KWin's X11 property is expressed in native pixels even though Qt's
        // widget and QRegion geometry is device-independent.
        values.append(static_cast<quint32>(qFloor(rect.x() * dpr)));
        values.append(static_cast<quint32>(qFloor(rect.y() * dpr)));
        values.append(static_cast<quint32>(qCeil(rect.width() * dpr)));
        values.append(static_cast<quint32>(qCeil(rect.height() * dpr)));
    }
    return values;
}

bool verifyKWinBlurProperty(XcbClient* client, xcb::Connection* connection, quint32 nativeId,
                            quint32 blurAtom, const QVector<quint32>* expectedValues)
{
    if (!client || !connection || !nativeId || !blurAtom)
        return false;
    const quint32 expectedCount = expectedValues ? static_cast<quint32>(expectedValues->size()) : 0;
    const quint32 requestedLength = qMax<quint32>(1, expectedCount);
    const auto property = client->api.reply(
        connection,
        client->api.getProperty(connection, 0, nativeId, blurAtom,
                                expectedValues ? XcbCardinalAtom : XcbNone, 0, requestedLength),
        client->api.getPropertyReply);
    if (!property || property->bytesAfter != 0)
        return false;
    if (!expectedValues)
        return property->type == XcbNone && property->format == 0 && property->valueLength == 0;
    if (property->type != XcbCardinalAtom || property->format != 32 ||
        property->valueLength != expectedCount ||
        client->api.propertyValueLength(property.get()) != expectedValues->size() * 4)
        return false;
    const auto* values = static_cast<const quint32*>(client->api.propertyValue(property.get()));
    if (!values)
        return false;
    for (quint32 i = 0; i < expectedCount; ++i) {
        if (values[i] != expectedValues->at(static_cast<int>(i)))
            return false;
    }
    return true;
}

bool applyKWinBlurBehindNow(QWidget* window, bool enabled, bool validateEnvironment = true)
{
    if (!window || !isXcbPlatform())
        return false;
    if (enabled && validateEnvironment &&
        (!queryLinuxBackdropCapabilities().nativeBlurAvailable() ||
         !x11WindowHasAlphaSurface(window)))
        return false;
    const quint32 nativeId = static_cast<quint32>(window->internalWinId());
    XcbClient* client = xcbClient();
    xcb::Connection* connection = client ? client->connection() : nullptr;
    if (!nativeId || !connection)
        return false;
    const quint32 blurAtom = client->internAtom(connection, QByteArray(KWinBlurAtomName), false);
    if (!blurAtom)
        return false;
    // Checked void requests wait for either server acceptance or a protocol
    // error. BadWindow is recoverable even if Qt destroyed a surface between
    // the capability probe and this write; it never invokes a global handler.
    if (enabled) {
        const QVector<quint32> values = kwinBlurRegionValues(window);
        if (values.isEmpty())
            return false;
        const auto cookie = client->api.changePropertyChecked(
            connection, 0, nativeId, blurAtom, XcbCardinalAtom, 32,
            static_cast<quint32>(values.size()), values.constData());
        if (!client->api.check(connection, cookie))
            return false;
        return !validateEnvironment ||
               verifyKWinBlurProperty(client, connection, nativeId, blurAtom, &values);
    }
    const auto cookie = client->api.deletePropertyChecked(connection, nativeId, blurAtom);
    return client->api.check(connection, cookie) &&
           verifyKWinBlurProperty(client, connection, nativeId, blurAtom, nullptr);
}

class KWinBlurRegionUpdater final : public QObject {
public:
    explicit KWinBlurRegionUpdater(QWidget* window) : QObject(window), m_window(window)
    {
        setObjectName(QString::fromLatin1(KWinBlurUpdaterObjectName));
        window->installEventFilter(this);
        m_capabilityProbe.setInterval(2000);
        m_capabilityProbe.setTimerType(Qt::VeryCoarseTimer);
        QObject::connect(&m_capabilityProbe, &QTimer::timeout, this, [this] {
            if (!m_blurEnabled || !m_window || !m_window->isVisible())
                return;
            if (!queryLinuxBackdropCapabilities().nativeBlurAvailable() ||
                !x11WindowHasAlphaSurface(m_window)) {
                disableBlurAndRequestReevaluation();
            }
        });
    }

    void setBlurEnabled(bool enabled)
    {
        m_blurEnabled = enabled;
        if (enabled) {
            if (!m_capabilityProbe.isActive())
                m_capabilityProbe.start();
        } else {
            m_capabilityProbe.stop();
        }
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched != m_window || !event || !m_blurEnabled)
            return QObject::eventFilter(watched, event);

        const QEvent::Type eventType = event->type();
        bool geometryMayHaveChanged = eventType == QEvent::Resize || eventType == QEvent::Show ||
                                      eventType == QEvent::WindowStateChange ||
                                      eventType == QEvent::WinIdChange ||
                                      eventType == QEvent::ScreenChangeInternal;
        bool fullValidate = eventType == QEvent::Show || eventType == QEvent::WindowStateChange ||
                            eventType == QEvent::WinIdChange ||
                            eventType == QEvent::ScreenChangeInternal;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        geometryMayHaveChanged =
            geometryMayHaveChanged || eventType == QEvent::DevicePixelRatioChange;
        fullValidate = fullValidate || eventType == QEvent::DevicePixelRatioChange;
#endif
        if (eventType == QEvent::DynamicPropertyChange) {
            const auto* propertyEvent = static_cast<QDynamicPropertyChangeEvent*>(event);
            geometryMayHaveChanged =
                propertyEvent->propertyName() == BackdropSurfaceRectPropertyName ||
                propertyEvent->propertyName() == BackdropSurfaceRadiusPropertyName;
        }
        if (geometryMayHaveChanged)
            scheduleRefresh(fullValidate);
        return QObject::eventFilter(watched, event);
    }

private:
    void scheduleRefresh(bool fullValidate)
    {
        m_fullValidationPending = m_fullValidationPending || fullValidate;
        if (m_refreshPending)
            return;
        m_refreshPending = true;
        QTimer::singleShot(0, this, [this] {
            m_refreshPending = false;
            const bool fullValidate = m_fullValidationPending;
            m_fullValidationPending = false;
            if (m_blurEnabled && m_window && m_window->isVisible()) {
                const bool refreshed = applyKWinBlurBehindNow(m_window, true, fullValidate);
                if (!refreshed)
                    disableBlurAndRequestReevaluation();
            }
        });
    }

    void disableBlurAndRequestReevaluation()
    {
        if (!m_blurEnabled)
            return;
        m_blurEnabled = false;
        m_capabilityProbe.stop();
        // Leave the last blur hint in place until Window has submitted its
        // opaque fallback. The shared transition then removes it through the
        // Solid backend after the Qt-connection submission fence. A compositor that exits
        // independently cannot be kept alive by retaining this property.
        // zh_CN: 保留 blur 属性到 Window 提交不透明回退帧并完成 XCB 同连接同步之后；
        // 共享流程经 Solid 后端移除属性。保留属性不能阻止外部 compositor 退出。
        compatibility::detail::requestWindowBackdropReevaluation(m_window);
    }

    QWidget* m_window = nullptr;
    bool m_blurEnabled = false;
    bool m_refreshPending = false;
    bool m_fullValidationPending = false;
    QTimer m_capabilityProbe;
};

KWinBlurRegionUpdater* findKWinBlurRegionUpdater(QWidget* window)
{
    if (!window)
        return nullptr;
    const auto children = window->children();
    for (QObject* child : children) {
        if (child && child->objectName() == QString::fromLatin1(KWinBlurUpdaterObjectName)) {
            if (auto* updater = dynamic_cast<KWinBlurRegionUpdater*>(child))
                return updater;
        }
    }
    return nullptr;
}

KWinBlurRegionUpdater* ensureKWinBlurRegionUpdater(QWidget* window)
{
    if (KWinBlurRegionUpdater* updater = findKWinBlurRegionUpdater(window))
        return updater;
    return window ? new KWinBlurRegionUpdater(window) : nullptr;
}

bool setKWinBlurBehind(QWidget* window, bool enabled)
{
    if (!window || !isXcbPlatform())
        return false;

    if (!enabled) {
        if (KWinBlurRegionUpdater* updater = findKWinBlurRegionUpdater(window))
            updater->setBlurEnabled(false);
        return applyKWinBlurBehindNow(window, false);
    }

    if (!applyKWinBlurBehindNow(window, true))
        return false;
    if (KWinBlurRegionUpdater* updater = ensureKWinBlurRegionUpdater(window))
        updater->setBlurEnabled(true);
    return true;
}

} // namespace

namespace detail {

void preparePlatformWindowBackdropSurface(QWidget* window, const QColor& opaqueColor,
                                          fluent::windowing::BackdropSurfaceMode mode)
{
    // Wayland maps only after Qt submits its first buffer. Its lifecycle does
    // not need an X11 background write or a separate native connection.
    if (!window || !isXcbPlatform() || !window->internalWinId())
        return;
    XcbClient* client = xcbClient();
    if (!client || !client->connection())
        return;
    X11BackdropSurfacePreparation* preparation = ensureX11BackdropSurfacePreparation(window);
    if (preparation)
        preparation->prepare(client, opaqueColor,
                             mode == fluent::windowing::BackdropSurfaceMode::CompositedTransparent);
}

bool flushPlatformWindowBackdropSurface(QWidget* window)
{
    return qtXcbRoundTrip(window);
}

void applyPlatformWindowFlags(QWidget* window, const WindowChromeOptions& options)
{
    if (!window)
        return;

    window->setWindowFlag(Qt::Window, true);
    window->setWindowFlag(Qt::FramelessWindowHint, options.useCustomWindowChrome);
    window->setWindowFlag(Qt::WindowMinimizeButtonHint, true);
    window->setWindowFlag(Qt::WindowMaximizeButtonHint, true);
    window->setWindowFlag(Qt::WindowCloseButtonHint, true);
    if (options.useCustomWindowChrome)
        window->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
}

bool handlePlatformNativeEvent(QWidget* window, const WindowChromeOptions& options,
                               const QByteArray& eventType, void* message,
                               FluentNativeEventResult* result)
{
    Q_UNUSED(window);
    Q_UNUSED(options);
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

bool beginPlatformSystemMove(QWidget* window, const QPoint& globalPos)
{
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

bool beginPlatformSystemResize(QWidget* window, Qt::Edges edges, const QPoint& globalPos)
{
    Q_UNUSED(window);
    Q_UNUSED(edges);
    Q_UNUSED(globalPos);
    return false;
}

bool performPlatformTitleBarDoubleClick(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    Q_UNUSED(options);
    return false;
}

bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos)
{
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    Q_UNUSED(options);
}

int nativeTitleBarLeadingInset(QWidget* window)
{
    Q_UNUSED(window);
    return 0;
}

int clientSideFrameMargin(QWidget* window, const WindowChromeOptions& options)
{
    if (!window || !options.useCustomWindowChrome)
        return 0;
    return LinuxClientFrameMargin;
}

bool manualMoveResizeFallbackAllowed(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    return options.useCustomWindowChrome && isXcbPlatform();
}

BackdropCapabilities platformBackdropCapabilities()
{
    BackdropCapabilities capabilities;
    const LinuxBackdropCapabilities x11 = queryLinuxBackdropCapabilities();
    if (x11.nativeBlurAvailable()) {
        capabilities.alphaSurfaceSupported = true;
        capabilities.compositorBlur = true;
        capabilities.provider = QStringLiteral("kwin-x11-blur");
        return capabilities;
    }

    // An active X11 compositing manager and Wayland both support alpha-bearing
    // surfaces, even when no standardized blur protocol is available. The
    // higher-level renderer keeps material fallbacks opaque in that case.
    capabilities.alphaSurfaceSupported = x11.compositorActive || isWaylandPlatform();
    capabilities.provider = QStringLiteral("painted-material");
    return capabilities;
}

bool requestPlatformForegroundActivation(QWidget* window)
{
    if (!window || !isXcbPlatform())
        return false;
    const quint32 nativeId = static_cast<quint32>(window->internalWinId());
    XcbClient* client = xcbClient();
    xcb::Connection* connection = client ? client->connection() : nullptr;
    if (!nativeId || !connection)
        return false;
    // Preserve the existing explicit-user-action X11 activation fallback.
    // Every native request is checked so a destroyed native window cannot
    // turn a secondary launch into a process-fatal Xlib BadWindow error.
    const auto mapped = client->api.mapWindowChecked(connection, nativeId);
    constexpr quint16 StackModeMask = 64;
    constexpr quint32 Above = 0;
    const auto raised =
        client->api.configureWindowChecked(connection, nativeId, StackModeMask, &Above);
    constexpr quint8 RevertToParent = 2;
    constexpr quint32 CurrentTime = 0;
    const auto focused =
        client->api.setInputFocusChecked(connection, RevertToParent, nativeId, CurrentTime);
    const bool mappedSuccessfully = client->api.check(connection, mapped);
    const bool raisedSuccessfully = client->api.check(connection, raised);
    const bool focusedSuccessfully = client->api.check(connection, focused);
    return mappedSuccessfully && raisedSuccessfully && focusedSuccessfully;
}

bool platformSupportsSystemBackdrop()
{
    return platformBackdropCapabilities().compositorBlur;
}

BackdropApplyResult applyPlatformSystemBackdrop(QWidget* window, BackdropEffect effect, bool dark,
                                                bool forceRecomposite)
{
    Q_UNUSED(dark);
    Q_UNUSED(forceRecomposite);
    BackdropApplyResult result;

    if (effect == BackdropEffect::Solid) {
        // Solid is fulfilled by the opaque client renderer. Removing a stale
        // X11 blur hint is best-effort and does not change that outcome.
        if (window && isXcbPlatform())
            setKWinBlurBehind(window, false);
        result.applied = true;
        result.backend = fluent::windowing::BackdropBackend::Solid;
        result.fidelity = fluent::windowing::BackdropFidelity::Solid;
        result.surfaceMode = fluent::windowing::BackdropSurfaceMode::SolidOpaque;
        result.reason = QStringLiteral("solid-requested");
        return result;
    }

    if (!window) {
        result.reason = QStringLiteral("native-window-unavailable");
        return result;
    }
    if (!isXcbPlatform()) {
        result.reason = isWaylandPlatform() ? QStringLiteral("wayland-compositor-blur-unavailable")
                                            : QStringLiteral("linux-compositor-blur-unavailable");
        return result;
    }
    if (effect == BackdropEffect::Mica) {
        // X11 blur-behind matches Acrylic semantics. Mica remains on the stable
        // UILib-painted material instead of being misreported as a
        // high-fidelity compositor implementation.
        result.reason = QStringLiteral("linux-blur-represents-acrylic-only");
        return result;
    }

    const LinuxBackdropCapabilities x11 = queryLinuxBackdropCapabilities();
    if (!x11.compositorActive) {
        result.reason = QStringLiteral("x11-compositor-inactive");
        return result;
    }
    if (!x11.blurProtocolAdvertised) {
        result.reason = QStringLiteral("x11-compositor-blur-unavailable");
        return result;
    }
    if (!x11WindowHasAlphaSurface(window)) {
        result.reason = QStringLiteral("x11-alpha-surface-unavailable");
        return result;
    }
    if (!setKWinBlurBehind(window, true)) {
        result.reason = QStringLiteral("kwin-blur-property-apply-failed");
        return result;
    }

    result.applied = true;
    result.backend = fluent::windowing::BackdropBackend::LinuxCompositor;
    result.fidelity = fluent::windowing::BackdropFidelity::Composited;
    result.surfaceMode = fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    result.reason = QStringLiteral("kwin-x11-blur-active");
    return result;
}

} // namespace detail
} // namespace compatibility

#endif // Q_OS_LINUX
