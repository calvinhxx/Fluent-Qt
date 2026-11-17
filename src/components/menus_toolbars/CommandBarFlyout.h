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
 * @brief Contextual floating command bar with collapsed and expanded modes.
 * Supports repeated opening anchored to a widget or local point inside its parent window without taking ownership of QAction or FluentMenu objects.
 * Opening a submenu via QAction::setMenu() keeps the flyout open; triggering a leaf command closes both the submenu cascade and the flyout.
 * zh_CN: 带折叠与展开状态的浮动上下文命令栏。
 * 支持在同一顶层窗口内按控件锚点或局部坐标重复弹出，不接管 QAction 和 FluentMenu 的所有权；通过 QAction::setMenu() 展开子菜单时保持浮层打开，触发叶动作后一并关闭。
 */
class CommandBarFlyout final : public dialogs_flyouts::Flyout {
    Q_OBJECT

    Q_PROPERTY(ShowMode showMode READ showMode WRITE setShowMode NOTIFY showModeChanged)
    Q_PROPERTY(bool expanded READ isExpanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(bool alwaysExpanded READ isAlwaysExpanded WRITE setAlwaysExpanded NOTIFY alwaysExpandedChanged)

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
     * @brief Removes an action from the flyout without deleting it.
     * zh_CN: 从浮动命令栏及所属命令分组中移除动作，但不删除该动作。
     */
    void removeAction(QAction* action);

    /** @brief Appends an action to the primary section without taking ownership.
     *  zh_CN: 将动作追加到主命令区（不接管所有权）。
     */
    bool addPrimaryAction(QAction* action);
    /** @brief Inserts a primary action before `before` without taking ownership.
     *  zh_CN: 在指定主命令之前插入动作（不接管所有权）。
     */
    bool insertPrimaryAction(QAction* before, QAction* action);
    /** @brief Appends an action to the secondary section without taking ownership.
     *  zh_CN: 将动作追加到次命令区（不接管所有权）。
     */
    bool addSecondaryAction(QAction* action);
    /** @brief Inserts a secondary action before `before` without taking ownership.
     *  zh_CN: 在指定次命令之前插入动作（不接管所有权）。
     */
    bool insertSecondaryAction(QAction* before, QAction* action);
    /** @brief Removes an action from either section without deleting it.
     *  zh_CN: 从主命令区或次命令区移除指定动作，但不删除该动作。
     */
    bool removeCommandAction(QAction* action);
    /** @brief Clears all primary commands without deleting the actions.
     *  zh_CN: 清空主命令区，不删除其中的动作。
     */
    void clearPrimaryActions();
    /** @brief Clears all secondary commands without deleting the actions.
     *  zh_CN: 清空次命令区，不删除其中的动作。
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

    ShowMode showMode() const;
    void setShowMode(ShowMode mode);

    bool isExpanded() const;
    bool isAlwaysExpanded() const;
    void setAlwaysExpanded(bool expanded);

    void onThemeUpdated() override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /**
     * @brief Sets the anchor widget for placement without opening the flyout.
     * zh_CN: 设置锚点控件但不立即打开浮层。
     */
    void setAnchor(QWidget* anchor);
    /**
     * @brief Opens the flyout anchored to `anchor` using the current show mode.
     * zh_CN: 按当前显示模式在锚点控件附近打开浮层。
     */
    void showAt(QWidget* anchor);
    /**
     * @brief Opens the flyout anchored to `anchor` using `mode`.
     * zh_CN: 按指定显示模式在锚点控件附近打开浮层。
     */
    void showAt(QWidget* anchor, ShowMode mode);
    /**
     * @brief Opens the flyout near `localPosition` relative to `relativeTo` using the current show mode.
     * zh_CN: 按当前显示模式在相对控件的局部坐标附近打开浮层。
     */
    void showAtPoint(QWidget* relativeTo, const QPoint& localPosition);
    /**
     * @brief Opens the flyout near `localPosition` relative to `relativeTo` using `mode`.
     * zh_CN: 按指定显示模式在相对控件的局部坐标附近打开浮层。
     */
    void showAtPoint(QWidget* relativeTo, const QPoint& localPosition, ShowMode mode);

public slots:
    /**
     * @brief Expands or collapses the secondary command menu while the flyout is open.
     * zh_CN: 在浮层打开时展开或收起次命令菜单（受 alwaysExpanded 与是否存在主命令约束）。
     */
    void setExpanded(bool expanded);

signals:
    void showModeChanged(ShowMode mode);
    void expandedChanged(bool expanded);
    void alwaysExpandedChanged(bool expanded);

protected:
    void paintEvent(QPaintEvent* event) override;
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
