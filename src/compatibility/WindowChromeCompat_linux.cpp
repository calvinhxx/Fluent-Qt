#include "WindowChromeCompat.h"

#ifdef Q_OS_LINUX

#include <QByteArray>
#include <QGuiApplication>
#include <QLibrary>
#include <QWindow>

namespace compatibility {
namespace {

constexpr int LinuxClientFrameMargin = 16;
constexpr unsigned long X11False = 0;
constexpr unsigned long X11True = 1;
constexpr int X11PropModeReplace = 0;
constexpr int X11Success = 0;
constexpr unsigned long X11AtomType = 4;

using XDisplay = struct _XDisplay;
using XAtom = unsigned long;
using XWindow = unsigned long;

struct X11Api {
    QLibrary library{QStringLiteral("X11")};
    using XOpenDisplayFn = XDisplay* (*)(const char*);
    using XCloseDisplayFn = int (*)(XDisplay*);
    using XDefaultScreenFn = int (*)(XDisplay*);
    using XRootWindowFn = XWindow (*)(XDisplay*, int);
    using XInternAtomFn = XAtom (*)(XDisplay*, const char*, int);
    using XGetSelectionOwnerFn = XWindow (*)(XDisplay*, XAtom);
    using XGetWindowPropertyFn = int (*)(XDisplay*, XWindow, XAtom, long, long, int,
                                         XAtom, XAtom*, int*, unsigned long*, unsigned long*,
                                         unsigned char**);
    using XChangePropertyFn = int (*)(XDisplay*, XWindow, XAtom, XAtom, int, int,
                                      const unsigned char*, int);
    using XDeletePropertyFn = int (*)(XDisplay*, XWindow, XAtom);
    using XFlushFn = int (*)(XDisplay*);
    using XFreeFn = int (*)(void*);

    XOpenDisplayFn openDisplay = nullptr;
    XCloseDisplayFn closeDisplay = nullptr;
    XDefaultScreenFn defaultScreen = nullptr;
    XRootWindowFn rootWindow = nullptr;
    XInternAtomFn internAtom = nullptr;
    XGetSelectionOwnerFn getSelectionOwner = nullptr;
    XGetWindowPropertyFn getWindowProperty = nullptr;
    XChangePropertyFn changeProperty = nullptr;
    XDeletePropertyFn deleteProperty = nullptr;
    XFlushFn flush = nullptr;
    XFreeFn freeData = nullptr;

    bool load()
    {
        if (!library.isLoaded() && !library.load())
            return false;

        openDisplay = reinterpret_cast<XOpenDisplayFn>(library.resolve("XOpenDisplay"));
        closeDisplay = reinterpret_cast<XCloseDisplayFn>(library.resolve("XCloseDisplay"));
        defaultScreen = reinterpret_cast<XDefaultScreenFn>(library.resolve("XDefaultScreen"));
        rootWindow = reinterpret_cast<XRootWindowFn>(library.resolve("XRootWindow"));
        internAtom = reinterpret_cast<XInternAtomFn>(library.resolve("XInternAtom"));
        getSelectionOwner =
            reinterpret_cast<XGetSelectionOwnerFn>(library.resolve("XGetSelectionOwner"));
        getWindowProperty =
            reinterpret_cast<XGetWindowPropertyFn>(library.resolve("XGetWindowProperty"));
        changeProperty = reinterpret_cast<XChangePropertyFn>(library.resolve("XChangeProperty"));
        deleteProperty = reinterpret_cast<XDeletePropertyFn>(library.resolve("XDeleteProperty"));
        flush = reinterpret_cast<XFlushFn>(library.resolve("XFlush"));
        freeData = reinterpret_cast<XFreeFn>(library.resolve("XFree"));
        return openDisplay && closeDisplay && defaultScreen && rootWindow && internAtom
            && getSelectionOwner && getWindowProperty && changeProperty && deleteProperty
            && flush && freeData;
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

bool isKdeSession()
{
    const QString desktop =
        QString::fromLocal8Bit(qgetenv("XDG_CURRENT_DESKTOP")).toLower();
    const QString session =
        QString::fromLocal8Bit(qgetenv("DESKTOP_SESSION")).toLower();
    return desktop.contains(QStringLiteral("kde"))
        || desktop.contains(QStringLiteral("plasma"))
        || session.contains(QStringLiteral("kde"))
        || session.contains(QStringLiteral("plasma"))
        || qEnvironmentVariableIsSet("KDE_FULL_SESSION");
}

bool x11CompositorAdvertisesKWinBlur()
{
    X11Api* api = loadedX11Api();
    if (!api)
        return false;

    XDisplay* display = api->openDisplay(nullptr);
    if (!display)
        return false;

    const int screen = api->defaultScreen(display);
    const QByteArray compositorSelectionName =
        QByteArrayLiteral("_NET_WM_CM_S") + QByteArray::number(screen);
    const XAtom compositorSelection =
        api->internAtom(display, compositorSelectionName.constData(), X11True);
    const XAtom blurAtom =
        api->internAtom(display, "_KDE_NET_WM_BLUR_BEHIND_REGION", X11True);
    const XAtom supportedAtom = api->internAtom(display, "_NET_SUPPORTED", X11True);
    const XWindow compositorOwner = compositorSelection
        ? api->getSelectionOwner(display, compositorSelection)
        : 0;

    bool blurAdvertised = false;
    if (compositorOwner && blurAtom && supportedAtom) {
        XAtom actualType = 0;
        int actualFormat = 0;
        unsigned long itemCount = 0;
        unsigned long bytesAfter = 0;
        unsigned char* data = nullptr;
        const XWindow root = api->rootWindow(display, screen);
        const int status = api->getWindowProperty(display,
                                                  root,
                                                  supportedAtom,
                                                  0,
                                                  4096,
                                                  X11False,
                                                  X11AtomType,
                                                  &actualType,
                                                  &actualFormat,
                                                  &itemCount,
                                                  &bytesAfter,
                                                  &data);
        if (status == X11Success && actualType == X11AtomType && actualFormat == 32 && data) {
            const auto* atoms = reinterpret_cast<const XAtom*>(data);
            for (unsigned long i = 0; i < itemCount; ++i) {
                if (atoms[i] == blurAtom) {
                    blurAdvertised = true;
                    break;
                }
            }
        }
        if (data)
            api->freeData(data);
    }

    api->closeDisplay(display);
    return blurAdvertised;
}

bool supportsKWinBlurBehind()
{
    // KWin exposes compositor blur through _KDE_NET_WM_BLUR_BEHIND_REGION.
    // Require both an active X11 compositor and an advertised blur atom; merely
    // running inside a Plasma session is not sufficient when compositing or the
    // blur effect is disabled. Other compositors and Wayland have no stable Qt
    // Widgets equivalent.
    return isXcbPlatform() && isKdeSession() && x11CompositorAdvertisesKWinBlur();
}

bool setKWinBlurBehind(QWidget* window, bool enabled)
{
    if (!window || !isXcbPlatform())
        return false;

    X11Api* api = loadedX11Api();
    if (!api)
        return false;

    XDisplay* display = api->openDisplay(nullptr);
    if (!display)
        return false;

    const XWindow xWindow = static_cast<XWindow>(window->winId());
    const XAtom blurAtom =
        api->internAtom(display, "_KDE_NET_WM_BLUR_BEHIND_REGION", X11False);
    if (!blurAtom) {
        api->closeDisplay(display);
        return false;
    }

    if (enabled) {
        const XAtom cardinalAtom = api->internAtom(display, "CARDINAL", X11False);
        api->changeProperty(display,
                            xWindow,
                            blurAtom,
                            cardinalAtom,
                            32,
                            X11PropModeReplace,
                            nullptr,
                            0);
    } else {
        api->deleteProperty(display, xWindow, blurAtom);
    }
    api->flush(display);
    api->closeDisplay(display);
    return true;
}

} // namespace

namespace detail {

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

bool handlePlatformNativeEvent(QWidget* window,
                               const WindowChromeOptions& options,
                               const QByteArray& eventType,
                               void* message,
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

bool platformSupportsSystemBackdrop()
{
    return supportsKWinBlurBehind();
}

bool applyPlatformSystemBackdrop(QWidget* window,
                                 BackdropEffect effect,
                                 bool dark,
                                 bool forceRecomposite)
{
    Q_UNUSED(dark);
    Q_UNUSED(forceRecomposite);
    if (!supportsKWinBlurBehind())
        return false;

    const bool enableBlur = effect != BackdropEffect::Solid;
    return setKWinBlurBehind(window, enableBlur);
}

} // namespace detail
} // namespace compatibility

#endif // Q_OS_LINUX
