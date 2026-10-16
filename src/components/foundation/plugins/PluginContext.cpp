#include "PluginContext.h"

#include "PluginManager.h"

namespace fluent::plugins {

PluginContext::PluginContext(PluginManager* manager, QObject* host) : m_manager(manager), m_host(host) {}

QObject* PluginContext::host() const
{
    return m_host.data();
}

Plugin* PluginContext::activePlugin(const QString& id) const
{
    return m_manager ? m_manager->activePlugin(id) : nullptr;
}

} // namespace fluent::plugins
