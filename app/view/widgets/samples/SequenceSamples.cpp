#include "SequenceSamples.h"
#include <QStandardItemModel>
#include <QVBoxLayout>
#include "components/basicinput/Button.h"
#include "components/collections/Timeline.h"
#include "components/navigation/Stepper.h"
#include "components/textfields/Label.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {
using collections::Timeline;
using navigation::Stepper;
using navigation::StepperItem;
using basicinput::Button;
using samples::makeSample;
using samples::verticalGroup;

QWidget* timelineExample(QWidget* parent, Timeline::NodeAlignment alignment)
{
    auto* group = verticalGroup(parent, 12);
    group->layout()->setAlignment(Qt::AlignTop);
    auto* timeline = new Timeline(group);
    timeline->setObjectName(QStringLiteral("timelineExample"));
    timeline->setAccessibleName(QStringLiteral("Timeline example"));
    timeline->setNodeAlignment(alignment);
    timeline->setMinimumWidth(240);
    timeline->setMinimumHeight(320);
    timeline->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* model = new QStandardItemModel(3, 1, group);
    for (int i = 0; i < 3; ++i) {
        const auto index = model->index(i, 0);
        model->setData(index, QStringLiteral("Node %1").arg(i + 1));
        model->setData(index,
                       QStringLiteral("Optional supporting content supplied by the application."),
                       Timeline::DescriptionRole);
        model->setData(index, QStringLiteral("Time label %1").arg(i + 1), Timeline::TimestampRole);
    }
    model->setData(model->index(1, 0), Timeline::Active, Timeline::StatusRole);
    timeline->setModel(model);
    auto* change = new Button(QStringLiteral("Change node state"), group);
    change->setObjectName(QStringLiteral("timelineChangeState"));
    QObject::connect(change, &Button::clicked, model, [model] {
        const auto index = model->index(1, 0);
        const int status = model->data(index, Timeline::StatusRole).toInt();
        model->setData(index, status == Timeline::Active ? Timeline::Success : Timeline::Active,
                       Timeline::StatusRole);
    });
    group->layout()->addWidget(timeline);
    group->layout()->addWidget(change);
    return group;
}
QWidget* stepperExample(QWidget* parent, Qt::Orientation axis)
{
    auto* group = verticalGroup(parent, 12);
    group->layout()->setAlignment(Qt::AlignTop);
    auto* stepper = new Stepper(group);
    stepper->setObjectName(QStringLiteral("stepperExample"));
    stepper->setAccessibleName(QStringLiteral("Stepper example"));
    stepper->setOrientation(axis);
    for (int i = 0; i < 4; ++i) {
        StepperItem item(QStringLiteral("Step %1").arg(i + 1),
                         QStringLiteral("Optional description"));
        if (i == 0)
            item.state = Stepper::Completed;
        if (i == 3)
            item.enabled = false;
        stepper->addItem(item);
    }
    stepper->setCurrentIndex(1);
    stepper->setMinimumWidth(240);
    stepper->setMinimumHeight(axis == Qt::Horizontal ? 120 : 280);
    // The sample accepts requests; a real application can validate first.
    // zh_CN: 示例直接接受请求；实际应用可先执行校验。
    QObject::connect(stepper, &Stepper::stepRequested, stepper, &Stepper::setCurrentIndex);
    auto* status = new textfields::Label(QStringLiteral("Current step: 2"), group);
    status->setTextColorRole(textfields::Label::TextColorRole::Primary);
    QObject::connect(stepper, &Stepper::currentIndexChanged, status, [status](int index) {
        status->setText(QStringLiteral("Current step: %1").arg(index + 1));
    });
    auto* toggle = new Button(QStringLiteral("Toggle step 2 error"), group);
    toggle->setObjectName(QStringLiteral("stepperToggleError"));
    QObject::connect(toggle, &Button::clicked, stepper, [stepper] {
        stepper->setItemState(1, stepper->itemAt(1).state == Stepper::Error ? Stepper::Pending
                                                                            : Stepper::Error);
    });
    group->layout()->addWidget(stepper);
    group->layout()->addWidget(status);
    group->layout()->addWidget(toggle);
    return group;
}
} // namespace

QVector<GallerySample> timelineSamples()
{
    return {makeSample(
                QStringLiteral("timeline-basic"), QStringLiteral("Model-backed timeline"),
                QStringLiteral("The application supplies text, time labels and node states. Click "
                               "the button to see a state transition."),
                QStringLiteral(
                    "auto* timeline = new Timeline(this);\n"
                    "auto* model = new QStandardItemModel(3, 1, this);\n"
                    "for (int i = 0; i < 3; ++i) {\n"
                    "    model->setData(model->index(i, 0), QString(\"Node %1\").arg(i + 1));\n"
                    "    model->setData(model->index(i, 0), \"Optional supporting content supplied "
                    "by the application.\", Timeline::DescriptionRole);\n"
                    "    model->setData(model->index(i, 0), QString(\"Time label %1\").arg(i + 1), "
                    "Timeline::TimestampRole);\n"
                    "}\n"
                    "model->setData(model->index(1, 0), Timeline::Active, Timeline::StatusRole);\n"
                    "timeline->setModel(model);\n"
                    "connect(changeState, &Button::clicked, model, [model] {\n"
                    "    const auto index = model->index(1, 0);\n"
                    "    model->setData(index, model->data(index, Timeline::StatusRole).toInt() == "
                    "Timeline::Active\n"
                    "        ? Timeline::Success : Timeline::Active, Timeline::StatusRole);\n"
                    "});\n"),
                [](QWidget* parent) { return timelineExample(parent, Timeline::Leading); }, true),
            makeSample(
                QStringLiteral("timeline-alternate"), QStringLiteral("Alternating content"),
                QStringLiteral("The same model can place content on alternating sides of the rail; "
                               "RTL mirrors its logical layout."),
                QStringLiteral(
                    "auto* timeline = new Timeline(this);\n"
                    "auto* model = new QStandardItemModel(3, 1, this);\n"
                    "for (int i = 0; i < 3; ++i) {\n"
                    "    model->setData(model->index(i, 0), QString(\"Node %1\").arg(i + 1));\n"
                    "    model->setData(model->index(i, 0), \"Optional supporting content supplied "
                    "by the application.\", Timeline::DescriptionRole);\n"
                    "    model->setData(model->index(i, 0), QString(\"Time label %1\").arg(i + 1), "
                    "Timeline::TimestampRole);\n"
                    "}\n"
                    "model->setData(model->index(1, 0), Timeline::Active, Timeline::StatusRole);\n"
                    "timeline->setModel(model);\n"
                    "connect(changeState, &Button::clicked, model, [model] {\n"
                    "    const auto index = model->index(1, 0);\n"
                    "    model->setData(index, model->data(index, Timeline::StatusRole).toInt() == "
                    "Timeline::Active\n"
                    "        ? Timeline::Success : Timeline::Active, Timeline::StatusRole);\n"
                    "});\n") +
                    QStringLiteral("timeline->setNodeAlignment(Timeline::Alternate);\n"),
                [](QWidget* parent) { return timelineExample(parent, Timeline::Alternate); },
                true)};
}
QVector<GallerySample> stepperSamples()
{
    return {
        makeSample(
            QStringLiteral("stepper-horizontal"), QStringLiteral("Application-controlled steps"),
            QStringLiteral("Click a step to request navigation. Completion, errors and "
                           "availability stay independent of the current step."),
            QStringLiteral(
                "auto* stepper = new Stepper(this);\n"
                "for (int i = 0; i < 4; ++i) {\n"
                "    StepperItem item(QString(\"Step %1\").arg(i + 1), \"Optional description\");\n"
                "    if (i == 0) item.state = Stepper::Completed;\n"
                "    if (i == 3) item.enabled = false;\n"
                "    stepper->addItem(item);\n"
                "}\n"
                "stepper->setCurrentIndex(1);\n"
                "connect(stepper, &Stepper::stepRequested, stepper, &Stepper::setCurrentIndex);\n"
                "connect(stepper, &Stepper::currentIndexChanged, statusLabel, [statusLabel](int i) "
                "{\n"
                "    statusLabel->setText(QString(\"Current step: %1\").arg(i + 1));\n"
                "});\n"
                "connect(toggleError, &Button::clicked, stepper, [stepper] {\n"
                "    stepper->setItemState(1, stepper->itemAt(1).state == Stepper::Error ? "
                "Stepper::Pending : Stepper::Error);\n"
                "});\n"),
            [](QWidget* parent) { return stepperExample(parent, Qt::Horizontal); }, true),
        makeSample(
            QStringLiteral("stepper-vertical"), QStringLiteral("Vertical steps"),
            QStringLiteral("The same steps use a vertical rail. Arrow keys move focus; Enter or "
                           "Space requests navigation."),
            QStringLiteral(
                "auto* stepper = new Stepper(this);\n"
                "for (int i = 0; i < 4; ++i) {\n"
                "    StepperItem item(QString(\"Step %1\").arg(i + 1), \"Optional description\");\n"
                "    if (i == 0) item.state = Stepper::Completed;\n"
                "    if (i == 3) item.enabled = false;\n"
                "    stepper->addItem(item);\n"
                "}\n"
                "stepper->setCurrentIndex(1);\n"
                "connect(stepper, &Stepper::stepRequested, stepper, &Stepper::setCurrentIndex);\n"
                "connect(stepper, &Stepper::currentIndexChanged, statusLabel, [statusLabel](int i) "
                "{\n"
                "    statusLabel->setText(QString(\"Current step: %1\").arg(i + 1));\n"
                "});\n"
                "connect(toggleError, &Button::clicked, stepper, [stepper] {\n"
                "    stepper->setItemState(1, stepper->itemAt(1).state == Stepper::Error ? "
                "Stepper::Pending : Stepper::Error);\n"
                "});\n") +
                QStringLiteral("stepper->setOrientation(Qt::Vertical);\n"),
            [](QWidget* parent) { return stepperExample(parent, Qt::Vertical); }, true)};
}
} // namespace fluent::gallery
