#ifndef FLUENTWINDOW_H
#define FLUENTWINDOW_H

#include <QPoint>
#include <QWidget>

#include "compatibility/WindowChromeCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QShowEvent;

namespace fluent::basicinput {
class Button;
}

namespace fluent::windowing {

class TitleBar;

/**
 * @brief Application shell window with custom title bar and content hosting.
 * zh_CN: 支持自定义标题栏和内容承载的应用外壳窗口。
 *
 * Window provides the top-level surface for composing a TitleBar and application
 * content widget while keeping window chrome decisions in one component.
 * zh_CN: Window 提供组合 TitleBar 与应用内容控件的顶层表面，并集中管理窗口 chrome 决策。
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
    ~Window() override = default;

    TitleBar* titleBar() const { return m_titleBar; }
    QWidget* contentHost() const { return m_contentHost; }

    QWidget* contentWidget() const { return m_contentWidget; }
    void setContentWidget(QWidget* widget);

    void onThemeUpdated() override;

    /**
     * @brief Re-asserts the Mica system backdrop (no-op when unsupported).
     * zh_CN: 重新施加 Mica 系统背景（不支持时为空操作）。
     *
     * DWM occasionally fails to composite the backdrop on the very first show — a timing race
     * with the translucent window coming up, so it lands on a flat neutral surface until the
     * window is next activated. Call this from a reliably-late point (e.g. once startup loading
     * finishes) to force the backdrop on without the user switching away and back.
     * zh_CN: DWM 偶尔在首次显示时未能合成背景——与半透明窗口启动存在时序竞争，于是停在一片扁平中性表面，
     * 直到窗口下次被激活。从一个可靠的较晚时机调用（如启动加载完成后），即可强制背景显示，无需用户切走再切回。
     */
    void reapplySystemBackdrop();

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

    TitleBar* m_titleBar = nullptr;
    QWidget* m_contentHost = nullptr;
    QWidget* m_contentWidget = nullptr;
    QWidget* m_captionButtonHost = nullptr;
    fluent::basicinput::Button* m_minimizeButton = nullptr;
    fluent::basicinput::Button* m_maximizeButton = nullptr;
    fluent::basicinput::Button* m_closeButton = nullptr;
    compatibility::WindowChromeCompat m_chrome;
    // True when a Windows 11 Mica backdrop is in use: the window is translucent and the
    // chrome paints transparent so the OS-managed backdrop (incl. active/inactive) shows.
    // zh_CN: 启用 Windows 11 Mica 背景时为真：窗口半透明、chrome 透明绘制，露出系统管理的背景
    //（含激活/非激活）。
    bool m_micaBackdrop = false;
    // One-shot guard so the deferred first-show backdrop refresh (which works around DWM not
    // compositing Mica until the next activation) runs only once.
    // zh_CN: 一次性标记：延迟到首次显示后再刷新背景（规避 DWM 要到下次激活才合成 Mica 的问题）仅执行一次。
    bool m_micaBackdropPrimed = false;
    bool m_fallbackDragging = false;
    QPoint m_fallbackDragOffset;
};

} // namespace fluent::windowing

#endif // FLUENTWINDOW_H
