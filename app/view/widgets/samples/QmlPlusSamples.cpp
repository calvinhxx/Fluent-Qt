#include "QmlPlusSamples.h"

#include <QVBoxLayout>

#include "SampleBuilders.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/foundation/QMLPlus.h"
#include "components/layout/Card.h"
#include "components/status_info/ProgressBar.h"
#include "components/textfields/Label.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::basicinput::Slider;
using fluent::basicinput::ToggleSwitch;
using fluent::layout::Card;
using fluent::status_info::ProgressBar;
using fluent::textfields::Label;

// Layout and accessibility metadata belong to the preview host, not the teaching snippet.
// zh_CN: 布局和无障碍元数据由预览宿主负责，不混入特性教学代码。
class QmlPlusExample final : public QWidget, public FluentElement, public QMLPlus {
public:
    explicit QmlPlusExample(QWidget* parent) : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        m_body = new QVBoxLayout(this);
        m_body->setContentsMargins(0, 0, 0, 0);
        m_body->setSpacing(12);
        m_body->setAlignment(Qt::AlignTop);
    }

    Slider* slider(const QString& name, const QString& accessibleName, int value = 40)
    {
        auto* control = new Slider(Qt::Horizontal, this);
        control->setObjectName(name);
        control->setAccessibleName(accessibleName);
        control->setRange(0, 100);
        control->setValue(value);
        control->setMaximumWidth(220);
        m_body->addWidget(control);
        return control;
    }

    ProgressBar* bar(const QString& name, int value = 0)
    {
        auto* control = new ProgressBar(this);
        control->setObjectName(name);
        control->setAccessibleName(QStringLiteral("Result"));
        control->setRange(0, 100);
        control->setValue(value);
        m_body->addWidget(control);
        return control;
    }

    ToggleSwitch* toggle(const QString& name, const QString& on, const QString& off)
    {
        auto* control = new ToggleSwitch(this);
        control->setObjectName(name);
        control->setAccessibleName(on);
        control->setOnContent(on);
        control->setOffContent(off);
        m_body->addWidget(control, 0, Qt::AlignLeft);
        return control;
    }

    Label* label(const QString& name, const QString& text)
    {
        auto* control = new Label(text, this);
        control->setObjectName(name);
        m_body->addWidget(control);
        return control;
    }

    void add(QWidget* control) { m_body->addWidget(control); }

private:
    QVBoxLayout* m_body = nullptr;
};

} // namespace

QVector<GallerySample> qmlPlusSamples(const QString& routeId)
{
    if (routeId != QStringLiteral("foundation-qmlplus"))
        return {};

    using samples::makeSample;
    return {
        makeSample(
            QStringLiteral("qmlplus-binding-one-way"), QStringLiteral("One-way binding"),
            QStringLiteral("Move the slider. The progress bar follows its value."),
            QString::fromUtf8(R"(auto* slider = new Slider(Qt::Horizontal, this);
slider->setRange(0, 100);
slider->setValue(40);
auto* bar = new ProgressBar(this);
bar->setRange(0, 100);
bar->bind("value", slider, "value");)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* slider = example->slider(QStringLiteral("qmlplusOneWaySource"),
                                               QStringLiteral("Source value"));
                auto* bar = example->bar(QStringLiteral("qmlplusOneWayTarget"));
                bar->bind("value", slider, "value");
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-binding-two-way"), QStringLiteral("Two-way binding"),
            QStringLiteral("Flip either switch. Both switches keep the same value."),
            QString::fromUtf8(R"(auto* first = new ToggleSwitch(this);
auto* second = new ToggleSwitch(this);
first->bind("isOn", second, "isOn", PropertyBinder::TwoWay);)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* first =
                    example->toggle(QStringLiteral("qmlplusTwoWayFirst"),
                                    QStringLiteral("First: on"), QStringLiteral("First: off"));
                auto* second =
                    example->toggle(QStringLiteral("qmlplusTwoWaySecond"),
                                    QStringLiteral("Second: on"), QStringLiteral("Second: off"));
                first->bind("isOn", second, "isOn", PropertyBinder::TwoWay);
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-binding-computed"), QStringLiteral("Computed binding"),
            QStringLiteral("Move either slider. One expression reads both inputs and updates "
                           "the sum."),
            QString::fromUtf8(R"(auto* first = new Slider(Qt::Horizontal, this);
first->setRange(0, 100);
first->setValue(20);
auto* second = new Slider(Qt::Horizontal, this);
second->setRange(0, 100);
second->setValue(30);
auto* result = new Label(this);
result->bindComputed("text", {{first, "value"}, {second, "value"}},
    [first, second] {
        return QString("%1 + %2 = %3")
            .arg(first->value()).arg(second->value())
            .arg(first->value() + second->value());
    });)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* first = example->slider(QStringLiteral("qmlplusComputedFirst"),
                                              QStringLiteral("First value"), 20);
                auto* second = example->slider(QStringLiteral("qmlplusComputedSecond"),
                                               QStringLiteral("Second value"), 30);
                auto* result = example->label(QStringLiteral("qmlplusComputedResult"), QString());
                result->bindComputed("text", {{first, "value"}, {second, "value"}},
                                     [first, second] {
                                         return QStringLiteral("%1 + %2 = %3")
                                             .arg(first->value())
                                             .arg(second->value())
                                             .arg(first->value() + second->value());
                                     });
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-binding-pause"), QStringLiteral("Pause and resume a binding"),
            QStringLiteral("Pause, move the slider, then resume. The bar holds its value while "
                           "paused and catches up on resume."),
            QString::fromUtf8(R"(auto* slider = new Slider(Qt::Horizontal, this);
slider->setRange(0, 100);
slider->setValue(40);
auto* bar = new ProgressBar(this);
auto* pause = new ToggleSwitch(this);
auto binding = bar->bindManaged("value", slider, "value");
connect(pause, &ToggleSwitch::toggled, this, [binding](bool on) {
    if (on)
        binding.pause();
    else
        binding.resume();
});)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* slider = example->slider(QStringLiteral("qmlplusPauseSource"),
                                               QStringLiteral("Source value"));
                auto* bar = example->bar(QStringLiteral("qmlplusPauseTarget"));
                auto* pause = example->toggle(QStringLiteral("qmlplusPauseToggle"),
                                              QStringLiteral("Paused"), QStringLiteral("Live"));
                auto binding = bar->bindManaged("value", slider, "value");
                QObject::connect(pause, &ToggleSwitch::toggled, example, [binding](bool on) {
                    if (on)
                        binding.pause();
                    else
                        binding.resume();
                });
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-state-manual"), QStringLiteral("Named state"),
            QStringLiteral("Turn the switch on to enter a state. Turn it off to restore the "
                           "original text."),
            QString::fromUtf8(R"(auto* label = new Label("Default", this);
auto* toggle = new ToggleSwitch(this);
QMLState active;
active.name = "active";
active.changes = {{label, "text", "Active"}};
addState(active);
connect(toggle, &ToggleSwitch::toggled, this, [this](bool on) {
    setState(on ? "active" : QString());
});)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* label = example->label(QStringLiteral("qmlplusManualResult"),
                                             QStringLiteral("Default"));
                auto* toggle = example->toggle(QStringLiteral("qmlplusManualToggle"),
                                               QStringLiteral("Active"), QStringLiteral("Default"));
                QMLState active;
                active.name = QStringLiteral("active");
                active.changes = {{label, "text", QStringLiteral("Active")}};
                example->addState(active);
                QObject::connect(toggle, &ToggleSwitch::toggled, example, [example](bool on) {
                    example->setState(on ? QStringLiteral("active") : QString());
                });
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-state-conditional"), QStringLiteral("Conditional state"),
            QStringLiteral("The same text change, selected automatically by when. The dependency "
                           "tells QMLPlus when to reevaluate the condition."),
            QString::fromUtf8(R"(auto* label = new Label("Default", this);
auto* toggle = new ToggleSwitch(this);
QMLState active;
active.name = "active";
active.changes = {{label, "text", "Active"}};
active.when = [toggle] { return toggle->isOn(); };
active.dependencies = {{toggle, "isOn"}};
addState(active);)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* label = example->label(QStringLiteral("qmlplusConditionalResult"),
                                             QStringLiteral("Default"));
                auto* toggle = example->toggle(QStringLiteral("qmlplusConditionalToggle"),
                                               QStringLiteral("Active"), QStringLiteral("Default"));
                QMLState active;
                active.name = QStringLiteral("active");
                active.changes = {{label, "text", QStringLiteral("Active")}};
                active.when = [toggle] { return toggle->isOn(); };
                active.dependencies = {{toggle, "isOn"}};
                example->addState(active);
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-state-inheritance"), QStringLiteral("State inheritance"),
            QStringLiteral("The saving state inherits the disabled button from busy, then changes "
                           "only the caption."),
            QString::fromUtf8(R"(auto* button = new Button("Ready", this);
auto* toggle = new ToggleSwitch(this);
QMLState busy;
busy.name = "busy";
busy.changes = {{button, "enabled", false}};
addState(busy);
QMLState saving;
saving.name = "saving";
saving.extend = "busy";
saving.changes = {{button, "text", "Saving..."}};
addState(saving);
connect(toggle, &ToggleSwitch::toggled, this, [this](bool on) {
    setState(on ? "saving" : QString());
});)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* button = new Button(QStringLiteral("Ready"), example);
                button->setObjectName(QStringLiteral("qmlplusInheritedTarget"));
                example->add(button);
                auto* toggle = example->toggle(QStringLiteral("qmlplusInheritedToggle"),
                                               QStringLiteral("Saving"), QStringLiteral("Ready"));
                QMLState busy;
                busy.name = QStringLiteral("busy");
                busy.changes = {{button, "enabled", false}};
                example->addState(busy);
                QMLState saving;
                saving.name = QStringLiteral("saving");
                saving.extend = QStringLiteral("busy");
                saving.changes = {{button, "text", QStringLiteral("Saving...")}};
                example->addState(saving);
                QObject::connect(toggle, &ToggleSwitch::toggled, example, [example](bool on) {
                    example->setState(on ? QStringLiteral("saving") : QString());
                });
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-state-binding-restore"),
            QStringLiteral("Restore a binding after a state"),
            QStringLiteral("Override the bar to 100, then move the slider. Leaving the state "
                           "restores the binding and reads the latest slider value."),
            QString::fromUtf8(R"(auto* slider = new Slider(Qt::Horizontal, this);
slider->setRange(0, 100);
slider->setValue(40);
auto* bar = new ProgressBar(this);
bar->bind("value", slider, "value");
auto* toggle = new ToggleSwitch(this);
QMLState override;
override.name = "override";
override.changes = {{bar, "value", 100.0}};
addState(override);
connect(toggle, &ToggleSwitch::toggled, this, [this](bool on) {
    setState(on ? "override" : QString());
});)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* slider = example->slider(QStringLiteral("qmlplusRestoreSource"),
                                               QStringLiteral("Source value"));
                auto* bar = example->bar(QStringLiteral("qmlplusRestoreTarget"));
                bar->bind("value", slider, "value");
                auto* toggle = example->toggle(QStringLiteral("qmlplusRestoreToggle"),
                                               QStringLiteral("Override: 100"),
                                               QStringLiteral("Follow slider"));
                QMLState override;
                override.name = QStringLiteral("override");
                override.changes = {{bar, "value", 100.0}};
                example->addState(override);
                QObject::connect(toggle, &ToggleSwitch::toggled, example, [example](bool on) {
                    example->setState(on ? QStringLiteral("override") : QString());
                });
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-behavior"), QStringLiteral("Property animation: Behavior"),
            QStringLiteral("The one-way binding with one addition: value changes animate for "
                           "300 ms. Move the slider to see the bar catch up."),
            QString::fromUtf8(R"(auto* slider = new Slider(Qt::Horizontal, this);
slider->setRange(0, 100);
slider->setValue(40);
auto* bar = new ProgressBar(this);
PropertyAnimation motion;
motion.duration = 300;
bar->setBehavior("value", motion);
bar->bind("value", slider, "value");)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* slider = example->slider(QStringLiteral("qmlplusBehaviorSource"),
                                               QStringLiteral("Source value"));
                auto* bar = example->bar(QStringLiteral("qmlplusBehaviorTarget"));
                PropertyAnimation motion;
                motion.duration = 300;
                bar->setBehavior("value", motion);
                bar->bind("value", slider, "value");
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-transition"), QStringLiteral("State animation: Transition"),
            QStringLiteral("Enter and leave the high state. Transition animates the state's "
                           "value change between 20 and 80."),
            QString::fromUtf8(R"(auto* bar = new ProgressBar(this);
bar->setValue(20);
auto* toggle = new ToggleSwitch(this);
QMLState high;
high.name = "high";
high.changes = {{bar, "value", 80.0}};
addState(high);
StateTransition transition;
transition.properties = {"value"};
transition.animation.duration = 300;
addTransition(transition);
connect(toggle, &ToggleSwitch::toggled, this, [this](bool on) {
    setState(on ? "high" : QString());
});)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* bar = example->bar(QStringLiteral("qmlplusTransitionTarget"), 20);
                auto* toggle =
                    example->toggle(QStringLiteral("qmlplusTransitionToggle"),
                                    QStringLiteral("High: 80"), QStringLiteral("Default: 20"));
                QMLState high;
                high.name = QStringLiteral("high");
                high.changes = {{bar, "value", 80.0}};
                example->addState(high);
                StateTransition transition;
                transition.properties = {"value"};
                transition.animation.duration = 300;
                example->addTransition(transition);
                QObject::connect(toggle, &ToggleSwitch::toggled, example, [example](bool on) {
                    example->setState(on ? QStringLiteral("high") : QString());
                });
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-anchors-center"), QStringLiteral("Center anchors"),
            QStringLiteral("The button stays centered when the preview is resized. Each center "
                           "line has one anchor."),
            QString::fromUtf8(R"(auto* surface = new Card(this);
surface->setMinimumHeight(104);
auto* layout = new AnchorLayout(surface);
auto* button = new Button("Centered", surface);
AnchorLayout::Anchors center;
center.horizontalCenter = {surface, AnchorLayout::Edge::HCenter};
center.verticalCenter = {surface, AnchorLayout::Edge::VCenter};
layout->addAnchoredWidget(button, center);)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* surface = new Card(example);
                surface->setObjectName(QStringLiteral("qmlplusAnchorSurface"));
                surface->setMinimumHeight(104);
                auto* layout = new AnchorLayout(surface);
                auto* button = new Button(QStringLiteral("Centered"), surface);
                button->setObjectName(QStringLiteral("qmlplusAnchorTarget"));
                AnchorLayout::Anchors center;
                center.horizontalCenter = {surface, AnchorLayout::Edge::HCenter};
                center.verticalCenter = {surface, AnchorLayout::Edge::VCenter};
                layout->addAnchoredWidget(button, center);
                example->add(surface);
                return example;
            },
            true),
        makeSample(
            QStringLiteral("qmlplus-anchors-state"), QStringLiteral("Change anchors in a state"),
            QStringLiteral("Switch between left and right. The state replaces the button's "
                           "anchors; leaving it restores the original anchors."),
            QString::fromUtf8(R"(auto* surface = new Card(this);
surface->setMinimumHeight(104);
auto* layout = new AnchorLayout(surface);
auto* button = new Button("Move me", surface);
auto* toggle = new ToggleSwitch(this);
using Edge = AnchorLayout::Edge;
AnchorLayout::Anchors left;
left.left = {surface, Edge::Left, 12};
left.verticalCenter = {surface, Edge::VCenter};
layout->addAnchoredWidget(button, left);
AnchorLayout::Anchors right;
right.right = {surface, Edge::Right, -12};
right.verticalCenter = {surface, Edge::VCenter};
QMLState moved;
moved.name = "right";
moved.anchorChanges = {{button, right}};
addState(moved);
connect(toggle, &ToggleSwitch::toggled, this, [this](bool on) {
    setState(on ? "right" : QString());
});)"),
            [](QWidget* parent) -> QWidget* {
                auto* example = new QmlPlusExample(parent);
                auto* surface = new Card(example);
                surface->setObjectName(QStringLiteral("qmlplusStateAnchorSurface"));
                surface->setMinimumHeight(104);
                auto* layout = new AnchorLayout(surface);
                auto* button = new Button(QStringLiteral("Move me"), surface);
                button->setObjectName(QStringLiteral("qmlplusStateAnchorTarget"));
                example->add(surface);
                auto* toggle = example->toggle(QStringLiteral("qmlplusStateAnchorToggle"),
                                               QStringLiteral("Right"), QStringLiteral("Left"));
                using Edge = AnchorLayout::Edge;
                AnchorLayout::Anchors left;
                left.left = {surface, Edge::Left, 12};
                left.verticalCenter = {surface, Edge::VCenter};
                layout->addAnchoredWidget(button, left);
                AnchorLayout::Anchors right;
                right.right = {surface, Edge::Right, -12};
                right.verticalCenter = {surface, Edge::VCenter};
                QMLState moved;
                moved.name = QStringLiteral("right");
                moved.anchorChanges = {{button, right}};
                example->addState(moved);
                QObject::connect(toggle, &ToggleSwitch::toggled, example, [example](bool on) {
                    example->setState(on ? QStringLiteral("right") : QString());
                });
                return example;
            },
            true),
    };
}

} // namespace fluent::gallery
