#ifndef FLUENTQT_COMPONENTS_TEXTFIELDS_EDITINGCOMMANDROUTER_H
#define FLUENTQT_COMPONENTS_TEXTFIELDS_EDITINGCOMMANDROUTER_H

#include <QList>
#include <QMetaType>
#include <QObject>

class QAction;
class QWidget;

namespace fluent::textfields {

class EditingCommandRouterPrivate;

/**
 * @brief Routes window-scoped editing actions to the focused Fluent editor.
 * zh_CN: 将窗口级编辑动作路由到当前聚焦的 Fluent 编辑器。
 *
 * Owns persistent QAction instances and updates their enabled state as focus,
 * selection, undo history, read-only state, and clipboard content change across
 * LineEdit (including subclasses) and TextEdit.
 * zh_CN: 内部管理地址稳定的 QAction，随当前顶层窗口内聚焦的 LineEdit 或 TextEdit 的
 * 选区、撤销历史、只读状态和剪贴板变化自动更新可用性（不暴露 TextEdit 内部编辑器）。
 */
class EditingCommandRouter final : public QObject {
    Q_OBJECT

public:
    enum class Command { Undo, Redo, Cut, Copy, Paste, Delete, SelectAll };
    Q_ENUM(Command)

    /**
     * @brief Creates a router scoped to scopeWindow's top-level window (used as parent when parent is null).
     * zh_CN: 创建作用于 scopeWindow 所属顶层窗口的路由器（parent 为空时以该顶层窗口为父对象；每窗口建一个即可）。
     */
    explicit EditingCommandRouter(QWidget* scopeWindow, QObject* parent = nullptr);
    ~EditingCommandRouter() override;

    QWidget* scopeWindow() const;
    bool hasActiveTarget() const;

    /**
     * @brief Returns the QAction for command (owned by the router; text, icon, and shortcuts may be customized).
     * zh_CN: 返回 command 对应的 QAction（由路由器管理生命周期与启用状态，可自由修改文本、图标和快捷键）。
     */
    QAction* action(Command command) const;

    /**
     * @brief Returns all actions in Command enum order.
     * zh_CN: 按 Command 枚举顺序返回全部动作。
     */
    QList<QAction*> actions() const;

    bool canExecute(Command command) const;

    /**
     * @brief Revalidates and executes command on the active editor; returns false if unavailable.
     * zh_CN: 校验并对当前活动编辑器执行指定命令；不可用时返回 false。
     */
    bool execute(Command command);

    /**
     * @brief Recomputes action enabled states for the current focus target.
     * zh_CN: 重新计算当前活动目标各命令的可用状态。
     */
    void refresh();

signals:
    void activeTargetChanged(bool hasActiveTarget);
    void commandCapabilityChanged(fluent::textfields::EditingCommandRouter::Command command, bool enabled);

private:
    Q_DISABLE_COPY(EditingCommandRouter)

    EditingCommandRouterPrivate* d = nullptr;
};

} // namespace fluent::textfields

Q_DECLARE_METATYPE(fluent::textfields::EditingCommandRouter::Command)

#endif // FLUENTQT_COMPONENTS_TEXTFIELDS_EDITINGCOMMANDROUTER_H
