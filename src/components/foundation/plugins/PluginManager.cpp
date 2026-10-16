#include "PluginManager.h"

#include <QApplication>
#include <QHash>
#include <QScopedValueRollback>
#include <QSet>
#include <QThread>
#include <QWidget>
#include <exception>

#include "WidgetPluginInterface.h"

namespace fluent::plugins {

namespace {

bool fail(QString* error, const QString& message)
{
    if (error) {
        *error = message;
    }
    return false;
}

} // namespace

struct PluginManager::Private {
    struct Entry {
        QPointer<Plugin> plugin;
        PluginMetadata metadata;
        QMetaObject::Connection destroyedConnection;
        bool active = false;
    };

    Private(PluginManager* manager, QObject* host) : q(manager), context(manager, host) {}

    bool canMutate(QString* error) const
    {
        if (error) {
            error->clear();
        }
        if (QThread::currentThread() != q->thread()) {
            return fail(error, QStringLiteral("Plugin operations require the manager's thread."));
        }
        if (busy) {
            return fail(error, QStringLiteral("Plugin manager is processing another operation."));
        }
        return true;
    }

    bool dependenciesActive(const QString& id) const
    {
        const auto entry = entries.constFind(id);
        if (entry == entries.cend() || !entry->plugin) {
            return false;
        }
        for (const QString& dependency : entry->metadata.dependencies) {
            if (!q->isActive(dependency)) {
                return false;
            }
        }
        return true;
    }

    bool visit(const QString& id, QHash<QString, int>& marks, QStringList& path, QStringList& plan, QString* error)
        const
    {
        const auto entry = entries.constFind(id);
        if (entry == entries.cend() || !entry->plugin) {
            return fail(error, QStringLiteral("Missing plugin dependency: %1").arg(id));
        }
        if (marks.value(id) == 2) {
            return true;
        }
        if (marks.value(id) == 1) {
            QStringList cycle = path.mid(path.indexOf(id));
            cycle.append(id);
            return fail(error, QStringLiteral("Plugin dependency cycle: %1").arg(cycle.join(QStringLiteral(" -> "))));
        }
        marks.insert(id, 1);
        path.append(id);
        for (const QString& dependency : entry->metadata.dependencies) {
            if (!visit(dependency, marks, path, plan, error)) {
                return false;
            }
        }
        path.removeLast();
        marks.insert(id, 2);
        plan.append(id);
        return true;
    }

    void stop(const QString& id)
    {
        auto entry = entries.find(id);
        if (entry == entries.end() || !entry->active) {
            return;
        }
        QPointer<Plugin> candidate = entry->plugin;
        // Clear state before foreign code: a hook may destroy its own plugin.
        entry->active = false;
        activationOrder.removeAll(id);
        if (candidate) {
            candidate->onDeactivate();
        }
        if (!destroying) {
            emit q->pluginDeactivated(id);
        }
    }

    void stopAffected(const QString& id)
    {
        QSet<QString> affected{id};
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto entry = entries.cbegin(); entry != entries.cend(); ++entry) {
                if (!entry->active || affected.contains(entry.key())) {
                    continue;
                }
                for (const QString& dependency : entry->metadata.dependencies) {
                    if (affected.contains(dependency)) {
                        affected.insert(entry.key());
                        changed = true;
                        break;
                    }
                }
            }
        }
        const QStringList order = activationOrder;
        for (auto entry = order.crbegin(); entry != order.crend(); ++entry) {
            if (affected.contains(*entry)) {
                stop(*entry);
            }
        }
    }

    void remove(const QString& id)
    {
        const auto entry = entries.constFind(id);
        if (entry == entries.cend()) {
            return;
        }
        if (entry->plugin) {
            entry->plugin->m_manager = nullptr;
        }
        QObject::disconnect(entry->destroyedConnection);
        entries.remove(id);
        registrationOrder.removeAll(id);
        if (!destroying) {
            emit q->pluginUnregistered(id);
        }
    }

    void destroyed(const QString& id)
    {
        QScopedValueRollback<bool> guard(busy, true);
        stopAffected(id);
        remove(id);
    }

    void rollback(QStringList started)
    {
        for (auto entry = started.crbegin(); entry != started.crend(); ++entry) {
            stop(*entry);
        }
    }

    PluginManager* q;
    PluginContext context;
    QHash<QString, Entry> entries;
    QStringList registrationOrder;
    QStringList activationOrder;
    bool busy = false;
    bool destroying = false;
};

PluginManager::PluginManager(QObject* host, QObject* parent) : QObject(parent), d(std::make_unique<Private>(this, host))
{}

PluginManager::~PluginManager()
{
    d->destroying = true;
    d->busy = true;
    d->rollback(d->activationOrder);
    const QStringList ids = d->registrationOrder;
    for (const QString& id : ids) {
        d->remove(id);
    }
}

bool PluginManager::registerPlugin(Plugin* candidate, QString* error)
{
    if (!d->canMutate(error)) {
        return false;
    }
    QScopedValueRollback<bool> guard(d->busy, true);
    if (!candidate) {
        return fail(error, QStringLiteral("Cannot register a null plugin."));
    }
    if (candidate->thread() != thread() || (d->context.host() && d->context.host()->thread() != thread())) {
        return fail(error, QStringLiteral("Plugins and host must share the manager's thread."));
    }
    if (candidate->m_manager) {
        return fail(error, QStringLiteral("Plugin is already registered with a manager."));
    }
    QPointer<Plugin> lifetime(candidate);
    PluginMetadata descriptor;
    try {
        descriptor = candidate->metadata();
    } catch (const std::exception& exception) {
        return fail(error, QStringLiteral("Plugin metadata failed: %1").arg(QString::fromUtf8(exception.what())));
    } catch (...) {
        return fail(error, QStringLiteral("Plugin metadata failed with an unknown exception."));
    }
    if (!lifetime) {
        return fail(error, QStringLiteral("Plugin was destroyed while reading its metadata."));
    }
    if (candidate->m_manager) {
        return fail(error, QStringLiteral("Plugin was registered with another manager while reading metadata."));
    }
    if (QThread::currentThread() != thread() || candidate->thread() != thread()
        || (d->context.host() && d->context.host()->thread() != thread())) {
        return fail(error, QStringLiteral("Plugin, host or manager changed thread while reading metadata."));
    }
    if (descriptor.id.isEmpty() || descriptor.id != descriptor.id.trimmed()) {
        return fail(error, QStringLiteral("Plugin ID must be nonempty and trimmed."));
    }
    if (d->entries.contains(descriptor.id)) {
        return fail(error, QStringLiteral("Duplicate plugin ID: %1").arg(descriptor.id));
    }
    QSet<QString> dependencies;
    for (const QString& dependency : descriptor.dependencies) {
        if (dependency.isEmpty() || dependency != dependency.trimmed() || dependencies.contains(dependency)) {
            return fail(error, QStringLiteral("Plugin dependencies must be nonempty, trimmed and unique."));
        }
        dependencies.insert(dependency);
    }
    Private::Entry entry;
    entry.plugin = candidate;
    entry.metadata = descriptor;
    entry.destroyedConnection = connect(candidate, &QObject::destroyed, this, [this, id = descriptor.id] {
        d->destroyed(id);
    });
    candidate->m_manager = this;
    d->entries.insert(descriptor.id, entry);
    d->registrationOrder.append(descriptor.id);
    emit pluginRegistered(descriptor.id);
    return lifetime ? true : fail(error, QStringLiteral("Plugin was destroyed during registration."));
}

bool PluginManager::unregisterPlugin(const QString& id, QString* error)
{
    if (!d->canMutate(error)) {
        return false;
    }
    QScopedValueRollback<bool> guard(d->busy, true);
    const QString requestedId = id;
    if (!d->entries.contains(requestedId)) {
        return fail(error, QStringLiteral("Unknown plugin: %1").arg(requestedId));
    }
    d->stopAffected(requestedId);
    d->remove(requestedId);
    return true;
}

bool PluginManager::activate(const QString& id, QString* error)
{
    if (!d->canMutate(error)) {
        return false;
    }
    QScopedValueRollback<bool> guard(d->busy, true);
    QStringList plan;
    QStringList path;
    QHash<QString, int> marks;
    if (!d->visit(id, marks, path, plan, error)) {
        return false;
    }
    QStringList started;
    for (const QString& next : plan) {
        if (isActive(next)) {
            continue;
        }
        QPointer<Plugin> candidate = plugin(next);
        QString detail;
        bool activated = false;
        if (candidate) {
            try {
                activated = candidate->onActivate(d->context, &detail);
            } catch (const std::exception& exception) {
                detail = QString::fromUtf8(exception.what());
            } catch (...) {
                detail = QStringLiteral("Unknown exception.");
            }
        }
        if (!activated || !candidate || !d->dependenciesActive(next)) {
            if (candidate) {
                candidate->onDeactivate();
            }
            d->rollback(started);
            if (detail.isEmpty()) {
                detail = QStringLiteral("Activation failed or a required plugin was destroyed.");
            }
            return fail(error, QStringLiteral("Cannot activate %1: %2").arg(next, detail));
        }
        d->entries[next].active = true;
        d->activationOrder.append(next);
        started.append(next);
        emit pluginActivated(next);
        if (!isActive(next) || !d->dependenciesActive(next)) {
            d->rollback(started);
            return fail(error, QStringLiteral("Plugin or dependency was destroyed during activation: %1").arg(next));
        }
    }
    return true;
}

bool PluginManager::deactivate(const QString& id, QString* error)
{
    if (!d->canMutate(error)) {
        return false;
    }
    QScopedValueRollback<bool> guard(d->busy, true);
    if (!d->entries.contains(id)) {
        return fail(error, QStringLiteral("Unknown plugin: %1").arg(id));
    }
    d->stopAffected(id);
    return true;
}

bool PluginManager::deactivateAll(QString* error)
{
    if (!d->canMutate(error)) {
        return false;
    }
    QScopedValueRollback<bool> guard(d->busy, true);
    d->rollback(d->activationOrder);
    return true;
}

QStringList PluginManager::pluginIds() const
{
    return d->registrationOrder;
}

QStringList PluginManager::activePluginIds() const
{
    return d->activationOrder;
}

PluginMetadata PluginManager::metadata(const QString& id) const
{
    return d->entries.value(id).metadata;
}

Plugin* PluginManager::plugin(const QString& id) const
{
    return d->entries.value(id).plugin.data();
}

Plugin* PluginManager::activePlugin(const QString& id) const
{
    return isActive(id) ? plugin(id) : nullptr;
}

bool PluginManager::isActive(const QString& id) const
{
    const auto entry = d->entries.constFind(id);
    return entry != d->entries.cend() && entry->active && entry->plugin;
}

const PluginContext& PluginManager::context() const
{
    return d->context;
}

QWidget* PluginManager::createWidget(const QString& pluginId, const QString& key, QWidget* parent, QString* error)
{
    if (!d->canMutate(error)) {
        return nullptr;
    }
    QScopedValueRollback<bool> guard(d->busy, true);
    const QString requestedId = pluginId;
    const QString requestedKey = key;
    QPointer<QWidget> requestedParent(parent);
    const bool requiresParent = parent != nullptr;
    auto* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app || QThread::currentThread() != app->thread()) {
        fail(error, QStringLiteral("Widget plugins require QApplication's GUI thread."));
        return nullptr;
    }
    QPointer<Plugin> candidate = activePlugin(requestedId);
    auto* factory = qobject_cast<WidgetPluginInterface*>(candidate.data());
    if (!factory) {
        fail(error, QStringLiteral("Plugin is inactive or has no widget interface: %1").arg(requestedId));
        return nullptr;
    }
    QString detail;
    QPointer<QWidget> widget;
    try {
        const QStringList keys = factory->widgetKeys();
        if (!candidate || !isActive(requestedId) || !keys.contains(requestedKey)
            || (requiresParent && !requestedParent)) {
            fail(error, QStringLiteral("Widget key or parent is unavailable: %1").arg(requestedKey));
            return nullptr;
        }
        widget = factory->createWidget(requestedKey, requestedParent.data(), &detail);
    } catch (const std::exception& exception) {
        detail = QString::fromUtf8(exception.what());
    } catch (...) {
        detail = QStringLiteral("Unknown exception.");
    }
    if (!widget || !candidate || !isActive(requestedId) || !d->dependenciesActive(requestedId)
        || (requiresParent && !requestedParent) || widget->parentWidget() != requestedParent.data()) {
        delete widget.data();
        if (detail.isEmpty()) {
            detail = QStringLiteral("Factory failed, returned an incorrect parent, or lost an active dependency.");
        }
        fail(error, QStringLiteral("Cannot create %1/%2: %3").arg(requestedId, requestedKey, detail));
        return nullptr;
    }
    return widget.data();
}

} // namespace fluent::plugins
