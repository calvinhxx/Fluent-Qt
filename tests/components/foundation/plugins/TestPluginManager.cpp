#include <FluentQt/FluentQt.h>

#include <QPointer>
#include <QSignalSpy>
#include <QThread>
#include <QWidget>
#include <gtest/gtest.h>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

using fluent::plugins::Plugin;
using fluent::plugins::PluginContext;
using fluent::plugins::PluginManager;
using fluent::plugins::PluginMetadata;
using fluent::plugins::WidgetPluginInterface;

class ProbePlugin : public Plugin {
public:
    explicit ProbePlugin(const QString& id, const QStringList& dependencies = {},
                         QStringList* journal = nullptr, QObject* parent = nullptr)
        : Plugin(parent), descriptor{id, id, QVersionNumber(1, 0), dependencies}, m_id(id),
          m_journal(journal)
    {}

    PluginMetadata metadata() const override
    {
        ++metadataReads;
        const auto callback = readMetadata;
        return callback ? callback() : descriptor;
    }

    PluginMetadata descriptor;
    mutable int metadataReads = 0;
    int activationCount = 0;
    int deactivationCount = 0;
    bool activationResult = true;
    bool resourceAllocated = false;
    std::optional<PluginContext> savedContext;
    std::function<PluginMetadata()> readMetadata;
    std::function<bool(const PluginContext&, QString*)> activation;
    std::function<void()> deactivation;

protected:
    bool onActivate(const PluginContext& context, QString* error) override
    {
        ++activationCount;
        resourceAllocated = true;
        savedContext = context;
        if (m_journal) {
            m_journal->append(QStringLiteral("+") + m_id);
        }
        const auto callback = activation;
        if (callback) {
            return callback(context, error);
        }
        if (!activationResult && error) {
            *error = QStringLiteral("Partial initialization failed.");
        }
        return activationResult;
    }

    void onDeactivate() noexcept override
    {
        ++deactivationCount;
        resourceAllocated = false;
        if (m_journal) {
            m_journal->append(QStringLiteral("-") + m_id);
        }
        const auto callback = deactivation;
        if (callback) {
            callback();
        }
    }

private:
    QString m_id;
    QStringList* m_journal;
};

class WidgetProbePlugin : public ProbePlugin, public WidgetPluginInterface {
    Q_OBJECT
    Q_INTERFACES(fluent::plugins::WidgetPluginInterface)

public:
    using ProbePlugin::ProbePlugin;

    QStringList widgetKeys() const override
    {
        return keysCallback ? keysCallback() : QStringList{QStringLiteral("button")};
    }

    QWidget* createWidget(const QString& key, QWidget* parent, QString* error) override
    {
        ++factoryCalls;
        QWidget* widget = factoryCallback
                              ? factoryCallback(key, parent, error)
                              : new fluent::basicinput::Button(QStringLiteral("Extension"), parent);
        lastWidget = widget;
        return widget;
    }

    int factoryCalls = 0;
    QPointer<QWidget> lastWidget;
    std::function<QStringList()> keysCallback;
    std::function<QWidget*(const QString&, QWidget*, QString*)> factoryCallback;
};

class SelfDeletingPlugin : public Plugin {
public:
    SelfDeletingPlugin(QString id, int* cleanupCount)
        : m_id(std::move(id)), m_cleanupCount(cleanupCount)
    {}

    PluginMetadata metadata() const override { return {m_id, m_id, QVersionNumber(1, 0), {}}; }
    const QString& id() const { return m_id; }

protected:
    bool onActivate(const PluginContext&, QString*) override { return true; }

    void onDeactivate() noexcept override
    {
        ++*m_cleanupCount;
        delete this;
    }

private:
    QString m_id;
    int* m_cleanupCount;
};

class PortableWidgetPlugin : public fluent::plugins::WidgetPlugin {
public:
    PluginMetadata metadata() const override
    {
        return {QStringLiteral("portable"), QStringLiteral("Portable"), QVersionNumber(1, 0), {}};
    }
    QStringList widgetKeys() const override { return {QStringLiteral("button")}; }
    QWidget* createWidget(const QString&, QWidget* parent, QString*) override
    {
        return new fluent::basicinput::Button(QStringLiteral("Portable extension"), parent);
    }

    int cleanupCount = 0;

protected:
    bool onActivate(const PluginContext&, QString*) override { return true; }
    void onDeactivate() noexcept override { ++cleanupCount; }
};

TEST(PluginManagerContract, Contract_WidgetBaseDispatchesFactoriesAndPreservesDeliveredWidgets)
{
    QWidget host;
    PortableWidgetPlugin plugin;
    PluginManager manager(&host);
    ASSERT_TRUE(manager.registerPlugin(&plugin));
    EXPECT_NE(qobject_cast<WidgetPluginInterface*>(&plugin), nullptr);
    EXPECT_EQ(manager.createWidget(QStringLiteral("portable"), QStringLiteral("button"), &host),
              nullptr);
    ASSERT_TRUE(manager.activate(QStringLiteral("portable")));
    QPointer<QWidget> product =
        manager.createWidget(QStringLiteral("portable"), QStringLiteral("button"), &host);
    ASSERT_FALSE(product.isNull());
    EXPECT_EQ(product->parentWidget(), &host);
    EXPECT_NE(qobject_cast<fluent::basicinput::Button*>(product), nullptr);
    ASSERT_TRUE(manager.deactivateAll());
    EXPECT_EQ(plugin.cleanupCount, 1);
    EXPECT_FALSE(product.isNull());
    EXPECT_EQ(product->parentWidget(), &host);
}

TEST(PluginManagerContract, Contract_RegistrationBorrowsObjectsAndSnapshotsMetadata)
{
    QObject owner;
    auto* plugin = new ProbePlugin(QStringLiteral("sample"), {}, nullptr, &owner);
    QPointer<ProbePlugin> lifetime(plugin);
    PluginManager manager;
    QSignalSpy registered(&manager, &PluginManager::pluginRegistered);
    QSignalSpy unregistered(&manager, &PluginManager::pluginUnregistered);
    QString error = QStringLiteral("Previous error");

    ASSERT_TRUE(manager.registerPlugin(plugin, &error)) << error.toStdString();
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(plugin->parent(), &owner);
    EXPECT_EQ(registered.count(), 1);
    EXPECT_EQ(plugin->activationCount, 0);

    plugin->descriptor.id = QStringLiteral("changed");
    plugin->descriptor.name = QStringLiteral("Changed name");
    plugin->descriptor.dependencies = {QStringLiteral("missing")};
    EXPECT_EQ(manager.metadata(QStringLiteral("sample")).name, QStringLiteral("sample"));
    EXPECT_TRUE(manager.metadata(QStringLiteral("sample")).dependencies.isEmpty());
    EXPECT_EQ(manager.plugin(QStringLiteral("sample")), plugin);
    EXPECT_EQ(manager.pluginIds(), QStringList{QStringLiteral("sample")});
    ASSERT_TRUE(manager.activate(QStringLiteral("sample"), &error)) << error.toStdString();
    EXPECT_EQ(plugin->metadataReads, 1);

    ASSERT_TRUE(manager.unregisterPlugin(QStringLiteral("sample"), &error));
    EXPECT_EQ(plugin->deactivationCount, 1);
    EXPECT_EQ(unregistered.count(), 1);
    EXPECT_FALSE(lifetime.isNull());
    EXPECT_EQ(plugin->parent(), &owner);
    EXPECT_TRUE(manager.pluginIds().isEmpty());
}

TEST(PluginManagerContract, Contract_DiamondDependenciesActivateOnceAndDeactivateInReverseOrder)
{
    QStringList journal;
    ProbePlugin base(QStringLiteral("base"), {}, &journal);
    ProbePlugin left(QStringLiteral("left"), {QStringLiteral("base")}, &journal);
    ProbePlugin right(QStringLiteral("right"), {QStringLiteral("base")}, &journal);
    ProbePlugin top(QStringLiteral("top"), {QStringLiteral("left"), QStringLiteral("right")},
                    &journal);
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&top));
    ASSERT_TRUE(manager.registerPlugin(&right));
    ASSERT_TRUE(manager.registerPlugin(&base));
    ASSERT_TRUE(manager.registerPlugin(&left));
    QString error;

    ASSERT_TRUE(manager.activate(QStringLiteral("top"), &error)) << error.toStdString();
    EXPECT_EQ(journal, (QStringList{QStringLiteral("+base"), QStringLiteral("+left"),
                                    QStringLiteral("+right"), QStringLiteral("+top")}));
    ASSERT_TRUE(manager.activate(QStringLiteral("top")));
    EXPECT_EQ(base.activationCount, 1);
    EXPECT_EQ(top.activationCount, 1);
    ASSERT_TRUE(manager.deactivateAll(&error)) << error.toStdString();
    EXPECT_EQ(journal, (QStringList{QStringLiteral("+base"), QStringLiteral("+left"),
                                    QStringLiteral("+right"), QStringLiteral("+top"),
                                    QStringLiteral("-top"), QStringLiteral("-right"),
                                    QStringLiteral("-left"), QStringLiteral("-base")}));
    ASSERT_TRUE(manager.deactivateAll());
    EXPECT_EQ(base.deactivationCount, 1);
    EXPECT_TRUE(manager.activePluginIds().isEmpty());
}

TEST(PluginManagerContract, Contract_DuplicateIdsAndCrossManagerRegistrationDoNotTransferOwnership)
{
    ProbePlugin plugin(QStringLiteral("extension"));
    ProbePlugin duplicate(QStringLiteral("extension"));
    PluginManager first;
    PluginManager second;
    QString error;
    ASSERT_TRUE(first.registerPlugin(&plugin));
    EXPECT_FALSE(first.registerPlugin(&plugin, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_FALSE(first.registerPlugin(&duplicate, &error));
    EXPECT_FALSE(second.registerPlugin(&plugin, &error));
    EXPECT_EQ(first.plugin(QStringLiteral("extension")), &plugin);
    EXPECT_TRUE(second.pluginIds().isEmpty());
    EXPECT_EQ(plugin.parent(), nullptr);

    ASSERT_TRUE(first.unregisterPlugin(QStringLiteral("extension")));
    ASSERT_TRUE(second.registerPlugin(&plugin, &error)) << error.toStdString();
    EXPECT_EQ(second.plugin(QStringLiteral("extension")), &plugin);
    EXPECT_TRUE(first.pluginIds().isEmpty());
}

TEST(PluginManagerContract, Contract_MetadataCallbackCannotRegisterOnePluginWithTwoManagers)
{
    ProbePlugin plugin(QStringLiteral("extension"));
    PluginManager first;
    PluginManager second;
    QSignalSpy firstRegistered(&first, &PluginManager::pluginRegistered);
    QSignalSpy secondRegistered(&second, &PluginManager::pluginRegistered);
    bool readingMetadata = false;
    bool innerRegistered = false;
    plugin.readMetadata = [&] {
        if (!readingMetadata) {
            readingMetadata = true;
            innerRegistered = second.registerPlugin(&plugin);
            readingMetadata = false;
        }
        return plugin.descriptor;
    };
    QString error;

    EXPECT_FALSE(first.registerPlugin(&plugin, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(innerRegistered);
    EXPECT_TRUE(first.pluginIds().isEmpty());
    EXPECT_EQ(second.plugin(QStringLiteral("extension")), &plugin);
    EXPECT_EQ(firstRegistered.count(), 0);
    EXPECT_EQ(secondRegistered.count(), 1);

    plugin.readMetadata = {};
    ASSERT_TRUE(second.unregisterPlugin(QStringLiteral("extension")));
    ASSERT_TRUE(first.registerPlugin(&plugin, &error)) << error.toStdString();
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(first.plugin(QStringLiteral("extension")), &plugin);
}

TEST(PluginManagerContract, Contract_RegistrationSignalDestructionIsSafe)
{
    auto plugin = std::make_unique<ProbePlugin>(QStringLiteral("temporary"));
    QPointer<ProbePlugin> lifetime(plugin.get());
    ProbePlugin surviving(QStringLiteral("surviving"));
    PluginManager manager;
    QSignalSpy unregistered(&manager, &PluginManager::pluginUnregistered);
    QObject::connect(&manager, &PluginManager::pluginRegistered, &manager, [&](const QString& id) {
        if (id == QStringLiteral("temporary")) {
            plugin.reset();
        }
    });
    QString error;

    EXPECT_FALSE(manager.registerPlugin(plugin.get(), &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(lifetime.isNull());
    EXPECT_TRUE(manager.pluginIds().isEmpty());
    EXPECT_EQ(unregistered.count(), 1);
    ASSERT_TRUE(manager.registerPlugin(&surviving, &error)) << error.toStdString();
    EXPECT_TRUE(error.isEmpty());
}

TEST(PluginManagerContract, Contract_InvalidMetadataAndExceptionsLeaveNoRegistration)
{
    PluginManager manager;
    QString error;
    EXPECT_FALSE(manager.registerPlugin(nullptr, &error));
    EXPECT_FALSE(error.isEmpty());
    const PluginMetadata invalid[] = {
        {QString(), QStringLiteral("Empty"), {}, {}},
        {QStringLiteral(" padded "), QStringLiteral("Whitespace"), {}, {}},
        {QStringLiteral("extension"), {}, {}, {QString()}},
        {QStringLiteral("extension"), {}, {}, {QStringLiteral(" padded ")}},
        {QStringLiteral("extension"), {}, {}, {QStringLiteral("base"), QStringLiteral("base")}},
    };
    for (const auto& descriptor : invalid) {
        SCOPED_TRACE(descriptor.id.toStdString());
        ProbePlugin candidate(QStringLiteral("candidate"));
        candidate.descriptor = descriptor;
        EXPECT_FALSE(manager.registerPlugin(&candidate, &error));
        EXPECT_FALSE(error.isEmpty());
        EXPECT_TRUE(manager.pluginIds().isEmpty());
    }

    ProbePlugin throwing(QStringLiteral("throwing"));
    throwing.readMetadata = []() -> PluginMetadata {
        throw std::runtime_error("metadata failure");
    };
    EXPECT_FALSE(manager.registerPlugin(&throwing, &error));
    EXPECT_TRUE(error.contains(QStringLiteral("metadata failure")));
    throwing.readMetadata = []() -> PluginMetadata { throw 42; };
    EXPECT_FALSE(manager.registerPlugin(&throwing, &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(manager.pluginIds().isEmpty());
    throwing.readMetadata = {};
    EXPECT_TRUE(manager.registerPlugin(&throwing, &error));
    EXPECT_TRUE(error.isEmpty());
}

TEST(PluginManagerContract, Contract_MissingDependenciesFailBeforeAnyActivationHook)
{
    QStringList journal;
    ProbePlugin base(QStringLiteral("base"), {}, &journal);
    ProbePlugin top(QStringLiteral("top"), {QStringLiteral("base"), QStringLiteral("missing")},
                    &journal);
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&base));
    ASSERT_TRUE(manager.registerPlugin(&top));
    QString error;
    EXPECT_FALSE(manager.activate(QStringLiteral("top"), &error));
    EXPECT_TRUE(error.contains(QStringLiteral("missing")));
    EXPECT_TRUE(journal.isEmpty());
    EXPECT_EQ(base.activationCount, 0);
    EXPECT_EQ(top.activationCount, 0);

    ProbePlugin missing(QStringLiteral("missing"), {}, &journal);
    ASSERT_TRUE(manager.registerPlugin(&missing));
    ASSERT_TRUE(manager.activate(QStringLiteral("top"), &error)) << error.toStdString();
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(journal, (QStringList{QStringLiteral("+base"), QStringLiteral("+missing"),
                                    QStringLiteral("+top")}));
}

TEST(PluginManagerContract, Contract_CyclesAndSelfDependenciesFailBeforeHooks)
{
    QStringList journal;
    ProbePlugin first(QStringLiteral("first"), {QStringLiteral("second")}, &journal);
    ProbePlugin second(QStringLiteral("second"), {QStringLiteral("first")}, &journal);
    ProbePlugin self(QStringLiteral("self"), {QStringLiteral("self")}, &journal);
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&first));
    ASSERT_TRUE(manager.registerPlugin(&second));
    ASSERT_TRUE(manager.registerPlugin(&self));
    for (const QString& id : {QStringLiteral("first"), QStringLiteral("self")}) {
        SCOPED_TRACE(id.toStdString());
        QString error;
        EXPECT_FALSE(manager.activate(id, &error));
        EXPECT_FALSE(error.isEmpty());
        EXPECT_TRUE(journal.isEmpty());
        EXPECT_TRUE(manager.activePluginIds().isEmpty());
    }
}

TEST(PluginManagerContract, Contract_ActivationFailureCleansAndRetainsExistingDependencies)
{
    QStringList journal;
    ProbePlugin existing(QStringLiteral("existing"), {}, &journal);
    ProbePlugin fresh(QStringLiteral("fresh"), {QStringLiteral("existing")}, &journal);
    ProbePlugin failing(QStringLiteral("failing"), {QStringLiteral("fresh")}, &journal);
    failing.activationResult = false;
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&existing));
    ASSERT_TRUE(manager.registerPlugin(&fresh));
    ASSERT_TRUE(manager.registerPlugin(&failing));
    ASSERT_TRUE(manager.activate(QStringLiteral("existing")));
    journal.clear();
    QString error;

    EXPECT_FALSE(manager.activate(QStringLiteral("failing"), &error));
    EXPECT_TRUE(error.contains(QStringLiteral("Partial initialization failed")));
    EXPECT_EQ(journal, (QStringList{QStringLiteral("+fresh"), QStringLiteral("+failing"),
                                    QStringLiteral("-failing"), QStringLiteral("-fresh")}));
    EXPECT_TRUE(manager.isActive(QStringLiteral("existing")));
    EXPECT_EQ(existing.deactivationCount, 0);
    EXPECT_EQ(failing.deactivationCount, 1);
    EXPECT_FALSE(failing.resourceAllocated);
    EXPECT_FALSE(fresh.resourceAllocated);

    failing.activationResult = true;
    ASSERT_TRUE(manager.activate(QStringLiteral("failing"), &error)) << error.toStdString();
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(existing.activationCount, 1);
    EXPECT_EQ(fresh.activationCount, 2);
    EXPECT_TRUE(manager.isActive(QStringLiteral("failing")));
}

TEST(PluginManagerContract, Contract_ActivationExceptionsUseTheSameCleanupAndRollback)
{
    QStringList journal;
    ProbePlugin dependency(QStringLiteral("dependency"), {}, &journal);
    ProbePlugin extension(QStringLiteral("extension"), {QStringLiteral("dependency")}, &journal);
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&dependency));
    ASSERT_TRUE(manager.registerPlugin(&extension));
    for (const bool knownException : {true, false}) {
        SCOPED_TRACE(knownException);
        journal.clear();
        extension.activation = [knownException](const PluginContext&, QString*) -> bool {
            if (knownException) {
                throw std::runtime_error("activation exception");
            }
            throw 42;
        };
        QString error;
        EXPECT_FALSE(manager.activate(QStringLiteral("extension"), &error));
        EXPECT_FALSE(error.isEmpty());
        if (knownException) {
            EXPECT_TRUE(error.contains(QStringLiteral("activation exception")));
        }
        EXPECT_EQ(journal,
                  (QStringList{QStringLiteral("+dependency"), QStringLiteral("+extension"),
                               QStringLiteral("-extension"), QStringLiteral("-dependency")}));
        EXPECT_FALSE(extension.resourceAllocated);
        EXPECT_FALSE(dependency.resourceAllocated);
        EXPECT_TRUE(manager.activePluginIds().isEmpty());
    }
    EXPECT_EQ(extension.deactivationCount, 2);
    EXPECT_EQ(dependency.deactivationCount, 2);
}

TEST(PluginManagerContract, Contract_DeactivationCascadesToDependentsAndKeepsDependenciesAvailable)
{
    QStringList journal;
    ProbePlugin base(QStringLiteral("base"), {}, &journal);
    ProbePlugin middle(QStringLiteral("middle"), {QStringLiteral("base")}, &journal);
    ProbePlugin top(QStringLiteral("top"), {QStringLiteral("middle")}, &journal);
    ProbePlugin unrelated(QStringLiteral("unrelated"), {}, &journal);
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&base));
    ASSERT_TRUE(manager.registerPlugin(&middle));
    ASSERT_TRUE(manager.registerPlugin(&top));
    ASSERT_TRUE(manager.registerPlugin(&unrelated));
    top.deactivation = [&] {
        EXPECT_EQ(manager.context().activePlugin(QStringLiteral("middle")), &middle);
        EXPECT_EQ(manager.context().activePlugin(QStringLiteral("base")), &base);
    };
    middle.deactivation = [&] {
        EXPECT_EQ(manager.context().activePlugin(QStringLiteral("base")), &base);
    };
    ASSERT_TRUE(manager.activate(QStringLiteral("top")));
    ASSERT_TRUE(manager.activate(QStringLiteral("unrelated")));
    journal.clear();

    ASSERT_TRUE(manager.deactivate(QStringLiteral("middle")));
    EXPECT_EQ(journal, (QStringList{QStringLiteral("-top"), QStringLiteral("-middle")}));
    EXPECT_TRUE(manager.isActive(QStringLiteral("base")));
    EXPECT_TRUE(manager.isActive(QStringLiteral("unrelated")));
    EXPECT_EQ(manager.plugin(QStringLiteral("top")), &top);
    ASSERT_TRUE(manager.deactivate(QStringLiteral("middle")));
    EXPECT_EQ(middle.deactivationCount, 1);
    EXPECT_EQ(top.deactivationCount, 1);
}

TEST(PluginManagerContract, Contract_UnregisterStopsDependentsWithoutDeletingOrUnregisteringThem)
{
    ProbePlugin base(QStringLiteral("base"));
    ProbePlugin dependent(QStringLiteral("dependent"), {QStringLiteral("base")});
    PluginManager manager;
    PluginManager other;
    ASSERT_TRUE(manager.registerPlugin(&base));
    ASSERT_TRUE(manager.registerPlugin(&dependent));
    ASSERT_TRUE(manager.activate(QStringLiteral("dependent")));
    QSignalSpy unregistered(&manager, &PluginManager::pluginUnregistered);

    ASSERT_TRUE(manager.unregisterPlugin(QStringLiteral("base")));
    EXPECT_EQ(base.deactivationCount, 1);
    EXPECT_EQ(dependent.deactivationCount, 1);
    EXPECT_EQ(manager.pluginIds(), QStringList{QStringLiteral("dependent")});
    EXPECT_EQ(manager.plugin(QStringLiteral("dependent")), &dependent);
    EXPECT_EQ(unregistered.count(), 1);
    EXPECT_EQ(unregistered.at(0).at(0).toString(), QStringLiteral("base"));
    EXPECT_FALSE(manager.activate(QStringLiteral("dependent")));
    EXPECT_EQ(dependent.activationCount, 1);
    ASSERT_TRUE(other.registerPlugin(&base));
}

TEST(PluginManagerContract, Contract_UnregisterSnapshotsCallerIdAcrossSignals)
{
    ProbePlugin first(QStringLiteral("first"));
    ProbePlugin second(QStringLiteral("second"));
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&first));
    ASSERT_TRUE(manager.registerPlugin(&second));
    ASSERT_TRUE(manager.activate(QStringLiteral("first")));
    ASSERT_TRUE(manager.activate(QStringLiteral("second")));
    QString id = QStringLiteral("first");
    QObject::connect(&manager, &PluginManager::pluginDeactivated, &manager,
                     [&](const QString& stopped) {
                         if (stopped == QStringLiteral("first")) {
                             id = QStringLiteral("second");
                         }
                     });

    ASSERT_TRUE(manager.unregisterPlugin(id));
    EXPECT_EQ(id, QStringLiteral("second"));
    EXPECT_EQ(manager.plugin(QStringLiteral("first")), nullptr);
    EXPECT_EQ(manager.plugin(QStringLiteral("second")), &second);
    EXPECT_TRUE(manager.isActive(QStringLiteral("second")));
    EXPECT_EQ(second.deactivationCount, 0);
}

TEST(PluginManagerContract, Contract_SelfDeletionDuringCleanupDoesNotLeaveDanglingRegistration)
{
    int cleanupCount = 0;
    auto* plugin = new SelfDeletingPlugin(QStringLiteral("self"), &cleanupCount);
    QPointer<SelfDeletingPlugin> lifetime(plugin);
    ProbePlugin unrelated(QStringLiteral("unrelated"));
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(plugin));
    ASSERT_TRUE(manager.registerPlugin(&unrelated));
    ASSERT_TRUE(manager.activate(QStringLiteral("self")));
    ASSERT_TRUE(manager.activate(QStringLiteral("unrelated")));
    QSignalSpy unregistered(&manager, &PluginManager::pluginUnregistered);

    ASSERT_TRUE(manager.unregisterPlugin(plugin->id()));
    EXPECT_TRUE(lifetime.isNull());
    EXPECT_EQ(cleanupCount, 1);
    EXPECT_EQ(unregistered.count(), 1);
    EXPECT_EQ(manager.pluginIds(), QStringList{QStringLiteral("unrelated")});
    EXPECT_TRUE(manager.isActive(QStringLiteral("unrelated")));
}

TEST(PluginManagerContract, Contract_UnexpectedDestructionStopsDependentsAndAllowsReplacement)
{
    QStringList journal;
    auto base = std::make_unique<ProbePlugin>(QStringLiteral("base"), QStringList{}, &journal);
    ProbePlugin middle(QStringLiteral("middle"), {QStringLiteral("base")}, &journal);
    ProbePlugin top(QStringLiteral("top"), {QStringLiteral("middle")}, &journal);
    ProbePlugin unrelated(QStringLiteral("unrelated"), {}, &journal);
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(base.get()));
    ASSERT_TRUE(manager.registerPlugin(&middle));
    ASSERT_TRUE(manager.registerPlugin(&top));
    ASSERT_TRUE(manager.registerPlugin(&unrelated));
    ASSERT_TRUE(manager.activate(QStringLiteral("top")));
    ASSERT_TRUE(manager.activate(QStringLiteral("unrelated")));
    journal.clear();
    QSignalSpy unregistered(&manager, &PluginManager::pluginUnregistered);

    base.reset();
    EXPECT_EQ(journal, (QStringList{QStringLiteral("-top"), QStringLiteral("-middle")}));
    EXPECT_EQ(manager.plugin(QStringLiteral("base")), nullptr);
    EXPECT_FALSE(manager.isActive(QStringLiteral("top")));
    EXPECT_EQ(manager.plugin(QStringLiteral("middle")), &middle);
    EXPECT_TRUE(manager.isActive(QStringLiteral("unrelated")));
    EXPECT_EQ(unregistered.count(), 1);

    base = std::make_unique<ProbePlugin>(QStringLiteral("base"));
    ASSERT_TRUE(manager.registerPlugin(base.get()));
    ASSERT_TRUE(manager.activate(QStringLiteral("top")));
    EXPECT_EQ(top.activationCount, 2);
}

TEST(PluginManagerContract, Contract_DestroyingDependencyInActivationHookFailsAndCleansPartialWork)
{
    auto dependency = std::make_unique<ProbePlugin>(QStringLiteral("dependency"));
    ProbePlugin extension(QStringLiteral("extension"), {QStringLiteral("dependency")});
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(dependency.get()));
    ASSERT_TRUE(manager.registerPlugin(&extension));
    ASSERT_TRUE(manager.activate(QStringLiteral("dependency")));
    extension.activation = [&](const PluginContext& context, QString*) {
        EXPECT_EQ(context.activePlugin(QStringLiteral("dependency")), dependency.get());
        dependency.reset();
        EXPECT_EQ(context.activePlugin(QStringLiteral("dependency")), nullptr);
        return true;
    };
    QString error;

    EXPECT_FALSE(manager.activate(QStringLiteral("extension"), &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_EQ(extension.deactivationCount, 1);
    EXPECT_FALSE(extension.resourceAllocated);
    EXPECT_FALSE(manager.isActive(QStringLiteral("extension")));
    EXPECT_EQ(manager.pluginIds(), QStringList{QStringLiteral("extension")});
}

TEST(PluginManagerContract, Contract_ActivationSignalDependencyDestructionStopsDependents)
{
    auto base = std::make_unique<ProbePlugin>(QStringLiteral("base"));
    ProbePlugin left(QStringLiteral("left"), {QStringLiteral("base")});
    ProbePlugin right(QStringLiteral("right"), {QStringLiteral("base")});
    ProbePlugin top(QStringLiteral("top"), {QStringLiteral("left"), QStringLiteral("right")});
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(base.get()));
    ASSERT_TRUE(manager.registerPlugin(&left));
    ASSERT_TRUE(manager.registerPlugin(&right));
    ASSERT_TRUE(manager.registerPlugin(&top));
    QObject::connect(&manager, &PluginManager::pluginActivated, &manager, [&](const QString& id) {
        if (id == QStringLiteral("top")) {
            base.reset();
        }
    });
    QString error;

    EXPECT_FALSE(manager.activate(QStringLiteral("top"), &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(manager.activePluginIds().isEmpty());
    EXPECT_EQ(left.deactivationCount, 1);
    EXPECT_EQ(right.deactivationCount, 1);
    EXPECT_EQ(top.deactivationCount, 1);
    EXPECT_EQ(manager.plugin(QStringLiteral("base")), nullptr);
}

TEST(PluginManagerContract, Contract_ActivationSignalDestructionRetainsUnrelatedPlugins)
{
    auto plugin = std::make_unique<ProbePlugin>(QStringLiteral("temporary"));
    QPointer<ProbePlugin> lifetime(plugin.get());
    ProbePlugin surviving(QStringLiteral("surviving"));
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(plugin.get()));
    ASSERT_TRUE(manager.registerPlugin(&surviving));
    ASSERT_TRUE(manager.activate(QStringLiteral("surviving")));
    QObject::connect(&manager, &PluginManager::pluginActivated, &manager, [&](const QString& id) {
        if (id == QStringLiteral("temporary")) {
            plugin.reset();
        }
    });
    QString error;

    EXPECT_FALSE(manager.activate(QStringLiteral("temporary"), &error));
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(lifetime.isNull());
    EXPECT_EQ(manager.plugin(QStringLiteral("temporary")), nullptr);
    EXPECT_EQ(manager.activePluginIds(), QStringList{QStringLiteral("surviving")});
    EXPECT_EQ(surviving.deactivationCount, 0);
}

TEST(PluginManagerContract, Contract_DependencyDestructionDuringCleanupHooksAndSignalsIsSafe)
{
    for (const bool fromSignal : {false, true}) {
        SCOPED_TRACE(fromSignal);
        QStringList journal;
        auto base = std::make_unique<ProbePlugin>(QStringLiteral("base"), QStringList{}, &journal);
        ProbePlugin middle(QStringLiteral("middle"), {QStringLiteral("base")}, &journal);
        ProbePlugin top(QStringLiteral("top"), {QStringLiteral("middle")}, &journal);
        PluginManager manager;
        ASSERT_TRUE(manager.registerPlugin(base.get()));
        ASSERT_TRUE(manager.registerPlugin(&middle));
        ASSERT_TRUE(manager.registerPlugin(&top));
        ASSERT_TRUE(manager.activate(QStringLiteral("top")));
        journal.clear();
        if (fromSignal) {
            QObject::connect(&manager, &PluginManager::pluginDeactivated, &manager,
                             [&](const QString& id) {
                                 if (id == QStringLiteral("top")) {
                                     base.reset();
                                 }
                             });
        } else {
            top.deactivation = [&] { base.reset(); };
        }

        ASSERT_TRUE(manager.deactivate(QStringLiteral("top")));
        EXPECT_EQ(journal, (QStringList{QStringLiteral("-top"), QStringLiteral("-middle")}));
        EXPECT_EQ(top.deactivationCount, 1);
        EXPECT_EQ(middle.deactivationCount, 1);
        EXPECT_TRUE(manager.activePluginIds().isEmpty());
        EXPECT_EQ(manager.plugin(QStringLiteral("base")), nullptr);
    }
}

TEST(PluginManagerContract, Contract_MetadataHooksLifecycleHooksAndSignalsRejectNestedMutations)
{
    ProbePlugin plugin(QStringLiteral("plugin"));
    ProbePlugin extra(QStringLiteral("extra"));
    PluginManager manager;
    int attempts = 0;
    const auto attemptMutations = [&] {
        ++attempts;
        QString error;
        EXPECT_FALSE(manager.registerPlugin(&extra, &error));
        EXPECT_FALSE(error.isEmpty());
        EXPECT_FALSE(manager.unregisterPlugin(QStringLiteral("plugin"), &error));
        EXPECT_FALSE(manager.activate(QStringLiteral("plugin"), &error));
        EXPECT_FALSE(manager.deactivate(QStringLiteral("plugin"), &error));
        EXPECT_FALSE(manager.deactivateAll(&error));
        EXPECT_EQ(manager.createWidget(QStringLiteral("plugin"), QStringLiteral("button"), nullptr,
                                       &error),
                  nullptr);
        EXPECT_FALSE(error.isEmpty());
    };
    plugin.readMetadata = [&] {
        attemptMutations();
        return plugin.descriptor;
    };
    plugin.activation = [&](const PluginContext&, QString*) {
        attemptMutations();
        return true;
    };
    plugin.deactivation = attemptMutations;
    QObject::connect(&manager, &PluginManager::pluginRegistered, &manager,
                     [&](const QString&) { attemptMutations(); });
    QObject::connect(&manager, &PluginManager::pluginActivated, &manager,
                     [&](const QString&) { attemptMutations(); });
    QObject::connect(&manager, &PluginManager::pluginDeactivated, &manager,
                     [&](const QString&) { attemptMutations(); });
    QObject::connect(&manager, &PluginManager::pluginUnregistered, &manager,
                     [&](const QString&) { attemptMutations(); });

    ASSERT_TRUE(manager.registerPlugin(&plugin));
    ASSERT_TRUE(manager.activate(QStringLiteral("plugin")));
    ASSERT_TRUE(manager.deactivate(QStringLiteral("plugin")));
    ASSERT_TRUE(manager.unregisterPlugin(QStringLiteral("plugin")));
    EXPECT_EQ(attempts, 7);
    EXPECT_EQ(plugin.activationCount, 1);
    EXPECT_EQ(plugin.deactivationCount, 1);
    EXPECT_EQ(extra.activationCount, 0);
    EXPECT_TRUE(manager.pluginIds().isEmpty());
}

TEST(PluginManagerContract, Contract_ManagerDestructionDeactivatesWithoutDeletingBorrowedObjects)
{
    QStringList journal;
    QObject owner;
    auto* base = new ProbePlugin(QStringLiteral("base"), {}, &journal, &owner);
    auto* top = new ProbePlugin(QStringLiteral("top"), {QStringLiteral("base")}, &journal, &owner);
    QPointer<ProbePlugin> baseLifetime(base);
    QPointer<ProbePlugin> topLifetime(top);
    {
        PluginManager manager;
        ASSERT_TRUE(manager.registerPlugin(base));
        ASSERT_TRUE(manager.registerPlugin(top));
        ASSERT_TRUE(manager.activate(QStringLiteral("top")));
    }
    EXPECT_EQ(journal, (QStringList{QStringLiteral("+base"), QStringLiteral("+top"),
                                    QStringLiteral("-top"), QStringLiteral("-base")}));
    EXPECT_FALSE(baseLifetime.isNull());
    EXPECT_FALSE(topLifetime.isNull());
    EXPECT_EQ(base->parent(), &owner);
    EXPECT_EQ(top->parent(), &owner);
    PluginManager replacement;
    EXPECT_TRUE(replacement.registerPlugin(base));
    EXPECT_TRUE(replacement.registerPlugin(top));
}

TEST(PluginManagerContract, Contract_ContextCopiesBorrowHostAndGuardManagerAndPluginLifetimes)
{
    auto host = std::make_unique<QObject>();
    auto dependency = std::make_unique<ProbePlugin>(QStringLiteral("dependency"));
    ProbePlugin consumer(QStringLiteral("consumer"), {QStringLiteral("dependency")});
    std::optional<PluginContext> saved;
    {
        PluginManager manager(host.get());
        ASSERT_TRUE(manager.registerPlugin(dependency.get()));
        ASSERT_TRUE(manager.registerPlugin(&consumer));
        consumer.activation = [&](const PluginContext& context, QString*) {
            EXPECT_EQ(context.host(), host.get());
            EXPECT_EQ(context.activePlugin(QStringLiteral("dependency")), dependency.get());
            EXPECT_EQ(context.activePlugin(QStringLiteral("consumer")), nullptr);
            saved = context;
            return true;
        };
        ASSERT_TRUE(manager.activate(QStringLiteral("consumer")));
        ASSERT_TRUE(saved.has_value());
        host.reset();
        EXPECT_EQ(saved->host(), nullptr);
        dependency.reset();
        EXPECT_EQ(saved->activePlugin(QStringLiteral("dependency")), nullptr);
        EXPECT_EQ(saved->activePlugin(QStringLiteral("consumer")), nullptr);
        EXPECT_EQ(consumer.deactivationCount, 1);
    }
    ASSERT_TRUE(saved.has_value());
    EXPECT_EQ(saved->host(), nullptr);
    EXPECT_EQ(saved->activePlugin(QStringLiteral("consumer")), nullptr);
}

TEST(PluginManagerContract, Contract_CrossThreadOperationsAreRejectedBeforeStateChanges)
{
    ProbePlugin plugin(QStringLiteral("plugin"));
    ProbePlugin extra(QStringLiteral("extra"));
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&plugin));
    ASSERT_TRUE(manager.activate(QStringLiteral("plugin")));
    bool registered = true;
    bool activated = true;
    bool deactivated = true;
    bool allDeactivated = true;
    bool unregistered = true;
    QWidget* widget = nullptr;
    QString error;
    std::thread worker([&] {
        registered = manager.registerPlugin(&extra, &error);
        activated = manager.activate(QStringLiteral("plugin"), &error);
        deactivated = manager.deactivate(QStringLiteral("plugin"), &error);
        allDeactivated = manager.deactivateAll(&error);
        unregistered = manager.unregisterPlugin(QStringLiteral("plugin"), &error);
        widget = manager.createWidget(QStringLiteral("plugin"), QStringLiteral("button"), nullptr,
                                      &error);
    });
    worker.join();

    EXPECT_FALSE(registered);
    EXPECT_FALSE(activated);
    EXPECT_FALSE(deactivated);
    EXPECT_FALSE(allDeactivated);
    EXPECT_FALSE(unregistered);
    EXPECT_EQ(widget, nullptr);
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(manager.isActive(QStringLiteral("plugin")));
    EXPECT_EQ(plugin.activationCount, 1);
    EXPECT_EQ(plugin.deactivationCount, 0);
    EXPECT_EQ(manager.pluginIds(), QStringList{QStringLiteral("plugin")});
}

TEST(PluginManagerContract, Contract_RegistrationRejectsPluginsAndHostsOnAnotherThread)
{
    QThread worker;
    worker.start();
    ProbePlugin plugin(QStringLiteral("plugin"));
    QObject host;
    PluginManager manager;
    plugin.moveToThread(&worker);
    QString error;
    const bool pluginAccepted = manager.registerPlugin(&plugin, &error);
    const QString pluginError = error;
    QThread* originalThread = manager.thread();
    QMetaObject::invokeMethod(
        &plugin, [&] { plugin.moveToThread(originalThread); }, Qt::BlockingQueuedConnection);
    host.moveToThread(&worker);
    PluginManager hostManager(&host);
    const bool hostAccepted = hostManager.registerPlugin(&plugin, &error);
    QMetaObject::invokeMethod(
        &host, [&] { host.moveToThread(originalThread); }, Qt::BlockingQueuedConnection);
    worker.quit();
    const bool workerStopped = worker.wait(1000);

    EXPECT_TRUE(workerStopped);
    EXPECT_FALSE(pluginAccepted);
    EXPECT_FALSE(pluginError.isEmpty());
    EXPECT_FALSE(hostAccepted);
    EXPECT_FALSE(error.isEmpty());
    EXPECT_TRUE(manager.pluginIds().isEmpty());
    EXPECT_TRUE(hostManager.pluginIds().isEmpty());
    ASSERT_TRUE(manager.registerPlugin(&plugin));
}

TEST(PluginWidgetContract, Contract_FactoryRequiresActiveInterfaceAndSupportedKey)
{
    WidgetProbePlugin widgetPlugin(QStringLiteral("widgets"));
    ProbePlugin ordinary(QStringLiteral("ordinary"));
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&widgetPlugin));
    ASSERT_TRUE(manager.registerPlugin(&ordinary));
    QString error;
    EXPECT_EQ(
        manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"), nullptr, &error),
        nullptr);
    EXPECT_FALSE(error.isEmpty());
    EXPECT_EQ(widgetPlugin.activationCount, 0);
    ASSERT_TRUE(manager.activate(QStringLiteral("ordinary")));
    EXPECT_EQ(
        manager.createWidget(QStringLiteral("ordinary"), QStringLiteral("button"), nullptr, &error),
        nullptr);
    ASSERT_TRUE(manager.activate(QStringLiteral("widgets")));
    EXPECT_EQ(manager.createWidget(QStringLiteral("widgets"), QStringLiteral("unsupported"),
                                   nullptr, &error),
              nullptr);
    EXPECT_EQ(widgetPlugin.factoryCalls, 0);
    EXPECT_FALSE(error.isEmpty());
}

TEST(PluginWidgetContract, Contract_DeliveredWidgetsRemainCallerOwned)
{
    WidgetProbePlugin plugin(QStringLiteral("widgets"));
    QWidget parent;
    auto manager = std::make_unique<PluginManager>();
    ASSERT_TRUE(manager->registerPlugin(&plugin));
    ASSERT_TRUE(manager->activate(QStringLiteral("widgets")));
    QString error;
    QPointer<QWidget> child(manager->createWidget(QStringLiteral("widgets"),
                                                  QStringLiteral("button"), &parent, &error));
    ASSERT_FALSE(child.isNull()) << error.toStdString();
    EXPECT_EQ(child->parentWidget(), &parent);
    std::unique_ptr<QWidget> first(manager->createWidget(
        QStringLiteral("widgets"), QStringLiteral("button"), nullptr, &error));
    std::unique_ptr<QWidget> second(manager->createWidget(
        QStringLiteral("widgets"), QStringLiteral("button"), nullptr, &error));
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    EXPECT_NE(first.get(), second.get());
    EXPECT_EQ(first->parent(), nullptr);
    EXPECT_EQ(second->parent(), nullptr);
    QPointer<QWidget> firstLifetime(first.get());

    ASSERT_TRUE(manager->deactivate(QStringLiteral("widgets")));
    ASSERT_TRUE(manager->unregisterPlugin(QStringLiteral("widgets")));
    manager.reset();
    EXPECT_FALSE(child.isNull());
    EXPECT_FALSE(firstLifetime.isNull());
    EXPECT_EQ(plugin.deactivationCount, 1);
    EXPECT_EQ(plugin.factoryCalls, 3);
    first.reset();
    EXPECT_TRUE(firstLifetime.isNull());
}

TEST(PluginWidgetContract, Contract_FactoryFailuresDoNotDeliverWidgets)
{
    WidgetProbePlugin plugin(QStringLiteral("widgets"));
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&plugin));
    ASSERT_TRUE(manager.activate(QStringLiteral("widgets")));
    QString error;
    plugin.factoryCallback = [](const QString&, QWidget*, QString* detail) -> QWidget* {
        *detail = QStringLiteral("Factory declined request");
        return nullptr;
    };
    EXPECT_EQ(
        manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"), nullptr, &error),
        nullptr);
    EXPECT_TRUE(error.contains(QStringLiteral("Factory declined request")));
    plugin.factoryCallback = [](const QString&, QWidget*, QString*) -> QWidget* {
        throw std::runtime_error("factory exception");
    };
    EXPECT_EQ(
        manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"), nullptr, &error),
        nullptr);
    EXPECT_TRUE(error.contains(QStringLiteral("factory exception")));
    plugin.factoryCallback = [](const QString&, QWidget*, QString*) -> QWidget* { throw 42; };
    EXPECT_EQ(
        manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"), nullptr, &error),
        nullptr);
    EXPECT_FALSE(error.isEmpty());

    QWidget requestedParent;
    plugin.factoryCallback = [](const QString&, QWidget*, QString*) -> QWidget* {
        return new fluent::basicinput::Button(QStringLiteral("Wrong parent"));
    };
    EXPECT_EQ(manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"),
                                   &requestedParent, &error),
              nullptr);
    EXPECT_TRUE(plugin.lastWidget.isNull());
    EXPECT_TRUE(requestedParent.children().isEmpty());
    EXPECT_TRUE(manager.isActive(QStringLiteral("widgets")));
}

TEST(PluginWidgetContract, Contract_CallerPluginIdAndKeyAreSnapshottedAcrossFactoryCallbacks)
{
    WidgetProbePlugin plugin(QStringLiteral("widgets"));
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&plugin));
    ASSERT_TRUE(manager.activate(QStringLiteral("widgets")));
    QString id = QStringLiteral("widgets");
    QString key = QStringLiteral("button");
    plugin.factoryCallback = [&](const QString& requestedKey, QWidget* parent, QString*) {
        id = QStringLiteral("changed-plugin");
        key = QStringLiteral("changed-key");
        EXPECT_EQ(requestedKey, QStringLiteral("button"));
        return new fluent::basicinput::Button(requestedKey, parent);
    };
    QString error;

    std::unique_ptr<QWidget> widget(manager.createWidget(id, key, nullptr, &error));
    ASSERT_NE(widget, nullptr) << error.toStdString();
    EXPECT_EQ(id, QStringLiteral("changed-plugin"));
    EXPECT_EQ(key, QStringLiteral("changed-key"));
    EXPECT_TRUE(error.isEmpty());
    EXPECT_TRUE(manager.isActive(QStringLiteral("widgets")));
}

TEST(PluginWidgetContract, Contract_DependencyLossDuringKeyLookupOrFactoryRejectsTheResult)
{
    for (const bool duringKeyLookup : {true, false}) {
        SCOPED_TRACE(duringKeyLookup);
        auto dependency = std::make_unique<ProbePlugin>(QStringLiteral("dependency"));
        WidgetProbePlugin plugin(QStringLiteral("widgets"), {QStringLiteral("dependency")});
        PluginManager manager;
        ASSERT_TRUE(manager.registerPlugin(dependency.get()));
        ASSERT_TRUE(manager.registerPlugin(&plugin));
        ASSERT_TRUE(manager.activate(QStringLiteral("widgets")));
        if (duringKeyLookup) {
            plugin.keysCallback = [&] {
                dependency.reset();
                return QStringList{QStringLiteral("button")};
            };
        } else {
            plugin.factoryCallback = [&](const QString&, QWidget* parent, QString*) {
                dependency.reset();
                return new fluent::basicinput::Button(QStringLiteral("Unusable"), parent);
            };
        }
        QString error;
        EXPECT_EQ(manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"), nullptr,
                                       &error),
                  nullptr);
        EXPECT_FALSE(error.isEmpty());
        EXPECT_EQ(plugin.factoryCalls, duringKeyLookup ? 0 : 1);
        EXPECT_TRUE(plugin.lastWidget.isNull());
        EXPECT_FALSE(manager.isActive(QStringLiteral("widgets")));
        EXPECT_EQ(plugin.deactivationCount, 1);
    }
}

TEST(PluginWidgetContract, Contract_ParentDestructionDuringKeyLookupOrFactoryRejectsTheResult)
{
    for (const bool duringKeyLookup : {true, false}) {
        SCOPED_TRACE(duringKeyLookup);
        WidgetProbePlugin plugin(QStringLiteral("widgets"));
        PluginManager manager;
        ASSERT_TRUE(manager.registerPlugin(&plugin));
        ASSERT_TRUE(manager.activate(QStringLiteral("widgets")));
        auto parent = std::make_unique<QWidget>();
        if (duringKeyLookup) {
            plugin.keysCallback = [&] {
                parent.reset();
                return QStringList{QStringLiteral("button")};
            };
        } else {
            plugin.factoryCallback = [&](const QString&, QWidget* requestedParent, QString*) {
                EXPECT_EQ(requestedParent, parent.get());
                parent.reset();
                return new fluent::basicinput::Button(QStringLiteral("Orphaned result"));
            };
        }
        QString error;

        EXPECT_EQ(manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"),
                                       parent.get(), &error),
                  nullptr);
        EXPECT_FALSE(error.isEmpty());
        EXPECT_EQ(parent, nullptr);
        EXPECT_EQ(plugin.factoryCalls, duringKeyLookup ? 0 : 1);
        EXPECT_TRUE(plugin.lastWidget.isNull());
        EXPECT_TRUE(manager.isActive(QStringLiteral("widgets")));
    }
}

TEST(PluginWidgetContract, Contract_FactoryCallbacksRejectNestedOperationsButDeliverTheOuterWidget)
{
    WidgetProbePlugin plugin(QStringLiteral("widgets"));
    ProbePlugin extra(QStringLiteral("extra"));
    PluginManager manager;
    ASSERT_TRUE(manager.registerPlugin(&plugin));
    ASSERT_TRUE(manager.activate(QStringLiteral("widgets")));
    plugin.factoryCallback = [&](const QString&, QWidget* parent, QString*) {
        QString error;
        EXPECT_FALSE(manager.registerPlugin(&extra, &error));
        EXPECT_FALSE(manager.deactivateAll(&error));
        EXPECT_EQ(manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"), nullptr,
                                       &error),
                  nullptr);
        EXPECT_FALSE(error.isEmpty());
        return new fluent::basicinput::Button(QStringLiteral("Outer widget"), parent);
    };
    QString error;

    std::unique_ptr<QWidget> widget(
        manager.createWidget(QStringLiteral("widgets"), QStringLiteral("button"), nullptr, &error));
    ASSERT_NE(widget, nullptr) << error.toStdString();
    EXPECT_TRUE(error.isEmpty());
    EXPECT_EQ(plugin.factoryCalls, 1);
    EXPECT_TRUE(manager.isActive(QStringLiteral("widgets")));
    EXPECT_EQ(manager.pluginIds(), QStringList{QStringLiteral("widgets")});
}

} // namespace

#include "TestPluginManager.moc"
