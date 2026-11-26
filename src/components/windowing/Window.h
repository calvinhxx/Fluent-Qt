#ifndef FLUENTWINDOW_H
#define FLUENTWINDOW_H

#include <memory>

#include <QPoint>
#include <QRect>
#include <QWidget>

#include "../../compatibility/WindowChromeCompat.h"
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QMouseEvent;
class QPaintEvent;
class QPainter;
class QPixmap;
class QResizeEvent;
class QShowEvent;
class QVBoxLayout;

namespace fluent::basicinput {
class Button;
}
namespace fluent::spatial {
class SpatialRuntime;
}

namespace fluent::windowing {

class ClientSideFrameEdgeOverlay;
struct ClientSideFramePaintOptions;
struct WindowBackdropMaterialOptions;
class TitleBar;
class WindowResizeSession;

/**
 * @brief Application shell window with title-bar and content hosting; uses native window management by default and client-side frames where unsupported.
 * zh_CN: 带标题栏和内容区的应用主窗口；优先使用系统原生窗口管理，不支持时自动改用自绘边框与拖拽缩放。
 */
class Window : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Widget hosted in the window content area.
     * zh_CN: 窗口内容区承载的控件。
     */
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget)
    Q_PROPERTY(BackdropEffect backdropEffect READ backdropEffect WRITE setBackdropEffect NOTIFY backdropEffectChanged)

public:
    explicit Window(QWidget* parent = nullptr);
    ~Window() override;

    TitleBar* titleBar() const { return m_titleBar; }
    QWidget* contentHost() const { return m_contentHost; }

    /**
     * @brief Visible window frame bounds in local coordinates, excluding outer shadow margins.
     * zh_CN: 窗口局部坐标下的可见窗框矩形（不含外侧阴影边距）。
     */
    QRect chromeFrameRect() const;

    QWidget* contentWidget() const { return m_contentWidget; }
    /**
     * @brief Sets the window content widget without taking ownership.
     * zh_CN: 设置窗口内容区控件（不负责销毁）。
     */
    void setContentWidget(QWidget* widget);

    /**
     * @brief Enables or disables custom title-bar integration; native move and resize remain preferred where supported.
     * zh_CN: 启用或禁用自定义标题栏集成；在支持的平台上仍优先使用系统原生移动和缩放。
     */
    void setCustomWindowChromeEnabled(bool enabled);
    bool customWindowChromeEnabled() const;

    void onThemeUpdated() override;

    /**
     * @brief Re-queries platform capabilities and re-applies the configured backdrop.
     * zh_CN: 重新检测平台能力并应用当前配置的背景效果。
     */
    void reapplySystemBackdrop();

    /**
     * @brief Shows or hides the window, preserving maximized geometry when shown maximized (including under macOS Stage Manager).
     * zh_CN: 显示或隐藏窗口；以最大化状态显示时恢复完整最大化尺寸（兼容 macOS 台前调度）。
     */
    void setVisible(bool visible) override;

    /**
     * @brief Resets transient caption button state and re-applies native chrome before restoring a hidden window.
     * zh_CN: 在隐藏窗口恢复显示前重置标题栏按钮状态并重新应用原生窗口边框配置。
     */
    void prepareForNativeRestore();

    /**
     * @brief Raises and requests foreground activation for the window.
     * zh_CN: 请求将窗口提升并激活到前台。
     */
    void requestForegroundActivation();

    /**
     * @brief Switches the window translucent backdrop effect without recreating the window.
     * zh_CN: 切换窗口半透明背景效果（不重建窗口）。
     */
    void setBackdropEffect(BackdropEffect effect);
    BackdropEffect backdropEffect() const { return m_backdropEffect; }

    /**
     * @brief Resolved backdrop state actually in use by the window.
     * zh_CN: 经平台能力匹配后当前实际生效的背景状态。
     */
    BackdropState backdropState() const { return m_backdropState; }

    /**
     * @brief Backdrop effects supported in the current platform session.
     * zh_CN: 当前平台环境支持的背景效果能力。
     */
    BackdropCapabilities backdropCapabilities() const { return m_backdropCapabilities; }

    /**
     * @brief Enables or disables user window dragging and edge resizing (for example while a modal overlay is open).
     * zh_CN: 启用或禁用通过标题栏和边框拖拽移动或缩放窗口（例如模态弹窗打开期间）。
     */
    void setChromeInteractive(bool interactive);
    bool isChromeInteractive() const { return m_chromeInteractive; }

    /**
     * @brief Sets tooltips for the minimize, maximize, close, and restore caption buttons; empty strings hide them.
     * zh_CN: 设置最小化、最大化、关闭和还原按钮的提示文案；默认全空且库内不预设语言，传空字符串即不显示。
     */
    void setCaptionButtonToolTips(const QString& minimizeTooltip,
        const QString& maximizeTooltip,
        const QString& closeTooltip,
        const QString& restoreTooltip = QString());
    /**
     * @brief Sets accessible names for the caption buttons (empty by default).
     * zh_CN: 设置标题栏按钮的无障碍名称（默认为空，库内不自动生成或翻译）。
     */
    void setCaptionButtonAccessibleNames(const QString& minimizeName,
        const QString& maximizeName,
        const QString& closeName,
        const QString& restoreName = QString());

public slots:
    void minimizeWindow();
    void toggleMaximizeRestore();
    void closeWindow();

signals:
    void backdropEffectChanged(BackdropEffect effect);
    void backdropStateChanged(const BackdropState& state);
    void minimizeRequested();
    void maximizeRequested();
    void restoreRequested();
    void closeRequested();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool nativeEvent(const QByteArray& eventType, void* message, compatibility::FluentNativeEventResult* result)
        override;

private slots:
    void updateChromeOptions();
    void syncTitleBarSystemInsets();
    void syncCaptionButtons();
    void syncCaptionButtonActivation(bool active);
    void handleTitleBarDragStarted(const QPoint& globalPos);
    void handleTitleBarDragMoved(const QPoint& globalPos);
    void handleTitleBarDragFinished();
    void handleTitleBarDoubleClicked(const QPoint& globalPos);
    void handleTitleBarContextMenuRequested(const QPoint& globalPos);

private:
    friend class fluent::spatial::SpatialRuntime;
    void refreshBackdropCapabilities();
    void scheduleBackdropResolution();
    void scheduleNativeChromeRepair();
    void resolveBackdropState(bool applyPlatform, bool forceRecomposite = false);
    void finishPendingBackdropChange();
    void beginOpaqueBackdropCommit(const BackdropState& next);
    void prepareBackdropSurface();
    void setEffectiveBackdropState(const BackdropState& state);
    BackdropState paintedFallbackState(const QString& reason) const;
    void setupCaptionButtons();
    void updateMaximizeButtonIcon();
    bool usesHostedWindowSurface() const;
    bool isEffectivelyActive() const;
    bool isEffectivelyMaximized() const;
    int captionButtonReservedWidth() const;
    int activeClientSideFrameMargin() const;
    QRect windowFrameRect() const;
    ClientSideFramePaintOptions clientSideFramePaintOptions() const;
    WindowBackdropMaterialOptions paintedSurfaceMaterialOptions(bool active) const;
    void invalidatePaintedSurfaceCache();
    void paintPaintedSurface(QPainter& painter, bool includeClientFrame);
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
    BackdropEffect m_backdropEffect = BackdropEffect::Mica;
    BackdropCapabilities m_backdropCapabilities;
    BackdropState m_backdropState;
    quint64 m_backdropStateRevision = 0;
    bool m_windowTranslucent = false;
    bool m_backdropPrimed = false;
    bool m_backdropResolutionPending = false;
    bool m_backdropDisablePending = false;
    bool m_backdropOpaqueFramePainted = false;
    WId m_backdropCommitWinId = 0;
    bool m_nativeChromeRepairPending = false;
    bool m_fallbackDragging = false;
    bool m_chromeInteractive = true;
    QString m_minimizeTooltip;
    QString m_maximizeTooltip;
    QString m_closeTooltip;
    QString m_restoreTooltip;
    QString m_minimizeAccessibleName;
    QString m_maximizeAccessibleName;
    QString m_closeAccessibleName;
    QString m_restoreAccessibleName;
    QPoint m_fallbackDragOffset;
    QRect m_hostedRestoreGeometry;
    std::unique_ptr<WindowResizeSession> m_resizeSession;
    std::unique_ptr<QPixmap> m_paintedSurfaceCache;
    QSize m_paintedSurfaceCachePixelSize;
    qreal m_paintedSurfaceCacheDpr = 0.0;
    bool m_paintedSurfaceCacheIncludesFrame = false;
};

} // namespace fluent::windowing

#endif // FLUENTWINDOW_H
