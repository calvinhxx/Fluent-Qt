#ifndef FLUENTQT_COMPONENTS_STATUS_INFO_TOAST_H
#define FLUENTQT_COMPONENTS_STATUS_INFO_TOAST_H

#include "compatibility/InputEventCompat.h"
#include <QAction>
#include <QMargins>
#include <QMetaObject>
#include <QPixmap>
#include <QPointF>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QEvent;
class QFrame;
class QGraphicsOpacityEffect;
class QHideEvent;
class QHBoxLayout;
class QPaintEvent;
class QPropertyAnimation;
class QTimer;
class QVariantAnimation;
class QVBoxLayout;

namespace fluent::overlay {
class OverlayCoordinator;
}

namespace fluent {
class FontIcon;
}

namespace fluent::basicinput {
class Button;
}

namespace fluent::layout {
class Card;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::status_info {

/**
 * @brief Transient in-app notification toast anchored to a window edge.
 * Managed toasts stack per placement up to `maximumVisible()` inside the anchor's top-level window.
 * zh_CN: 锚定在应用窗口边缘的同窗口短暂通知浮层；通过 showToast() 创建的托管通知会按位置堆叠，上限为 `maximumVisible()`。
 */
class Toast : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QString message READ message WRITE setMessage NOTIFY messageChanged)
    Q_PROPERTY(Severity severity READ severity WRITE setSeverity NOTIFY severityChanged)
    Q_PROPERTY(Placement placement READ placement WRITE setPlacement NOTIFY placementChanged)
    Q_PROPERTY(QMargins placementMargins READ placementMargins WRITE setPlacementMargins NOTIFY placementMarginsChanged)
    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    Q_PROPERTY(QAction* action READ action WRITE setAction NOTIFY actionChanged)
    Q_PROPERTY(bool closable READ isClosable WRITE setClosable NOTIFY closableChanged)
    Q_PROPERTY(bool pauseOnHoverEnabled
        READ isPauseOnHoverEnabled
        WRITE setPauseOnHoverEnabled
        NOTIFY pauseOnHoverEnabledChanged)
    Q_PROPERTY(QString updateKey READ updateKey WRITE setUpdateKey NOTIFY updateKeyChanged)
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY isOpenChanged)
    Q_PROPERTY(qreal toastProgress READ toastProgress WRITE setToastProgress)

public:
    enum Severity { Informational, Success, Warning, Error };
    Q_ENUM(Severity)

    /**
     * @brief Edge anchor for the toast card (`*Start` / `*End` follow layout direction).
     * zh_CN: Toast 卡片的边缘定位方式（`*Start` / `*End` 跟随布局方向）。
     */
    enum Placement { TopStart, Top, TopEnd, BottomStart, Bottom, BottomEnd };
    Q_ENUM(Placement)

    /**
     * @brief Reason that ended one toast presentation lifetime.
     * zh_CN: 结束一次 Toast 展示生命周期的原因。
     */
    enum DismissReason { Programmatic, TimedOut, ActionInvoked, Evicted, CloseButton };
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
     * @brief Optional action button inside the toast; triggering it closes the toast with `ActionInvoked`.
     * zh_CN: Toast 内的可选操作按钮；点击触发后会以 `ActionInvoked` 原因关闭通知。
     */
    QAction* action() const { return m_action.data(); }
    /** @brief Sets the action button shown in the toast (still managed by the caller; not deleted by Toast).
     * zh_CN: 设置要显示的操作按钮（仍由应用管理，不会随通知销毁）。 */
    void setAction(QAction* action);

    /**
     * @brief Whether a close button is shown to dismiss the toast with `CloseButton` (off by default for pointer pass-through).
     * zh_CN: 是否显示关闭按钮（点击以 `CloseButton` 关闭，默认关闭以保留鼠标穿透）。
     */
    bool isClosable() const { return m_closable; }
    void setClosable(bool closable);

    /**
     * @brief Whether hovering pauses the auto-dismiss timer (off by default for pointer pass-through).
     * zh_CN: 鼠标悬停时是否暂停自动关闭计时器（默认关闭以保留鼠标穿透）。
     */
    bool isPauseOnHoverEnabled() const { return m_pauseOnHoverEnabled; }
    void setPauseOnHoverEnabled(bool enabled);

    /**
     * @brief Optional key used by `showOrUpdateToast()` to update an existing toast in place.
     * zh_CN: 供 `showOrUpdateToast()` 匹配并原地更新的标识键。
     */
    QString updateKey() const { return m_updateKey; }
    void setUpdateKey(const QString& key);

    bool isOpen() const { return m_isOpen; }
    qreal toastProgress() const { return m_progress; }
    void setToastProgress(qreal progress);

    /**
     * @brief Maximum managed toasts visible per host window and placement.
     * Changing this affects only future `showToast()` calls, not already-open toasts.
     * zh_CN: 同一窗口同一位置同时显示的托管 Toast 上限；修改后仅对之后创建的 Toast 生效。
     */
    static int maximumVisible();
    static void setMaximumVisible(int count);

    /**
     * @brief Shows this toast inside the anchor's top-level window.
     * zh_CN: 在锚点所属顶层窗口中显示当前 Toast。
     */
    bool present(QWidget* anchor);

    /**
     * @brief Dismisses and hides the toast.
     * zh_CN: 关闭并隐藏当前 Toast。
     */
    void dismiss();

    /**
     * @brief Shows a self-deleting toast and stacks it with other managed toasts, evicting the oldest if exceeding `maximumVisible()`.
     * zh_CN: 显示自动销毁的托管 Toast 并按位置堆叠；超过 `maximumVisible()` 时会关闭最早的一条。
     */
    static Toast* showToast(QWidget* anchor,
        const QString& message,
        Severity severity = Informational,
        int durationMs = 2200,
        Placement placement = Top,
        const QMargins& margins = QMargins(16, 16, 16, 16));

    /**
     * @brief Shows a managed toast, or updates an open one sharing the same host, placement, and non-empty `updateKey`.
     * In-place updates reset the timer and emit `updated()`; an empty key behaves like `showToast()`.
     * zh_CN: 显示托管 Toast，或按窗口、位置与非空 `updateKey` 原地更新已有通知并重置计时器；空键等同于 `showToast()`。
     */
    static Toast* showOrUpdateToast(QWidget* anchor,
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
    void closableChanged(bool closable);
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
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QSize visibleCardSizeHint() const;
    QPoint resolvedEndPosition() const;
    int stackOffset() const;
    void updateMessageWrapping();
    void syncGeometry(bool animatePosition = false);
    void updatePresentationPosition();
    void refreshStackGeometry();
    void startAnimation(qreal endValue);
    void requestDismiss(DismissReason reason, bool immediate = false);
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
    QColor severityBackground() const;
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
    bool m_closable = false;
    bool m_geometryInitialized = false;
    bool m_hoverPaused = false;
    int m_remainingDuration = 0;
    qreal m_progress = 0.0;
    qreal m_enterStartProgress = 0.0;
    qreal m_enterStartOffset = 0.0;
    qreal m_presentationOffset = 0.0;
    bool m_entering = true;
    QPointF m_basePosition;
    QPoint m_targetPosition;
    QSize m_cardSize;
    QString m_updateKey;
    QString m_autoAccessibleName;
    DismissReason m_pendingDismissReason = Programmatic;

    QFrame* m_card = nullptr;
    QHBoxLayout* m_cardLayout = nullptr;
    QVBoxLayout* m_textLayout = nullptr;
    QWidget* m_actionArea = nullptr;
    fluent::layout::Card* m_statusBadge = nullptr;
    fluent::FontIcon* m_icon = nullptr;
    textfields::Label* m_titleLabel = nullptr;
    textfields::Label* m_messageLabel = nullptr;
    basicinput::Button* m_actionButton = nullptr;
    basicinput::Button* m_closeButton = nullptr;
    QPointer<QAction> m_action;
    QGraphicsOpacityEffect* m_opacityEffect = nullptr;
    QPropertyAnimation* m_animation = nullptr;
    QVariantAnimation* m_positionAnimation = nullptr;
    QTimer* m_timer = nullptr;
    overlay::OverlayCoordinator* m_overlayCoordinator = nullptr;
    QMetaObject::Connection m_animationFinishedConnection;
    QMetaObject::Connection m_actionChangedConnection;
    QMetaObject::Connection m_actionDestroyedConnection;

    QColor m_surfaceColor;
    QColor m_borderColor;
    QPixmap m_surfaceCache;
};

} // namespace fluent::status_info

Q_DECLARE_METATYPE(fluent::status_info::Toast::DismissReason)

#endif // FLUENTQT_COMPONENTS_STATUS_INFO_TOAST_H
