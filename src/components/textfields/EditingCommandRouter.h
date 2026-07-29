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
 * @brief Provides window-scoped semantic actions for the focused Fluent editor.
 * zh_CN: 为当前窗口中获得焦点的 Fluent 编辑器提供窗口级语义动作。
 *
 * The router owns stable QAction objects and updates their enabled state as
 * focus, selection, history, read-only state, and clipboard content change.
 * It supports LineEdit and its subclasses plus TextEdit without exposing
 * TextEdit's private QTextEdit.
 * zh_CN: Router 持有地址稳定的 QAction，并随焦点、选择区、撤销历史、只读状态
 * 和剪贴板内容更新启用状态；它支持 LineEdit 及派生类和 TextEdit，但不会暴露
 * TextEdit 的私有 QTextEdit。
 */
class EditingCommandRouter final : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Editing commands exposed through stable QAction instances.
     * zh_CN: 通过稳定 QAction 实例暴露的编辑命令。
     */
    enum class Command {
        Undo,
        Redo,
        Cut,
        Copy,
        Paste,
        Delete,
        SelectAll
    };
    Q_ENUM(Command)

    /**
     * @brief Creates a router scoped to the top-level window containing scopeWindow.
     * zh_CN: 创建作用于 scopeWindow 所属顶层窗口的编辑命令路由器。
     *
     * When parent is null, the resolved top-level window becomes the QObject
     * parent. One router should be created for each top-level window.
     * zh_CN: parent 为空时，解析出的顶层窗口将成为 QObject 父对象；每个顶层
     * 窗口应只创建一个 Router。
     */
    explicit EditingCommandRouter(
        QWidget* scopeWindow,
        QObject* parent = nullptr);
    ~EditingCommandRouter() override;

    /**
     * @brief Returns the top-level window that owns this command scope.
     * zh_CN: 返回拥有当前命令作用域的顶层窗口。
     */
    QWidget* scopeWindow() const;

    /**
     * @brief Returns whether a supported Fluent editor is the active target.
     * zh_CN: 返回当前是否存在获得焦点的受支持 Fluent 编辑器。
     */
    bool hasActiveTarget() const;

    /**
     * @brief Returns the router-owned action for command.
     * zh_CN: 返回由 Router 持有的指定命令 QAction。
     *
     * The returned pointer remains stable for the router lifetime. Callers may
     * customize text, icon, and shortcuts; the router owns enabled state and
     * trigger dispatch.
     * zh_CN: 返回指针在 Router 生命周期内保持稳定。调用方可以自定义文字、
     * 图标和快捷键；启用状态与触发分发由 Router 管理。
     */
    QAction* action(Command command) const;

    /**
     * @brief Returns all actions in semantic command order.
     * zh_CN: 按语义命令顺序返回全部动作。
     */
    QList<QAction*> actions() const;

    /**
     * @brief Returns whether command can execute against the current target.
     * zh_CN: 返回指定命令当前是否可以作用于活动目标。
     */
    bool canExecute(Command command) const;

    /**
     * @brief Revalidates and executes command against the active target.
     * zh_CN: 重新校验并对活动目标执行指定命令。
     *
     * @return true when the command was dispatched; false when no valid target
     *         or capability remains.
     */
    bool execute(Command command);

    /**
     * @brief Recomputes active command capabilities without changing focus.
     * zh_CN: 在不改变焦点的情况下重新计算活动命令能力。
     */
    void refresh();

signals:
    /**
     * @brief Emitted when the active supported target changes.
     * zh_CN: 当前受支持编辑目标变化时发出。
     */
    void activeTargetChanged(bool hasActiveTarget);

    /**
     * @brief Emitted when one command's executable capability changes.
     * zh_CN: 某项命令的可执行能力变化时发出。
     */
    void commandCapabilityChanged(
        fluent::textfields::EditingCommandRouter::Command command,
        bool enabled);

private:
    Q_DISABLE_COPY(EditingCommandRouter)

    EditingCommandRouterPrivate* d = nullptr;
};

} // namespace fluent::textfields

Q_DECLARE_METATYPE(fluent::textfields::EditingCommandRouter::Command)

#endif // FLUENTQT_COMPONENTS_TEXTFIELDS_EDITINGCOMMANDROUTER_H
