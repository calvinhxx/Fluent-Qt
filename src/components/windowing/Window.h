#ifndef FLUENTWINDOW_H
#define FLUENTWINDOW_H

#include <memory>

#include <QPoint>
#include <QRect>
#include <QWidget>

#include "compatibility/WindowChromeCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QVBoxLayout;

namespace fluent::basicinput {
class Button;
}

namespace fluent::windowing {

class ClientSideFrameEdgeOverlay;
struct ClientSideFramePaintOptions;
class TitleBar;
class WindowResizeSession;

/**
 * @brief Application shell window with title-bar and content hosting.
 * zh_CN: 支持标题栏和内容承载的应用外壳窗口。
 *
 * Window keeps platform chrome policy in one place. Native platform window
 * management is preferred by default; client-side frame and resize handling are
 * fallback/opt-in paths.
 * zh_CN: Window 集中管理平台窗口 chrome 策略。默认优先使用系统窗口管理；
 * 客户端边框和缩放处理仅作为回退或显式启用路径。
 */
class Window : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Caller-owned widget hosted as component content.
     * zh_CN: 作为组件内容承载的调用方控件。
     */
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget)

public:
    explicit Window(QWidget* parent = nullptr);
    ~Window() override;

    TitleBar* titleBar() const { return m_titleBar; }
    QWidget* contentHost() const { return m_contentHost; }

    /**
     * @brief Visible chrome/content frame in this window's local coordinates.
     * zh_CN: 当前窗口局部坐标中的可见 chrome/内容框。
     */
    QRect chromeFrameRect() const;

    QWidget* contentWidget() const { return m_contentWidget; }
    void setContentWidget(QWidget* widget);

    /**
     * @brief Enables Fluent-managed title-bar/non-client integration.
     * zh_CN: 启用由 Fluent 管理的标题栏/非客户区集成。
     *
     * This is an opt-in integration point. The platform adapter still decides
     * whether native move/resize is available or a client-side fallback is needed.
     * zh_CN: 这是显式启用的集成点；平台适配层仍决定使用系统移动/缩放，
     * 还是启用客户端回退。
     */
    void setCustomWindowChromeEnabled(bool enabled);
    bool customWindowChromeEnabled() const;

    void onThemeUpdated() override;

    /**
     * @brief Re-asserts the configured system backdrop (no-op when unsupported).
     * zh_CN: 重新施加当前配置的系统背景；不支持时为空操作。
     */
    void reapplySystemBackdrop();

    /**
     * @brief Sets the window background effect and re-applies it live.
     * zh_CN: 设置窗口背景效果并实时重新施加。
     *
     * The top-level translucency decision is fixed at construction; switching
     * effects updates paint hints and the requested OS backdrop type without
     * restyling the native window.
     * zh_CN: 顶层半透明策略在构造时固定；切换效果只更新绘制提示和请求的系统背景类型。
     */
    void setBackdropEffect(compatibility::BackdropEffect effect);
    compatibility::BackdropEffect backdropEffect() const { return m_backdropEffect; }

    /**
     * @brief Enables/disables user move + resize through the window chrome.
     * zh_CN: 启用或禁用通过窗口 chrome 进行的用户移动和缩放。
     */
    void setChromeInteractive(bool interactive);
    bool isChromeInteractive() const { return m_chromeInteractive; }

public slots:
    void minimizeWindow();
    void toggleMaximizeRestore();
    void closeWindow();

signals:
    void minimizeRequested();
    void maximizeRequested();
    void restoreRequested();
    void closeRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType,
                     void* message,
                     compatibility::FluentNativeEventResult* result) override;

private slots:
    void updateChromeOptions();
    void syncTitleBarSystemInsets();
    void syncCaptionButtons();
    void handleTitleBarDragStarted(const QPoint& globalPos);
    void handleTitleBarDragMoved(const QPoint& globalPos);
    void handleTitleBarDragFinished();
    void handleTitleBarDoubleClicked(const QPoint& globalPos);
    void handleTitleBarContextMenuRequested(const QPoint& globalPos);

private:
    void setupCaptionButtons();
    void updateMaximizeButtonIcon();
    int captionButtonReservedWidth() const;
    int activeClientSideFrameMargin() const;
    QRect windowFrameRect() const;
    ClientSideFramePaintOptions clientSideFramePaintOptions() const;
    void syncClientSideFrameMargins();
    void syncClientSideFrameShape();
    void syncClientSideResizeInput();
    bool usesClientSideResizeInput() const;
    Qt::Edges resizeEdgesAtLocalPos(const QPoint& localPos) const;
    bool handleResizeBorderMouseEvent(QWidget* source, QMouseEvent* event);
    void resizeFromGlobalPoint(const QPoint& globalPos);

    TitleBar* m_titleBar = nullptr;
    QVBoxLayout* m_rootLayout = nullptr;
    QWidget* m_frameHost = nullptr;
    ClientSideFrameEdgeOverlay* m_frameEdgeOverlay = nullptr;
    QWidget* m_contentHost = nullptr;
    QWidget* m_contentWidget = nullptr;
    QWidget* m_captionButtonHost = nullptr;
    fluent::basicinput::Button* m_minimizeButton = nullptr;
    fluent::basicinput::Button* m_maximizeButton = nullptr;
    fluent::basicinput::Button* m_closeButton = nullptr;
    compatibility::WindowChromeCompat m_chrome;
    compatibility::BackdropEffect m_backdropEffect = compatibility::BackdropEffect::Mica;
    bool m_windowTranslucent = false;
    bool m_systemBackdropSupported = false;
    bool m_micaBackdrop = false;
    bool m_micaBackdropPrimed = false;
    bool m_fallbackDragging = false;
    bool m_chromeInteractive = true;
    QPoint m_fallbackDragOffset;
    std::unique_ptr<WindowResizeSession> m_resizeSession;
};

} // namespace fluent::windowing

#endif // FLUENTWINDOW_H
