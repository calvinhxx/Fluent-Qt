#include "CommandMenuController_p.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QSet>
#include <QTimer>
#include <QWindow>

#include <utility>

#include "compatibility/InputEventCompat.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/menus_toolbars/private/CommandPresenter_p.h"

namespace fluent::menus_toolbars::detail {

CommandMenuController::CommandMenuController(QWidget* owner) : m_owner(owner)
{
    if (owner)
        owner->installEventFilter(this);
}

CommandMenuController::~CommandMenuController()
{
    close();
}

bool CommandMenuController::open(CommandPresenter* presenter, std::function<void()> leafActivated)
{
    const QPointer<CommandPresenter> anchor = presenter;
    const QPointer<QAction> action = anchor ? anchor->action() : nullptr;
    const QPointer<QMenu> menu = action ? action->menu() : nullptr;
    if (!anchor || !anchor->isVisible() || !anchor->isEnabled() || !menu)
        return false;
    if (m_presenter == anchor && m_menu.data() == menu.data() && menu->isVisible()) {
        close(true);
        return true;
    }

    const QPointer<CommandMenuController> lifetime(this);
    close();
    if (!lifetime || !anchor || !action || !menu || action->menu() != menu)
        return true;

    m_presenter = anchor;
    m_action = action;
    m_menu = menu.data();
    m_leafActivated = std::move(leafActivated);
    m_triggered = false;
    m_restoreFocus = false;
    anchor->installEventFilter(this);
    menu->installEventFilter(this);
    m_connections.append(connect(menu, &QMenu::aboutToHide, this, [this] { finishLater(); }));
    observeMenus();
    m_connections.append(connect(menu, &QObject::destroyed, this, [this] {
        m_menu.clear();
        finishLater();
    }));
    m_connections.append(connect(action, &QObject::destroyed, this, [this] { close(); }));
    m_connections.append(connect(action, &QAction::changed, this, [this] {
        if (!m_action || m_action->menu() != m_menu.data() || !m_action->isEnabled() || !m_action->isVisible()
            || m_action->isSeparator())
            close();
    }));
    m_connections.append(connect(anchor, &QObject::destroyed, this, [this] {
        m_presenter.clear();
        close();
    }));

    menu->setLayoutDirection(anchor->layoutDirection());
    const bool rtl = anchor->layoutDirection() == Qt::RightToLeft;
    const bool side = anchor->mode() == CommandPresenter::Mode::Overflow;
    const QPoint local(side ? (rtl ? 0 : anchor->width()) : (rtl ? anchor->width() : 0), side ? 0 : anchor->height());
    const QPoint offset(rtl ? -menu->sizeHint().width() : 0, 0);
    overlay::popupMenuAt(menu, anchor, local, offset);
    if (!lifetime || !anchor || !menu || m_menu.data() != menu.data())
        return true;
    if (!menu->isVisible()) {
        finish();
        return true;
    }
    anchor->setMenuOpen(true);
    if (!lifetime || !menu || m_menu.data() != menu.data())
        return true;
    for (QAction* candidate : menu->actions()) {
        if (candidate && candidate->isVisible() && candidate->isEnabled() && !candidate->isSeparator()) {
            menu->setActiveAction(candidate);
            break;
        }
    }
    return true;
}

QList<QPointer<QMenu>> CommandMenuController::cascade() const
{
    QList<QPointer<QMenu>> result;
    auto* root = qobject_cast<QMenu*>(m_menu.data());
    if (!root)
        return result;
    result.append(root);
    QSet<QMenu*> seen{root};
    for (int index = 0; index < result.size(); ++index) {
        if (!result.at(index))
            continue;
        for (QAction* action : result.at(index)->actions()) {
            QMenu* child = action ? action->menu() : nullptr;
            if (child && !seen.contains(child)) {
                seen.insert(child);
                result.append(child);
            }
        }
    }
    return result;
}

void CommandMenuController::observeMenus()
{
    for (const auto& menu : cascade()) {
        if (!menu || m_observedMenus.contains(menu))
            continue;
        m_observedMenus.append(menu);
        // The leaf menu emits before application QAction handlers; propagation
        // to the root can happen after a handler detaches the originating menu.
        m_connections.append(connect(menu, &QMenu::triggered, this, [this](QAction* leaf) {
            if (!leaf || leaf->isSeparator() || leaf->menu())
                return;
            m_triggered = true;
            finishLater();
        }));
        // Include lazy and dynamically inserted submenus without cloning their
        // actions or taking over Qt's cascade navigation.
        m_connections.append(connect(menu, &QMenu::aboutToShow, this, [this] { observeMenus(); }));
        m_connections.append(connect(menu, &QMenu::hovered, this, [this] { observeMenus(); }));
    }
}

bool CommandMenuController::ownsEvent(QObject* watched, QEvent* event) const
{
    if (!event || !m_menu)
        return false;
    const auto menus = cascade();
    auto* widget = qobject_cast<QWidget*>(watched);
    for (const auto& menu : menus) {
        if (!menu || !menu->isVisible())
            continue;
        // Popup mouse grabs can deliver an outside press to the menu itself.
        // Only its actual footprint exempts that press from parent light-dismiss.
        if (event->type() == QEvent::MouseButtonPress) {
            if (menu->frameGeometry().contains(fluentMouseGlobalPos(static_cast<QMouseEvent*>(event))))
                return true;
        } else if ((widget && widget->window() == menu) || watched == menu->windowHandle()) {
            return true;
        }
    }
    return false;
}

bool CommandMenuController::eventFilter(QObject* watched, QEvent* event)
{
    if (!event)
        return false;
    // QWidget still delivers events after its derived destructors have run.
    // Only inspect a typed guard while the watched object retains that type.
    if (event->type() == QEvent::Hide) {
        auto* presenter = dynamic_cast<CommandPresenter*>(watched);
        if (watched == m_owner || (presenter && presenter == m_presenter))
            close();
    } else if (event->type() == QEvent::KeyPress) {
        auto* menu = qobject_cast<QMenu*>(watched);
        if (!menu || menu != m_menu.data())
            return false;
        updateCommandFocusVisual(m_presenter, event);
        auto* key = static_cast<QKeyEvent*>(event);
        const int back = menu->layoutDirection() == Qt::RightToLeft ? Qt::Key_Right : Qt::Key_Left;
        if (key->key() == back) {
            close(true);
            key->accept();
            return true;
        }
        if (key->key() == Qt::Key_Escape)
            m_restoreFocus = true;
    }
    return false;
}

void CommandMenuController::finishLater()
{
    const quint64 revision = m_revision;
    // QMenu may hide before emitting triggered(). Defer until all QAction
    // handlers have run, including handlers that move focus or delete the host.
    QTimer::singleShot(0, this, [this, revision] {
        const auto* menu = qobject_cast<QMenu*>(m_menu.data());
        if (revision == m_revision && (!menu || !menu->isVisible()))
            finish();
    });
}

void CommandMenuController::finish()
{
    const QPointer<CommandMenuController> lifetime(this);
    const auto anchor = m_presenter;
    const bool restoreFocus = m_restoreFocus && !m_triggered;
    const auto handler = m_triggered ? std::move(m_leafActivated) : std::function<void()>();
    ++m_revision;
    for (const auto& connection : std::as_const(m_connections))
        disconnect(connection);
    m_connections.clear();
    m_observedMenus.clear();
    if (m_menu)
        m_menu->removeEventFilter(this);
    if (anchor)
        anchor->removeEventFilter(this);
    m_presenter.clear();
    m_action.clear();
    m_menu.clear();
    m_leafActivated = {};
    m_triggered = false;
    m_restoreFocus = false;
    if (anchor) {
        anchor->setMenuOpen(false);
        if (restoreFocus && anchor && anchor->isVisible() && anchor->isEnabled())
            anchor->setFocus(Qt::PopupFocusReason);
    }
    if (lifetime && handler)
        handler();
}

void CommandMenuController::close(bool restoreFocus)
{
    const QPointer<CommandMenuController> lifetime(this);
    const auto menus = cascade();
    const auto anchor = m_presenter;
    const auto activated = m_triggered ? std::move(m_leafActivated) : std::function<void()>();
    m_triggered = false;
    m_restoreFocus = false;
    finish();
    for (auto it = menus.crbegin(); it != menus.crend(); ++it) {
        if (*it && (*it)->isVisible())
            (*it)->hide();
    }
    if (restoreFocus && anchor && anchor->isVisible() && anchor->isEnabled())
        anchor->setFocus(Qt::PopupFocusReason);
    if (lifetime && activated) {
        // A leaf handler can detach or remove its originating command. Keep
        // its completion, unless the handler opened a new menu in the meantime.
        const quint64 revision = m_revision;
        QTimer::singleShot(0, this, [this, revision, activated] {
            if (revision == m_revision)
                activated();
        });
    }
}

} // namespace fluent::menus_toolbars::detail
