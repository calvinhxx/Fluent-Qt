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
 * @brief Horizontal toolbar organizing commands into inline primary actions and an overflow menu.
 * Does not take ownership of added QAction or FluentMenu objects; attach a FluentMenu via QAction::setMenu() for nested commands (opening a submenu does not trigger or toggle its action).
 * zh_CN: 水平应用命令栏，将动作划分为内联主命令区与溢出菜单（容纳次命令及收起的主命令）。
 * 不接管 QAction 和通过 QAction::setMenu() 挂载的 FluentMenu 的所有权；展开子菜单不会触发或切换对应动作。
 */
class CommandBar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT

    Q_PROPERTY(LabelPosition labelPosition READ labelPosition WRITE setLabelPosition NOTIFY labelPositionChanged)
    Q_PROPERTY(bool dynamicOverflowEnabled
        READ isDynamicOverflowEnabled
        WRITE setDynamicOverflowEnabled
        NOTIFY dynamicOverflowEnabledChanged)
    Q_PROPERTY(bool overflowOpen READ isOverflowOpen WRITE setOverflowOpen NOTIFY overflowOpenChanged)
    Q_PROPERTY(bool backgroundVisible READ backgroundVisible WRITE setBackgroundVisible NOTIFY backgroundVisibleChanged)

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
     * @brief Adds an action to the primary section without taking ownership.
     * zh_CN: 通过 Qt 控件动作接口将 QAction 添加到主命令区（不接管所有权）。
     */
    void addAction(QAction* action);
    /**
     * @brief Inserts an action into the primary section before `before` without taking ownership.
     * zh_CN: 在指定主命令之前插入动作（不接管所有权）。
     */
    void insertAction(QAction* before, QAction* action);
    /**
     * @brief Removes an action from the bar and its command group without deleting it.
     * zh_CN: 从命令栏及所属命令分组中移除动作，但不删除该动作。
     */
    void removeAction(QAction* action);

    /**
     * @brief Appends an action to the primary section without taking ownership.
     * zh_CN: 将动作追加到主命令区（不接管所有权）。
     */
    bool addPrimaryAction(QAction* action);
    /**
     * @brief Inserts a primary action before `before` without taking ownership.
     * zh_CN: 在指定主命令之前插入动作（不接管所有权）。
     */
    bool insertPrimaryAction(QAction* before, QAction* action);
    /**
     * @brief Appends an action to the secondary (overflow) section without taking ownership.
     * zh_CN: 将动作追加到次命令区（溢出菜单，不接管所有权）。
     */
    bool addSecondaryAction(QAction* action);
    /**
     * @brief Inserts a secondary action before `before` without taking ownership.
     * zh_CN: 在指定次命令之前插入动作（不接管所有权）。
     */
    bool insertSecondaryAction(QAction* before, QAction* action);
    /**
     * @brief Removes an action from either primary or secondary commands without deleting it.
     * zh_CN: 从主命令区或次命令区移除指定动作，但不删除该动作。
     */
    bool removeCommandAction(QAction* action);
    /**
     * @brief Clears all primary commands without deleting the actions.
     * zh_CN: 清空主命令区，不删除其中的动作。
     */
    void clearPrimaryActions();
    /**
     * @brief Clears all secondary commands without deleting the actions.
     * zh_CN: 清空次命令区，不删除其中的动作。
     */
    void clearSecondaryActions();

    /** @brief Returns primary actions in display order.
     *  zh_CN: 按顺序返回主命令列表。
     */
    QList<QAction*> primaryActions() const;
    /** @brief Returns secondary actions in display order.
     *  zh_CN: 按顺序返回次命令列表。
     */
    QList<QAction*> secondaryActions() const;
    /** @brief Returns primary actions currently moved into the overflow menu.
     *  zh_CN: 返回当前因宽度不足收起到溢出菜单中的主命令。
     */
    QList<QAction*> overflowedPrimaryActions() const;
    /** @brief Returns whether a primary action is currently in the overflow menu.
     *  zh_CN: 返回指定主命令当前是否已收起到溢出菜单。
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
     * @brief Opens or closes the overflow menu when overflow items exist.
     * zh_CN: 在存在溢出命令时打开或关闭溢出菜单。
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
