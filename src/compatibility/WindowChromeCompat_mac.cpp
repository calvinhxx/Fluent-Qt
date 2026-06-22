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

    if (respondsTo(nsWindow, selector("standardWindowButton:"))) {
        id zoomButton = sendUnsignedLongReturnsId(nsWindow,
                                                   "standardWindowButton:",
                                                   NSWindowZoomButton);
        if (zoomButton && respondsTo(zoomButton, selector("setEnabled:")))
            sendBool(zoomButton, "setEnabled:", resizeEnabled ? YES : NO);
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
//   1. base  — an NSVisualEffectView (sidebar vibrancy) carrying the desktop's color and warmth;
//   2. tint  — a plain layer washed with the Windows-Mica base color at ~0.42 alpha.
// The lighter tint lets the sidebar material's wallpaper tint come through for a rich, living
// vibrancy (the polished-native-app look), while still keeping the chrome legible and coherent with
// the content. zh_CN: 两层叠放的兄弟视图铺满整窗、位于 Qt（半透明）内容之后，使标题栏与导航窗格成为一整片
// 连贯表面，不透明内容卡片坐落其上：1) base —— NSVisualEffectView（sidebar vibrancy）承载桌面色彩与暖意；
// 2) tint —— 一层以 Windows Mica 基色、约 0.42 alpha 铺成的纯色。更淡的 tint 让 sidebar 材质的壁纸着色透出，
// 形成丰富、鲜活的 vibrancy（精致原生 app 的观感），同时保持 chrome 清晰、与内容协调。

// NSView identifiers used to find-or-reuse our backdrop layers across re-applies.
// zh_CN: NSView 标识符，用于跨多次重新施加时查找/复用我们的背景层。
constexpr char kBackdropBaseIdentifier[] = "fluentBackdropBase";
constexpr char kBackdropTintIdentifier[] = "fluentBackdropTint";

// AppKit enum values pinned locally so this file needs no Cocoa headers.
// zh_CN: 在本地固定 AppKit 枚举值，使本文件无需 Cocoa 头文件。
constexpr long NSVisualEffectMaterialSidebar = 7;           // translucent source-list material
constexpr long NSVisualEffectMaterialWindowBackground = 12; // subtle window-wide material
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
    if (existing)
        return existing;

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
    return effectView;
}

// Sets a layer-backed view's background to the Windows-Mica base color (matching the
// Material::Mica tokens) at the given alpha — a near-opaque app-surface wash over the vibrancy.
// zh_CN: 把图层视图的背景设为 Windows Mica 基色（对齐 Material::Mica token）并取给定 alpha——
// 在 vibrancy 之上铺一层接近不透明的应用表面色。
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

bool platformSupportsSystemBackdrop() {
    // macOS gets the Mica-equivalent via a native NSVisualEffectView (vibrancy). Available on
    // every Qt 6.9-supported macOS, so just confirm the class is present.
    // zh_CN: macOS 通过原生 NSVisualEffectView（vibrancy）获得 Mica 等价效果。在 Qt 6.9 支持的所有 macOS
    // 上都可用，故只需确认类存在。
    return QGuiApplication::platformName() == QStringLiteral("cocoa")
        && objc_getClass("NSVisualEffectView") != nullptr;
}

bool applyPlatformSystemBackdrop(QWidget* window, BackdropEffect effect, bool dark,
                                 bool forceRecomposite) {
    Q_UNUSED(forceRecomposite);  // macOS vibrancy re-composites itself; no manual nudge needed.
    // Solid keeps an opaque window: no vibrancy, the app paints its own themeBackdrop.
    // zh_CN: Solid 为不透明窗口：不挂 vibrancy，由 App 自绘 themeBackdrop。
    if (effect == BackdropEffect::Solid)
        return false;

    id contentView = nil;
    id superview = nil;
    if (!resolveBackdropHost(window, &contentView, &superview))
        return false;

    // Base layer: a subtle, window-wide vibrancy behind everything (incl. the full-width title
    // bar). Sits directly behind Qt's content view and tracks resizes via autoresizing. It supplies
    // the "hint of blurred desktop" under the tint; the tint above supplies most of the color.
    // zh_CN: 基础层：置于一切之后（含整宽标题栏）的低调全窗 vibrancy。紧贴 Qt 内容视图之后，并通过
    // autoresizing 跟随尺寸变化。它在 tint 之下提供「少许模糊桌面」，主色由其上的 tint 提供。
    // The native source-list ("sidebar") material picks up more of the desktop's color and warmth
    // than the flatter windowBackground — paired with a lighter tint below, that gives the chrome a
    // richer, more alive vibrancy (the look of polished native apps) instead of a monotone gray.
    // zh_CN: 原生源列表（sidebar）材质比扁平的 windowBackground 吸收更多桌面色彩与暖意——配合下方更淡的 tint，
    // 让 chrome 呈现更丰富、更鲜活的 vibrancy（精致原生 app 的观感），而非一片单调灰。
    id base = ensureBackdropView(superview,
                                 kBackdropBaseIdentifier,
                                 NSVisualEffectMaterialSidebar,
                                 NSWindowBelow,
                                 contentView);
    if (!base)
        return false;

    sendUnsignedLong(base, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
    sendCGRect(base, "setFrame:", sendRect(superview, "bounds"));
    applyEffectAppearance(base, dark);

    // App-surface tint over the vibrancy: enough to keep the chrome legible and coherent with the
    // (opaque) content, but light enough (~0.42) that the sidebar material's wallpaper color and
    // warmth come through — a rich, living vibrancy rather than a flat tinted gray. Raise it toward
    // 0.7 for a cleaner/flatter pane, lower it toward 0.3 for a more see-through (busier) one.
    // zh_CN: 在 vibrancy 之上的应用表面色：足以让 chrome 清晰、与（不透明）内容协调，但又足够淡（~0.42），
    // 使 sidebar 材质的壁纸色彩与暖意透出——丰富、鲜活的 vibrancy，而非一片扁平着色灰。调高到 0.7 更干净扁平，
    // 调低到 0.3 更通透（也更杂）。
    // Acrylic reads as more see-through than Mica, so lower the app-surface tint for it.
    // zh_CN: Acrylic 比 Mica 更通透，故为其降低应用表面 tint。
    const double tintAlpha = (effect == BackdropEffect::Acrylic) ? 0.30 : 0.42;
    if (id tint = ensureTintView(superview, base, contentView)) {
        sendUnsignedLong(tint, "setAutoresizingMask:", NSViewWidthSizable | NSViewHeightSizable);
        sendCGRect(tint, "setFrame:", sendRect(superview, "bounds"));
        setMicaTintColor(tint, dark, tintAlpha);
    }
    return true;
}

} // namespace detail
} // namespace compatibility

#endif // Q_OS_MAC
