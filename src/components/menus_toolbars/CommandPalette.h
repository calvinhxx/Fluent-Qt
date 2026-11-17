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
 * @brief In-window command search palette that binds existing menu or toolbar actions by label and keywords.
 * zh_CN: 窗口内快捷命令搜索面板，可直接绑定现有菜单或工具栏动作，按名称或关键词快速搜索执行。
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
     * zh_CN: 注册动作或更新其分组与关键词（不负责销毁 QAction）。
     */
    bool addCommand(QAction* action, const QString& group = QString(), const QStringList& keywords = QStringList());
    /** @brief Unregisters a command without deleting its QAction.
     *  zh_CN: 移除命令注册（不销毁 QAction）。
     */
    bool removeCommand(QAction* action);
    /** @brief Unregisters all commands without deleting the QAction objects.
     *  zh_CN: 清空已注册命令（不销毁 QAction）。
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
     * Returns false when closed or no enabled match is selected.
     * zh_CN: 关闭面板并恢复焦点后触发当前选中的可用动作；未打开或无可用项时返回 false。
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
