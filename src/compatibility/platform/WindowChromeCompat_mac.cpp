#include "compatibility/private/WindowChromeBackend_p.h"

#ifdef Q_OS_MAC

#include <QGuiApplication>
#include <QColor>
#include <QWindow>

#include <cmath>

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>

#include <cstring>

#include "compatibility/private/WindowBackdropTransition_p.h"

namespace compatibility {
namespace detail {

namespace {

// Send AppKit messages via objc_msgSend so the header needs no Cocoa/ObjC++ includes.
// zh_CN: 通过 objc_msgSend 调 AppKit，避免头文件引入 Cocoa/Objective-C++ 依赖。
constexpr unsigned long NSWindowStyleMaskFullSizeContentView = 1UL << 15;
constexpr unsigned long NSWindowStyleMaskResizable = 1UL << 3;
constexpr long NSWindowTitleHidden = 1;
constexpr long NSTitlebarSeparatorStyleNone = 1;
constexpr unsigned long NSWindowCloseButton = 0;
constexpr unsigned long NSWindowMiniaturizeButton = 1;
constexpr unsigned long NSWindowZoomButton = 2;
constexpr unsigned long NSEventTypeLeftMouseDown = 1;
constexpr unsigned long NSLeftMouseButtonMask = 1UL << 0;

SEL selector(const char* name)
{
    return sel_registerName(name);
}

bool respondsTo(id object, SEL sel)
{
    if (!object)
        return false;

    using Send = BOOL (*)(id, SEL, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(object, selector("respondsToSelector:"), sel);
}

id sendId(id receiver, const char* name)
{
    using Send = id (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

id sendClassId(const char* className, const char* name)
{
    Class cls = objc_getClass(className);
    if (!cls)
        return nil;

    using Send = id (*)(Class, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(cls, selector(name));
}

unsigned long sendClassUnsignedLong(const char* className, const char* name)
{
    Class cls = objc_getClass(className);
    if (!cls)
        return 0;

    using Send = unsigned long (*)(Class, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(cls, selector(name));
}

id sendClassCStringReturnsId(const char* className, const char* name, const char* value)
{
    Class cls = objc_getClass(className);
    if (!cls)
        return nil;

    using Send = id (*)(Class, SEL, const char*);
    return reinterpret_cast<Send>(objc_msgSend)(cls, selector(name), value);
}

id sendIdReturnsId(id receiver, const char* name, id value)
{
    using Send = id (*)(id, SEL, id);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

const char* sendCString(id receiver, const char* name)
{
    using Send = const char* (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

void sendId(id receiver, const char* name, id value)
{
    using Send = void (*)(id, SEL, id);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

unsigned long sendUnsignedLong(id receiver, const char* name)
{
    using Send = unsigned long (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

long sendLong(id receiver, const char* name)
{
    using Send = long (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

void sendUnsignedLong(id receiver, const char* name, unsigned long value)
{
    using Send = void (*)(id, SEL, unsigned long);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

void sendLong(id receiver, const char* name, long value)
{
    using Send = void (*)(id, SEL, long);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

void sendBool(id receiver, const char* name, BOOL value)
{
    using Send = void (*)(id, SEL, BOOL);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

void sendVoid(id receiver, const char* name)
{
    using Send = void (*)(id, SEL);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

BOOL sendBool(id receiver, const char* name)
{
    using Send = BOOL (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

CGFloat sendCGFloat(id receiver, const char* name)
{
    using Send = CGFloat (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

id sendUnsignedLongReturnsId(id receiver, const char* name, unsigned long value)
{
    using Send = id (*)(id, SEL, unsigned long);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

CGRect sendRect(id receiver, const char* name)
{
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

CGPoint sendPoint(id receiver, const char* name)
{
    using Send = CGPoint (*)(id, SEL);
    return reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
}

void sendPoint(id receiver, const char* name, CGPoint value)
{
    using Send = void (*)(id, SEL, CGPoint);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

void sendCGRect(id receiver, const char* name, CGRect value)
{
    using Send = void (*)(id, SEL, CGRect);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

id sendClassIdReturnsId(const char* className, const char* name, id value)
{
    Class cls = objc_getClass(className);
    if (!cls)
        return nil;

    using Send = id (*)(Class, SEL, id);
    return reinterpret_cast<Send>(objc_msgSend)(cls, selector(name), value);
}

id allocInitWithFrame(const char* className, CGRect frame)
{
    Class cls = objc_getClass(className);
    if (!cls)
        return nil;

    using Alloc = id (*)(Class, SEL);
    id object = reinterpret_cast<Alloc>(objc_msgSend)(cls, selector("alloc"));
    if (!object)
        return nil;

    using InitFrame = id (*)(id, SEL, CGRect);
    return reinterpret_cast<InitFrame>(objc_msgSend)(object, selector("initWithFrame:"), frame);
}

void addSubviewPositioned(id superview, id view, long place, id relativeTo)
{
    using Send = void (*)(id, SEL, id, long, id);
    reinterpret_cast<Send>(
        objc_msgSend)(superview, selector("addSubview:positioned:relativeTo:"), view, place, relativeTo);
}

id nativeWindowFor(QWidget* window)
{
    if (!window || QGuiApplication::platformName() != QStringLiteral("cocoa"))
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

// Standard zoomed frame computed from the screen visible frame and window delegate.
CGRect standardZoomFrame(id nsWindow)
{
    id screen = respondsTo(nsWindow, selector("screen")) ? sendId(nsWindow, "screen") : nil;
    if (!screen)
        return CGRectNull;
    const CGRect visible = sendRect(screen, "visibleFrame");
    static constexpr const char* StandardFrame = "windowWillUseStandardFrame:defaultFrame:";
    id delegate = respondsTo(nsWindow, selector("delegate")) ? sendId(nsWindow, "delegate") : nil;
    if (!delegate || !respondsTo(delegate, selector(StandardFrame)))
        return visible;
#if defined(__x86_64__)
    CGRect frame = CGRectNull;
    using Send = void (*)(CGRect*, id, SEL, id, CGRect);
    reinterpret_cast<Send>(objc_msgSend_stret)(&frame, delegate, selector(StandardFrame), nsWindow, visible);
    return frame;
#else
    using Send = CGRect (*)(id, SEL, id, CGRect);
    return reinterpret_cast<Send>(objc_msgSend)(delegate, selector(StandardFrame), nsWindow, visible);
#endif
}

id createSystemMoveMouseDownEvent(id nsWindow)
{
    static constexpr const char* MouseEventFactory =
        "mouseEventWithType:location:modifierFlags:timestamp:windowNumber:context:"
        "eventNumber:clickCount:pressure:";

    Class eventClass = objc_getClass("NSEvent");
    if (!eventClass || !respondsTo(reinterpret_cast<id>(eventClass), selector(MouseEventFactory))
        || !respondsTo(nsWindow, selector("mouseLocationOutsideOfEventStream"))
        || !respondsTo(nsWindow, selector("windowNumber"))) {
        return nil;
    }

    const CGPoint location = sendPoint(nsWindow, "mouseLocationOutsideOfEventStream");
    const long windowNumber = sendLong(nsWindow, "windowNumber");
    using Send = id (*)(Class, SEL, unsigned long, CGPoint, unsigned long, double, long, id, long, long, float);
    return reinterpret_cast<Send>(objc_msgSend)(eventClass, selector(MouseEventFactory), NSEventTypeLeftMouseDown,
        location, 0UL, 0.0, windowNumber, nil, 0L, 1L, 1.0f);
}

// Convert Qt logical pixels to Cocoa points when QT_SCALE_FACTOR or app zoom differs from backingScaleFactor.
// zh_CN: 当 QT_SCALE_FACTOR 或应用缩放使 Qt DPR 不同于 backingScaleFactor 时，用于换算逻辑像素到 Cocoa pt。
qreal pointsPerLogicalPixel(QWidget* window, id nsWindow)
{
    const QWindow* handle = window ? window->windowHandle() : nullptr;
    if (!handle || !respondsTo(nsWindow, selector("backingScaleFactor")))
        return 1.0;
    const qreal backingScale = sendCGFloat(nsWindow, "backingScaleFactor");
    const qreal qtScale = handle->devicePixelRatio();
    return backingScale > 0.0 && qtScale > 0.0 ? qtScale / backingScale : 1.0;
}

void centerTrafficLights(id nsWindow, const QRect& titleBarRect, qreal pointsPerLogical)
{
    if (!respondsTo(nsWindow, selector("standardWindowButton:")))
        return;

    const unsigned long buttons[] = {NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton};

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
            // Map top-down QWidget title-bar center into the traffic-light superview's coordinate system.
            // zh_CN: 将 QWidget 自上而下的标题栏中心换算到交通灯父视图坐标系（需处理翻转坐标）。
            const CGFloat titleBarCenterFromTop = (titleBarRect.y() + titleBarRect.height() / 2.0) * pointsPerLogical;
            const bool flipped = respondsTo(superview, selector("isFlipped")) && sendBool(superview, "isFlipped");
            targetCenterY = flipped ? titleBarCenterFromTop : bounds.size.height - titleBarCenterFromTop;
        }

        const CGFloat y = qRound(targetCenterY - frame.size.height / 2.0);
        sendPoint(button, "setFrameOrigin:", CGPointMake(frame.origin.x, y));
    }
}

void syncUnifiedTitleBarGeometry(QWidget* window, const WindowChromeOptions& options)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow)
        return;

    centerTrafficLights(nsWindow, options.titleBarRect, pointsPerLogicalPixel(window, nsWindow));
}

void syncUnifiedTitleBarAppearance(id nsWindow)
{
    if (!nsWindow)
        return;

    sendLong(nsWindow, "setTitleVisibility:", NSWindowTitleHidden);
    sendBool(nsWindow, "setTitlebarAppearsTransparent:", YES);
    if (respondsTo(nsWindow, selector("setTitlebarSeparatorStyle:")))
        sendLong(nsWindow, "setTitlebarSeparatorStyle:", NSTitlebarSeparatorStyleNone);
    sendBool(nsWindow, "setMovableByWindowBackground:", NO);
}

void syncNativeChromeInteractivity(QWidget* window, const WindowChromeOptions& options)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow || !respondsTo(nsWindow, selector("styleMask")) || !respondsTo(nsWindow, selector("setStyleMask:"))) {
        return;
    }

    const bool qtAllowsResize = window->minimumWidth() < window->maximumWidth()
        || window->minimumHeight() < window->maximumHeight();
    const bool resizeEnabled = options.chromeInteractive && qtAllowsResize;
    const unsigned long styleMask = sendUnsignedLong(nsWindow, "styleMask");
    const unsigned long updatedStyleMask =
        resizeEnabled ? styleMask | NSWindowStyleMaskResizable : styleMask & ~NSWindowStyleMaskResizable;
    if (updatedStyleMask != styleMask)
        sendUnsignedLong(nsWindow, "setStyleMask:", updatedStyleMask);

    if (!respondsTo(nsWindow, selector("standardWindowButton:")))
        return;

    const unsigned long buttons[] = {NSWindowCloseButton, NSWindowMiniaturizeButton, NSWindowZoomButton};
    for (unsigned long buttonType : buttons) {
        id button = sendUnsignedLongReturnsId(nsWindow, "standardWindowButton:", buttonType);
        if (!button || !respondsTo(button, selector("setEnabled:")))
            continue;

        const bool enabled = options.chromeInteractive && (buttonType != NSWindowZoomButton || qtAllowsResize);
        sendBool(button, "setEnabled:", enabled ? YES : NO);
    }
}

void applyUnifiedTitleBar(QWidget* window, const WindowChromeOptions& options)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow)
        return;

    const unsigned long styleMask = sendUnsignedLong(nsWindow, "styleMask");
    sendUnsignedLong(nsWindow, "setStyleMask:", styleMask | NSWindowStyleMaskFullSizeContentView);
    syncUnifiedTitleBarAppearance(nsWindow);
    syncNativeChromeInteractivity(window, options);
    syncUnifiedTitleBarGeometry(window, options);
}

bool performNativeTitleBarDoubleClick(QWidget* window)
{
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
    const char* actionText =
        (action && respondsTo(action, selector("UTF8String"))) ? sendCString(action, "UTF8String") : nullptr;

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

// --- Native vibrancy backdrop ---
// Insert NSVisualEffectView as a sibling immediately behind QNSView (keeping
// NSWindow.contentView == QNSView) so text stays in one Qt backing store and no
// second WindowServer surface samples stale foreground frames.
// zh_CN: 将 NSVisualEffectView 作为 QNSView 下方的同级视图插入 frame view，保留单一 Qt 绘制缓冲且避免重采样前景旧帧。

constexpr char kBackdropBaseIdentifier[] = "fluentBackdropBase";
constexpr char kBackdropTintIdentifier[] = "fluentBackdropTint";
constexpr char kBackdropFailureCoverIdentifier[] = "fluentBackdropFailureCover";

// AppKit enum constants pinned locally to avoid Cocoa headers.
constexpr long NSVisualEffectMaterialSidebar = 7;    // Mica (quieter material)
constexpr long NSVisualEffectMaterialHUDWindow = 13; // Acrylic (livelier frosted glass)
constexpr long NSVisualEffectBlendingModeBehindWindow = 0;
constexpr long NSVisualEffectStateFollowsWindowActiveState = 0;
constexpr long NSWindowAbove = 1;
constexpr long NSWindowBelow = -1;
constexpr unsigned long NSViewWidthSizable = 2;
constexpr unsigned long NSViewHeightSizable = 16;

class BackdropLayerTransaction {
public:
    BackdropLayerTransaction() : m_transaction(objc_getClass("CATransaction"))
    {
        if (!m_transaction)
            return;
        using Send = void (*)(Class, SEL);
        using Disable = void (*)(Class, SEL, BOOL);
        reinterpret_cast<Send>(objc_msgSend)(m_transaction, selector("begin"));
        reinterpret_cast<Disable>(objc_msgSend)(m_transaction, selector("setDisableActions:"), YES);
    }

    ~BackdropLayerTransaction()
    {
        if (!m_transaction)
            return;
        using Send = void (*)(Class, SEL);
        reinterpret_cast<Send>(objc_msgSend)(m_transaction, selector("commit"));
    }

private:
    Class m_transaction;
};

id makeNSString(const char* text)
{
    return sendClassCStringReturnsId("NSString", "stringWithUTF8String:", text);
}

bool identifierEquals(id view, const char* identifier)
{
    if (!view || !respondsTo(view, selector("identifier")))
        return false;

    id current = sendId(view, "identifier");
    const char* text =
        (current && respondsTo(current, selector("UTF8String"))) ? sendCString(current, "UTF8String") : nullptr;
    return text && std::strcmp(text, identifier) == 0;
}

id findSubviewWithIdentifier(id superview, const char* identifier)
{
    if (!superview || !respondsTo(superview, selector("subviews")))
        return nil;

    id subviews = sendId(superview, "subviews");
    if (!subviews)
        return nil;

    const unsigned long count = sendUnsignedLong(subviews, "count");
    for (unsigned long index = 0; index < count; ++index) {
        id view = sendUnsignedLongReturnsId(subviews, "objectAtIndex:", index);
        if (identifierEquals(view, identifier))
            return view;
    }
    return nil;
}

bool isSubviewBelow(id superview, id lowerView, id upperView)
{
    if (!superview || !lowerView || !upperView || !respondsTo(superview, selector("subviews"))) {
        return false;
    }

    id subviews = sendId(superview, "subviews");
    const unsigned long count = subviews ? sendUnsignedLong(subviews, "count") : 0;
    unsigned long lowerIndex = count;
    unsigned long upperIndex = count;
    for (unsigned long index = 0; index < count; ++index) {
        id view = sendUnsignedLongReturnsId(subviews, "objectAtIndex:", index);
        if (view == lowerView)
            lowerIndex = index;
        if (view == upperView)
            upperIndex = index;
    }
    return lowerIndex < upperIndex;
}

void applyEffectAppearance(id effectView, bool dark)
{
    if (!effectView || !respondsTo(effectView, selector("setAppearance:")))
        return;

    // Drive light/dark vibrancy from the in-app theme rather than macOS system appearance.
    // zh_CN: 使用应用内主题而非系统外观驱动材质深浅变体。
    id name = makeNSString(dark ? "NSAppearanceNameDarkAqua" : "NSAppearanceNameAqua");
    id appearance = name ? sendClassIdReturnsId("NSAppearance", "appearanceNamed:", name) : nil;
    if (appearance)
        sendId(effectView, "setAppearance:", appearance);
}

// Find or create the NSVisualEffectView sibling directly below Qt's contentView.
id ensureBackdropView(id superview, id contentView, long material)
{
    id effectView = findSubviewWithIdentifier(superview, kBackdropBaseIdentifier);
    if (effectView) {
        sendLong(effectView, "setMaterial:", material);
        if (!isSubviewBelow(superview, effectView, contentView))
            addSubviewPositioned(superview, effectView, NSWindowBelow, contentView);
        return isSubviewBelow(superview, effectView, contentView) ? effectView : nil;
    }

    const CGRect frame = sendRect(contentView, "frame");
    effectView = allocInitWithFrame("NSVisualEffectView", frame);
    if (!effectView)
        return nil;

    if (id name = makeNSString(kBackdropBaseIdentifier))
        sendId(effectView, "setIdentifier:", name);
    sendBool(effectView, "setHidden:", YES);
    sendBool(effectView, "setWantsLayer:", YES);
    sendUnsignedLong(effectView, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
    sendLong(effectView, "setMaterial:", material);
    sendLong(effectView, "setBlendingMode:", NSVisualEffectBlendingModeBehindWindow);
    sendLong(effectView, "setState:", NSVisualEffectStateFollowsWindowActiveState);
    addSubviewPositioned(superview, effectView, NSWindowBelow, contentView);
    sendVoid(effectView, "release");
    return isSubviewBelow(superview, effectView, contentView) ? effectView : nil;
}

void setMicaTintColor(id view, bool dark, double alpha)
{
    if (!view || !respondsTo(view, selector("layer")))
        return;

    id layer = sendId(view, "layer");
    if (!layer || !respondsTo(layer, selector("setBackgroundColor:")))
        return;

    const CGFloat r = (dark ? 32.0 : 243.0) / 255.0;
    const CGFloat g = (dark ? 32.0 : 242.0) / 255.0;
    const CGFloat b = (dark ? 32.0 : 241.0) / 255.0;
    CGColorRef color = CGColorCreateGenericRGB(r, g, b, alpha);
    using SetBackground = void (*)(id, SEL, CGColorRef);
    reinterpret_cast<SetBackground>(objc_msgSend)(layer, selector("setBackgroundColor:"), color);
    CGColorRelease(color);
}

id ensureTintView(id base)
{
    if (!base || !respondsTo(base, selector("subviews")))
        return nil;

    id subviews = sendId(base, "subviews");
    const unsigned long count = subviews ? sendUnsignedLong(subviews, "count") : 0;
    for (unsigned long index = 0; index < count; ++index) {
        id view = sendUnsignedLongReturnsId(subviews, "objectAtIndex:", index);
        if (!identifierEquals(view, kBackdropTintIdentifier))
            continue;
        sendCGRect(view, "setFrame:", sendRect(base, "bounds"));
        return view;
    }

    const CGRect bounds = sendRect(base, "bounds");
    id view = allocInitWithFrame("NSView", bounds);
    if (!view)
        return nil;

    if (id name = makeNSString(kBackdropTintIdentifier))
        sendId(view, "setIdentifier:", name);
    sendBool(view, "setWantsLayer:", YES);
    sendUnsignedLong(view, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
    addSubviewPositioned(base, view, NSWindowAbove, nil);
    sendVoid(view, "release");
    return view;
}

bool resolveBackdropHost(QWidget* window, id* outContentView, id* outSuperview)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return false;

    id nativeObject = reinterpret_cast<id>(window->winId());
    id nsWindow = nativeWindowFor(window);
    if (!nsWindow || !respondsTo(nsWindow, selector("contentView")))
        return false;

    id contentView = sendId(nsWindow, "contentView");
    if (!contentView || !respondsTo(contentView, selector("superview")))
        return false;
    if (nativeObject != nsWindow && nativeObject != contentView)
        return false;
    if (!respondsTo(contentView, selector("window")) || sendId(contentView, "window") != nsWindow) {
        return false;
    }

    id superview = sendId(contentView, "superview");
    if (!superview || !respondsTo(superview, selector("window")) || sendId(superview, "window") != nsWindow) {
        return false;
    }

    *outContentView = contentView;
    *outSuperview = superview;
    return true;
}

void setBackdropViewHidden(id superview, BOOL hidden)
{
    id base = findSubviewWithIdentifier(superview, kBackdropBaseIdentifier);
    if (base && respondsTo(base, selector("setHidden:")))
        sendBool(base, "setHidden:", hidden);
    if (hidden) {
        id cover = findSubviewWithIdentifier(superview, kBackdropFailureCoverIdentifier);
        if (cover)
            sendBool(cover, "setHidden:", YES);
    }
}

CGRect convertRectFromView(id view, CGRect rect, id fromView)
{
#if defined(__x86_64__)
    CGRect converted = CGRectNull;
    using Send = void (*)(CGRect*, id, SEL, CGRect, id);
    reinterpret_cast<Send>(objc_msgSend_stret)(&converted, view, selector("convertRect:fromView:"), rect, fromView);
    return converted;
#else
    using Send = CGRect (*)(id, SEL, CGRect, id);
    return reinterpret_cast<Send>(objc_msgSend)(view, selector("convertRect:fromView:"), rect, fromView);
#endif
}

void setLayerOpaqueColor(id layer, const QColor& color)
{
    if (!layer)
        return;
    const CGFloat components[] = {color.redF(), color.greenF(), color.blueF(), 1.0};
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGColorRef nativeColor = CGColorCreate(space, components);
    CGColorSpaceRelease(space);
    using Send = void (*)(id, SEL, CGColorRef);
    reinterpret_cast<Send>(objc_msgSend)(layer, selector("setBackgroundColor:"), nativeColor);
    CGColorRelease(nativeColor);
}

void syncBackdropFailureCover(id cover, id contentView, id base, const QColor& opaqueColor)
{
    if (!cover || !contentView || !base)
        return;
    BackdropLayerTransaction transaction;
    sendCGRect(cover, "setFrame:", sendRect(contentView, "frame"));
    setLayerOpaqueColor(sendId(cover, "layer"), opaqueColor);

    // Clip any oversized effect layer while keeping it alive until Qt commits the opaque replacement frame.
    // zh_CN: 裁剪越出内容区的错尺寸材质，同时保留到 Qt 不透明替代帧提交。
    id layer = sendId(base, "layer");
    id mask = layer ? sendId(layer, "mask") : nil;
    if (!mask)
        mask = sendClassId("CALayer", "layer");
    if (mask && layer) {
        sendCGRect(mask, "setFrame:", convertRectFromView(base, sendRect(contentView, "bounds"), contentView));
        setLayerOpaqueColor(mask, Qt::black);
        sendId(layer, "setMask:", mask);
    }
}

void coverInvalidBackdrop(QWidget* window, id superview, id contentView, id base)
{
    // Hidden effect views never painted visible pixels; let the prepared window color cover cold/opaque surfaces.
    // zh_CN: 隐藏的材质视图未产生可见像素，保持隐藏由预备底色覆盖即可。
    if (!base || sendBool(base, "isHidden"))
        return;
    BackdropLayerTransaction transaction;
    id cover = findSubviewWithIdentifier(superview, kBackdropFailureCoverIdentifier);
    if (!cover) {
        cover = allocInitWithFrame("NSView", sendRect(contentView, "frame"));
        if (!cover)
            return;
        sendBool(cover, "setHidden:", YES);
        if (id name = makeNSString(kBackdropFailureCoverIdentifier))
            sendId(cover, "setIdentifier:", name);
        sendBool(cover, "setWantsLayer:", YES);
        sendUnsignedLong(cover, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
        addSubviewPositioned(superview, cover, NSWindowBelow, contentView);
        sendVoid(cover, "release");
    } else {
        addSubviewPositioned(superview, cover, NSWindowBelow, contentView);
    }
    syncBackdropFailureCover(cover, contentView, base, windowBackdropSurfaceColor(window));
    sendBool(cover, "setHidden:", NO);
}

} // namespace

void preparePlatformWindowBackdropSurface(QWidget* window,
    const QColor& opaqueColor,
    fluent::windowing::BackdropSurfaceMode mode)
{
    if (!window || QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;
    id nsWindow = nativeWindowFor(window);
    if (!nsWindow)
        return;
    const bool transparent = mode == fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    using ColorSend = id (*)(Class, SEL, CGFloat, CGFloat, CGFloat, CGFloat);
    id color = reinterpret_cast<ColorSend>(objc_msgSend)(objc_getClass("NSColor"),
        selector("colorWithSRGBRed:green:blue:alpha:"), opaqueColor.redF(), opaqueColor.greenF(), opaqueColor.blueF(),
        transparent ? 0.0 : 1.0);
    sendId(nsWindow, "setBackgroundColor:", color);
    id contentView = sendId(nsWindow, "contentView");
    id superview = contentView ? sendId(contentView, "superview") : nil;
    id cover = findSubviewWithIdentifier(superview, kBackdropFailureCoverIdentifier);
    if (cover && !sendBool(cover, "isHidden")) {
        syncBackdropFailureCover(cover, contentView, findSubviewWithIdentifier(superview, kBackdropBaseIdentifier),
            opaqueColor);
    }
    id layer = contentView ? sendId(contentView, "layer") : nil;
    if (layer) {
        // Disable implicit Core Animation crossfade so background color preparation takes effect immediately.
        // zh_CN: 禁用隐式 Core Animation 渐变动画，使底色立即生效以免露出上一帧。
        Class transaction = objc_getClass("CATransaction");
        using TransactionSend = void (*)(Class, SEL);
        using DisableSend = void (*)(Class, SEL, BOOL);
        if (transaction) {
            reinterpret_cast<TransactionSend>(objc_msgSend)(transaction, selector("begin"));
            reinterpret_cast<DisableSend>(objc_msgSend)(transaction, selector("setDisableActions:"), YES);
        }
        const auto cgColor = reinterpret_cast<CGColorRef>(sendId(color, "CGColor"));
        using LayerSend = void (*)(id, SEL, CGColorRef);
        reinterpret_cast<LayerSend>(objc_msgSend)(layer, selector("setBackgroundColor:"), cgColor);
        if (transaction)
            reinterpret_cast<TransactionSend>(objc_msgSend)(transaction, selector("commit"));
    }
}

bool flushPlatformWindowBackdropSurface(QWidget* window)
{
    if (!window || QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return false;
    id nsWindow = nativeWindowFor(window);
    Class transaction = objc_getClass("CATransaction");
    if (!nsWindow || !transaction)
        return false;
    // Flush pending Core Animation layer commits before hiding vibrancy (do not pump arbitrary events).
    // zh_CN: 先刷出 Qt 已提交的替换图层，再隐藏 vibrancy（不泵送其他事件或重建窗口）。
    using Send = void (*)(Class, SEL);
    reinterpret_cast<Send>(objc_msgSend)(transaction, selector("flush"));
    return true;
}

void applyPlatformWindowFlags(QWidget* window, const WindowChromeOptions& options)
{
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
    Q_UNUSED(globalPos);
    if (!window || QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return false;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow || !respondsTo(nsWindow, selector("performWindowDragWithEvent:")))
        return false;

    // QTBUG-141220: QCocoaWindow::startSystemMove() rejects intermittent trackpad pressure/gesture events
    // while left mouse is still down; invoke performWindowDragWithEvent: directly to preserve native drag & Spaces.
    // zh_CN: QTBUG-141220：QCocoaWindow::startSystemMove() 会间歇性拒绝触控板压力手势；在左键按下时直接交给 AppKit 拖动。
    if ((sendClassUnsignedLong("NSEvent", "pressedMouseButtons") & NSLeftMouseButtonMask) == 0) {
        return false;
    }

    id mouseDownEvent = createSystemMoveMouseDownEvent(nsWindow);
    if (!mouseDownEvent)
        return false;

    sendId(nsWindow, "performWindowDragWithEvent:", mouseDownEvent);
    return true;
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
    if (!options.preferNativeMacControls)
        return false;

    return performNativeTitleBarDoubleClick(window);
}

bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos)
{
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options)
{
    if (!window || !options.preferNativeMacControls)
        return;

    syncUnifiedTitleBarAppearance(nativeWindowFor(window));
    syncNativeChromeInteractivity(window, options);
    syncUnifiedTitleBarGeometry(window, options);
}

int nativeTitleBarLeadingInset(QWidget* window)
{
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return 0;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow || !respondsTo(nsWindow, selector("standardWindowButton:")))
        return 0;

    // Measure the rightmost traffic light (zoom button) + 8pt trailing gap.
    // zh_CN: 读取最右侧交通灯（缩放按钮）的 frame，以计算原生按钮占用宽度。
    id zoomButton = sendUnsignedLongReturnsId(nsWindow, "standardWindowButton:", NSWindowZoomButton);
    if (!zoomButton || !respondsTo(zoomButton, selector("frame")))
        return 0;

    const CGRect frame = sendRect(zoomButton, "frame");
    if (frame.size.width <= 0)
        return 0;

    constexpr int kTrailingGap = 8;
    const qreal pointsPerLogical = pointsPerLogicalPixel(window, nsWindow);
    return static_cast<int>(std::ceil((frame.origin.x + frame.size.width) / pointsPerLogical)) + kTrailingGap;
}

int clientSideFrameMargin(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    Q_UNUSED(options);
    return 0;
}

bool manualMoveResizeFallbackAllowed(QWidget* window, const WindowChromeOptions& options)
{
    Q_UNUSED(window);
    Q_UNUSED(options);
    return false;
}

BackdropCapabilities platformBackdropCapabilities()
{
    BackdropCapabilities capabilities;
    const bool supported = QGuiApplication::platformName() == QStringLiteral("cocoa")
        && objc_getClass("NSVisualEffectView") != nullptr;
    capabilities.alphaSurfaceSupported = supported;
    capabilities.nativeMica = supported;
    capabilities.nativeAcrylic = supported;
    capabilities.provider = supported ? QStringLiteral("mac-vibrancy") : QStringLiteral("painted-material");
    return capabilities;
}

bool requestPlatformForegroundActivation(QWidget* window)
{
    Q_UNUSED(window);
    return false;
}

bool restorePlatformMaximizedFrame(QWidget* window)
{
    // Stage Manager shrinks windows zoomed before ordering in; reapply the standard zoom frame without animation
    // (AppKit retains Qt's original restore frame).
    // zh_CN: 台前调度会把先缩放再显示的窗口收进舞台区；无动画重设标准最大化尺寸，同时保留 AppKit 原有的还原尺寸。
    id nsWindow = nativeWindowFor(window);
    if (!nsWindow || !respondsTo(nsWindow, selector("isZoomed")) || sendBool(nsWindow, "isZoomed")
        || !respondsTo(nsWindow, selector("setFrame:display:animate:")))
        return false;
    const CGRect frame = standardZoomFrame(nsWindow);
    if (CGRectIsNull(frame) || CGRectIsEmpty(frame))
        return false;
    using Send = void (*)(id, SEL, CGRect, BOOL, BOOL);
    reinterpret_cast<Send>(objc_msgSend)(nsWindow, selector("setFrame:display:animate:"), frame, YES, NO);
    return true;
}

bool platformSupportsSystemBackdrop()
{
    const BackdropCapabilities capabilities = platformBackdropCapabilities();
    return capabilities.nativeMica || capabilities.nativeAcrylic;
}

BackdropApplyResult
applyPlatformSystemBackdrop(QWidget* window, BackdropEffect effect, bool dark, bool forceRecomposite)
{
    BackdropApplyResult result;
    if (effect == BackdropEffect::Solid) {
        id contentView = nil;
        id superview = nil;
        if (resolveBackdropHost(window, &contentView, &superview)) {
            BackdropLayerTransaction transaction;
            setBackdropViewHidden(superview, YES);
        }
        result.applied = true;
        result.backend = fluent::windowing::BackdropBackend::Solid;
        result.fidelity = fluent::windowing::BackdropFidelity::Solid;
        result.surfaceMode = fluent::windowing::BackdropSurfaceMode::SolidOpaque;
        result.reason = QStringLiteral("solid-requested");
        return result;
    }

    id contentView = nil;
    id superview = nil;
    if (!resolveBackdropHost(window, &contentView, &superview)) {
        result.reason = QStringLiteral("cocoa-backdrop-host-unavailable");
        return result;
    }

    // Mica uses Sidebar material + stronger tint (cohesive wallpaper tone);
    // Acrylic uses HUDWindow material + lighter tint (stronger desktop blur-through).
    // zh_CN: Mica 用 Sidebar 材质配合较高 tint 呈现克制壁纸色调；Acrylic 用 HUDWindow 配合低 tint 呈现更通透的磨砂玻璃效果。
    const bool acrylic = effect == BackdropEffect::Acrylic;
    const long material = acrylic ? NSVisualEffectMaterialHUDWindow : NSVisualEffectMaterialSidebar;
    BackdropLayerTransaction transaction;
    id base = ensureBackdropView(superview, contentView, material);
    if (!base) {
        // Any surviving material is removed after Qt paints the opaque fallback.
        result.reason = QStringLiteral("visual-effect-view-unavailable");
        return result;
    }

    sendUnsignedLong(base, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
    const CGRect contentFrame = sendRect(contentView, "frame");
    sendCGRect(base, "setFrame:", contentFrame);
    if (!CGRectEqualToRect(sendRect(base, "frame"), contentFrame)) {
        coverInvalidBackdrop(window, superview, contentView, base);
        result.reason = QStringLiteral("visual-effect-view-geometry-mismatch");
        return result;
    }
    applyEffectAppearance(base, dark);

    const double tintAlpha = acrylic ? (dark ? 0.12 : 0.16) : (dark ? 0.20 : 0.22);
    if (id tint = ensureTintView(base)) {
        sendCGRect(tint, "setFrame:", sendRect(base, "bounds"));
        setMicaTintColor(tint, dark, tintAlpha);
    }
    if (id layer = sendId(base, "layer"))
        sendId(layer, "setMask:", nil);
    if (id cover = findSubviewWithIdentifier(superview, kBackdropFailureCoverIdentifier))
        sendBool(cover, "setHidden:", YES);
    sendBool(base, "setHidden:", NO);

    if (forceRecomposite) {
        if (respondsTo(base, selector("setNeedsDisplay:")))
            sendBool(base, "setNeedsDisplay:", YES);
        if (respondsTo(contentView, selector("setNeedsDisplay:")))
            sendBool(contentView, "setNeedsDisplay:", YES);
    }
    result.applied = true;
    result.backend = fluent::windowing::BackdropBackend::MacVibrancy;
    result.fidelity = fluent::windowing::BackdropFidelity::Native;
    result.surfaceMode = fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    result.reason = QStringLiteral("mac-vibrancy-active");
    return result;
}

} // namespace detail
} // namespace compatibility

#endif // Q_OS_MAC
