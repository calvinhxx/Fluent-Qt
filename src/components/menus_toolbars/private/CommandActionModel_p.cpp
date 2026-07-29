#include "CommandActionModel_p.h"

#include <QAction>
#include <QActionEvent>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <QWidgetAction>

#include "utils/private/FluentQtLogging_p.h"

namespace fluent::menus_toolbars::detail {
namespace {

constexpr const char* kEditingCommandScopeProperty =
    "_fluentqt_editingCommandScope";

QString stripAccessMarkers(QString text)
{
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    if (tabIndex >= 0)
        text.truncate(tabIndex);

    QString result;
    result.reserve(text.size());
    for (int index = 0; index < text.size(); ++index) {
        if (text.at(index) != QLatin1Char('&')) {
            result.append(text.at(index));
            continue;
        }
        if (index + 1 < text.size()
            && text.at(index + 1) == QLatin1Char('&')) {
            result.append(QLatin1Char('&'));
            ++index;
        }
    }
    return result.trimmed();
}

bool hasSemanticCaption(const QAction* action)
{
    return action
        && (!stripAccessMarkers(action->text()).isEmpty()
            || !stripAccessMarkers(action->iconText()).isEmpty());
}

} // namespace

CommandActionModel::CommandActionModel(QWidget* owner)
    : QObject(nullptr),
      m_owner(owner)
{
}

CommandActionModel::~CommandActionModel()
{
    for (auto it = m_connections.cbegin(); it != m_connections.cend(); ++it) {
        QObject::disconnect(it.value().changed);
        QObject::disconnect(it.value().destroyed);
    }
}

bool CommandActionModel::add(Section section, QAction* action)
{
    if (action && sectionActions(section).contains(action))
        return true;
    return insert(section, nullptr, action);
}

bool CommandActionModel::insert(Section section,
                                QAction* before,
                                QAction* action)
{
    QString reason;
    if (!validateForRegistration(action, &reason)) {
        warnRejected(action, reason);
        return false;
    }

    QList<QAction*>& destination = sectionActions(section);
    if (before && !destination.contains(before)) {
        qCWarning(logging::commandBarCategory)
            << "Command action insertion rejected: before action is not in"
            << (section == Section::Primary ? "primary" : "secondary")
            << "section"
            << "action=" << action
            << "before=" << before;
        return false;
    }

    const int existingIndex = destination.indexOf(action);
    int insertionIndex = before ? destination.indexOf(before)
                                : destination.size();
    if (existingIndex >= 0) {
        if (before == action
            || (!before && existingIndex == destination.size() - 1)) {
            return true;
        }
        destination.removeAt(existingIndex);
        if (existingIndex < insertionIndex)
            --insertionIndex;
        destination.insert(insertionIndex, action);
        emit structureChanged();
        return true;
    }

    QList<QAction*>& source = otherSectionActions(section);
    const bool alreadyRegistered = source.removeOne(action);
    if (insertionIndex < 0 || insertionIndex > destination.size())
        insertionIndex = destination.size();
    destination.insert(insertionIndex, action);

    if (!alreadyRegistered) {
        connectAction(action);
        m_presentationState.insert(action, true);
        ensureAssociated(action);
    }

    emit structureChanged();
    return true;
}

bool CommandActionModel::remove(QAction* action)
{
    if (!action)
        return false;

    const bool removed =
        m_primaryActions.removeOne(action)
        || m_secondaryActions.removeOne(action);
    if (!removed)
        return false;

    disconnectAction(action);
    removeAssociation(action);
    emit structureChanged();
    return true;
}

void CommandActionModel::clear(Section section)
{
    QList<QAction*>& target = sectionActions(section);
    if (target.isEmpty())
        return;

    const QList<QAction*> removed = target;
    target.clear();
    for (QAction* action : removed) {
        disconnectAction(action);
        removeAssociation(action);
    }
    emit structureChanged();
}

QList<QAction*> CommandActionModel::actions(Section section) const
{
    return sectionActions(section);
}

QList<QAction*> CommandActionModel::presentableActions(Section section) const
{
    QList<QAction*> result;
    const QList<QAction*>& source = sectionActions(section);
    result.reserve(source.size());
    for (QAction* action : source) {
        if (isPresentable(action))
            result.append(action);
    }
    return result;
}

bool CommandActionModel::contains(Section section,
                                  const QAction* action) const
{
    return sectionActions(section).contains(
        const_cast<QAction*>(action));
}

bool CommandActionModel::isPresentable(const QAction* action) const
{
    return action && m_presentationState.value(
        const_cast<QAction*>(action), false);
}

void CommandActionModel::handleActionEvent(QActionEvent* event)
{
    if (!event || isChangingAssociation())
        return;

    QAction* action = event->action();
    if (!action)
        return;

    if (event->type() == QEvent::ActionAdded) {
        if (m_primaryActions.contains(action)
            || m_secondaryActions.contains(action)) {
            return;
        }

        auto pendingIt = m_pendingRemovals.find(action);
        if (pendingIt != m_pendingRemovals.end()
            && pendingIt.value().actionGuard.data() != action) {
            m_pendingRemovals.erase(pendingIt);
            pendingIt = m_pendingRemovals.end();
        }
        if (pendingIt != m_pendingRemovals.end()) {
            const PendingRemoval pending = pendingIt.value();
            m_pendingRemovals.erase(pendingIt);
            QList<QAction*>& destination =
                sectionActions(pending.section);
            destination.insert(
                qBound(0, pending.index, destination.size()),
                action);
            connectAction(action);
            m_presentationState.insert(
                action, pending.presentable);
            if (pending.suppressionWarningIssued)
                m_warnedSuppressedActions.append(action);
            emit structureChanged();
            return;
        }

        QString reason;
        if (!validateForRegistration(action, &reason)) {
            warnRejected(action, reason);
            removeAssociation(action);
            return;
        }

        QAction* before = event->before();
        const int insertionIndex = m_primaryActions.indexOf(before);
        if (insertionIndex >= 0)
            m_primaryActions.insert(insertionIndex, action);
        else
            m_primaryActions.append(action);
        connectAction(action);
        m_presentationState.insert(action, true);
        emit structureChanged();
        return;
    }

    if (event->type() == QEvent::ActionRemoved) {
        const int primaryIndex = m_primaryActions.indexOf(action);
        const int secondaryIndex =
            m_secondaryActions.indexOf(action);
        if (primaryIndex < 0 && secondaryIndex < 0)
            return;

        PendingRemoval pending;
        pending.actionGuard = action;
        pending.section = primaryIndex >= 0
            ? Section::Primary
            : Section::Secondary;
        pending.index = primaryIndex >= 0
            ? primaryIndex
            : secondaryIndex;
        pending.presentable =
            m_presentationState.value(action, false);
        pending.suppressionWarningIssued =
            m_warnedSuppressedActions.contains(action);
        pending.revision = ++m_pendingRemovalRevision;
        m_pendingRemovals.insert(action, pending);

        sectionActions(pending.section).removeAt(pending.index);
        disconnectAction(action);
        QPointer<CommandActionModel> guard(this);
        emit structureChanged();
        if (!guard)
            return;
        QTimer::singleShot(
            0,
            this,
            [this, action, revision = pending.revision]() {
                const auto it = m_pendingRemovals.find(action);
                if (it != m_pendingRemovals.end()
                    && it.value().revision == revision) {
                    m_pendingRemovals.erase(it);
                }
            });
    }
}

QList<QAction*>& CommandActionModel::sectionActions(Section section)
{
    return section == Section::Primary
        ? m_primaryActions
        : m_secondaryActions;
}

const QList<QAction*>& CommandActionModel::sectionActions(
    Section section) const
{
    return section == Section::Primary
        ? m_primaryActions
        : m_secondaryActions;
}

QList<QAction*>& CommandActionModel::otherSectionActions(Section section)
{
    return section == Section::Primary
        ? m_secondaryActions
        : m_primaryActions;
}

bool CommandActionModel::validateForRegistration(
    const QAction* action,
    QString* reason) const
{
    if (!action) {
        if (reason)
            *reason = QStringLiteral("action is null");
        return false;
    }
    if (qobject_cast<const QWidgetAction*>(action)) {
        if (reason)
            *reason = QStringLiteral("QWidgetAction is not supported");
        return false;
    }
    QObject* scopeObject =
        action->property(kEditingCommandScopeProperty)
            .value<QObject*>();
    auto* scopeWindow = qobject_cast<QWidget*>(scopeObject);
    if (scopeWindow && m_owner
        && scopeWindow->window() != m_owner->window()) {
        if (reason) {
            *reason = QStringLiteral(
                "window-scoped action belongs to another top-level window");
        }
        return false;
    }
    return validateForPresentation(action, reason);
}

bool CommandActionModel::validateForPresentation(
    const QAction* action,
    QString* reason) const
{
    if (!action) {
        if (reason)
            *reason = QStringLiteral("action no longer exists");
        return false;
    }
    if (action->menu()) {
        if (reason)
            *reason = QStringLiteral("nested menus are not supported");
        return false;
    }
    if (!action->isSeparator() && !hasSemanticCaption(action)) {
        if (reason)
            *reason = QStringLiteral(
                "non-separator action has no semantic caption");
        return false;
    }
    return true;
}

void CommandActionModel::ensureAssociated(QAction* action)
{
    if (!m_owner || !action || m_owner->actions().contains(action))
        return;
    ++m_associationChangeDepth;
    m_owner->QWidget::addAction(action);
    --m_associationChangeDepth;
}

void CommandActionModel::removeAssociation(QAction* action)
{
    if (!m_owner || !action || !m_owner->actions().contains(action))
        return;
    ++m_associationChangeDepth;
    m_owner->QWidget::removeAction(action);
    --m_associationChangeDepth;
}

void CommandActionModel::connectAction(QAction* action)
{
    if (!action || m_connections.contains(action))
        return;

    Connections connections;
    connections.changed = QObject::connect(
        action,
        &QAction::changed,
        this,
        [this, action]() { handleActionChanged(action); });
    connections.destroyed = QObject::connect(
        action,
        &QObject::destroyed,
        this,
        [this, action]() { handleActionDestroyed(action); });
    m_connections.insert(action, connections);
}

void CommandActionModel::disconnectAction(QAction* action)
{
    const auto it = m_connections.find(action);
    if (it != m_connections.end()) {
        QObject::disconnect(it.value().changed);
        QObject::disconnect(it.value().destroyed);
        m_connections.erase(it);
    }
    m_presentationState.remove(action);
    m_warnedSuppressedActions.removeAll(action);
}

void CommandActionModel::handleActionChanged(QAction* action)
{
    if (!m_connections.contains(action))
        return;

    QString reason;
    const bool presentable = validateForPresentation(action, &reason);
    const bool wasPresentable = m_presentationState.value(action, false);
    if (presentable != wasPresentable) {
        m_presentationState.insert(action, presentable);
        if (!presentable)
            warnSuppressed(action, reason);
        else
            m_warnedSuppressedActions.removeAll(action);
    }
    emit presentationChanged();
}

void CommandActionModel::handleActionDestroyed(QAction* action)
{
    m_pendingRemovals.remove(action);
    const bool removed =
        m_primaryActions.removeOne(action)
        || m_secondaryActions.removeOne(action);
    disconnectAction(action);
    if (removed)
        emit structureChanged();
}

void CommandActionModel::warnRejected(
    const QAction* action,
    const QString& reason) const
{
    qCWarning(logging::commandBarCategory)
        << "Command action registration rejected"
        << "reason=" << reason
        << "action=" << action;
}

void CommandActionModel::warnSuppressed(
    const QAction* action,
    const QString& reason)
{
    QAction* mutableAction = const_cast<QAction*>(action);
    if (m_warnedSuppressedActions.contains(mutableAction))
        return;
    m_warnedSuppressedActions.append(mutableAction);
    qCWarning(logging::commandBarCategory)
        << "Registered command action suppressed from presentation"
        << "reason=" << reason
        << "action=" << action;
}

} // namespace fluent::menus_toolbars::detail
