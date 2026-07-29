#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDBAR_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDBAR_H

#include <QList>
#include <QMetaType>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QAction;
class QActionEvent;

namespace fluent::menus_toolbars {

class CommandBarPrivate;

/**
 * @brief Hosts ordered primary and secondary application commands.
 * zh_CN: 承载有序主命令和次命令的应用命令栏。
 *
 * CommandBar borrows caller-owned QAction objects. Its inline and overflow
 * presenters are derived from semantic section membership without changing
 * QAction ownership.
 * zh_CN: CommandBar 借用调用方持有的 QAction；内联和溢出呈现由语义分区派生，
 * 不改变 QAction 的所有权。
 */
class CommandBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT

    Q_PROPERTY(LabelPosition labelPosition
               READ labelPosition
               WRITE setLabelPosition
               NOTIFY labelPositionChanged)
    Q_PROPERTY(bool dynamicOverflowEnabled
               READ isDynamicOverflowEnabled
               WRITE setDynamicOverflowEnabled
               NOTIFY dynamicOverflowEnabledChanged)
    Q_PROPERTY(bool overflowOpen
               READ isOverflowOpen
               WRITE setOverflowOpen
               NOTIFY overflowOpenChanged)
    Q_PROPERTY(bool backgroundVisible
               READ backgroundVisible
               WRITE setBackgroundVisible
               NOTIFY backgroundVisibleChanged)

public:
    /**
     * @brief Controls whether inline command labels are shown beside icons.
     * zh_CN: 控制内联命令标签是否显示在图标右侧。
     */
    enum class LabelPosition {
        Collapsed,
        Right,
    };
    Q_ENUM(LabelPosition)

    explicit CommandBar(QWidget* parent = nullptr);
    ~CommandBar() override;

    using QWidget::addAction;
    using QWidget::insertAction;
    using QWidget::removeAction;

    /**
     * @brief Adds a QAction through the Qt widget API as a primary command.
     * zh_CN: 通过 Qt 控件 API 将 QAction 添加为主命令。
     */
    void addAction(QAction* action);
    /**
     * @brief Inserts a new Qt widget action into primary command order.
     * zh_CN: 将新的 Qt 控件动作插入主命令顺序。
     */
    void insertAction(QAction* before, QAction* action);
    /**
     * @brief Removes Qt widget association and semantic command membership.
     * zh_CN: 移除 Qt 控件关联和语义命令成员关系。
     */
    void removeAction(QAction* action);

    /**
     * @brief Appends a borrowed action to the primary section.
     * zh_CN: 将借用的动作追加到主命令分区。
     */
    bool addPrimaryAction(QAction* action);
    /**
     * @brief Inserts a borrowed primary action before another primary action.
     * zh_CN: 在指定主命令之前插入借用的主命令。
     */
    bool insertPrimaryAction(QAction* before, QAction* action);
    /**
     * @brief Appends a borrowed action to the secondary section.
     * zh_CN: 将借用的动作追加到次命令分区。
     */
    bool addSecondaryAction(QAction* action);
    /**
     * @brief Inserts a borrowed secondary action before another secondary action.
     * zh_CN: 在指定次命令之前插入借用的次命令。
     */
    bool insertSecondaryAction(QAction* before, QAction* action);
    /**
     * @brief Removes an action from either semantic section without deleting it.
     * zh_CN: 从任一语义分区移除动作，但不删除动作。
     */
    bool removeCommandAction(QAction* action);
    /**
     * @brief Clears primary membership without deleting actions.
     * zh_CN: 清空主命令成员关系，但不删除动作。
     */
    void clearPrimaryActions();
    /**
     * @brief Clears secondary membership without deleting actions.
     * zh_CN: 清空次命令成员关系，但不删除动作。
     */
    void clearSecondaryActions();

    /** @brief Returns primary actions in semantic order.
     *  zh_CN: 按语义顺序返回主命令。
     */
    QList<QAction*> primaryActions() const;
    /** @brief Returns secondary actions in semantic order.
     *  zh_CN: 按语义顺序返回次命令。
     */
    QList<QAction*> secondaryActions() const;
    /** @brief Returns the current presentation-only overflow snapshot.
     *  zh_CN: 返回当前仅用于呈现的溢出快照。
     */
    QList<QAction*> overflowedPrimaryActions() const;
    /** @brief Returns whether a primary action is currently overflowed.
     *  zh_CN: 返回指定主命令当前是否进入溢出区。
     */
    bool isPrimaryActionOverflowed(const QAction* action) const;

    LabelPosition labelPosition() const;
    void setLabelPosition(LabelPosition position);

    bool isDynamicOverflowEnabled() const;
    void setDynamicOverflowEnabled(bool enabled);

    bool isOverflowOpen() const;
    bool backgroundVisible() const;
    bool isBackgroundVisible() const { return backgroundVisible(); }
    void setBackgroundVisible(bool visible);

    void onThemeUpdated() override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    /**
     * @brief Opens or closes the overflow presentation when one is available.
     * zh_CN: 在存在可用溢出内容时打开或关闭溢出呈现。
     */
    void setOverflowOpen(bool open);

signals:
    void labelPositionChanged(LabelPosition position);
    void dynamicOverflowEnabledChanged(bool enabled);
    void overflowOpenChanged(bool open);
    void overflowedPrimaryActionsChanged();
    void backgroundVisibleChanged(bool visible);

protected:
    void actionEvent(QActionEvent* event) override;

private:
    Q_DISABLE_COPY(CommandBar)
    CommandBarPrivate* d = nullptr;
};

} // namespace fluent::menus_toolbars

Q_DECLARE_METATYPE(fluent::menus_toolbars::CommandBar::LabelPosition)

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDBAR_H
