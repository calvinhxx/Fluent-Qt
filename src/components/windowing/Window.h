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
    bool m_fallbackDragging = false;
    QPoint m_fallbackDragOffset;
};

} // namespace fluent::windowing

#endif // FLUENTWINDOW_H
