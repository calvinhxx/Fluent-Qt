#include "WindowChromeCompat.h"

#ifdef Q_OS_MAC

#include <QGuiApplication>

#include <cmath>

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>

#include <cstring>

namespace compatibility {
namespace detail {

namespace {

// Keep Objective-C message sends local to this file so the public chrome
// adapter stays free of Cocoa headers and Objective-C++ requirements.
// zh_CN: 将 Objective-C 消息发送限制在本文件内，避免公共 chrome 适配器依赖 Cocoa
// zh_CN: 头文件或 Objective-C++ 编译要求。
constexpr unsigned long NSWindowStyleMaskFullSizeContentView = 1UL << 15;
constexpr long NSWindowTitleHidden = 1;
constexpr unsigned long NSWindowCloseButton = 0;
constexpr unsigned long NSWindowMiniaturizeButton = 1;
constexpr unsigned long NSWindowZoomButton = 2;

SEL selector(const char* name) {
    return sel_registerName(name);
}

bool respondsTo(id object, SEL sel) {
    if (!object)
        return false;

    using Send = BOOL (*)(id, SEL, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(object, selector("respondsToSelector:"), sel);
}

id sendId(id receiver, const char* name) {
    using Send = id (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

id sendClassId(const char* className, const char* name) {
    Class cls = objc_getClass(className);
    if (!cls)
        return nil;

    using Send = id (*)(Class, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(cls, selector(name));
}

id sendClassCStringReturnsId(const char* className, const char* name, const char* value) {
    Class cls = objc_getClass(className);
    if (!cls)
        return nil;

    using Send = id (*)(Class, SEL, const char*);
    return reinterpret_cast<Send>(objc_msgSend)(cls, selector(name), value);
}

id sendIdReturnsId(id receiver, const char* name, id value) {
    using Send = id (*)(id, SEL, id);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

const char* sendCString(id receiver, const char* name) {
    using Send = const char* (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

void sendId(id receiver, const char* name, id value) {
    using Send = void (*)(id, SEL, id);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

unsigned long sendUnsignedLong(id receiver, const char* name) {
    using Send = unsigned long (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

void sendUnsignedLong(id receiver, const char* name, unsigned long value) {
    using Send = void (*)(id, SEL, unsigned long);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

void sendLong(id receiver, const char* name, long value) {
    using Send = void (*)(id, SEL, long);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

void sendBool(id receiver, const char* name, BOOL value) {
    using Send = void (*)(id, SEL, BOOL);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

BOOL sendBool(id receiver, const char* name) {
    using Send = BOOL (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

id sendUnsignedLongReturnsId(id receiver, const char* name, unsigned long value) {
    using Send = id (*)(id, SEL, unsigned long);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

CGRect sendRect(id receiver, const char* name) {
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

void sendPoint(id receiver, const char* name, CGPoint value) {
    using Send = void (*)(id, SEL, CGPoint);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

id nativeWindowFor(QWidget* window) {
    if (!window)
        return nil;

    id nativeObject = reinterpret_cast<id>(window->winId());
    if (!nativeObject)
        return nil;

    if (respondsTo(nativeObject, selector("styleMask")))
        return nativeObject;

    if (respondsTo(nativeObject, selector("window")))
        return sendId(nativeObject, "window");

    return nil;
}

void centerTrafficLights(id nsWindow, const QRect& titleBarRect = QRect()) {
    if (!respondsTo(nsWindow, selector("standardWindowButton:")))
        return;

    const unsigned long buttons[] = {
        NSWindowCloseButton,
        NSWindowMiniaturizeButton,
        NSWindowZoomButton
    };

    for (unsigned long buttonType : buttons) {
        id button = sendUnsignedLongReturnsId(nsWindow, "standardWindowButton:", buttonType);
        if (!button || !respondsTo(button, selector("frame")) || !respondsTo(button, selector("setFrameOrigin:")))
            continue;

        id superview = respondsTo(button, selector("superview")) ? sendId(button, "superview") : nil;
        if (!superview || !respondsTo(superview, selector("bounds")))
            continue;

        const CGRect frame = sendRect(button, "frame");
        const CGRect bounds = sendRect(superview, "bounds");
        if (bounds.size.height <= 0 || frame.size.height <= 0)
            continue;

        CGFloat targetCenterY = bounds.size.height / 2.0;
        if (titleBarRect.height() > 0) {
            // Cocoa button frames are expressed in the superview coordinate system;
            // convert the QWidget title-bar center while respecting flipped views.
            // zh_CN: Cocoa 按钮 frame 使用 superview 坐标系；这里将 QWidget 标题栏中心
            // zh_CN: 转换过去，并处理 flipped view。
            const CGFloat titleBarCenterFromTop = titleBarRect.y() + titleBarRect.height() / 2.0;
            const bool flipped = respondsTo(superview, selector("isFlipped"))
                && sendBool(superview, "isFlipped");
            targetCenterY = flipped
                ? titleBarCenterFromTop
                : bounds.size.height - titleBarCenterFromTop;
        }

        const CGFloat y = qRound(targetCenterY - frame.size.height / 2.0);
        sendPoint(button, "setFrameOrigin:", CGPointMake(frame.origin.x, y));
    }
}

void syncUnifiedTitleBarGeometry(QWidget* window, const WindowChromeOptions& options) {
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow)
        return;

    centerTrafficLights(nsWindow, options.titleBarRect);
}

void applyUnifiedTitleBar(QWidget* window, const WindowChromeOptions& options) {
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow)
        return;

    const unsigned long styleMask = sendUnsignedLong(nsWindow, "styleMask");
    sendUnsignedLong(nsWindow, "setStyleMask:", styleMask | NSWindowStyleMaskFullSizeContentView);
    sendLong(nsWindow, "setTitleVisibility:", NSWindowTitleHidden);
    sendBool(nsWindow, "setTitlebarAppearsTransparent:", YES);
    sendBool(nsWindow, "setMovableByWindowBackground:", NO);
    syncUnifiedTitleBarGeometry(window, options);
}

bool performNativeTitleBarDoubleClick(QWidget* window) {
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return false;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow)
        return false;

    id key = sendClassCStringReturnsId("NSString", "stringWithUTF8String:", "AppleActionOnDoubleClick");
    id defaults = sendClassId("NSUserDefaults", "standardUserDefaults");
    id action = (defaults && key && respondsTo(defaults, selector("stringForKey:")))
                    ? sendIdReturnsId(defaults, "stringForKey:", key)
                    : nil;
    const char* actionText = (action && respondsTo(action, selector("UTF8String")))
                                 ? sendCString(action, "UTF8String")
                                 : nullptr;

    // Match the macOS System Settings behavior for double-clicking a title bar.
    // zh_CN: 与 macOS 系统设置中的标题栏双击行为保持一致。
    if (actionText && std::strcmp(actionText, "None") == 0)
        return true;

    if (actionText && std::strcmp(actionText, "Minimize") == 0) {
        if (!respondsTo(nsWindow, selector("performMiniaturize:")))
            return false;

        sendId(nsWindow, "performMiniaturize:", nil);
        return true;
    }

    if (!respondsTo(nsWindow, selector("performZoom:")))
        return false;

    sendId(nsWindow, "performZoom:", nil);
    return true;
}

} // namespace

void applyPlatformWindowFlags(QWidget* window, const WindowChromeOptions& options) {
    if (!window)
        return;

    window->setWindowFlag(Qt::Window, true);
    window->setWindowFlag(Qt::FramelessWindowHint, false);

#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    if (options.preferNativeMacControls) {
        window->setWindowFlag(Qt::ExpandedClientAreaHint, true);
        window->setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
        window->setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
    }
#endif

    if (options.preferNativeMacControls)
        applyUnifiedTitleBar(window, options);
}

bool handlePlatformNativeEvent(QWidget* window,
                               const WindowChromeOptions& options,
                               const QByteArray& eventType,
                               void* message,
                               FluentNativeEventResult* result) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

bool beginPlatformSystemMove(QWidget* window, const QPoint& globalPos) {
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

bool beginPlatformSystemResize(QWidget* window, Qt::Edges edges, const QPoint& globalPos) {
    Q_UNUSED(window);
    Q_UNUSED(edges);
    Q_UNUSED(globalPos);
    return false;
}

bool performPlatformTitleBarDoubleClick(QWidget* window, const WindowChromeOptions& options) {
    if (!options.preferNativeMacControls)
        return false;

    return performNativeTitleBarDoubleClick(window);
}

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options) {
    if (!window || !options.preferNativeMacControls)
        return;

    syncUnifiedTitleBarGeometry(window, options);
}

int nativeTitleBarLeadingInset(QWidget* window) {
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return 0;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow || !respondsTo(nsWindow, selector("standardWindowButton:")))
        return 0;

    // Read the zoom button (rightmost traffic light) frame to determine
    // how much leading space the native controls occupy.
    // zh_CN: 读取缩放按钮（最右侧交通灯）的 frame，以确定原生控件占用的前置宽度。
    id zoomButton = sendUnsignedLongReturnsId(nsWindow, "standardWindowButton:", NSWindowZoomButton);
    if (!zoomButton || !respondsTo(zoomButton, selector("frame")))
        return 0;

    const CGRect frame = sendRect(zoomButton, "frame");
    if (frame.size.width <= 0)
        return 0;

    // frame is in Cocoa points which map 1:1 to Qt logical pixel coordinates.
    // Add an 8-point gap after the rightmost button.
    // zh_CN: Cocoa points 与 Qt 逻辑像素 1:1 对应，末尾留 8pt 间距。
    constexpr int kTrailingGap = 8;
    return static_cast<int>(std::ceil(frame.origin.x + frame.size.width)) + kTrailingGap;
}

} // namespace detail
} // namespace compatibility

#endif // Q_OS_MAC
