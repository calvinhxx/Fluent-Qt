#include "PluginContractProbe.h"

#include <FluentQt/FluentQt.h>
#include <stdexcept>
#include <utility>

namespace fluentqt::plugin_extension {
namespace {

class ContractPlugin final : public fluent::plugins::Plugin {
public:
    ContractPlugin(QString id, QStringList dependencies = {}, QStringList* events = nullptr)
        : m_id(std::move(id)), m_dependencies(std::move(dependencies)), m_events(events)
    {}

    fluent::plugins::PluginMetadata metadata() const override
    {
        return {m_id, m_id, QVersionNumber(1, 0), m_dependencies};
    }

    bool succeeds = true;
    bool throws = false;
    int activations = 0;
    int deactivations = 0;

protected:
    bool onActivate(const fluent::plugins::PluginContext&, QString* error) override
    {
        ++activations;
        if (m_events)
            m_events->append(QStringLiteral("+") + m_id);
        if (throws)
            throw std::runtime_error("plugin activation exception");
        if (!succeeds && error)
            *error = QStringLiteral("contract activation failure");
        return succeeds;
    }

    void onDeactivate() noexcept override
    {
        ++deactivations;
        if (m_events)
            m_events->append(QStringLiteral("-") + m_id);
    }

private:
    QString m_id;
    QStringList m_dependencies;
    QStringList* m_events;
};

class ThrowingWidgetPlugin final : public fluent::plugins::WidgetPlugin {
public:
    fluent::plugins::PluginMetadata metadata() const override
    {
        return {QStringLiteral("throwing-widget"), QStringLiteral("Throwing widget"), {}, {}};
    }
    QStringList widgetKeys() const override { return {QStringLiteral("throw")}; }
    QWidget* createWidget(const QString&, QWidget*, QString*) override
    {
        throw std::runtime_error("plugin factory exception");
    }

protected:
    bool onActivate(const fluent::plugins::PluginContext&, QString*) override { return true; }
    void onDeactivate() noexcept override {}
};

} // namespace

QVariantMap runPluginContractChecks()
{
    QVariantMap result;
    {
        ContractPlugin base(QStringLiteral("base"));
        ContractPlugin missing(QStringLiteral("missing-user"),
                               {QStringLiteral("base"), QStringLiteral("absent")});
        fluent::plugins::PluginManager manager;
        QString error;
        const bool registered = manager.registerPlugin(&base) && manager.registerPlugin(&missing);
        const bool activated = manager.activate(QStringLiteral("missing-user"), &error);
        result[QStringLiteral("missingBeforeHooks")] = registered && !activated &&
                                                       !error.isEmpty() && base.activations == 0 &&
                                                       missing.activations == 0;
    }
    {
        ContractPlugin first(QStringLiteral("first"), {QStringLiteral("second")});
        ContractPlugin second(QStringLiteral("second"), {QStringLiteral("first")});
        fluent::plugins::PluginManager manager;
        QString error;
        const bool registered = manager.registerPlugin(&first) && manager.registerPlugin(&second);
        const bool activated = manager.activate(QStringLiteral("first"), &error);
        result[QStringLiteral("cycleBeforeHooks")] = registered && !activated && !error.isEmpty() &&
                                                     first.activations == 0 &&
                                                     second.activations == 0;
    }
    {
        QStringList events;
        ContractPlugin base(QStringLiteral("base"), {}, &events);
        ContractPlugin left(QStringLiteral("left"), {QStringLiteral("base")}, &events);
        ContractPlugin right(QStringLiteral("right"), {QStringLiteral("base")}, &events);
        ContractPlugin top(QStringLiteral("top"), {QStringLiteral("left"), QStringLiteral("right")},
                           &events);
        fluent::plugins::PluginManager manager;
        const bool registered = manager.registerPlugin(&top) && manager.registerPlugin(&right) &&
                                manager.registerPlugin(&base) && manager.registerPlugin(&left);
        const bool activated = manager.activate(QStringLiteral("top"));
        const QStringList expected{QStringLiteral("+base"), QStringLiteral("+left"),
                                   QStringLiteral("+right"), QStringLiteral("+top")};
        result[QStringLiteral("dependencyOrder")] =
            registered && activated && events == expected && base.activations == 1;
        result[QStringLiteral("activationOrder")] = events;
        events.clear();
        const bool stopped = manager.deactivateAll();
        const QStringList reverse{QStringLiteral("-top"), QStringLiteral("-right"),
                                  QStringLiteral("-left"), QStringLiteral("-base")};
        result[QStringLiteral("reverseCleanup")] = stopped && events == reverse;
        result[QStringLiteral("cleanupOrder")] = events;
    }
    for (const bool throwing : {false, true}) {
        QStringList events;
        ContractPlugin existing(QStringLiteral("existing"), {}, &events);
        ContractPlugin fresh(QStringLiteral("fresh"), {QStringLiteral("existing")}, &events);
        ContractPlugin failing(QStringLiteral("failing"), {QStringLiteral("fresh")}, &events);
        failing.succeeds = false;
        failing.throws = throwing;
        fluent::plugins::PluginManager manager;
        const bool registered = manager.registerPlugin(&existing) &&
                                manager.registerPlugin(&fresh) && manager.registerPlugin(&failing);
        const bool active = manager.activate(QStringLiteral("existing"));
        events.clear();
        QString error;
        const bool activated = manager.activate(QStringLiteral("failing"), &error);
        const QStringList expected{QStringLiteral("+fresh"), QStringLiteral("+failing"),
                                   QStringLiteral("-failing"), QStringLiteral("-fresh")};
        const bool passed = registered && active && !activated && !error.isEmpty() &&
                            manager.isActive(QStringLiteral("existing")) &&
                            existing.deactivations == 0 &&
                            !manager.isActive(QStringLiteral("fresh")) &&
                            failing.deactivations == 1 && events == expected;
        result[throwing ? QStringLiteral("activationExceptionCaught")
                        : QStringLiteral("rollbackKeepsExisting")] = passed;
    }
    {
        ThrowingWidgetPlugin plugin;
        fluent::plugins::PluginManager manager;
        const bool registered = manager.registerPlugin(&plugin);
        const bool active = manager.activate(QStringLiteral("throwing-widget"));
        QString error;
        QWidget* widget = manager.createWidget(QStringLiteral("throwing-widget"),
                                               QStringLiteral("throw"), nullptr, &error);
        result[QStringLiteral("factoryExceptionCaught")] =
            registered && active && !widget &&
            error.contains(QStringLiteral("plugin factory exception"));
        delete widget;
    }
    bool allPassed = true;
    for (const QString& key :
         {QStringLiteral("missingBeforeHooks"), QStringLiteral("cycleBeforeHooks"),
          QStringLiteral("dependencyOrder"), QStringLiteral("reverseCleanup"),
          QStringLiteral("rollbackKeepsExisting"), QStringLiteral("activationExceptionCaught"),
          QStringLiteral("factoryExceptionCaught")}) {
        allPassed = allPassed && result.value(key).toBool();
    }
    result[QStringLiteral("allPassed")] = allPassed;
    return result;
}

} // namespace fluentqt::plugin_extension
