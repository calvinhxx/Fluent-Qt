#ifndef FLUENTQT_COMPONENTS_STATUS_INFO_TOAST_H
#define FLUENTQT_COMPONENTS_STATUS_INFO_TOAST_H

#include <QAction>
#include <QMargins>
#include <QMetaObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

#include "compatibility/QtCompat.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QEvent;
class QFrame;
class QGraphicsOpacityEffect;
class QPaintEvent;
class QPropertyAnimation;
class QTimer;

namespace fluent::overlay {
class OverlayCoordinator;
}

namespace fluent {
class FontIcon;
}

namespace fluent::basicinput {
class Button;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::status_info {

/**
 * @brief Transient same-window notification with severity and edge placement.
 * zh_CN: 支持严重级别和边缘定位的同窗口短暂通知。
 *
 * Toast is an in-app overlay rather than an operating-system notification. The
 * application supplies all visible text and the anchor whose top-level window
 * hosts the toast. Managed toasts can stack per placement up to
 * `maximumVisible()`, shifting away from the chosen edge in show order.
 * zh_CN: Toast 是应用内浮层而不是操作系统通知。所有可见文案和用于确定顶层宿主
 * 的锚点均由应用提供。托管 Toast 可按定位点堆叠，数量上限为 `maximumVisible()`，
 * 并按显示顺序沿对应边缘向外偏移。
 */
class Toast : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(Severity severity READ severity WRITE setSeverity NOTIFY severityChanged)
    Q_PROPERTY(Placement placement READ placement WRITE setPlacement
                   NOTIFY placementChanged)
    Q_PROPERTY(QMargins placementMargins READ placementMargins
                   WRITE setPlacementMargins NOTIFY placementMarginsChanged)
    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled
                   WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(QAction* action READ action WRITE setAction
                   NOTIFY actionChanged)
    Q_PROPERTY(bool pauseOnHoverEnabled READ isPauseOnHoverEnabled
                   WRITE setPauseOnHoverEnabled
                   NOTIFY pauseOnHoverEnabledChanged)
    Q_PROPERTY(QString updateKey READ updateKey WRITE setUpdateKey
                   NOTIFY updateKeyChanged)
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY isOpenChanged)
    Q_PROPERTY(qreal toastProgress READ toastProgress WRITE setToastProgress)

public:
    enum Severity {
        Informational,
        Success,
        Warning,
        Error
    };
    Q_ENUM(Severity)

    /**
     * @brief Edge anchor for the toast card.
     * zh_CN: Toast 卡片的边缘锚点。
     *
     * `*Start` / `*End` follow the host layout direction (leading/trailing).
     * zh_CN: `*Start` / `*End` 跟随宿主布局方向（行首/行尾）。
     */
    enum Placement {
        TopStart,
        Top,
        TopEnd,
        BottomStart,
        Bottom,
        BottomEnd
    };
    Q_ENUM(Placement)

    /**
     * @brief Reason that ended one toast presentation lifetime.
     * zh_CN: 结束一次 Toast 展示生命周期的原因。
     */
    enum DismissReason {
        Programmatic,
        TimedOut,
        ActionInvoked,
        Evicted
    };
    Q_ENUM(DismissReason)

    explicit Toast(QWidget* parent = nullptr);
    ~Toast() override;

    QString title() const { return m_title; }
    void setTitle(const QString& title);

    QString message() const { return m_message; }
    void setMessage(const QString& message);

    Severity severity() const { return m_severity; }
    void setSeverity(Severity severity);

    Placement placement() const { return m_placement; }
    void setPlacement(Placement placement);

    QMargins placementMargins() const { return m_placementMargins; }
    void setPlacementMargins(const QMargins& margins);

    int duration() const { return m_duration; }
    void setDuration(int durationMs);

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

    /**
     * @brief Optional caller-owned action presented inside the toast.
     * zh_CN: 在 Toast 内呈现的可选调用方所有动作。
     *
     * The toast borrows the action without reparenting it. Invoking the
     * action from the toast dismisses the current presentation with
     * `ActionInvoked`.
     * zh_CN: Toast 仅借用该动作而不改变其父对象；从 Toast 触发动作后，本次展示将以
     * `ActionInvoked` 原因关闭。
     */
    QAction* action() const { return m_action.data(); }
    void setAction(QAction* action);

    /**
     * @brief Whether an active duration timer pauses while the toast is hovered.
     * zh_CN: 鼠标悬停 Toast 时是否暂停正在运行的持续时间计时器。
     *
     * Disabled by default so a non-interactive toast keeps the existing
     * pointer-pass-through behavior.
     * zh_CN: 默认为关闭，使非交互 Toast 保持既有的鼠标穿透行为。
     */
    bool isPauseOnHoverEnabled() const { return m_pauseOnHoverEnabled; }
    void setPauseOnHoverEnabled(bool enabled);

    /**
     * @brief Optional key used by `showOrUpdateToast()` within one managed stack.
     * zh_CN: `showOrUpdateToast()` 在单个托管堆栈内使用的可选更新键。
     */
    QString updateKey() const { return m_updateKey; }
    void setUpdateKey(const QString& key);

    bool isOpen() const { return m_isOpen; }
    qreal toastProgress() const { return m_progress; }
    void setToastProgress(qreal progress);

    /**
     * @brief Maximum managed toasts kept visible per host and placement.
     * zh_CN: 同一宿主、同一锚点下同时可见的托管 Toast 上限。
     *
     * Intended as process-wide startup configuration. Changing the value does
     * not dismiss already-open toasts; it only affects subsequent `showToast`
     * stacking and eviction.
     * zh_CN: 作为进程级启动配置使用。修改该值不会关闭已打开的 Toast，只影响之后
     * `showToast` 的堆叠与淘汰。
     */
    static int maximumVisible();
    static void setMaximumVisible(int count);

    /**
     * @brief Presents this toast in the anchor's top-level window.
     * zh_CN: 在锚点所属顶层窗口中呈现当前 Toast。
     */
    bool present(QWidget* anchor);

    /**
     * @brief Dismisses the toast and ends its current display lifetime.
     * zh_CN: 关闭 Toast 并结束本次展示生命周期。
     */
    void dismiss();

    /**
     * @brief Creates a self-deleting toast and stacks it with other managed toasts.
     * zh_CN: 创建自动销毁的 Toast，并与同定位的其他托管 Toast 堆叠。
     *
     * When the stack exceeds `maximumVisible()`, the oldest managed toast for
     * that host and placement is dismissed.
     * zh_CN: 超过 `maximumVisible()` 时，会关闭该宿主与定位下最旧的托管 Toast。
     */
    static Toast* showToast(
        QWidget* anchor,
        const QString& message,
        Severity severity = Informational,
        int durationMs = 2200,
        Placement placement = Top,
        const QMargins& margins = QMargins(16, 16, 16, 16));

    /**
     * @brief Shows a managed toast or updates the open toast with the same key.
     * zh_CN: 显示托管 Toast，或原地更新同一更新键对应的已打开 Toast。
     *
     * Matching is scoped by top-level host, placement, and non-empty
     * `updateKey`. An in-place update preserves stack order, resets the
     * duration timer, emits `updated()`, and does not consume another
     * `maximumVisible()` slot. An empty key behaves like `showToast()`.
     * zh_CN: 匹配范围由顶层宿主、定位和非空 `updateKey` 共同确定。原地更新会保留
     * 堆叠顺序、重置持续时间计时器并发出 `updated()`，且不额外占用
     * `maximumVisible()` 名额；空键等同于 `showToast()`。
     */
    static Toast* showOrUpdateToast(
        QWidget* anchor,
        const QString& updateKey,
        const QString& message,
        Severity severity = Informational,
        int durationMs = 2200,
        Placement placement = Top,
        const QMargins& margins = QMargins(16, 16, 16, 16));

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void titleChanged(const QString& title);
    void messageChanged(const QString& message);
    void severityChanged(Severity severity);
    void placementChanged(Placement placement);
    void placementMarginsChanged(const QMargins& margins);
    void durationChanged(int durationMs);
    void animationEnabledChanged(bool enabled);
    void actionChanged(QAction* action);
    void pauseOnHoverEnabledChanged(bool enabled);
    void updateKeyChanged(const QString& key);
    void isOpenChanged(bool open);
    void presented();
    void updated();
    void dismissed();
    void dismissedWithReason(DismissReason reason);

protected:
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QSize visibleCardSizeHint() const;
    QPoint resolvedEndPosition() const;
    int stackOffset() const;
    void updateMessageWrapping();
    void syncGeometry();
    void startAnimation(qreal endValue);
    void requestDismiss(
        DismissReason reason, bool immediate = false);
    void finalizeDismiss();
    void restartDurationTimer();
    void pauseDurationTimer();
    void resumeDurationTimer();
    void updatePointerInteraction();
    void syncActionButton();
    void syncAccessibleName();
    void announceAccessibility();
    QString accessibleAnnouncementText() const;
    void applyPalette();
    QString severityGlyph() const;
    QColor severityForeground() const;
    bool isTopPlacement() const;
    bool isStartPlacement() const;
    bool isEndPlacement() const;
    static void relayoutHostStack(QWidget* host, Placement placement);
    static QVector<Toast*> openToastsFor(QWidget* host, Placement placement);
    static QVector<Toast*> managedOpenToastsFor(QWidget* host, Placement placement);

    QString m_title;
    QString m_message;
    Severity m_severity = Informational;
    Placement m_placement = Top;
    QMargins m_placementMargins = QMargins(16, 16, 16, 16);
    int m_duration = 2200;
    bool m_animationEnabled = true;
    bool m_isOpen = false;
    bool m_deleteOnDismiss = false;
    bool m_dismissInProgress = false;
    bool m_actionInvocationInProgress = false;
    bool m_pauseOnHoverEnabled = false;
    bool m_hoverPaused = false;
    int m_remainingDuration = 0;
    qreal m_progress = 0.0;
    QString m_updateKey;
    QString m_autoAccessibleName;
    DismissReason m_pendingDismissReason = Programmatic;

    QFrame* m_card = nullptr;
    fluent::FontIcon* m_icon = nullptr;
    textfields::Label* m_titleLabel = nullptr;
    textfields::Label* m_messageLabel = nullptr;
    basicinput::Button* m_actionButton = nullptr;
    QPointer<QAction> m_action;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QPropertyAnimation* m_animation = nullptr;
    QTimer* m_timer = nullptr;
    overlay::OverlayCoordinator* m_overlayCoordinator = nullptr;
    QMetaObject::Connection m_animationFinishedConnection;
    QMetaObject::Connection m_actionChangedConnection;
    QMetaObject::Connection m_actionDestroyedConnection;

    QColor m_surfaceColor;
    QColor m_borderColor;
    QColor m_shadowColor;
    qreal m_shadowOpacity = 0.18;
};

} // namespace fluent::status_info

Q_DECLARE_METATYPE(fluent::status_info::Toast::DismissReason)

#endif // FLUENTQT_COMPONENTS_STATUS_INFO_TOAST_H
