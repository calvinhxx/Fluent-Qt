#ifndef POPUP_H
#define POPUP_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QPointer>
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

    bool isDim() const { return m_dim; }
    void setDim(bool d) { m_dim = d; }

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool e) { m_animationEnabled = e; }

    double popupProgress() const { return m_popupProgress; }
    void   setPopupProgress(double p);

    /// Positions the popup's top-left in the given widget's local coordinates;
    /// with relativeTo == topLevelWidget this is window-relative. Coordinates
    /// reference the visible card corner; open() compensates the shadow margin.
    /// Without a call, open() centers the popup.
    /// zh_CN: 以指定 widget 的局部坐标定位 popup 左上角；relativeTo 为顶层窗口时
    /// 等价于相对顶层定位。坐标以可见卡片左上角为准，open() 自动补偿阴影 margin；
    /// 未调用则 open() 时自动居中。
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

private:
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

    ClosePolicy m_closePolicy = ClosePolicy(CloseOnPressOutside | CloseOnEscape);
    bool m_modal = false;
    bool m_dim   = false;
    bool m_animationEnabled = true;
    bool m_positionSet = false;
    QPoint m_targetPos;

    double m_popupProgress = 0.0;
    QPropertyAnimation* m_anim = nullptr;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;

    QPointer<QWidget> m_scrim;
};

} // namespace fluent::dialogs_flyouts

Q_DECLARE_OPERATORS_FOR_FLAGS(fluent::dialogs_flyouts::Popup::ClosePolicy)

#endif // POPUP_H
