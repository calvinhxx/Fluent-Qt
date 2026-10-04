#include "WindowChromeCompat.h"

#ifdef Q_OS_LINUX

#include <QByteArray>
#include <QColor>
#include <QDynamicPropertyChangeEvent>
#include <QEvent>
#include <QGuiApplication>
#include <QLibrary>
#include <QPixmap>
#include <QRegion>
#include <QScreen>
#include <QTimer>
#include <QVariant>
#include <QWindow>
#include <QtMath>

#include "compatibility/private/WindowBackdropEvents_p.h"

#include <cstddef>
#include <limits>

namespace compatibility {
namespace {

constexpr int LinuxClientFrameMargin = 16;
constexpr unsigned long X11False = 0;
constexpr unsigned long X11True = 1;
constexpr int X11PropModeReplace = 0;
constexpr int X11Success = 0;
constexpr unsigned long X11AtomType = 4;
constexpr unsigned long X11None = 0;
constexpr int X11TrueColor = 4;
constexpr const char* KWinBlurAtomName = "_KDE_NET_WM_BLUR_BEHIND_REGION";
constexpr const char* BackdropSurfaceRectPropertyName = "fluentOverlaySurfaceRect";
constexpr const char* BackdropSurfaceRadiusPropertyName = "fluentClientSideFrameRadius";
constexpr const char* KWinBlurUpdaterObjectName = "_fluentKWinBlurRegionUpdater";

using XDisplay = struct _XDisplay;
using XAtom = unsigned long;
using XWindow = unsigned long;
using XDrawable = unsigned long;

// Stable Xlib ABI declarations keep the optional runtime adapter independent
// of X11 development headers and link libraries.
struct XVisual {
    void* extensionData;
    unsigned long visualId;
    int visualClass;
    unsigned long redMask;
    unsigned long greenMask;
    unsigned long blueMask;
    int bitsPerRgb;
    int mapEntries;
};

struct XWindowAttributes {
    int x;
    int y;
    int width;
    int height;
    int borderWidth;
    int depth;
    XVisual* visual;
    XWindow root;
    int windowClass;
    int bitGravity;
    int windowGravity;
    int backingStore;
    unsigned long backingPlanes;
    unsigned long backingPixel;
    int saveUnder;
    unsigned long colormap;
    int mapInstalled;
    int mapState;
    long allEventMasks;
    long yourEventMask;
    long doNotPropagateMask;
    int overrideRedirect;
    void* screen;
};

static_assert((sizeof(void*) == 4 || sizeof(void*) == 8) &&
                  sizeof(unsigned long) == sizeof(void*) && sizeof(int) == 4,
              "The dynamic Xlib adapter requires the Linux ILP32 or LP64 ABI");
static_assert(sizeof(XVisual) == (sizeof(void*) == 8 ? 56 : 32) &&
                  offsetof(XVisual, redMask) == (sizeof(void*) == 8 ? 24 : 12),
              "XVisual must retain the Xlib Visual layout");
static_assert(sizeof(XWindowAttributes) == (sizeof(void*) == 8 ? 136 : 92) &&
                  offsetof(XWindowAttributes, visual) == 24 &&
                  offsetof(XWindowAttributes, screen) == (sizeof(void*) == 8 ? 128 : 88),
              "XWindowAttributes must retain the complete Xlib output-buffer layout");

struct X11Api {
    QLibrary library{QStringLiteral("X11")};
    using XOpenDisplayFn = XDisplay* (*)(const char*);
    using XCloseDisplayFn = int (*)(XDisplay*);
    using XDefaultScreenFn = int (*)(XDisplay*);
    using XScreenCountFn = int (*)(XDisplay*);
    using XRootWindowFn = XWindow (*)(XDisplay*, int);
    using XInternAtomFn = XAtom (*)(XDisplay*, const char*, int);
    using XGetSelectionOwnerFn = XWindow (*)(XDisplay*, XAtom);
    using XListPropertiesFn = XAtom* (*)(XDisplay*, XWindow, int*);
    using XGetWindowPropertyFn = int (*)(XDisplay*, XWindow, XAtom, long, long, int, XAtom, XAtom*,
                                         int*, unsigned long*, unsigned long*, unsigned char**);
    using XChangePropertyFn = int (*)(XDisplay*, XWindow, XAtom, XAtom, int, int,
                                      const unsigned char*, int);
    using XDeletePropertyFn = int (*)(XDisplay*, XWindow, XAtom);
    using XFlushFn = int (*)(XDisplay*);
    using XFreeFn = int (*)(void*);
    using XGetGeometryFn = int (*)(XDisplay*, XDrawable, XWindow*, int*, int*, unsigned int*,
                                   unsigned int*, unsigned int*, unsigned int*);
    using XMapRaisedFn = int (*)(XDisplay*, XWindow);
    using XRaiseWindowFn = int (*)(XDisplay*, XWindow);
    using XSetInputFocusFn = int (*)(XDisplay*, XWindow, int, unsigned long);
    using XGetWindowAttributesFn = int (*)(XDisplay*, XWindow, XWindowAttributes*);
    using XSetWindowBackgroundFn = int (*)(XDisplay*, XWindow, unsigned long);

    XOpenDisplayFn openDisplay = nullptr;
    XCloseDisplayFn closeDisplay = nullptr;
    XDefaultScreenFn defaultScreen = nullptr;
    XScreenCountFn screenCount = nullptr;
    XRootWindowFn rootWindow = nullptr;
    XInternAtomFn internAtom = nullptr;
    XGetSelectionOwnerFn getSelectionOwner = nullptr;
    XListPropertiesFn listProperties = nullptr;
    XGetWindowPropertyFn getWindowProperty = nullptr;
    XChangePropertyFn changeProperty = nullptr;
    XDeletePropertyFn deleteProperty = nullptr;
    XFlushFn flush = nullptr;
    XFreeFn freeData = nullptr;
    XGetGeometryFn getGeometry = nullptr;
    XMapRaisedFn mapRaised = nullptr;
    XRaiseWindowFn raiseWindow = nullptr;
    XSetInputFocusFn setInputFocus = nullptr;
    XGetWindowAttributesFn getWindowAttributes = nullptr;
    XSetWindowBackgroundFn setWindowBackground = nullptr;

    bool load()
    {
        if (!library.isLoaded() && !library.load())
            return false;

        openDisplay = reinterpret_cast<XOpenDisplayFn>(library.resolve("XOpenDisplay"));
        closeDisplay = reinterpret_cast<XCloseDisplayFn>(library.resolve("XCloseDisplay"));
        defaultScreen = reinterpret_cast<XDefaultScreenFn>(library.resolve("XDefaultScreen"));
        screenCount = reinterpret_cast<XScreenCountFn>(library.resolve("XScreenCount"));
        rootWindow = reinterpret_cast<XRootWindowFn>(library.resolve("XRootWindow"));
        internAtom = reinterpret_cast<XInternAtomFn>(library.resolve("XInternAtom"));
        getSelectionOwner =
            reinterpret_cast<XGetSelectionOwnerFn>(library.resolve("XGetSelectionOwner"));
        listProperties = reinterpret_cast<XListPropertiesFn>(library.resolve("XListProperties"));
        getWindowProperty =
            reinterpret_cast<XGetWindowPropertyFn>(library.resolve("XGetWindowProperty"));
        changeProperty = reinterpret_cast<XChangePropertyFn>(library.resolve("XChangeProperty"));
        deleteProperty = reinterpret_cast<XDeletePropertyFn>(library.resolve("XDeleteProperty"));
        flush = reinterpret_cast<XFlushFn>(library.resolve("XFlush"));
        freeData = reinterpret_cast<XFreeFn>(library.resolve("XFree"));
        getGeometry = reinterpret_cast<XGetGeometryFn>(library.resolve("XGetGeometry"));
        mapRaised = reinterpret_cast<XMapRaisedFn>(library.resolve("XMapRaised"));
        raiseWindow = reinterpret_cast<XRaiseWindowFn>(library.resolve("XRaiseWindow"));
        setInputFocus = reinterpret_cast<XSetInputFocusFn>(library.resolve("XSetInputFocus"));
        getWindowAttributes =
            reinterpret_cast<XGetWindowAttributesFn>(library.resolve("XGetWindowAttributes"));
        setWindowBackground =
            reinterpret_cast<XSetWindowBackgroundFn>(library.resolve("XSetWindowBackground"));
        return openDisplay && closeDisplay && defaultScreen && screenCount && rootWindow &&
               internAtom && getSelectionOwner && listProperties && getWindowProperty &&
               changeProperty && deleteProperty && flush && freeData && getGeometry && mapRaised &&
               raiseWindow && setInputFocus;
    }
};

X11Api* loadedX11Api()
{
    static X11Api api;
    return api.load() ? &api : nullptr;
}

bool isXcbPlatform()
{
    return QGuiApplication::platformName().compare(QStringLiteral("xcb"), Qt::CaseInsensitive) == 0;
}

bool isWaylandPlatform()
{
    return QGuiApplication::platformName().startsWith(QStringLiteral("wayland"),
                                                      Qt::CaseInsensitive);
}

bool x11ColorComponentPixel(int component, unsigned long mask, unsigned long* pixel)
{
    if (!mask || !pixel)
        return false;
    unsigned long maximum = mask;
    unsigned int shift = 0;
    while ((maximum & 1UL) == 0) {
        maximum >>= 1;
        ++shift;
    }
    if ((maximum & (maximum + 1UL)) != 0)
        return false;
    const quint64 scaled = (static_cast<quint64>(component) * maximum + 127) / 255;
    *pixel |= (static_cast<unsigned long>(scaled) << shift) & mask;
    return true;
}

bool x11OpaqueBackgroundPixel(const XWindowAttributes& attributes, const QColor& color,
                              unsigned long* pixel)
{
    if (!pixel || !attributes.visual || attributes.visual->visualClass != X11TrueColor ||
        attributes.depth <= 0 || attributes.depth > 32)
        return false;
    const XVisual& visual = *attributes.visual;
    const unsigned long colorMask = visual.redMask | visual.greenMask | visual.blueMask;
    const unsigned long depthMask =
        static_cast<unsigned long>((quint64{1} << attributes.depth) - 1);
    if ((colorMask & ~depthMask) != 0 || (visual.redMask & visual.greenMask) != 0 ||
        (visual.redMask & visual.blueMask) != 0 || (visual.greenMask & visual.blueMask) != 0)
        return false;
    // Qt's XCB raster visual puts alpha in the depth bits outside the RGB masks.
    // Set those bits opaque instead of assuming a 0xAARRGGBB channel layout.
    *pixel = depthMask & ~colorMask;
    return x11ColorComponentPixel(color.red(), visual.redMask, pixel) &&
           x11ColorComponentPixel(color.green(), visual.greenMask, pixel) &&
           x11ColorComponentPixel(color.blue(), visual.blueMask, pixel);
}

bool closeX11Display(X11Api* api, XDisplay* display)
{
    return api && display && api->closeDisplay(display) == X11Success;
}

bool freeX11Data(X11Api* api, unsigned char*& data)
{
    if (!data)
        return true;
    const int status = api->freeData(data);
    data = nullptr;
    return status != 0;
}

bool validX11Screen(X11Api* api, XDisplay* display, int* screen)
{
    if (!api || !display || !screen)
        return false;
    const int count = api->screenCount(display);
    if (count <= 0)
        return false;
    const int candidate = api->defaultScreen(display);
    if (candidate < 0 || candidate >= count)
        return false;
    *screen = candidate;
    return true;
}

bool windowHasPropertyAtom(X11Api* api, XDisplay* display, XWindow window, XAtom expectedAtom)
{
    if (!api || !display || !window || !expectedAtom)
        return false;

    int propertyCount = 0;
    XAtom* properties = api->listProperties(display, window, &propertyCount);
    const bool listResultValid =
        propertyCount >= 0 && (propertyCount == 0 || properties != nullptr);

    bool found = false;
    for (int i = 0; listResultValid && properties && i < propertyCount; ++i) {
        if (properties[i] == expectedAtom) {
            found = true;
            break;
        }
    }
    const bool freeSucceeded = !properties || api->freeData(properties) != 0;
    return listResultValid && found && freeSucceeded;
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

    X11Api* api = loadedX11Api();
    capabilities.x11LibraryAvailable = api != nullptr;
    if (!api)
        return capabilities;

    XDisplay* display = api->openDisplay(nullptr);
    if (!display)
        return capabilities;

    int screen = 0;
    if (!validX11Screen(api, display, &screen)) {
        closeX11Display(api, display);
        return capabilities;
    }

    const QByteArray compositorSelectionName =
        QByteArrayLiteral("_NET_WM_CM_S") + QByteArray::number(screen);
    const XAtom compositorSelection =
        api->internAtom(display, compositorSelectionName.constData(), X11True);
    const XAtom blurAtom = api->internAtom(display, KWinBlurAtomName, X11True);
    const XWindow compositorOwner =
        compositorSelection ? api->getSelectionOwner(display, compositorSelection) : X11None;
    capabilities.compositorActive = compositorOwner != X11None;
    // On X11, an active compositing manager is what makes an ARGB visual useful;
    // the concrete window visual is probed separately before applying blur.
    capabilities.alphaCompositionAvailable = capabilities.compositorActive;

    const XWindow root = api->rootWindow(display, screen);
    if (capabilities.compositorActive && root && blurAtom) {
        // KWindowEffects advertises blur availability by publishing the blur
        // property on the root window, so inspect the root property list.
        capabilities.blurProtocolAdvertised = windowHasPropertyAtom(api, display, root, blurAtom);
    }

    if (!closeX11Display(api, display))
        capabilities = LinuxBackdropCapabilities{};
    return capabilities;
}

bool x11WindowHasAlphaSurface(QWidget* window)
{
    if (!window || !isXcbPlatform() || !window->testAttribute(Qt::WA_TranslucentBackground)) {
        return false;
    }

    const XWindow xWindow = static_cast<XWindow>(window->winId());
    if (!xWindow)
        return false;

    X11Api* api = loadedX11Api();
    if (!api)
        return false;

    XDisplay* display = api->openDisplay(nullptr);
    if (!display)
        return false;

    XWindow root = X11None;
    int x = 0;
    int y = 0;
    unsigned int width = 0;
    unsigned int height = 0;
    unsigned int borderWidth = 0;
    unsigned int depth = 0;
    const bool geometryAvailable = api->getGeometry(display, xWindow, &root, &x, &y, &width,
                                                    &height, &borderWidth, &depth) != 0;
    const bool closeSucceeded = closeX11Display(api, display);
    return geometryAvailable && root != X11None && width > 0 && height > 0 && depth == 32 &&
           closeSucceeded;
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

QVector<unsigned long> kwinBlurRegionValues(QWidget* window)
{
    QVector<unsigned long> values;
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
        values.append(static_cast<unsigned long>(qFloor(rect.x() * dpr)));
        values.append(static_cast<unsigned long>(qFloor(rect.y() * dpr)));
        values.append(static_cast<unsigned long>(qCeil(rect.width() * dpr)));
        values.append(static_cast<unsigned long>(qCeil(rect.height() * dpr)));
    }
    return values;
}

bool verifyKWinBlurProperty(X11Api* api, XDisplay* display, XWindow window, XAtom blurAtom,
                            XAtom cardinalAtom, const QVector<unsigned long>* expectedValues)
{
    XAtom actualType = X11None;
    int actualFormat = 0;
    unsigned long itemCount = 0;
    unsigned long bytesAfter = 0;
    unsigned char* data = nullptr;
    const unsigned long expectedCount =
        expectedValues ? static_cast<unsigned long>(expectedValues->size()) : 0;
    const long requestedLength =
        expectedValues ? qMax<long>(1, static_cast<long>(expectedCount)) : 1;
    const int status =
        api->getWindowProperty(display, window, blurAtom, 0, requestedLength, X11False,
                               expectedValues ? cardinalAtom : X11None, &actualType, &actualFormat,
                               &itemCount, &bytesAfter, &data);
    bool matches = status == X11Success && bytesAfter == 0;
    if (expectedValues) {
        matches = matches && actualType == cardinalAtom && actualFormat == 32 &&
                  itemCount == expectedCount && data;
        if (matches) {
            const auto* actualValues = reinterpret_cast<const unsigned long*>(data);
            for (unsigned long i = 0; i < itemCount; ++i) {
                if (actualValues[i] != expectedValues->at(static_cast<int>(i))) {
                    matches = false;
                    break;
                }
            }
        }
    } else {
        matches = matches && actualType == X11None && actualFormat == 0 && itemCount == 0;
    }
    if (!freeX11Data(api, data))
        matches = false;
    return matches;
}

bool applyKWinBlurBehindNow(QWidget* window, bool enabled, bool validateEnvironment = true)
{
    if (!window || !isXcbPlatform())
        return false;
    if (enabled && validateEnvironment &&
        (!queryLinuxBackdropCapabilities().nativeBlurAvailable() ||
         !x11WindowHasAlphaSurface(window))) {
        return false;
    }

    const XWindow xWindow = static_cast<XWindow>(window->winId());
    if (!xWindow)
        return false;

    X11Api* api = loadedX11Api();
    if (!api)
        return false;
    XDisplay* display = api->openDisplay(nullptr);
    if (!display)
        return false;

    const XAtom blurAtom = api->internAtom(display, KWinBlurAtomName, X11False);
    const XAtom cardinalAtom = api->internAtom(display, "CARDINAL", X11False);
    if (!blurAtom || !cardinalAtom) {
        closeX11Display(api, display);
        return false;
    }

    bool requestAccepted = false;
    if (enabled) {
        const QVector<unsigned long> values = kwinBlurRegionValues(window);
        if (values.isEmpty() || values.size() > std::numeric_limits<int>::max()) {
            closeX11Display(api, display);
            return false;
        }
        requestAccepted =
            api->changeProperty(display, xWindow, blurAtom, cardinalAtom, 32, X11PropModeReplace,
                                reinterpret_cast<const unsigned char*>(values.constData()),
                                static_cast<int>(values.size())) != 0;
        if (requestAccepted)
            requestAccepted = api->flush(display) != 0;
        if (requestAccepted && validateEnvironment) {
            requestAccepted =
                verifyKWinBlurProperty(api, display, xWindow, blurAtom, cardinalAtom, &values);
        }
    } else {
        requestAccepted = api->deleteProperty(display, xWindow, blurAtom) != 0;
        if (requestAccepted)
            requestAccepted = api->flush(display) != 0;
        if (requestAccepted) {
            requestAccepted =
                verifyKWinBlurProperty(api, display, xWindow, blurAtom, cardinalAtom, nullptr);
        }
    }

    const bool closeSucceeded = closeX11Display(api, display);
    return requestAccepted && closeSucceeded;
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
        // Solid backend after the XCB readback fence. A compositor that exits
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
    // Wayland maps a surface only after Qt submits its first buffer. Keep that
    // compositor-owned lifecycle rather than mapping or clearing it ourselves.
    if (!window || !isXcbPlatform() || !window->internalWinId())
        return;
    X11Api* api = loadedX11Api();
    if (!api || !api->getWindowAttributes || !api->setWindowBackground)
        return;
    QWindow* handle = window->windowHandle();
    QScreen* screen = handle ? handle->screen() : nullptr;
    if (!screen || screen->geometry().isEmpty())
        return;
    // A WId can exist before Qt's XCB create request reaches the server.
    // topLevelAt() queries the native screen through Qt's own connection in
    // Qt 5.15/6, even while this window is unmapped. Its reply orders creation
    // before the separate Xlib query and avoids a fatal early BadWindow error.
    // No window is activated, mapped, captured, or changed by this read.
    // zh_CN: WId 分配不等于 server 已完成创建；先用 Qt 同连接的只读屏幕查询等待
    // create 请求完成，再跨连接获取属性，避免原生窗口早期出现 BadWindow。
    QGuiApplication::topLevelAt(screen->geometry().center());
    XDisplay* display = api->openDisplay(nullptr);
    if (!display)
        return;
    const XWindow xWindow = static_cast<XWindow>(window->internalWinId());
    XWindowAttributes attributes{};
    unsigned long pixel = 0;
    if (api->getWindowAttributes(display, xWindow, &attributes) != 0 &&
        x11OpaqueBackgroundPixel(attributes, opaqueColor, &pixel)) {
        if (mode == fluent::windowing::BackdropSurfaceMode::CompositedTransparent)
            pixel = 0;
        // Setting the background changes future map/expose pixels without
        // clearing the already-painted client surface. CloseDisplay completes
        // this connection's request before Qt is allowed to map the window.
        // zh_CN: 修改后续映射/曝光使用的底色，不清除已绘制内容；关闭此连接会先完成请求。
        api->setWindowBackground(display, xWindow, pixel);
    }
    closeX11Display(api, display);
}

bool flushPlatformWindowBackdropSurface(QWidget* window)
{
    if (!window || !window->internalWinId() || !isXcbPlatform())
        return false;
    QWindow* handle = window->windowHandle();
    QScreen* screen = handle ? handle->screen() : nullptr;
    if (!screen)
        return false;
    // QScreen's XCB readback performs a round trip on Qt's own connection in
    // Qt 5.15 and Qt 6.2+. This orders its submitted replacement buffer before
    // removing the blur hint on our separate Xlib connection. A separate
    // XSync/XFlush cannot provide that ordering. Discard the one-pixel sample;
    // this confirms X server receipt, not physical compositor presentation.
    // zh_CN: 只读取本窗口一个像素以等待 Qt 同连接上的请求完成；不保存像素、不泵送事件，
    // 确认的是 X server 接收次序，并非合成器已经物理呈现到屏幕。
    return !screen->grabWindow(window->internalWinId(), 0, 0, 1, 1).isNull();
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

    const XWindow xWindow = static_cast<XWindow>(window->winId());
    X11Api* api = loadedX11Api();
    if (!xWindow || !api)
        return false;

    XDisplay* display = api->openDisplay(nullptr);
    if (!display)
        return false;

    // QWindow::requestActivate() sends the EWMH activation request first. The
    // secondary process launch is an explicit user action, so X11 may safely
    // finish the operation by mapping/raising the existing client and assigning
    // input focus. This is intentionally X11-only: Wayland compositors own focus
    // policy and may reject activation without a compositor-issued token.
    // zh_CN: QWindow::requestActivate() 先发送 EWMH 激活请求。第二次启动属于明确的
    // 用户操作，因此 X11 可继续映射、提升已有窗口并设置输入焦点。Wayland 的焦点策略
    // 由 compositor 管理，没有 compositor token 时不得强制抢焦点。
    const bool mapped = api->mapRaised(display, xWindow) != 0;
    const bool raised = api->raiseWindow(display, xWindow) != 0;
    constexpr int RevertToParent = 2;
    constexpr unsigned long CurrentTime = 0;
    const bool focused = api->setInputFocus(display, xWindow, RevertToParent, CurrentTime) != 0;
    const bool flushed = api->flush(display) != 0;
    const bool closed = closeX11Display(api, display);
    return mapped && raised && focused && flushed && closed;
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
