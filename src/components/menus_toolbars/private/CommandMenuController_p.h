#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDMENUCONTROLLER_P_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDMENUCONTROLLER_P_H

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include <functional>

class QAction;
class QMenu;
class QWidget;

namespace fluent::menus_toolbars::detail {

class CommandPresenter;

// Bridge a borrowed QMenu cascade to a command surface without copying actions
// or replacing QMenu's navigation, trigger, and nested-menu behavior.
class CommandMenuController final : public QObject {
public:
    explicit CommandMenuController(QWidget* owner);
    ~CommandMenuController() override;

    bool open(CommandPresenter* presenter, std::function<void()> leafActivated = {});
    void close(bool restoreFocus = false);
    bool ownsEvent(QObject* watched, QEvent* event) const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QList<QPointer<QMenu>> cascade() const;
    void observeMenus();
    void finishLater();
    void finish();

    QPointer<QObject> m_owner;
    QPointer<CommandPresenter> m_presenter;
    QPointer<QAction> m_action;
    // Application filters can run after QMenu has become its QWidget base.
    QPointer<QObject> m_menu;
    QList<QPointer<QMenu>> m_observedMenus;
    QList<QMetaObject::Connection> m_connections;
    std::function<void()> m_leafActivated;
    quint64 m_revision = 0;
    bool m_triggered = false;
    bool m_restoreFocus = false;
};

} // namespace fluent::menus_toolbars::detail

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDMENUCONTROLLER_P_H
