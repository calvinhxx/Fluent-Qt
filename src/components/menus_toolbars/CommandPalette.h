#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDPALETTE_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDPALETTE_H

#include <QList>
#include <QStringList>

#include "components/dialogs_flyouts/Popup.h"

class QAction;
class QActionEvent;
class QResizeEvent;

namespace fluent::menus_toolbars {

class CommandPalettePrivate;

/**
 * @brief Searchable same-window command palette for invoking application actions.
 * Register the same leaf QAction instances used by menus and toolbars without taking ownership; text, shortcuts, enabled, and checked states stay synchronized with QAction.
 * Separators, submenu branches, and QWidgetAction are not searchable commands unless their leaf actions are registered explicitly. Search matches all whitespace-separated terms (case-insensitive) against label, group, and keywords, and retains the query across openings.
 * zh_CN: 同一窗口内的可搜索命令面板，用于快速检索并触发应用动作（不接管 QAction 所有权）。
 * 可直接复用菜单或工具栏中的叶动作，文本、快捷键、启用与勾选状态均与 QAction 同步；分隔符、子菜单节点及 QWidgetAction 不参与搜索，需显式注册其叶动作。搜索要求空白分隔的词全部匹配名称、分组或关键词（忽略大小写），重新打开时保留上次输入。
 */
class CommandPalette final : public dialogs_flyouts::Popup {
    Q_OBJECT
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QString placeholderText READ placeholderText WRITE setPlaceholderText NOTIFY placeholderTextChanged)
    Q_PROPERTY(QString emptyText READ emptyText WRITE setEmptyText NOTIFY emptyTextChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)
    Q_PROPERTY(QAction* currentAction READ currentAction WRITE setCurrentAction NOTIFY currentActionChanged)

public:
    explicit CommandPalette(QWidget* parent = nullptr);
    ~CommandPalette() override;

    /**
     * @brief Registers a leaf action or updates its group and keywords without taking ownership.
     * zh_CN: 注册叶动作（不接管所有权）；若已注册则更新其分组与搜索关键词。
     */
    bool addCommand(QAction* action, const QString& group = QString(), const QStringList& keywords = QStringList());
    /** @brief Unregisters a command without deleting or reparenting its QAction.
     *  zh_CN: 移除命令注册，不删除或更改 QAction 的父对象。
     */
    bool removeCommand(QAction* action);
    /** @brief Unregisters all commands without deleting the QAction objects.
     *  zh_CN: 清空所有已注册命令，不删除对应的 QAction。
     */
    void clearCommands();
    QList<QAction*> commands() const;
    QList<QAction*> filteredCommands() const;

    QString query() const;
    void setQuery(const QString& query);
    QString placeholderText() const;
    void setPlaceholderText(const QString& text);
    QString emptyText() const;
    void setEmptyText(const QString& text);
    int resultCount() const;
    QAction* currentAction() const;
    void setCurrentAction(QAction* action);

    QSize sizeHint() const override;
    void onThemeUpdated() override;

public slots:
    /**
     * @brief Closes the palette, restores focus, and triggers the current enabled action.
     * Returns false when closed or no enabled match is selected; revalidates the action after closing in case a close handler disables, removes, or destroys it.
     * zh_CN: 关闭面板并恢复焦点后触发当前选中的可用动作；面板关闭或无可用选中项时返回 false，触发前会再次校验动作，防止关闭回调已将其禁用、移除或销毁。
     */
    bool triggerCurrent();

signals:
    void queryChanged(const QString& query);
    void placeholderTextChanged(const QString& text);
    void emptyTextChanged(const QString& text);
    void resultCountChanged(int count);
    void currentActionChanged(QAction* action);

protected:
    void actionEvent(QActionEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void onHostGeometryChanged() override;

private:
    Q_DISABLE_COPY(CommandPalette)
    CommandPalettePrivate* d = nullptr;
};

} // namespace fluent::menus_toolbars

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDPALETTE_H
