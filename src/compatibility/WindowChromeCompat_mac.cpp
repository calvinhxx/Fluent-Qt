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
constexpr unsigned long NSWindowStyleMaskResizable = 1UL << 3;
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

void sendVoid(id receiver, const char* name) {
    using Send = void (*)(id, SEL);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name));
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

void sendCGRect(id receiver, const char* name, CGRect value) {
    using Send = void (*)(id, SEL, CGRect);
    reinterpret_cast<Send>(objc_msgSend)(receiver, selector(name), value);
}

id sendClassIdReturnsId(const char* className, const char* name, id value) {
    Class cls = objc_getClass(className);
    if (!cls)
        return nil;

    using Send = id (*)(Class, SEL, id);
    return reinterpret_cast<Send>(objc_msgSend)(cls, selector(name), value);
}

id allocInitWithFrame(const char* className, CGRect frame) {
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

void addSubviewPositioned(id superview, id view, long place, id relativeTo) {
    using Send = void (*)(id, SEL, id, long, id);
    reinterpret_cast<Send>(objc_msgSend)(superview,
                                         selector("addSubview:positioned:relativeTo:"),
                                         view,
                                         place,
                                         relativeTo);
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

void syncNativeChromeInteractivity(QWidget* window, const WindowChromeOptions& options) {
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow || !respondsTo(nsWindow, selector("styleMask"))
        || !respondsTo(nsWindow, selector("setStyleMask:"))) {
        return;
    }

    const bool qtAllowsResize = window->minimumWidth() < window->maximumWidth()
        || window->minimumHeight() < window->maximumHeight();
    const bool resizeEnabled = options.chromeInteractive && qtAllowsResize;
    const unsigned long styleMask = sendUnsignedLong(nsWindow, "styleMask");
    const unsigned long updatedStyleMask = resizeEnabled
        ? styleMask | NSWindowStyleMaskResizable
        : styleMask & ~NSWindowStyleMaskResizable;
    if (updatedStyleMask != styleMask)
        sendUnsignedLong(nsWindow, "setStyleMask:", updatedStyleMask);

    if (!respondsTo(nsWindow, selector("standardWindowButton:")))
        return;

    const unsigned long buttons[] = {
        NSWindowCloseButton,
        NSWindowMiniaturizeButton,
        NSWindowZoomButton
    };
    for (unsigned long buttonType : buttons) {
        id button = sendUnsignedLongReturnsId(nsWindow, "standardWindowButton:", buttonType);
        if (!button || !respondsTo(button, selector("setEnabled:")))
            continue;

        const bool enabled = options.chromeInteractive
            && (buttonType != NSWindowZoomButton || qtAllowsResize);
        sendBool(button, "setEnabled:", enabled ? YES : NO);
    }
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
    syncNativeChromeInteractivity(window, options);
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

// --- Native vibrancy backdrop (the macOS analogue of Windows 11 Mica) ---------------------------
// zh_CN: 原生 vibrancy 背景（Windows 11 Mica 的 macOS 对应物）。
//
// Two stacked sibling layers fill the whole window behind Qt's (translucent) content, so the title
// bar and navigation pane share one cohesive surface that the opaque content card sits on:
//   1. base  — an effect-specific NSVisualEffectView carrying the desktop blur and color;
//   2. tint  — a plain app-surface wash controlling how strongly that backdrop shows through.
// Mica uses a quieter material and stronger tint; Acrylic uses a livelier material and lighter tint.
// zh_CN: 两层兄弟视图铺满整窗、位于 Qt（半透明）内容之后：1) base 使用随效果变化的 NSVisualEffectView
// 承载桌面模糊与色彩；2) tint 通过应用表面色控制背景透出程度。Mica 更克制且 tint 更强，Acrylic 更鲜活且 tint 更浅。

// NSView identifiers used to find-or-reuse our backdrop layers across re-applies.
// zh_CN: NSView 标识符，用于跨多次重新施加时查找/复用我们的背景层。
constexpr char kBackdropBaseIdentifier[] = "fluentBackdropBase";
constexpr char kBackdropTintIdentifier[] = "fluentBackdropTint";

// AppKit enum values pinned locally so this file needs no Cocoa headers.
// zh_CN: 在本地固定 AppKit 枚举值，使本文件无需 Cocoa 头文件。
constexpr long NSVisualEffectMaterialSidebar = 7;           // moderate source-list material → Mica
constexpr long NSVisualEffectMaterialHUDWindow = 13;        // frosted-glass material → Acrylic (most see-through)
constexpr long NSVisualEffectBlendingModeBehindWindow = 0;
constexpr long NSVisualEffectStateFollowsWindowActiveState = 0;
constexpr long NSWindowAbove = 1;
constexpr long NSWindowBelow = -1;
constexpr unsigned long NSViewWidthSizable = 2;
constexpr unsigned long NSViewHeightSizable = 16;

id makeNSString(const char* text) {
    return sendClassCStringReturnsId("NSString", "stringWithUTF8String:", text);
}

bool identifierEquals(id view, const char* identifier) {
    if (!view || !respondsTo(view, selector("identifier")))
        return false;

    id current = sendId(view, "identifier");
    const char* text = (current && respondsTo(current, selector("UTF8String")))
                           ? sendCString(current, "UTF8String")
                           : nullptr;
    return text && std::strcmp(text, identifier) == 0;
}

id findSubviewWithIdentifier(id superview, const char* identifier) {
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

void applyEffectAppearance(id effectView, bool dark) {
    if (!effectView || !respondsTo(effectView, selector("setAppearance:")))
        return;

    // Drive the material's light/dark variant from the app theme rather than the system
    // appearance, so vibrancy matches our in-app theme toggle.
    // zh_CN: 用应用主题（而非系统外观）驱动材质的明暗变体，使 vibrancy 跟随应用内主题切换。
    id name = makeNSString(dark ? "NSAppearanceNameDarkAqua" : "NSAppearanceNameAqua");
    id appearance = name ? sendClassIdReturnsId("NSAppearance", "appearanceNamed:", name) : nil;
    if (appearance)
        sendId(effectView, "setAppearance:", appearance);
}

// Finds, or lazily creates, one of our NSVisualEffectView backdrop layers as a sibling positioned
// behind Qt's content view (so Qt's translucent surface composites over the vibrancy).
// zh_CN: 查找或惰性创建我们的 NSVisualEffectView 背景层之一，作为兄弟视图置于 Qt 内容视图之后
//（这样 Qt 的半透明表面会叠加在 vibrancy 之上）。
id ensureBackdropView(id superview,
                      const char* identifier,
                      long material,
                      long orderingPlace,
                      id relativeTo) {
    id existing = findSubviewWithIdentifier(superview, identifier);
    if (existing) {
        sendLong(existing, "setMaterial:", material);
        return existing;
    }

    const CGRect bounds = sendRect(superview, "bounds");
    id effectView = allocInitWithFrame("NSVisualEffectView", bounds);
    if (!effectView)
        return nil;

    if (id name = makeNSString(identifier))
        sendId(effectView, "setIdentifier:", name);
    sendBool(effectView, "setWantsLayer:", YES);
    sendLong(effectView, "setMaterial:", material);
    sendLong(effectView, "setBlendingMode:", NSVisualEffectBlendingModeBehindWindow);
    sendLong(effectView, "setState:", NSVisualEffectStateFollowsWindowActiveState);
    addSubviewPositioned(superview, effectView, orderingPlace, relativeTo);
    sendVoid(effectView, "release");
    return effectView;
}

// Sets a layer-backed view's app-surface tint at the requested alpha.
// zh_CN: 按指定 alpha 设置图层视图的应用表面 tint。
void setMicaTintColor(id view, bool dark, double alpha) {
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

// Finds, or lazily creates, the plain tint view layered just above the vibrancy (below Qt's
// content). zh_CN: 查找或惰性创建紧贴 vibrancy 之上、Qt 内容之下的纯色 tint 视图。
id ensureTintView(id superview, id base, id contentView) {
    id existing = findSubviewWithIdentifier(superview, kBackdropTintIdentifier);
    if (existing)
        return existing;

    const CGRect bounds = sendRect(superview, "bounds");
    id view = allocInitWithFrame("NSView", bounds);
    if (!view)
        return nil;

    if (id name = makeNSString(kBackdropTintIdentifier))
        sendId(view, "setIdentifier:", name);
    sendBool(view, "setWantsLayer:", YES);
    addSubviewPositioned(superview, view, base ? NSWindowAbove : NSWindowBelow, base ? base : contentView);
    sendVoid(view, "release");
    return view;
}

// Resolves the NSWindow's content view and its (frame-view) superview, the host for our siblings.
// zh_CN: 解析 NSWindow 的 content view 及其（frame view）superview，作为我们兄弟视图的宿主。
bool resolveBackdropHost(QWidget* window, id* outContentView, id* outSuperview) {
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return false;

    id nsWindow = nativeWindowFor(window);
    if (!nsWindow || !respondsTo(nsWindow, selector("contentView")))
        return false;

    id contentView = sendId(nsWindow, "contentView");
    if (!contentView || !respondsTo(contentView, selector("superview")))
        return false;

    id superview = sendId(contentView, "superview");
    if (!superview)
        return false;

    *outContentView = contentView;
    *outSuperview = superview;
    return true;
}

void setBackdropViewsHidden(id superview, BOOL hidden) {
    if (!superview)
        return;
    const char* identifiers[] = {kBackdropBaseIdentifier, kBackdropTintIdentifier};
    for (const char* identifier : identifiers) {
        id view = findSubviewWithIdentifier(superview, identifier);
        if (view && respondsTo(view, selector("setHidden:")))
            sendBool(view, "setHidden:", hidden);
    }
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

bool showPlatformSystemMenu(QWidget* window, const QPoint& globalPos) {
    Q_UNUSED(window);
    Q_UNUSED(globalPos);
    return false;
}

void syncPlatformTitleBarGeometry(QWidget* window, const WindowChromeOptions& options) {
    if (!window || !options.preferNativeMacControls)
        return;

    syncNativeChromeInteractivity(window, options);
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

int clientSideFrameMargin(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    return 0;
}

bool manualMoveResizeFallbackAllowed(QWidget* window, const WindowChromeOptions& options) {
    Q_UNUSED(window);
    Q_UNUSED(options);
    return false;
}

BackdropCapabilities platformBackdropCapabilities() {
    BackdropCapabilities capabilities;
    const bool supported = QGuiApplication::platformName() == QStringLiteral("cocoa")
        && objc_getClass("NSVisualEffectView") != nullptr;
    capabilities.alphaSurfaceSupported = supported;
    capabilities.nativeMica = supported;
    capabilities.nativeAcrylic = supported;
    capabilities.provider = supported ? QStringLiteral("mac-vibrancy")
                                      : QStringLiteral("painted-material");
    return capabilities;
}

bool requestPlatformForegroundActivation(QWidget* window) {
    Q_UNUSED(window);
    return false;
}

bool platformSupportsSystemBackdrop() {
    // macOS gets the Mica-equivalent via a native NSVisualEffectView (vibrancy). Available on
    // every Qt 6.9-supported macOS, so just confirm the class is present.
    // zh_CN: macOS 通过原生 NSVisualEffectView（vibrancy）获得 Mica 等价效果。在 Qt 6.9 支持的所有 macOS
    // 上都可用，故只需确认类存在。
    const BackdropCapabilities capabilities = platformBackdropCapabilities();
    return capabilities.nativeMica || capabilities.nativeAcrylic;
}

BackdropApplyResult applyPlatformSystemBackdrop(QWidget* window,
                                                BackdropEffect effect,
                                                bool dark,
                                                bool forceRecomposite) {
    BackdropApplyResult result;
    Q_UNUSED(forceRecomposite);  // macOS vibrancy re-composites itself; no manual nudge needed.
    // Solid keeps an opaque window: no vibrancy, the app paints its own themeBackdrop.
    // zh_CN: Solid 为不透明窗口：不挂 vibrancy，由 App 自绘 themeBackdrop。
    if (effect == BackdropEffect::Solid) {
        id contentView = nil;
        id superview = nil;
        if (resolveBackdropHost(window, &contentView, &superview))
            setBackdropViewsHidden(superview, YES);
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

    // Three visibly distinct surfaces (Normal/Solid already returned above as fully opaque):
    //   • Mica    — sidebar material + a moderate app tint: a subtle, mostly-cohesive wallpaper tint.
    //   • Acrylic — HUD-window (frosted-glass) material + a light tint: a much more see-through frost
    //               that lets the desktop blur read clearly through the chrome.
    // The previous mapping (window-background + 0.58 tint for Mica) sat so close to opaque that Mica
    // was nearly indistinguishable from Normal, while Acrylic (sidebar + 0.20) read like a proper Mica.
    // zh_CN: 三种可明显区分的表面（Normal/Solid 已在上方按全不透明返回）：
    //   • Mica    —— sidebar material + 中等应用 tint：克制、基本统一的壁纸着色。
    //   • Acrylic —— HUD-window（磨砂玻璃）material + 更浅 tint：更通透，桌面模糊清晰透过 chrome。
    // 旧映射（Mica 用 window-background + 0.58 tint）过于接近不透明，使 Mica 与 Normal 几乎无法区分，
    // 而 Acrylic（sidebar + 0.20）看起来反而才像真正的 Mica。
    const bool acrylic = effect == BackdropEffect::Acrylic;
    const long material = acrylic
        ? NSVisualEffectMaterialHUDWindow
        : NSVisualEffectMaterialSidebar;
    id base = ensureBackdropView(superview,
                                 kBackdropBaseIdentifier,
                                 material,
                                 NSWindowBelow,
                                 contentView);
    if (!base) {
        setBackdropViewsHidden(superview, YES);
        result.reason = QStringLiteral("visual-effect-view-unavailable");
        return result;
    }
    sendBool(base, "setHidden:", NO);

    sendUnsignedLong(base, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
    sendCGRect(base, "setFrame:", sendRect(superview, "bounds"));
    applyEffectAppearance(base, dark);

    const double tintAlpha = acrylic
        ? (dark ? 0.12 : 0.16)
        : (dark ? 0.20 : 0.22);
    if (id tint = ensureTintView(superview, base, contentView)) {
        sendBool(tint, "setHidden:", NO);
        sendUnsignedLong(tint, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
        sendCGRect(tint, "setFrame:", sendRect(superview, "bounds"));
        setMicaTintColor(tint, dark, tintAlpha);
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
