#include <FluentQt/WebAssembly.h>

#include <QApplication>
#include <QEvent>
#include <QPointer>
#include <QRect>
#include <QResizeEvent>
#include <QTimer>
#include <QWidget>

#include <emscripten/emscripten.h>

#include "compatibility/private/RuntimePlatformCapabilities_p.h"

namespace fluent::webassembly {
namespace {

class BrowserDesktopSurface;

QPointer<BrowserDesktopSurface>& desktopSurface()
{
    static QPointer<BrowserDesktopSurface> surface;
    return surface;
}

QPointer<QWidget>& primaryHostedWindow()
{
    static QPointer<QWidget> window;
    return window;
}

QRect constrainedGeometry(const QRect& requested, const QWidget* desktop)
{
    if (!desktop)
        return requested;

    const QRect bounds = desktop->rect();
    QSize size = requested.size().boundedTo(bounds.size());
    size.setWidth(qMax(1, size.width()));
    size.setHeight(qMax(1, size.height()));
    const int x = qBound(bounds.left(), requested.x(),
                         qMax(bounds.left(), bounds.right() - size.width() + 1));
    const int y = qBound(bounds.top(), requested.y(),
                         qMax(bounds.top(), bounds.bottom() - size.height() + 1));
    return QRect(QPoint(x, y), size);
}

void publishPrimaryWindowGeometry()
{
    QWidget* window = primaryHostedWindow();
    if (!window || !window->parentWidget())
        return;

    const QRect geometry = window->geometry();
    const bool maximized = geometry == window->parentWidget()->rect();
    EM_ASM({
        const root = document.documentElement;
        root.dataset.fluentQtWindowX = String($0);
        root.dataset.fluentQtWindowY = String($1);
        root.dataset.fluentQtWindowWidth = String($2);
        root.dataset.fluentQtWindowHeight = String($3);
        root.dataset.fluentQtWindowMaximized = $4 ? 'true' : 'false';
    }, geometry.x(), geometry.y(), geometry.width(), geometry.height(), maximized);
}

class BrowserDesktopSurface final : public QWidget {
public:
    using QWidget::QWidget;

    void hostWindow(QWidget* window,
                    const QRect& normalGeometry,
                    WindowPresentation presentation)
    {
        if (!window)
            return;

        window->setParent(this, Qt::Widget);
        window->installEventFilter(this);
        if (!primaryHostedWindow()) {
            primaryHostedWindow() = window;
            connect(window, &QObject::destroyed, this, [] {
                primaryHostedWindow().clear();
            });
        }

        // Apply the normal geometry first so Fluent Window can retain a
        // meaningful restore rectangle even when the requested presentation is
        // maximized.
        // zh_CN: 先施加普通几何，使 Fluent Window 即使初始为最大化展示，也能保留
        // 有意义的还原矩形。
        window->setGeometry(constrainedGeometry(normalGeometry, this));
        if (presentation == WindowPresentation::Maximized)
            window->setGeometry(rect());
        window->show();
        window->raise();
        publishPrimaryWindowGeometry();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == primaryHostedWindow() && event) {
            switch (event->type()) {
            case QEvent::Move:
            case QEvent::Resize:
            case QEvent::Show:
            case QEvent::Hide:
                QTimer::singleShot(0, this, &publishPrimaryWindowGeometry);
                break;
            default:
                break;
            }
        }
        return QWidget::eventFilter(watched, event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        const QRect previousStage(QPoint(0, 0), event->oldSize());
        QWidget::resizeEvent(event);
        const QRect nextStage = rect();
        const auto windows = findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget* window : windows) {
            if (window->geometry() == previousStage) {
                window->setGeometry(nextStage);
                continue;
            }
            window->setGeometry(constrainedGeometry(window->geometry(), this));
        }
        publishPrimaryWindowGeometry();
    }
};

void ensureDesktopSurface()
{
    if (desktopSurface() || !qApp)
        return;

    auto* surface = new BrowserDesktopSurface();
    surface->setObjectName(QStringLiteral("fluentQtBrowserDesktopSurface"));
    surface->setWindowFlags(
        Qt::Window
        | Qt::FramelessWindowHint);
    surface->setAttribute(Qt::WA_TranslucentBackground);
    surface->setAutoFillBackground(false);
    desktopSurface() = surface;
    QObject::connect(qApp, &QCoreApplication::aboutToQuit,
                     surface, &QObject::deleteLater);
    // Qt WASM assigns a browser-sized canvas to every top-level QWidget. Own
    // exactly one such surface and host Fluent application windows inside it;
    // this preserves real widget move/resize geometry without a fake HTML
    // title bar or duplicated full-screen canvases. The surface must remain
    // focusable: Qt's browser input context focuses a hidden HTML input only
    // while its owning QWindow accepts activation. WindowDoesNotAcceptFocus or
    // WA_ShowWithoutActivating would leave mouse input working but silently
    // discard all real keyboard and IME input for hosted text controls.
    // zh_CN: Qt WASM 会为每个顶层 QWidget 分配浏览器大小的画布。这里只保留一个
    // 桌面表面并在其中承载 Fluent 应用窗口，从而保留真实控件移动/缩放几何，且无需
    // 伪 HTML 标题栏或重复的全屏画布。该表面必须允许聚焦：Qt 浏览器输入上下文
    // 仅会在所属 QWindow 可激活时聚焦隐藏 HTML input；禁用焦点会造成鼠标仍可用，
    // 但所有文本控件的真实键盘及输入法输入被静默丢弃。
    surface->showMaximized();
    surface->lower();
}

} // namespace

void configureRuntime()
{
    compatibility::detail::RuntimePlatformCapabilities capabilities;
    capabilities.customWindowChromePreferred = true;
    capabilities.clientSideFrameMargin = 12;
    capabilities.manualMoveResizeFallback = true;
    // Qt's browser popup windows do not provide a dependable alpha surface.
    // Keep menus opaque and animation-stable while regular Fluent windows use
    // the Qt screen compositor.
    // zh_CN: Qt 浏览器 popup 窗口没有稳定的 alpha 表面；菜单使用不透明、
    // 无揭示闪烁的表面，普通 Fluent Window 仍由 Qt screen compositor 管理。
    capabilities.translucentPopupSurfaces = false;
    capabilities.hostsApplicationWindowsInDesktopSurface = true;
    // A transparent child title bar forces the software Mica field below it to
    // repaint and looks soft at reduced backing-store DPR. Give browser chrome
    // an opaque token surface and cache the otherwise static painted window
    // material; desktop compositors keep their existing behavior.
    // zh_CN: 透明子标题栏会迫使下方软件 Mica 重绘，并在降低 backing-store DPR
    // 时显得发虚。浏览器 chrome 使用不透明 token 表面，同时缓存静态窗口材质；
    // 桌面合成器行为保持不变。
    capabilities.opaqueClientTitleBarSurface = true;
    capabilities.cachePaintedWindowSurfaces = true;
    compatibility::detail::setRuntimePlatformCapabilities(capabilities);
}

void showWindow(QWidget* window,
                const QRect& normalGeometry,
                WindowPresentation presentation)
{
    if (!window)
        return;

    ensureDesktopSurface();
    BrowserDesktopSurface* surface = desktopSurface();
    if (!surface)
        return;
    surface->hostWindow(window, normalGeometry, presentation);

    // Reassert once Qt has completed the first child layout turn. This remains
    // QWidget-only; browser-specific ownership stays inside this adapter.
    // zh_CN: Qt 完成首轮子控件布局后再确认一次。公共组件仍只处理 QWidget，
    // 浏览器特有的宿主所有权完全留在该适配层。
    const QPointer<QWidget> guard(window);
    const QPointer<BrowserDesktopSurface> surfaceGuard(surface);
    QTimer::singleShot(0, window, [guard, normalGeometry]() {
        if (guard)
            guard->update();
    });
    QTimer::singleShot(100, window, [guard, surfaceGuard, normalGeometry, presentation]() {
        if (guard && surfaceGuard) {
            const QRect geometry = presentation == WindowPresentation::Maximized
                ? surfaceGuard->rect()
                : constrainedGeometry(normalGeometry, surfaceGuard);
            guard->setGeometry(geometry);
            guard->raise();
            publishPrimaryWindowGeometry();
        }
    });
}

} // namespace fluent::webassembly
