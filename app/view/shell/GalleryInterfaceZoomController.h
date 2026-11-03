#ifndef GALLERYINTERFACEZOOMCONTROLLER_H
#define GALLERYINTERFACEZOOMCONTROLLER_H

#include <QList>
#include <QObject>
#include <QPointer>
#include <QSize>

class QAction;
class QEvent;
class QTimer;
class QWidget;

namespace fluent::basicinput {
class Button;
}

namespace fluent::dialogs_flyouts {
class Popup;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

/**
 * @brief Owns browser-style interface zoom for one top-level window: its commands, saved level,
 * and the transient zoom bubble.
 * zh_CN: 管理一个顶层窗口的浏览器式界面缩放：缩放命令、已保存档位与短暂显示的缩放气泡。
 *
 * The native window keeps its size while its content relayouts at the new scale. Its logical
 * minimum follows metrics::AppWindow::minimumSize(), so the native minimum stays fixed. Like a
 * browser, each zoom command shows a compact bubble below the trailing title bar with zoom out,
 * the level, zoom in, and Reset; it closes shortly after the last change.
 * zh_CN: 原生窗口尺寸保持不变，内容按新倍率重新布局；逻辑最小尺寸遵循
 * metrics::AppWindow::minimumSize()，使原生最小尺寸固定。与浏览器一致，每次缩放命令在标题栏尾部下方
 * 显示紧凑气泡（缩小、档位、放大、Reset），最后一次变化后片刻自动收起。
 */
class GalleryInterfaceZoomController final : public QObject {
    Q_OBJECT
public:
    explicit GalleryInterfaceZoomController(QWidget* window);

    /// Zoom In, Zoom Out, and Actual Size commands, in menu order. zh_CN: 按菜单顺序返回放大、缩小、实际大小命令。
    QList<QAction*> commands() const;

    void zoomIn();
    void zoomOut();
    void resetZoom();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void changeZoom(int percent);
    void applySavedZoom(int percent);
    void buildBubble();
    void showBubble();
    void placeBubble();
    void syncBubble();
    void closeBubbleWhenIdle();
    QSize minimumSizeAt(int percent) const;

    QWidget* m_window = nullptr;
    int m_appliedPercent = 100;
    QList<QAction*> m_commands;
    QPointer<fluent::dialogs_flyouts::Popup> m_bubble;
    QPointer<fluent::basicinput::Button> m_zoomOutButton;
    QPointer<fluent::basicinput::Button> m_zoomInButton;
    QPointer<fluent::basicinput::Button> m_resetButton;
    QPointer<fluent::textfields::Label> m_levelLabel;
    QPointer<fluent::textfields::Label> m_restartHint;
    QTimer* m_idleTimer = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERYINTERFACEZOOMCONTROLLER_H
