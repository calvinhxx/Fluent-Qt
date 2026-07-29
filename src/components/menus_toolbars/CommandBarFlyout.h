#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDBARFLYOUT_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDBARFLYOUT_H

#include <QList>
#include <QMetaType>
#include <QPoint>

#include "components/dialogs_flyouts/Flyout.h"

class QAction;
class QActionEvent;

namespace fluent::menus_toolbars {

class CommandBarFlyoutPrivate;

/**
 * @brief Contextual command surface with collapsed and expanded show modes.
 * zh_CN: 具有折叠和展开显示模式的上下文命令表面。
 *
 * The flyout borrows QAction objects and supports repeatable widget-anchor or
 * local-point invocation inside its owning top-level window.
 * zh_CN: 该浮层借用 QAction，并支持在所属顶层窗口内按控件锚点或局部点重复调用。
 */
class CommandBarFlyout final : public dialogs_flyouts::Flyout {
    Q_OBJECT

    Q_PROPERTY(ShowMode showMode
               READ showMode
               WRITE setShowMode
               NOTIFY showModeChanged)
    Q_PROPERTY(bool expanded
               READ isExpanded
               WRITE setExpanded
               NOTIFY expandedChanged)
    Q_PROPERTY(bool alwaysExpanded
               READ isAlwaysExpanded
               WRITE setAlwaysExpanded
               NOTIFY alwaysExpandedChanged)

public:
    /**
     * @brief Controls focus and initial expansion behavior when opening.
     * zh_CN: 控制打开时的焦点与初始展开行为。
     */
    enum class ShowMode {
        Standard,
        Transient,
    };
    Q_ENUM(ShowMode)

    explicit CommandBarFlyout(QWidget* parent = nullptr);
    ~CommandBarFlyout() override;

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

    /** @brief Appends a borrowed action to the primary section.
     *  zh_CN: 将借用的动作追加到主命令分区。
     */
    bool addPrimaryAction(QAction* action);
    /** @brief Inserts a borrowed primary action before another primary action.
     *  zh_CN: 在指定主命令之前插入借用的主命令。
     */
    bool insertPrimaryAction(QAction* before, QAction* action);
    /** @brief Appends a borrowed action to the secondary section.
     *  zh_CN: 将借用的动作追加到次命令分区。
     */
    bool addSecondaryAction(QAction* action);
    /** @brief Inserts a borrowed secondary action before another secondary action.
     *  zh_CN: 在指定次命令之前插入借用的次命令。
     */
    bool insertSecondaryAction(QAction* before, QAction* action);
    /** @brief Removes an action from either section without deleting it.
     *  zh_CN: 从任一分区移除动作，但不删除动作。
     */
    bool removeCommandAction(QAction* action);
    /** @brief Clears primary membership without deleting actions.
     *  zh_CN: 清空主命令成员关系，但不删除动作。
     */
    void clearPrimaryActions();
    /** @brief Clears secondary membership without deleting actions.
     *  zh_CN: 清空次命令成员关系，但不删除动作。
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

    ShowMode showMode() const;
    void setShowMode(ShowMode mode);

    bool isExpanded() const;
    bool isAlwaysExpanded() const;
    void setAlwaysExpanded(bool expanded);

    void onThemeUpdated() override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /**
     * @brief Selects widget-anchor placement without opening.
     * zh_CN: 选择控件锚点定位，但不立即打开。
     */
    void setAnchor(QWidget* anchor);
    /**
     * @brief Opens at a widget anchor using the current show mode.
     * zh_CN: 使用当前显示模式在控件锚点处打开。
     */
    void showAt(QWidget* anchor);
    /**
     * @brief Opens at a widget anchor using an explicit show mode.
     * zh_CN: 使用指定显示模式在控件锚点处打开。
     */
    void showAt(QWidget* anchor, ShowMode mode);
    /**
     * @brief Opens near a local point using the current show mode.
     * zh_CN: 使用当前显示模式在局部坐标点附近打开。
     */
    void showAtPoint(QWidget* relativeTo,
                     const QPoint& localPosition);
    /**
     * @brief Opens near a local point using an explicit show mode.
     * zh_CN: 使用指定显示模式在局部坐标点附近打开。
     */
    void showAtPoint(QWidget* relativeTo,
                     const QPoint& localPosition,
                     ShowMode mode);

public slots:
    /**
     * @brief Changes effective expansion while the flyout is open.
     * zh_CN: 在浮层打开时修改有效展开状态。
     */
    void setExpanded(bool expanded);

signals:
    void showModeChanged(ShowMode mode);
    void expandedChanged(bool expanded);
    void alwaysExpandedChanged(bool expanded);

protected:
    void actionEvent(QActionEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    QPoint computePosition() const override;
    QWidget* automaticPositionAnchor() const override;

private:
    Q_DISABLE_COPY(CommandBarFlyout)
    using dialogs_flyouts::Popup::setPosition;
    friend class CommandBarFlyoutPrivate;

    CommandBarFlyoutPrivate* d = nullptr;
};

} // namespace fluent::menus_toolbars

Q_DECLARE_METATYPE(fluent::menus_toolbars::CommandBarFlyout::ShowMode)

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDBARFLYOUT_H
