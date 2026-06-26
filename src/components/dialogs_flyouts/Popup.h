#ifndef POPUP_H
#define POPUP_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QPointer>
#include <QList>
#include <QFlags>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QGraphicsOpacityEffect;

namespace fluent::dialogs_flyouts {

/**
 * @brief Base same-window popup surface with open state and close policy control.
 * zh_CN: 具备打开态和关闭策略控制的同窗口弹层基础组件。
 *
 * Popup centralizes modal, dim, animation, light-dismiss style behavior, and
 * content hosting for popup-derived controls.
 * zh_CN: Popup 集中管理模态、遮罩、动画、light-dismiss 风格行为和内容承载，
 * 供派生弹层控件复用。
 */
class Popup : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT

public:
    enum CloseFlag {
        NoAutoClose         = 0,
        CloseOnPressOutside = 1 << 0,
        CloseOnEscape       = 1 << 1,
    };
    Q_DECLARE_FLAGS(ClosePolicy, CloseFlag)
    Q_FLAG(ClosePolicy)

    /**
     * @brief Whether the popup, drawer, or notification surface is open.
     * zh_CN: 弹层、抽屉或通知表面是否处于打开状态。
     */
    Q_PROPERTY(bool isOpen READ isOpen WRITE setIsOpen NOTIFY isOpenChanged)
    /**
     * @brief Policy controlling outside-click, escape, or programmatic close behavior.
     * zh_CN: 控制外部点击、Esc 或编程关闭行为的策略。
     */
    Q_PROPERTY(ClosePolicy closePolicy READ closePolicy WRITE setClosePolicy)
    /**
     * @brief Whether the overlay blocks interaction outside itself.
     * zh_CN: 浮层是否阻止其外部交互。
     */
    Q_PROPERTY(bool modal READ isModal WRITE setModal)
    /**
     * @brief Whether a dim/scrim layer is shown behind the overlay.
     * zh_CN: 浮层后方是否显示遮罩层。
     */
    Q_PROPERTY(bool dim READ isDim WRITE setDim)
    /**
     * @brief Whether popup open/close transitions are animated.
     * zh_CN: 弹层打开/关闭过程是否播放过渡动画。
     */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled)
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
    void setClosePolicy(ClosePolicy p) { m_closePolicy = p; }

    bool isModal() const { return m_modal; }
    void setModal(bool m) { m_modal = m; }

    // When true, a CloseOnPressOutside light-dismiss SWALLOWS the dismissing press instead of letting
    // it fall through to whatever is underneath — ComboBox-dropdown semantics (one click only closes
    // the popup; it does not also activate the control beneath). Default false keeps the menu-style
    // behaviour where the dismiss click still hits its target. zh_CN: 为 true 时，CloseOnPressOutside 轻关闭会
    // 「吞掉」这次关闭点击，而非穿透到下方控件——即 ComboBox 下拉的语义（一次点击只关闭弹窗，不会顺带激活下方控件）。
    // 默认 false，保持菜单式行为：关闭点击仍命中其目标。
    bool lightDismissConsumesPress() const { return m_lightDismissConsumesPress; }
    void setLightDismissConsumesPress(bool consume) { m_lightDismissConsumesPress = consume; }

    // Regions exempt from the consume above: a dismissing press that lands inside one of these widgets
    // still falls through to it, so a sibling toolbar / nav bar stays one-click reachable while the
    // popup closes. Only meaningful together with lightDismissConsumesPress. zh_CN: 上面「吞掉」的豁免区域:
    // 落在这些控件内的关闭点击仍会穿透给它们，使同级工具栏/导航栏在弹窗关闭的同时仍可「一次点击」直达。
    // 仅在 lightDismissConsumesPress 为真时有意义。
    void addLightDismissPassthrough(QWidget* widget) {
        if (widget) m_lightDismissPassthrough.append(widget);
    }
    void clearLightDismissPassthrough() { m_lightDismissPassthrough.clear(); }

    bool isDim() const { return m_dim; }
    void setDim(bool d) { m_dim = d; }

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool e) { m_animationEnabled = e; }

    // Overrides the exit (close) animation duration in ms; -1 (default) uses the theme's normal
    // duration (250ms, matching Dialog). Lets a caller make a specific popup's dismissal snappier than
    // the shared default without affecting other Popup users. zh_CN: 覆盖关闭动画时长（毫秒）；-1（默认）
    // 使用主题 normal 时长（250ms，与 Dialog 一致）。让调用方在不影响其它 Popup 使用者的前提下，使某个弹窗的关闭更利落。
    int exitDuration() const { return m_exitDuration; }
    void setExitDuration(int ms) { m_exitDuration = ms; }

    /**
     * @brief Uses a widget as the local theme source when no anchor drives the popup.
     * zh_CN: 无锚点 Popup 显示时用于继承局部主题的来源控件。
     */
    void setThemeSource(QWidget* source);

    double popupProgress() const { return m_popupProgress; }
    void   setPopupProgress(double p);

    /// Positions the popup's top-left in the given widget's local coordinates;
    /// with relativeTo == topLevelWidget this is window-relative. Coordinates
    /// reference the visible card corner; open() compensates the shadow margin.
    /// While open, the popup follows relativeTo and closes when it is fully clipped.
    /// Without a call, open() centers the popup.
    /// zh_CN: 以指定 widget 的局部坐标定位 popup 左上角；relativeTo 为顶层窗口时
    /// 等价于相对顶层定位。坐标以可见卡片左上角为准，open() 自动补偿阴影 margin；
    /// 打开期间会跟随 relativeTo；relativeTo 被完全裁剪后自动关闭。未调用则 open() 时自动居中。
    void setPosition(QWidget* relativeTo, const QPoint& localPos);

public slots:
    void open();
    void close();
    void setIsOpen(bool open);

signals:
    void isOpenChanged(bool open);
    void opened();
    void closed();
    void aboutToShow();
    void aboutToHide();
    void popupProgressChanged(double progress);

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

private:
    QPoint resolvedPosition() const;
    QWidget* trackedPositionAnchor() const;
    QWidget* themeOverrideSource() const;
    bool syncThemeOverrideFromSource();
    void queuePositionSync();
    void syncPositionToAnchor();

    void  startEnterAnimation();
    void  startExitAnimation();
    void  finalizeOpened();
    void  finalizeClosed();

    void  ensureScrim();
    void  destroyScrim();

    QWidget* originalParentTopLevel() const;

    // ── State ───────────────────────────────────────────────────────────────
    bool m_isOpen = false;
    bool m_isClosing = false;

    QPointer<QWidget> m_originalParent;
    QPointer<QWidget> m_topLevel;
    QPointer<QWidget> m_themeSource;

    ClosePolicy m_closePolicy = ClosePolicy(CloseOnPressOutside | CloseOnEscape);
    bool m_lightDismissConsumesPress = false;
    QList<QPointer<QWidget>> m_lightDismissPassthrough;
    bool m_modal = false;
    bool m_dim   = false;
    bool m_animationEnabled = true;
    int  m_exitDuration = -1;
    bool m_positionSet = false;
    QPoint m_targetPos;
    QPointer<QWidget> m_positionRelativeTo;
    QPoint m_positionLocalPos;
    bool m_positionSyncPending = false;

    double m_popupProgress = 0.0;
    QPropertyAnimation* m_anim = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;

    QPointer<QWidget> m_scrim;
};

} // namespace fluent::dialogs_flyouts

Q_DECLARE_OPERATORS_FOR_FLAGS(fluent::dialogs_flyouts::Popup::ClosePolicy)

#endif // POPUP_H
