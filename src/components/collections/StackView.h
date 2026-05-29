#ifndef STACKVIEW_H
#define STACKVIEW_H

#include <QMetaObject>
#include <QPointer>
#include <QStackedWidget>
#include <QVector>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QGraphicsOpacityEffect;
class QKeyEvent;
class QParallelAnimationGroup;
class QResizeEvent;

namespace fluent::collections {

/**
 * @brief Stacked page host with push/pop navigation semantics.
 * zh_CN: 具备 push/pop 导航语义的堆叠页面宿主。
 *
 * StackView extends QStackedWidget with a navigation stack and Fluent transition
 * control while preserving QWidget page ownership and compatibility.
 * zh_CN: StackView 在 QStackedWidget 基础上增加导航栈和 Fluent 过渡控制，
 * 同时保留 QWidget 页面所有权与兼容性。
 */
class StackView : public QStackedWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Number of pages currently in the stack.
     * zh_CN: 当前堆栈中的页面数量。
     */
    Q_PROPERTY(int depth READ depth NOTIFY depthChanged)
    /**
     * @brief Currently visible stack page.
     * zh_CN: 当前可见的堆栈页面。
     */
    Q_PROPERTY(QWidget* currentItem READ currentItem NOTIFY currentItemChanged)
    /**
     * @brief Initial page inserted into the stack.
     * zh_CN: 插入堆栈的初始页面。
     */
    Q_PROPERTY(QWidget* initialItem READ initialItem WRITE setInitialItem NOTIFY initialItemChanged)
    /**
     * @brief Whether a transition or navigation operation is running.
     * zh_CN: 是否正在执行转场或导航操作。
     */
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    /**
     * @brief Primary layout or motion orientation.
     * zh_CN: 主要布局或运动方向。
     */
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    /**
     * @brief Whether stack transition animation is enabled.
     * zh_CN: 是否启用堆栈转场动画。
     */
    Q_PROPERTY(bool transitionAnimationEnabled READ transitionAnimationEnabled WRITE setTransitionAnimationEnabled NOTIFY transitionAnimationEnabledChanged)
    /**
     * @brief Stack transition duration in milliseconds.
     * zh_CN: 堆栈转场时长，单位为毫秒。
     */
    Q_PROPERTY(int transitionDuration READ transitionDuration WRITE setTransitionDuration NOTIFY transitionDurationChanged)
    /**
     * @brief Transition effect used for stack navigation.
     * zh_CN: 堆栈导航使用的转场效果。
     */
    Q_PROPERTY(StackViewTransitionType transitionType READ transitionType WRITE setTransitionType NOTIFY transitionTypeChanged)

public:
    enum class StackViewItemStatus {
        Inactive,
        Activating,
        Active,
        Deactivating
    };
    Q_ENUM(StackViewItemStatus)

    enum class StackViewTransitionOperation {
        Push,
        Pop,
        Replace,
        Clear,
        None
    };
    Q_ENUM(StackViewTransitionOperation)

    enum class StackViewTransitionType {
        SlideFade,
        ScaleFade,
        Immediate
    };
    Q_ENUM(StackViewTransitionType)

    enum class StackViewItemOwnership {
        OwnsItem,
        DoesNotOwnItem
    };
    Q_ENUM(StackViewItemOwnership)

    explicit StackView(QWidget* parent = nullptr);
    ~StackView() override;

    int depth() const { return m_stack.size(); }
    QWidget* currentItem() const;
    QWidget* initialItem() const;
    bool busy() const { return m_busy; }
    bool canPop() const { return depth() > 1; }

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);

    bool transitionAnimationEnabled() const { return m_transitionAnimationEnabled; }
    void setTransitionAnimationEnabled(bool enabled);

    int transitionDuration() const { return m_transitionDuration; }
    void setTransitionDuration(int durationMs);

    StackViewTransitionType transitionType() const { return m_transitionType; }
    void setTransitionType(StackViewTransitionType type);

    StackViewItemOwnership defaultItemOwnership() const { return m_defaultOwnership; }
    void setDefaultItemOwnership(StackViewItemOwnership ownership);

    void setInitialItem(QWidget* item);
    bool setInitialItem(QWidget* item, StackViewItemOwnership ownership);

    bool push(QWidget* item);
    bool push(QWidget* item, StackViewItemOwnership ownership);
    bool push(const QVector<QWidget*>& items, StackViewItemOwnership ownership = StackViewItemOwnership::OwnsItem);

    bool replace(QWidget* item);
    bool replace(QWidget* item, StackViewItemOwnership ownership);
    bool replace(int index, QWidget* item, StackViewItemOwnership ownership = StackViewItemOwnership::OwnsItem);

    bool pop();
    bool pop(QWidget* item);
    bool popToRoot();
    bool popToItem(QWidget* item);
    bool goBack();
    bool clear();

    bool adoptWidget(QWidget* item, StackViewItemOwnership ownership = StackViewItemOwnership::DoesNotOwnItem);

    QWidget* itemAt(int index) const;
    int indexOf(QWidget* item) const;
    bool contains(QWidget* item) const { return indexOf(item) >= 0; }
    StackViewItemStatus itemStatus(QWidget* item) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

public slots:
    void setCurrentIndex(int index);
    void setCurrentWidget(QWidget* widget);

signals:
    void depthChanged(int depth);
    void currentItemChanged(QWidget* item);
    void initialItemChanged(QWidget* item);
    void busyChanged(bool busy);
    void orientationChanged(Qt::Orientation orientation);
    void transitionAnimationEnabledChanged(bool enabled);
    void transitionDurationChanged(int durationMs);
    void transitionTypeChanged(StackViewTransitionType type);
    void defaultItemOwnershipChanged(StackViewItemOwnership ownership);
    void itemPushed(QWidget* item);
    void itemPopped(QWidget* item);
    void itemReplaced(QWidget* oldItem, QWidget* newItem);
    void itemStatusChanged(QWidget* item, StackViewItemStatus status);
    void transitionStarted(StackViewTransitionOperation operation, QWidget* fromItem, QWidget* toItem);
    void transitionFinished(StackViewTransitionOperation operation, QWidget* fromItem, QWidget* toItem);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct StackEntry {
        QPointer<QWidget> item;
        QWidget* rawItem = nullptr;
        StackViewItemOwnership ownership = StackViewItemOwnership::OwnsItem;
        StackViewItemStatus status = StackViewItemStatus::Inactive;
        QMetaObject::Connection destroyedConnection;
    };

    struct PendingRemoval {
        QPointer<QWidget> item;
        QWidget* rawItem = nullptr;
        StackViewItemOwnership ownership = StackViewItemOwnership::OwnsItem;
        QMetaObject::Connection destroyedConnection;
    };

    bool canStartOperation() const;
    int stackIndexOf(QWidget* item) const;
    StackEntry makeEntry(QWidget* item, StackViewItemOwnership ownership, StackViewItemStatus status);
    void prepareItem(QWidget* item);
    void setItemStatus(QWidget* item, StackViewItemStatus status);
    void emitItemStatus(QWidget* item, StackViewItemStatus status);
    void emitDepthIfChanged(int oldDepth);
    void emitCurrentIfChanged(QWidget* oldCurrent);
    void emitInitialIfChanged(QWidget* oldInitial);
    void setBusy(bool busy);
    void startTransition(StackViewTransitionOperation operation,
                         QWidget* fromItem,
                         QWidget* toItem,
                         const QVector<PendingRemoval>& removals);
    void completeTransition();
    void cleanupPendingRemoval(const PendingRemoval& removal, bool immediateDelete = false);
    void cleanupAll(bool immediateDelete);
    void layoutStackItems();
    QRect itemRect() const;
    QPoint transitionOffset(StackViewTransitionOperation operation, bool entering) const;
    bool shouldAnimate(QWidget* fromItem, QWidget* toItem) const;
    QRect scaledTransitionRect(const QRect& rect, qreal scale) const;
    void setGraphicsOpacity(QWidget* item, qreal opacity, QPointer<QGraphicsOpacityEffect>& effectStore);
    void clearGraphicsOpacity(QWidget* item, QPointer<QGraphicsOpacityEffect>& effectStore);
    void onCurrentChanged(int index);
    void onItemDestroyed(QObject* object);
    void pruneRemovedWidgets();
    void removeEntryAt(int index);

    QVector<StackEntry> m_stack;
    StackViewItemOwnership m_defaultOwnership = StackViewItemOwnership::OwnsItem;
    bool m_busy = false;
    bool m_transitionAnimationEnabled = true;
    int m_transitionDuration = 250;
    StackViewTransitionType m_transitionType = StackViewTransitionType::SlideFade;
    Qt::Orientation m_orientation = Qt::Horizontal;
    QParallelAnimationGroup* m_transitionGroup = nullptr;
    StackViewTransitionOperation m_transitionOperation = StackViewTransitionOperation::None;
    QPointer<QWidget> m_transitionFrom;
    QPointer<QWidget> m_transitionTo;
    QVector<PendingRemoval> m_transitionRemovals;
    QPointer<QGraphicsOpacityEffect> m_fromOpacityEffect;
    QPointer<QGraphicsOpacityEffect> m_toOpacityEffect;
    bool m_internalStackChange = false;
    bool m_destroying = false;
};

} // namespace fluent::collections

Q_DECLARE_METATYPE(fluent::collections::StackView::StackViewItemStatus)
Q_DECLARE_METATYPE(fluent::collections::StackView::StackViewTransitionOperation)
Q_DECLARE_METATYPE(fluent::collections::StackView::StackViewTransitionType)
Q_DECLARE_METATYPE(fluent::collections::StackView::StackViewItemOwnership)

#endif // STACKVIEW_H
