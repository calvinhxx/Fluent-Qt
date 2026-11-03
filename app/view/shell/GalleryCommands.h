#ifndef GALLERYCOMMANDS_H
#define GALLERYCOMMANDS_H

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

#include <memory>

class QAction;
class QKeySequence;
class QMenu;
class QMenuBar;
class QWidget;

namespace fluent::menus_toolbars {
class CommandPalette;
}

namespace fluent::gallery {

/**
 * @brief Gallery's window-level commands and the places that list them.
 * zh_CN: Gallery 的窗口级命令，以及列出这些命令的位置。
 *
 * Each command is one QAction that owns its text, shortcuts, and enabled state. Registering it
 * adds it to the window so its shortcut works, to the searchable command palette, and on macOS to
 * the system menu bar, so every shortcut can be looked up instead of remembered.
 * zh_CN: 每条命令是一个 QAction，自带文字、快捷键与启用状态。注册后命令会加入窗口使快捷键生效、
 * 加入可搜索的命令面板，并在 macOS 上加入系统菜单栏，让所有快捷键都能查到而无需记忆。
 */
class GalleryCommands final : public QObject {
    Q_OBJECT
public:
    enum class Group { View, Go, Help };

    explicit GalleryCommands(QWidget* window);
    ~GalleryCommands() override;

    /// Registers a caller-owned @p action under @p group. zh_CN: 在 @p group 下注册调用方持有的 @p action。
    void addCommand(Group group, QAction* action);
    /// Creates and registers a window-owned command. zh_CN: 创建并注册由窗口持有的命令。
    QAction*
    addCommand(Group group, const QString& objectName, const QString& text, const QList<QKeySequence>& shortcuts = {});

    QList<QAction*> commands(Group group) const;
    fluent::menus_toolbars::CommandPalette* palette() const { return m_palette; }
    /// Opens the searchable list of every command with its shortcut. zh_CN: 打开可搜索的全部命令及其快捷键。
    void showPalette();

private:
    static QString groupTitle(Group group);
    QMenu* menuFor(Group group) const;

    QWidget* m_window = nullptr;
    QList<QPair<Group, QAction*>> m_commands;
    fluent::menus_toolbars::CommandPalette* m_palette = nullptr;
    // A parentless menu bar is the macOS global menu bar; other platforms keep the Fluent title bar.
    // zh_CN: 无父对象的菜单栏即 macOS 全局菜单栏；其他平台保留 Fluent 标题栏。
    std::unique_ptr<QMenuBar> m_menuBar;
    QList<QPair<Group, QMenu*>> m_menus;
};

} // namespace fluent::gallery

#endif // GALLERYCOMMANDS_H
