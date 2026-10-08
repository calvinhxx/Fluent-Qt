#include "PluginExtensionWindow.h"

#include <QHBoxLayout>
#include <QPointer>
#include <QResizeEvent>
#include <QShowEvent>
#include <QVBoxLayout>

namespace fluentqt::plugin_extension {
namespace {

const QString kPluginId = QStringLiteral("example.greeting");
const QString kWidgetKey = QStringLiteral("greeting");

class GreetingPlugin final : public fluent::plugins::WidgetPlugin {
public:
    fluent::plugins::PluginMetadata metadata() const override
    {
        return {kPluginId, QStringLiteral("Greeting extension"), QVersionNumber(1, 0), {}};
    }

    QStringList widgetKeys() const override { return {kWidgetKey}; }

    QWidget* createWidget(const QString& key, QWidget* parent, QString* error) override
    {
        if (!m_active || key != kWidgetKey) {
            if (error)
                *error = QStringLiteral("Greeting extension is unavailable.");
            return nullptr;
        }
        auto* button =
            new fluent::basicinput::Button(QStringLiteral("Hello from an extension"), parent);
        button->setFluentStyle(fluent::basicinput::Button::Accent);
        QObject::connect(button, &fluent::basicinput::Button::clicked, button,
                         [button] { button->setText(QStringLiteral("Hello, you!")); });
        return button;
    }

    int activations = 0;
    int deactivations = 0;

protected:
    bool onActivate(const fluent::plugins::PluginContext&, QString*) override
    {
        m_active = true;
        ++activations;
        return true;
    }

    void onDeactivate() noexcept override
    {
        m_active = false;
        ++deactivations;
    }

private:
    bool m_active = false;
};

QVariantMap widgetGeometry(const QWidget* widget)
{
    if (!widget || !widget->isVisible())
        return {};
    const QPoint origin = widget->mapToGlobal(QPoint());
    return {{QStringLiteral("x"), origin.x()},
            {QStringLiteral("y"), origin.y()},
            {QStringLiteral("width"), widget->width()},
            {QStringLiteral("height"), widget->height()}};
}

} // namespace

struct PluginExtensionWindow::Private {
    explicit Private(QObject* host) : manager(host) {}
    GreetingPlugin plugin;
    fluent::plugins::PluginManager manager;
    fluent::basicinput::Button* activation = nullptr;
    fluent::basicinput::Button* creation = nullptr;
    fluent::textfields::Label* status = nullptr;
    QWidget* outputs = nullptr;
    QVBoxLayout* outputLayout = nullptr;
    QList<QPointer<fluent::basicinput::Button>> greetings;
    QString error;
    int rejectedFactories = 0;
    int greetingClicks = 0;
};

PluginExtensionWindow::PluginExtensionWindow() : d(std::make_unique<Private>(this))
{
    setObjectName(QStringLiteral("pluginExtensionWindow"));
    setWindowTitle(QStringLiteral("FluentQt Plugin Extension"));
    resize(600, 500);
    auto* content = new QWidget;
    content->setObjectName(QStringLiteral("pluginContent"));
    auto* column = new QVBoxLayout(content);
    column->setContentsMargins(24, 24, 24, 24);
    column->setSpacing(12);
    d->status = new fluent::textfields::Label(content);
    d->status->setObjectName(QStringLiteral("pluginStatus"));
    d->status->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    column->addWidget(d->status);
    auto* commands = new QHBoxLayout;
    d->activation = new fluent::basicinput::Button(content);
    d->activation->setObjectName(QStringLiteral("pluginActivation"));
    d->creation = new fluent::basicinput::Button(QStringLiteral("Create greeting"), content);
    d->creation->setObjectName(QStringLiteral("pluginCreateGreeting"));
    commands->addWidget(d->activation);
    commands->addWidget(d->creation);
    column->addLayout(commands);
    d->outputs = new QWidget(content);
    d->outputs->setObjectName(QStringLiteral("pluginOutputs"));
    d->outputLayout = new QVBoxLayout(d->outputs);
    d->outputLayout->setContentsMargins(0, 0, 0, 0);
    d->outputLayout->setSpacing(12);
    column->addWidget(d->outputs);
    column->addStretch();
    setContentWidget(content);
    d->manager.registerPlugin(&d->plugin, &d->error);
    connect(d->activation, &fluent::basicinput::Button::clicked, this,
            [this] { setExtensionActive(!d->manager.isActive(kPluginId)); });
    connect(d->creation, &fluent::basicinput::Button::clicked, this, [this] { createGreeting(); });
    connect(&d->manager, &fluent::plugins::PluginManager::pluginActivated, this,
            [this](const QString&) { updateState(); });
    connect(&d->manager, &fluent::plugins::PluginManager::pluginDeactivated, this,
            [this](const QString&) { updateState(); });
    updateState();
}

PluginExtensionWindow::~PluginExtensionWindow()
{
    disconnect(&d->manager, nullptr, this, nullptr);
    d->manager.deactivateAll();
}

bool PluginExtensionWindow::isReady() const
{
    return d->manager.plugin(kPluginId) == &d->plugin;
}

bool PluginExtensionWindow::setExtensionActive(bool active)
{
    const bool changed = active ? d->manager.activate(kPluginId, &d->error)
                                : d->manager.deactivate(kPluginId, &d->error);
    updateState();
    return changed;
}

QWidget* PluginExtensionWindow::createGreeting()
{
    auto* widget = d->manager.createWidget(kPluginId, kWidgetKey, d->outputs, &d->error);
    auto* greeting = qobject_cast<fluent::basicinput::Button*>(widget);
    if (!greeting) {
        ++d->rejectedFactories;
        updateState();
        return nullptr;
    }
    greeting->setObjectName(QStringLiteral("pluginGreeting%1").arg(d->greetings.size() + 1));
    d->greetings.append(greeting);
    d->outputLayout->addWidget(greeting);
    connect(greeting, &fluent::basicinput::Button::clicked, this, [this] {
        ++d->greetingClicks;
        updateState();
    });
    updateState();
    return greeting;
}

QVariantMap PluginExtensionWindow::pluginState() const
{
    int alive = 0;
    QVariantList delivered;
    for (const auto& greeting : d->greetings) {
        if (greeting) {
            ++alive;
            auto record = widgetGeometry(greeting);
            record.insert(QStringLiteral("name"), greeting->objectName());
            record.insert(QStringLiteral("text"), greeting->text());
            delivered.append(record);
        }
    }
    return {{QStringLiteral("registered"), isReady()},
            {QStringLiteral("active"), d->manager.isActive(kPluginId)},
            {QStringLiteral("activations"), d->plugin.activations},
            {QStringLiteral("deactivations"), d->plugin.deactivations},
            {QStringLiteral("created"), d->greetings.size()},
            {QStringLiteral("alive"), alive},
            {QStringLiteral("rejectedFactories"), d->rejectedFactories},
            {QStringLiteral("greetingClicks"), d->greetingClicks},
            {QStringLiteral("error"), d->error},
            {QStringLiteral("activateButton"), widgetGeometry(d->activation)},
            {QStringLiteral("createButton"), widgetGeometry(d->creation)},
            {QStringLiteral("greetings"), delivered}};
}

bool PluginExtensionWindow::runSmokeTest()
{
    if (!isReady() || !setExtensionActive(true))
        return false;
    const QPointer<QWidget> first(createGreeting());
    if (!first || !setExtensionActive(false) || !first || createGreeting())
        return false;
    auto* retained = qobject_cast<fluent::basicinput::Button*>(first.data());
    if (!retained)
        return false;
    retained->click();
    if (retained->text() != QStringLiteral("Hello, you!") || d->greetingClicks != 1)
        return false;
    if (!setExtensionActive(true) || !first)
        return false;
    const QPointer<QWidget> second(createGreeting());
    if (!second || first == second || !setExtensionActive(false))
        return false;
    return first && second && !d->manager.isActive(kPluginId) && d->plugin.activations == 2 &&
           d->plugin.deactivations == 2 && d->rejectedFactories == 1;
}

void PluginExtensionWindow::updateState()
{
    const bool active = d->manager.isActive(kPluginId);
    d->activation->setText(active ? QStringLiteral("Deactivate extension")
                                  : QStringLiteral("Activate extension"));
    d->activation->setEnabled(isReady());
    d->creation->setEnabled(active);
    d->status->setText(
        active ? QStringLiteral("Extension active")
               : QStringLiteral("Extension inactive; created greetings stay available"));
    emit stateChanged();
}

void PluginExtensionWindow::resizeEvent(QResizeEvent* event)
{
    fluent::windowing::Window::resizeEvent(event);
    emit stateChanged();
}

void PluginExtensionWindow::showEvent(QShowEvent* event)
{
    fluent::windowing::Window::showEvent(event);
    emit stateChanged();
}

} // namespace fluentqt::plugin_extension
