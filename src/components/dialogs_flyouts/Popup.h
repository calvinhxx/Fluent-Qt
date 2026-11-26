#ifndef POPUP_H
#define POPUP_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QPointer>
#include <QList>
#include <QFlags>
#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QGraphicsOpacityEffect;

namespace fluent::overlay {
class OverlayCoordinator;
}

namespace fluent::dialogs_flyouts {

/**
 * @brief Base same-window popup with open state and close policy control.
 * Centralizes modality, scrim dimming, fade animation, light-dismiss behavior, and content hosting.
 * zh_CN: 具备打开状态与关闭策略控制的同窗口弹层基类，统一管理模态、背景遮罩、淡入淡出动画、轻点关闭与内容承载。
 */
class Popup : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    enum CloseFlag {
        NoAutoClose = 0,
        CloseOnPressOutside = 1 << 0,
        CloseOnEscape = 1 << 1,
    };
    Q_DECLARE_FLAGS(ClosePolicy, CloseFlag)
    Q_FLAG(ClosePolicy)

    /**
     * @brief Why an overlay closed.
     * Values 0–4 match TeachingTip::CloseReason for source compatibility.
     * zh_CN: 浮层关闭原因；0–4 与 TeachingTip::CloseReason 保持一致以兼容源码。
     */
    enum CloseReason {
        Programmatic = 0,
        ActionButton = 1,
        CloseButton = 2,
        LightDismiss = 3,
        TargetDestroyed = 4,
        Escape = 5,
    };
    Q_ENUM(CloseReason)

    /**
     * @brief Requested open state (independent of animation progress or widget visibility).
     * zh_CN: 逻辑上的打开状态（不代表动画已结束或 QWidget 当前可见）。
     */
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    /**
     * @brief Policy controlling outside-click, escape, or programmatic close behavior.
     * zh_CN: 控制外部点击、Esc 或编程关闭行为的策略。
     */
    Q_PROPERTY(ClosePolicy closePolicy READ closePolicy WRITE setClosePolicy NOTIFY closePolicyChanged)
    /**
     * @brief Whether the overlay blocks interaction outside itself.
     * zh_CN: 浮层是否阻止其外部交互。
     */
    Q_PROPERTY(bool modal READ isModal WRITE setModal NOTIFY modalChanged)
    /**
     * @brief Whether a dim/scrim layer is shown behind the overlay.
     * zh_CN: 浮层后方是否显示遮罩层。
     */
    Q_PROPERTY(bool dim READ isDim WRITE setDim NOTIFY dimChanged)
    /**
     * @brief Scrim opacity in [0, 1]; -1 uses the theme smoke token. Only applies when dim is enabled.
     * zh_CN: 遮罩不透明度（0–1，-1 表示使用主题默认遮罩色），仅在 dim 开启时生效。
     */
    Q_PROPERTY(double dimOpacity READ dimOpacity WRITE setDimOpacity NOTIFY dimOpacityChanged)
    /**
     * @brief Blur radius [0, 32] for the captured backdrop (0 disables; defaults to 4 when modal, 0 otherwise).
     * zh_CN: 背景模糊半径（0–32，0 为关闭；未设置时模态默认 4，非模态默认 0）。
     */
    Q_PROPERTY(double backdropBlurRadius
        READ backdropBlurRadius
        WRITE setBackdropBlurRadius
        NOTIFY backdropBlurRadiusChanged)
    /**
     * @brief Whether popup open/close transitions are animated.
     * zh_CN: 弹层打开/关闭过程是否播放过渡动画。
     */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    /**
     * @brief Popup open/close animation progress.
     * zh_CN: 弹层打开/关闭动画进度。
     */
    Q_PROPERTY(double popupProgress READ popupProgress WRITE setPopupProgress NOTIFY popupProgressChanged)

public:
    explicit Popup(QWidget* parent = nullptr);
    ~Popup() override;

    void onThemeUpdated() override;

    bool isOpen() const { return m_isOpen; }

    ClosePolicy closePolicy() const { return m_closePolicy; }
    void setClosePolicy(ClosePolicy p);

    bool isModal() const;
    void setModal(bool m);

    /**
     * @brief Whether a CloseOnPressOutside dismissal consumes the mouse press instead of passing it through (default false).
     * zh_CN: 点击外部关闭弹层时是否拦截此次鼠标按下（默认 false 会继续触发下方控件，下拉框等可设为 true）。
     */
    bool lightDismissConsumesPress() const { return m_lightDismissConsumesPress; }
    void setLightDismissConsumesPress(bool consume) { m_lightDismissConsumesPress = consume; }

    /**
     * @brief Exempts a widget from lightDismissConsumesPress() without taking ownership.
     * zh_CN: 添加豁免控件（不负责销毁）；关闭弹层时点击该控件仍会继续触发它。
     */
    void addLightDismissPassthrough(QWidget* widget)
    {
        if (widget)
            m_lightDismissPassthrough.append(widget);
    }
    void clearLightDismissPassthrough() { m_lightDismissPassthrough.clear(); }

    bool isDim() const;
    void setDim(bool d);

    double dimOpacity() const;
    void setDimOpacity(double opacity);

    double backdropBlurRadius() const;
    void setBackdropBlurRadius(double radius);

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool e);

    /**
     * @brief Controls whether close() plays the exit fade animation (open animation is unaffected).
     * zh_CN: 控制 close() 是否播放淡出动画（不影响入场动画）。
     */
    bool isExitAnimationEnabled() const { return m_exitAnimationEnabled; }
    void setExitAnimationEnabled(bool e);

    /**
     * @brief Sets a widget as the fallback theme source when the popup has no anchor.
     * zh_CN: 设置无锚点时用于继承局部主题的来源控件（不负责销毁）。
     */
    void setThemeSource(QWidget* source);

    double popupProgress() const { return m_popupProgress; }
    void setPopupProgress(double p);

    /**
     * @brief Places the popup's top-left at localPos in relativeTo's coordinates; open() centers if unset.
     * zh_CN: 按 relativeTo 局部坐标定位弹层左上角，未设置时 open() 居中显示。
     */
    void setPosition(QWidget* relativeTo, const QPoint& localPos);

public slots:
    void open();
    void close();
    void setIsOpen(bool open);

    /**
     * @brief Refreshes the cached backdrop after application content changes behind an open popup.
     * zh_CN: 弹层打开期间，应用背景内容变化后刷新缓存背景。
     */
    void refreshBackdrop();

    /**
     * @brief Closes with an explicit reason; close() uses Programmatic unless a reason is pending.
     * zh_CN: 带关闭原因关闭；无参 close() 在未挂起原因时视为 Programmatic。
     */
    void closeWithReason(CloseReason reason);

signals:
    void isOpenChanged(bool open);
    /**
     * @brief Opening transition started. Compatibility alias: aboutToShow().
     * zh_CN: 开始打开。兼容别名：aboutToShow()。
     */
    void opening();
    void opened();
    /**
     * @brief Closing transition started. aboutToHide() is the parameterless compatibility alias.
     * zh_CN: 开始关闭。aboutToHide() 是无参兼容别名。
     */
    void closing(CloseReason reason);
    void closed();
    void aboutToShow();
    void aboutToHide();
    void popupProgressChanged(double progress);
    void closePolicyChanged(ClosePolicy policy);
    void modalChanged(bool modal);
    void dimChanged(bool dim);
    void dimOpacityChanged(double opacity);
    void backdropBlurRadiusChanged(double radius);
    void animationEnabledChanged(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    /// Subclasses may override the open()-time placement (Flyout does).
    /// zh_CN: 子类可重写 open() 时的定位策略（Flyout 会重写）。
    virtual QPoint computePosition() const;

    /// Returns the widget that drives automatic placement, if any.
    /// zh_CN: 返回驱动自动定位的锚点控件；没有锚点时返回空。
    virtual QWidget* automaticPositionAnchor() const { return nullptr; }

    /// Subclasses may update their size before host-geometry repositioning.
    /// zh_CN: 子类可在宿主几何变化重定位前同步自身尺寸。
    virtual void onHostGeometryChanged() {}

    /**
     * @brief Allows subclasses to exclude trigger regions from outside-press dismissal.
     * Return false to pass non-modal clicks to the target; modal blocking and Escape still apply.
     * zh_CN: 允许子类排除外部点击关闭的触发区域；返回 false 时非模态点击仍会触发目标控件。
     */
    virtual bool shouldDismissOnOutsidePress(const QPoint& globalPos) const;

    /**
     * @brief Controls whether open() moves keyboard focus into the popup (default true).
     * zh_CN: 控制 open() 是否将键盘焦点移入弹层（默认 true）。
     */
    void setFocusOnOpenEnabled(bool enabled);

    /**
     * @brief Records a close reason used by the next close() if none is passed explicitly.
     * zh_CN: 记录下一次无参 close() 使用的关闭原因。
     */
    void setPendingCloseReason(CloseReason reason);

private:
    QPoint resolvedPosition() const;
    QWidget* trackedPositionAnchor() const;
    QWidget* themeOverrideSource() const;
    bool syncThemeOverrideFromSource();
    void queuePositionSync();
    void syncPositionToAnchor();

    void startEnterAnimation();
    void startExitAnimation();
    void finalizeOpened();
    void finalizeClosed();

    void updateScrimState();
    void destroyScrim();
    void beginClose(CloseReason reason);
    void resetPendingCloseReason();

    QWidget* originalParentTopLevel() const;

    // ── State ───────────────────────────────────────────────────────────────
    bool m_isOpen = false;
    bool m_isClosing = false;
    bool m_openInProgress = false;

    QPointer<QWidget> m_originalParent;
    QPointer<QWidget> m_themeSource;
    QPointer<QWidget> m_focusRestoreTarget;

    ClosePolicy m_closePolicy = ClosePolicy(CloseOnPressOutside | CloseOnEscape);
    bool m_lightDismissConsumesPress = false;
    QList<QPointer<QWidget>> m_lightDismissPassthrough;
    bool m_animationEnabled = true;
    bool m_exitAnimationEnabled = true;
    bool m_positionSet = false;
    QPoint m_targetPos;
    QPointer<QWidget> m_positionRelativeTo;
    QPoint m_positionLocalPos;
    bool m_positionSyncPending = false;

    CloseReason m_pendingCloseReason = Programmatic;
    bool m_closeReasonExplicit = false;

    double m_popupProgress = 0.0;
    QPropertyAnimation* m_anim = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;

    ::fluent::overlay::OverlayCoordinator* m_overlayCoordinator = nullptr;
};

} // namespace fluent::dialogs_flyouts

Q_DECLARE_OPERATORS_FOR_FLAGS(fluent::dialogs_flyouts::Popup::ClosePolicy)

#endif // POPUP_H
