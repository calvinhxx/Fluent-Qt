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
 * @brief Searchable same-window presentation of borrowed application actions.
 * zh_CN: 在所属窗口内搜索并执行借用的应用命令。
 *
 * Register the same leaf QAction used by menus and toolbars. Names, shortcuts,
 * enabled and checked states remain QAction-owned. Separators, menu branches
 * and QWidgetAction are not searchable commands. Register their leaf actions
 * explicitly. Search matches all whitespace-separated words against the label,
 * group and keywords, ignoring case. Query text is retained across openings.
 * zh_CN: 注册菜单和工具栏使用的同一叶子 QAction；名称、快捷键、启用与勾选状态
 * 仍由 QAction 管理。分隔符、菜单分支及 QWidgetAction 不是可搜索命令，应显式
 * 注册叶子动作。搜索词按空白分隔，全部匹配名称、分组及关键词，不区分大小写；
 * 再次打开时保留查询文字。
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
     * @brief Registers a borrowed leaf action, or updates its search metadata.
     * zh_CN: 注册借用的叶子动作，已注册时更新其搜索信息。
     */
    bool addCommand(QAction* action, const QString& group = QString(), const QStringList& keywords = QStringList());
    /** @brief Removes a command without deleting or reparenting its action.
     *  zh_CN: 移除命令，不删除动作或改变其父对象。
     */
    bool removeCommand(QAction* action);
    /** @brief Removes all registrations without deleting their actions.
     *  zh_CN: 清空注册，不删除动作。
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
     * @brief Closes and restores focus before executing the current enabled leaf.
     * zh_CN: 关闭面板并恢复焦点后，执行当前可用的叶子命令。
     *
     * Returns false while closed or when no enabled result is selected. A close
     * handler may remove, disable or destroy the action; it is revalidated before
     * dispatch. QAction::triggered remains the execution notification.
     * zh_CN: 面板关闭或没有选中可用结果时返回 false；关闭处理器可移除、禁用或
     * 删除动作，因此执行前会再次检查。执行通知沿用 QAction::triggered。
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

private:
    Q_DISABLE_COPY(CommandPalette)
    CommandPalettePrivate* d = nullptr;
};

} // namespace fluent::menus_toolbars

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_COMMANDPALETTE_H
