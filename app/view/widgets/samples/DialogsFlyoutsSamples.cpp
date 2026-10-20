#include "DialogsFlyoutsSamples.h"

#include <QBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPointer>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <memory>

#include "SampleBuilders.h"
#include "OverlayEffectsSample.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/dialogs_flyouts/CoachMark.h"
#include "components/dialogs_flyouts/ContentDialog.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/dialogs_flyouts/TeachingTip.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "design/Typography.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::basicinput::CheckBox;
using fluent::basicinput::ToggleSwitch;
using fluent::dialogs_flyouts::CoachMark;
using fluent::dialogs_flyouts::ContentDialog;
using fluent::dialogs_flyouts::Dialog;
using fluent::dialogs_flyouts::Flyout;
using fluent::dialogs_flyouts::Popup;
using fluent::dialogs_flyouts::TeachingTip;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

class DialogsFlyoutsSampleSurface : public QWidget, public fluent::FluentElement {
public:
    explicit DialogsFlyoutsSampleSurface(QWidget* parent = nullptr, int spacing = 12) : QWidget(parent)
    {
        setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 14, 16, 16);
        layout->setSpacing(spacing);
        layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(themeColors().strokeCard);
        painter.setBrush(themeColors().bgCanvas);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), themeRadius().overlay, themeRadius().overlay);
    }
};

QWidget* sampleSurface(QWidget* parent, int spacing = 12)
{
    return new DialogsFlyoutsSampleSurface(parent, spacing);
}

QBoxLayout* boxLayout(QWidget* widget)
{
    return qobject_cast<QBoxLayout*>(widget->layout());
}

Label* makeBodyLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setWordWrap(true);
    // Color via the label's own style sheet so the styled sample preview surface can't drop it (it
    // would otherwise render near-black in dark theme). zh_CN: 用标签自身样式表上色,避免带样式表的预览面丢色(深色下近黑)。
    label->setTextColorRole(Label::TextColorRole::Primary);
    return label;
}

Label* makeStatusLabel(QWidget* parent, const QString& text)
{
    auto* label = makeBodyLabel(parent, text);
    label->setMinimumWidth(220);
    return label;
}

Label* makeTitleLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::BodyStrong);
    label->setTextColorRole(Label::TextColorRole::Primary); // QSS-proof on the styled preview surface
    return label;
}

Button* sampleButton(QWidget* parent, const QString& text)
{
    auto* button = new Button(text, parent);
    button->setFluentSize(Button::Small);
    button->setMinimumWidth(96);
    return button;
}

template <typename DialogType> DialogType* themedDialog(DialogType* dialog, QWidget* source)
{
    if (dialog)
        dialog->setThemeSource(source);
    return dialog;
}

QString contentDialogResultText(int result)
{
    if (result == ContentDialog::ResultPrimary)
        return QStringLiteral("Primary");
    if (result == ContentDialog::ResultSecondary)
        return QStringLiteral("Secondary");
    return QStringLiteral("Close");
}

QString closeReasonText(TeachingTip::CloseReason reason)
{
    switch (reason) {
    case TeachingTip::ActionButton:
        return QStringLiteral("action button");
    case TeachingTip::CloseButton:
        return QStringLiteral("close button");
    case TeachingTip::LightDismiss:
        return QStringLiteral("light dismiss");
    case TeachingTip::TargetDestroyed:
        return QStringLiteral("target destroyed");
    case TeachingTip::Programmatic:
    default:
        return QStringLiteral("programmatic");
    }
}

QString flyoutPlacementText(Flyout::Placement placement)
{
    switch (placement) {
    case Flyout::Top:
        return QStringLiteral("Top");
    case Flyout::Bottom:
        return QStringLiteral("Bottom");
    case Flyout::Left:
        return QStringLiteral("Left");
    case Flyout::Right:
        return QStringLiteral("Right");
    case Flyout::Full:
        return QStringLiteral("Full");
    case Flyout::Auto:
    default:
        return QStringLiteral("Auto");
    }
}

QString coachMarkPlacementText(CoachMark::Placement placement)
{
    switch (placement) {
    case CoachMark::Top:
        return QStringLiteral("Top");
    case CoachMark::Bottom:
        return QStringLiteral("Bottom");
    case CoachMark::Left:
        return QStringLiteral("Left");
    case CoachMark::Right:
        return QStringLiteral("Right");
    case CoachMark::Auto:
    default:
        return QStringLiteral("Auto");
    }
}

QString teachingTipPlacementText(TeachingTip::PreferredPlacement placement)
{
    switch (placement) {
    case TeachingTip::Top:
        return QStringLiteral("Top");
    case TeachingTip::RightTop:
        return QStringLiteral("RightTop");
    case TeachingTip::Bottom:
        return QStringLiteral("Bottom");
    case TeachingTip::Auto:
    default:
        return QStringLiteral("Auto");
    }
}

void addDialogButtons(QVBoxLayout* layout, Dialog* dialog, Label* status)
{
    auto* buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    buttonRow->addStretch(1);

    auto* applyButton = new Button(QStringLiteral("Apply"), dialog);
    applyButton->setFluentStyle(Button::Accent);
    auto* cancelButton = new Button(QStringLiteral("Cancel"), dialog);

    QObject::connect(applyButton, &Button::clicked, dialog, [dialog]() { dialog->done(1); });
    QObject::connect(cancelButton, &Button::clicked, dialog, [dialog]() { dialog->done(0); });

    buttonRow->addWidget(applyButton);
    buttonRow->addWidget(cancelButton);
    layout->addLayout(buttonRow);

    QObject::connect(dialog, &QDialog::finished, status, [status](int result) {
        status->setText(result == 1 ? QStringLiteral("Dialog result: Apply") : QStringLiteral("Dialog result: Cancel"));
    });
}

void populateFlyout(Flyout* flyout, const QString& title, const QString& body)
{
    flyout->setMinimumSize(340, 188);
    auto* layout = new QVBoxLayout(flyout);
    layout->setContentsMargins(24, 22, 24, 22);
    layout->setSpacing(10);
    layout->addWidget(makeTitleLabel(flyout, title));
    layout->addWidget(makeBodyLabel(flyout, body));
    layout->addStretch(1);
    auto* closeButton = sampleButton(flyout, QStringLiteral("Close"));
    QObject::connect(closeButton, &Button::clicked, flyout, [flyout]() { flyout->close(); });
    layout->addWidget(closeButton, 0, Qt::AlignRight);
}

void populateTeachingTip(TeachingTip* tip, const QString& titleText, const QString& bodyText, Label* status)
{
    auto* host = tip->contentHost();
    host->setObjectName(QStringLiteral("teachingTipContentHost"));
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* titleRow = new QHBoxLayout;
    titleRow->setSpacing(8);
    titleRow->addWidget(makeTitleLabel(host, titleText));
    titleRow->addStretch(1);

    auto* closeButton = new Button(QString(), host);
    closeButton->setObjectName(QStringLiteral("teachingTipCloseButton"));
    closeButton->setAccessibleName(tip->tr("Close tip"));
    closeButton->setFluentLayout(Button::IconOnly);
    closeButton->setFluentStyle(Button::Subtle);
    closeButton->setIconGlyph(Typography::Icons::ChromeClose, Typography::IconSize::Standard);
    closeButton->setFixedSize(30, 30);
    QObject::connect(closeButton, &Button::clicked, tip, [tip]() { tip->closeWithReason(TeachingTip::CloseButton); });
    titleRow->addWidget(closeButton);
    layout->addLayout(titleRow);

    layout->addWidget(makeBodyLabel(host, bodyText));
    layout->addStretch(1);

    auto* actionButton = sampleButton(host, QStringLiteral("Got it"));
    actionButton->setFluentStyle(Button::Accent);
    QObject::connect(actionButton, &Button::clicked, tip, [tip]() { tip->closeWithReason(TeachingTip::ActionButton); });
    layout->addWidget(actionButton, 0, Qt::AlignRight);

    QObject::connect(tip, &TeachingTip::closing, status, [status](TeachingTip::CloseReason reason) {
        status->setText(QStringLiteral("Closed by %1").arg(closeReasonText(reason)));
    });
}

QVector<GallerySample> contentDialogSamples()
{
    return {
        makeSample(QStringLiteral("content-dialog-result-buttons"),
            QStringLiteral("Result buttons and default command"),
            QStringLiteral("Primary, secondary, and close buttons update app state while "
                           "the primary command is the default action."),
            QStringLiteral(
                "auto* dialog = new ContentDialog(window());\n"
                "dialog->setTitle(\"Save your work?\");\n"
                "dialog->setPrimaryButtonText(\"Save\");\n"
                "dialog->setSecondaryButtonText(\"Don't save\");\n"
                "dialog->setCloseButtonText(\"Cancel\");\n"
                "dialog->setDefaultButton(ContentDialog::Primary);\n"
                "connect(dialog, &ContentDialog::primaryButtonClicked, status,\n"
                "        [status] { status->setText(\"Result: Save\"); });\n"
                "connect(dialog, &QDialog::finished, dialog,\n"
                "        &QObject::deleteLater);\n"
                "dialog->open();"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* row = horizontalGroup(surface, 12);
                auto* button = sampleButton(row, QStringLiteral("Show dialog"));
                button->setMinimumWidth(118);
                auto* status = makeStatusLabel(row, QStringLiteral("Result: not shown"));

                QObject::connect(button, &Button::clicked, button, [button, status]() {
                    auto* dialog = themedDialog(new ContentDialog(button->window()), button);
                    dialog->setTitle(QStringLiteral("Save your work?"));
                    dialog->setContent(makeBodyLabel(nullptr,
                        QStringLiteral(
                            "Unsaved changes in \"Quarterly report\" "
                            "will be lost unless you save them.")));
                    dialog->setPrimaryButtonText(QStringLiteral("Save"));
                    dialog->setSecondaryButtonText(QStringLiteral("Don't save"));
                    dialog->setCloseButtonText(QStringLiteral("Cancel"));
                    dialog->setDefaultButton(ContentDialog::Primary);

                    QObject::connect(dialog, &ContentDialog::primaryButtonClicked, status, [status]() {
                        status->setText(QStringLiteral("Result: Save"));
                    });
                    QObject::connect(dialog, &ContentDialog::secondaryButtonClicked, status, [status]() {
                        status->setText(QStringLiteral("Result: Don't save"));
                    });
                    QObject::connect(dialog, &ContentDialog::closeButtonClicked, status, [status]() {
                        status->setText(QStringLiteral("Result: Cancel"));
                    });

                    QObject::connect(dialog, &QDialog::finished, status, [status](int result) {
                        if (status->text() == QStringLiteral("Result: not shown")) {
                            status->setText(QStringLiteral("Result: %1").arg(contentDialogResultText(result)));
                        }
                    });
                    QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
                    dialog->open();
                });

                boxLayout(row)->addWidget(button);
                boxLayout(row)->addWidget(status);
                boxLayout(surface)->addWidget(row);
                return surface;
            }),
        makeSample(QStringLiteral("content-dialog-custom-content"),
            QStringLiteral("Custom content and command visibility"),
            QStringLiteral("Only non-empty command text is shown, so the same "
                           "dialog can host lightweight confirmation content."),
            QStringLiteral(
                "auto* dialog = new ContentDialog(window());\n"
                "dialog->setTitle(\"Share draft?\");\n"
                "dialog->setContent(customContent);\n"
                "dialog->setPrimaryButtonText(\"Share\");\n"
                "dialog->setCloseButtonText(\"Not now\");\n"
                "dialog->setDefaultButton(ContentDialog::None);\n"
                "connect(dialog, &ContentDialog::primaryButtonClicked, status,\n"
                "        [status] { status->setText(\"Draft shared\"); });\n"
                "connect(dialog, &QDialog::finished, dialog,\n"
                "        &QObject::deleteLater);\n"
                "dialog->open();"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* row = horizontalGroup(surface, 12);
                auto* button = sampleButton(row, QStringLiteral("Share draft"));
                button->setMinimumWidth(118);
                auto* status = makeStatusLabel(row, QStringLiteral("Draft state: private"));

                QObject::connect(button, &Button::clicked, button, [button, status]() {
                    auto* content = verticalGroup(nullptr, 8);
                    content->setMinimumWidth(360);
                    auto* body = makeBodyLabel(content,
                        QStringLiteral(
                            "Invite reviewers and include the latest "
                            "attachments with this draft."));
                    auto* upload = new CheckBox(QStringLiteral("Upload attachments before sharing"), content);
                    upload->setChecked(true);
                    boxLayout(content)->addWidget(body);
                    boxLayout(content)->addWidget(upload);

                    auto* dialog = themedDialog(new ContentDialog(button->window()), button);
                    dialog->setTitle(QStringLiteral("Share draft?"));
                    dialog->setContent(content);
                    dialog->setPrimaryButtonText(QStringLiteral("Share"));
                    dialog->setCloseButtonText(QStringLiteral("Not now"));
                    dialog->setDefaultButton(ContentDialog::None);

                    QObject::connect(dialog, &ContentDialog::primaryButtonClicked, status, [status, upload]() {
                        status->setText(upload->isChecked()
                                ? QStringLiteral("Draft shared with attachments")
                                : QStringLiteral("Draft shared without attachments"));
                    });
                    QObject::connect(dialog, &ContentDialog::closeButtonClicked, status, [status]() {
                        status->setText(QStringLiteral("Draft state: private"));
                    });

                    QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
                    dialog->open();
                });

                boxLayout(row)->addWidget(button);
                boxLayout(row)->addWidget(status);
                boxLayout(surface)->addWidget(row);
                return surface;
            })};
}

QVector<GallerySample> dialogSamples()
{
    QVector<GallerySample> samples{
        makeSample(QStringLiteral("dialog-owned-content"), QStringLiteral("Rename a project"),
            QStringLiteral("Edit a project name, then apply the change or cancel to keep the original."),
            QStringLiteral(R"sample(auto* root = new QWidget(this);
auto* layout = new QVBoxLayout(root);
auto* open = new Button(QStringLiteral("Rename project"), root);
open->setFluentStyle(Button::Accent);
auto* status = new Label(QStringLiteral("Project name: Northwind Analytics"), root);
layout->addWidget(open, 0, Qt::AlignLeft);
layout->addWidget(status);
QObject::connect(open, &Button::clicked, root, [open, status] {
    auto* dialog = new Dialog(open->window());
    dialog->setThemeSource(open);
    dialog->setMinimumSize(480, 280);
    auto* content = new QVBoxLayout(dialog);
    content->setContentsMargins(32, 28, 32, 28);
    content->setSpacing(14);
    auto* title = new Label(QStringLiteral("Rename project"), dialog);
    title->setFluentTypography(Typography::FontRole::Subtitle);
    content->addWidget(title);
    auto* name = new LineEdit(dialog);
    name->setAccessibleName(QStringLiteral("Project name"));
    name->setText(status->text().replace(QStringLiteral("Project name: "), QString()));
    content->addWidget(name);
    content->addStretch();
    auto* commands = new QHBoxLayout;
    auto* cancel = new Button(QStringLiteral("Cancel"), dialog);
    auto* apply = new Button(QStringLiteral("Apply"), dialog);
    apply->setFluentStyle(Button::Accent);
    commands->addStretch();
    commands->addWidget(cancel);
    commands->addWidget(apply);
    content->addLayout(commands);
    QObject::connect(cancel, &Button::clicked, dialog, &QDialog::reject);
    QObject::connect(apply, &Button::clicked, dialog, &QDialog::accept);
    QObject::connect(dialog, &QDialog::accepted, status, [status, name] {
        status->setText(QStringLiteral("Project name: %1").arg(name->text()));
    });
    QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    dialog->open();
});
return root;)sample"),
            [](QWidget* parent) {
                auto* root = new QWidget(parent);
                auto* layout = new QVBoxLayout(root);
                auto* open = new Button(QStringLiteral("Rename project"), root);
                open->setFluentStyle(Button::Accent);
                auto* status = new Label(QStringLiteral("Project name: Northwind Analytics"), root);
                layout->addWidget(open, 0, Qt::AlignLeft);
                layout->addWidget(status);
                QObject::connect(open, &Button::clicked, root, [open, status] {
                    auto* dialog = new Dialog(open->window());
                    dialog->setThemeSource(open);
                    dialog->setMinimumSize(480, 280);
                    auto* content = new QVBoxLayout(dialog);
                    content->setContentsMargins(32, 28, 32, 28);
                    content->setSpacing(14);
                    auto* title = new Label(QStringLiteral("Rename project"), dialog);
                    title->setFluentTypography(Typography::FontRole::Subtitle);
                    content->addWidget(title);
                    auto* name = new LineEdit(dialog);
                    name->setAccessibleName(QStringLiteral("Project name"));
                    name->setText(status->text().replace(QStringLiteral("Project name: "), QString()));
                    content->addWidget(name);
                    content->addStretch();
                    auto* commands = new QHBoxLayout;
                    auto* cancel = new Button(QStringLiteral("Cancel"), dialog);
                    auto* apply = new Button(QStringLiteral("Apply"), dialog);
                    apply->setFluentStyle(Button::Accent);
                    commands->addStretch();
                    commands->addWidget(cancel);
                    commands->addWidget(apply);
                    content->addLayout(commands);
                    QObject::connect(cancel, &Button::clicked, dialog, &QDialog::reject);
                    QObject::connect(apply, &Button::clicked, dialog, &QDialog::accept);
                    QObject::connect(dialog, &QDialog::accepted, status, [status, name] {
                        status->setText(QStringLiteral("Project name: %1").arg(name->text()));
                    });
                    QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
                    dialog->open();
                });
                return root;
            }),
        makeSample(QStringLiteral("dialog-backdrop-effects"), QStringLiteral("Modal and backdrop effects"),
            QStringLiteral("Blur or dim the background, then compare blocked and available background interaction."),
            QStringLiteral(R"sample(using basicinput::Button;
using basicinput::CheckBox;
using basicinput::Slider;
using textfields::Label;
auto* root = new QWidget(this);
auto* layout = new QVBoxLayout(root);
layout->setSpacing(12);
auto* open = new Button(QStringLiteral("Preview background effects"), root);
open->setFluentStyle(Button::Accent);
layout->addWidget(open, 0, Qt::AlignLeft);
auto* hint = new Label(QStringLiteral("Open the overlay to try this action."), root);
hint->setWordWrap(true);
hint->setTextColorRole(Label::TextColorRole::Secondary);
layout->addWidget(hint);
auto* row = new QWidget(root);
auto* rowLayout = new QHBoxLayout(row);
rowLayout->setContentsMargins(0, 0, 0, 0);
rowLayout->setSpacing(12);
auto* counter = new Button(QStringLiteral("Background action"), row);
row->layout()->addWidget(counter);
auto* status = new Label(QStringLiteral("Background clicks: 0"), row);
status->setTextColorRole(Label::TextColorRole::Secondary);
row->layout()->addWidget(status);
layout->addWidget(row);
QObject::connect(counter, &Button::clicked, status, [status, count = 0]() mutable {
    status->setText(QStringLiteral("Background clicks: %1").arg(++count));
});
QObject::connect(open, &Button::clicked, root, [root, open, counter, hint] {
    auto* overlay = new Dialog(root);
    overlay->setModal(true);
    overlay->setDim(true);
    overlay->setDimOpacity(0.12);
    overlay->setBackdropBlurRadius(8);
    overlay->setClosePolicy(Dialog::CloseOnEscape);
    QWidget* body = overlay;
    overlay->setThemeSource(open);
    overlay->setFixedWidth(400);
    auto* content = new QVBoxLayout(body);
    content->setContentsMargins(28, 26, 28, 26);
    content->setSpacing(10);
    auto* title = new Label(QStringLiteral("Modal and backdrop effects"), body);
    title->setFluentTypography(Typography::FontRole::BodyStrong);
    title->setWordWrap(true);
    content->addWidget(title);
    auto* modal = new CheckBox(QStringLiteral("Modal"), body);
    modal->setChecked(true);
    QObject::connect(modal, &QCheckBox::toggled, overlay, &Dialog::setModal);
    const auto explainModal = [hint](bool enabled) {
        hint->setText(enabled
            ? QStringLiteral("Modal on: background blocked.")
            : QStringLiteral("Modal off: background available."));
    };
    QObject::connect(modal, &QCheckBox::toggled, hint, explainModal);
    explainModal(true);
    content->addWidget(modal);
    auto* dimEnabled = new CheckBox(QStringLiteral("Dim background"), body);
    dimEnabled->setChecked(true);
    QObject::connect(dimEnabled, &QCheckBox::toggled, overlay, &Dialog::setDim);
    content->addWidget(dimEnabled);
    auto* outside = new CheckBox(QStringLiteral("Close on outside click"), body);
    QObject::connect(outside, &QCheckBox::toggled, overlay, [overlay](bool enabled) {
        Dialog::ClosePolicy policy(Dialog::CloseOnEscape);
        if (enabled)
            policy |= Dialog::CloseOnPressOutside;
        overlay->setClosePolicy(policy);
    });
    content->addWidget(outside);
    auto* blurLabel = new Label(QStringLiteral("Blur: 8 px"), body);
    auto* blur = new Slider(Qt::Horizontal, body);
    blur->setAccessibleName(QStringLiteral("Background blur radius"));
    blur->setRange(0, 24);
    blur->setValue(8);
    QObject::connect(blur, &QSlider::valueChanged, overlay, [overlay, blurLabel](int value) {
        overlay->setBackdropBlurRadius(value);
        blurLabel->setText(QStringLiteral("Blur: %1 px").arg(value));
    });
    content->addWidget(blurLabel);
    content->addWidget(blur);
    auto* dimLabel = new Label(QStringLiteral("Dim: 12%"), body);
    auto* dim = new Slider(Qt::Horizontal, body);
    dim->setAccessibleName(QStringLiteral("Background dim opacity"));
    dim->setRange(0, 60);
    dim->setValue(12);
    QObject::connect(dim, &QSlider::valueChanged, overlay, [overlay, dimLabel](int value) {
        overlay->setDimOpacity(value / 100.0);
        dimLabel->setText(QStringLiteral("Dim: %1%").arg(value));
    });
    content->addWidget(dimLabel);
    content->addWidget(dim);
    auto* close = new Button(QStringLiteral("Close"), body);
    QObject::connect(close, &Button::clicked, overlay, [overlay] { overlay->setIsOpen(false); });
    content->addWidget(close, 0, Qt::AlignRight);

    QObject::connect(counter, &Button::clicked, overlay, &Dialog::refreshBackdrop);
    QObject::connect(overlay, &Dialog::closed, hint, [hint] {
        hint->setText(QStringLiteral("Open the overlay to try this action."));
    });
    QObject::connect(overlay, &Dialog::closed, overlay, &QObject::deleteLater);
    overlay->open();
});)sample"),
            [](QWidget* parent) {
                return samples::overlayEffectsSample<Dialog>(parent, QStringLiteral("Gallery.Dialog"),
                    QStringLiteral("Preview background effects"));
            }),

        makeSample(QStringLiteral("dialog-animation-smoke"), QStringLiteral("Opening animation"),
            QStringLiteral("Compare animated and instant opening with the same modal backdrop."),
            QStringLiteral(R"sample(auto* root = new QWidget(this);
auto* layout = new QVBoxLayout(root);
auto* animated = new Button(QStringLiteral("Animated"), root);
auto* instant = new Button(QStringLiteral("Instant"), root);
auto* buttons = new QHBoxLayout;
buttons->addWidget(animated);
buttons->addWidget(instant);
buttons->addStretch();
layout->addLayout(buttons);
const auto showDialog = [](Button* trigger, bool animation) {
    auto* dialog = new Dialog(trigger->window());
    dialog->setThemeSource(trigger);
    dialog->setModal(true);
    dialog->setDim(true);
    dialog->setAnimationEnabled(animation);
    dialog->setMinimumSize(420, 220);
    auto* content = new QVBoxLayout(dialog);
    content->setContentsMargins(30, 28, 30, 28);
    auto* title = new Label(animation ? QStringLiteral("Animated dialog")
                                    : QStringLiteral("Instant dialog"), dialog);
    title->setFluentTypography(Typography::FontRole::Subtitle);
    content->addWidget(title);
    auto* text = new Label(QStringLiteral("Only the opening animation changes; the background remains blocked."), dialog);
    text->setWordWrap(true);
    content->addWidget(text);
    auto* close = new Button(QStringLiteral("Close"), dialog);
    content->addWidget(close, 0, Qt::AlignRight);
    QObject::connect(close, &Button::clicked, dialog, &QDialog::reject);
    QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
    dialog->open();
};
QObject::connect(animated, &Button::clicked, root, [animated, showDialog] { showDialog(animated, true); });
QObject::connect(instant, &Button::clicked, root, [instant, showDialog] { showDialog(instant, false); });
return root;)sample"),
            [](QWidget* parent) {
                auto* root = new QWidget(parent);
                auto* layout = new QVBoxLayout(root);
                auto* animated = new Button(QStringLiteral("Animated"), root);
                auto* instant = new Button(QStringLiteral("Instant"), root);
                auto* buttons = new QHBoxLayout;
                buttons->addWidget(animated);
                buttons->addWidget(instant);
                buttons->addStretch();
                layout->addLayout(buttons);
                const auto showDialog = [](Button* trigger, bool animation) {
                    auto* dialog = new Dialog(trigger->window());
                    dialog->setThemeSource(trigger);
                    dialog->setModal(true);
                    dialog->setDim(true);
                    dialog->setAnimationEnabled(animation);
                    dialog->setMinimumSize(420, 220);
                    auto* content = new QVBoxLayout(dialog);
                    content->setContentsMargins(30, 28, 30, 28);
                    auto* title = new Label(
                        animation ? QStringLiteral("Animated dialog") : QStringLiteral("Instant dialog"), dialog);
                    title->setFluentTypography(Typography::FontRole::Subtitle);
                    content->addWidget(title);
                    auto* text = new Label(
                        QStringLiteral("Only the opening animation changes; the background remains blocked."), dialog);
                    text->setWordWrap(true);
                    content->addWidget(text);
                    auto* close = new Button(QStringLiteral("Close"), dialog);
                    content->addWidget(close, 0, Qt::AlignRight);
                    QObject::connect(close, &Button::clicked, dialog, &QDialog::reject);
                    QObject::connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
                    dialog->open();
                };
                QObject::connect(animated, &Button::clicked, root, [animated, showDialog] {
                    showDialog(animated, true);
                });
                QObject::connect(instant, &Button::clicked, root, [instant, showDialog] {
                    showDialog(instant, false);
                });
                return root;
            })};
    samples::overlayEffectsSourceFiles(samples[1], QStringLiteral("Dialog"));
    return samples;
}

QVector<GallerySample> flyoutSamples()
{
    return {
        makeSample(QStringLiteral("flyout-placement-anchors"), QStringLiteral("Anchored placement"),
            QStringLiteral("Flyout positions itself around an anchor and can "
                           "fall back automatically near edges."),
            QStringLiteral(
                "auto* flyout = new Flyout(window());\n"
                "flyout->setPlacement(Flyout::Right);\n"
                "populateFlyout(flyout);\n"
                "connect(flyout, &Popup::closed, flyout, "
                "&QObject::deleteLater);\n"
                "flyout->showAt(anchorButton);"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* row = horizontalGroup(surface, 8);
                auto* bottom = sampleButton(row, QStringLiteral("Bottom"));
                auto* right = sampleButton(row, QStringLiteral("Right"));
                auto* automatic = sampleButton(row, QStringLiteral("Auto"));
                auto* status = makeStatusLabel(surface, QStringLiteral("Opened placement: none"));

                auto showPlacement = [status](Button* anchor, Flyout::Placement placement) {
                    auto* flyout = new Flyout(anchor->window());
                    flyout->setPlacement(placement);
                    populateFlyout(flyout, QStringLiteral("%1 flyout").arg(flyoutPlacementText(placement)),
                        QStringLiteral(
                            "Click outside, press Escape, or use Close to "
                            "dismiss this anchored surface."));
                    QObject::connect(flyout, &Popup::closed, flyout, &QObject::deleteLater);
                    QObject::connect(flyout, &Popup::opened, status, [status, placement]() {
                        status->setText(QStringLiteral("Opened placement: %1").arg(flyoutPlacementText(placement)));
                    });
                    flyout->showAt(anchor);
                };

                QObject::connect(bottom, &Button::clicked, bottom, [bottom, showPlacement]() {
                    showPlacement(bottom, Flyout::Bottom);
                });
                QObject::connect(right, &Button::clicked, right, [right, showPlacement]() {
                    showPlacement(right, Flyout::Right);
                });
                QObject::connect(automatic, &Button::clicked, automatic, [automatic, showPlacement]() {
                    showPlacement(automatic, Flyout::Auto);
                });

                boxLayout(row)->addWidget(bottom);
                boxLayout(row)->addWidget(right);
                boxLayout(row)->addWidget(automatic);
                boxLayout(surface)->addWidget(row);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(QStringLiteral("flyout-command-confirmation"), QStringLiteral("Command confirmation"),
            QStringLiteral("Use a Flyout for contextual confirmation that should "
                           "stay visually attached to its command source."),
            QStringLiteral(
                "auto* flyout = new Flyout(window());\n"
                "flyout->setPlacement(Flyout::Right);\n"
                "auto* confirm = new Button(\"Empty cart\", flyout);\n"
                "connect(confirm, &Button::clicked, flyout, [flyout] "
                "{ flyout->close(); });\n"
                "flyout->showAt(commandButton);"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* row = horizontalGroup(surface, 12);
                auto* button = sampleButton(row, QStringLiteral("Empty cart"));
                button->setMinimumWidth(118);
                auto* status = makeStatusLabel(row, QStringLiteral("Cart: 4 items"));

                QObject::connect(button, &Button::clicked, button, [button, status]() {
                    auto* flyout = new Flyout(button->window());
                    flyout->setPlacement(Flyout::Right);
                    flyout->setMinimumSize(360, 214);
                    auto* layout = new QVBoxLayout(flyout);
                    layout->setContentsMargins(24, 22, 24, 22);
                    layout->setSpacing(10);
                    layout->addWidget(makeTitleLabel(flyout, QStringLiteral("Empty cart?")));
                    layout->addWidget(makeBodyLabel(flyout,
                        QStringLiteral(
                            "All selected items will be removed from the cart. "
                            "You can restore them from order history later.")));
                    layout->addStretch(1);
                    auto* commandRow = horizontalGroup(flyout, 8);
                    auto* confirm = sampleButton(commandRow, QStringLiteral("Empty"));
                    confirm->setFluentStyle(Button::Accent);
                    auto* cancel = sampleButton(commandRow, QStringLiteral("Cancel"));
                    QObject::connect(confirm, &Button::clicked, status, [status, flyout]() {
                        status->setText(QStringLiteral("Cart: emptied"));
                        flyout->close();
                    });
                    QObject::connect(cancel, &Button::clicked, flyout, [flyout]() { flyout->close(); });
                    boxLayout(commandRow)->addWidget(confirm);
                    boxLayout(commandRow)->addWidget(cancel);
                    layout->addWidget(commandRow, 0, Qt::AlignRight);
                    QObject::connect(flyout, &Popup::closed, flyout, &QObject::deleteLater);
                    flyout->showAt(button);
                });

                boxLayout(row)->addWidget(button);
                boxLayout(row)->addWidget(status);
                boxLayout(surface)->addWidget(row);
                return surface;
            })};
}

QVector<GallerySample> popupSamples()
{
    QVector<GallerySample> samples{
        makeSample(QStringLiteral("popup-basic"), QStringLiteral("A popup beside a button"),
            QStringLiteral("Show a short note next to the button that opens it."),
            QStringLiteral(R"sample(auto* root = new QWidget(this);
auto* layout = new QVBoxLayout(root);
auto* open = new Button(QStringLiteral("Show quick note"), root);
open->setFluentStyle(Button::Accent);
layout->addWidget(open, 0, Qt::AlignLeft);
QObject::connect(open, &Button::clicked, root, [open] {
    auto* popup = new Popup(open->window());
    popup->setThemeSource(open);
    auto* content = new QVBoxLayout(popup);
    content->setContentsMargins(24, 22, 24, 22);
    auto* title = new Label(QStringLiteral("Quick note"), popup);
    title->setFluentTypography(Typography::FontRole::BodyStrong);
    content->addWidget(title);
    auto* text = new Label(QStringLiteral("Click outside or press Escape to close."), popup);
    content->addWidget(text);
    popup->setPosition(open, QPoint(0, open->height() + 8));
    QObject::connect(popup, &Popup::closed, popup, &QObject::deleteLater);
    popup->open();
});
return root;)sample"),
            [](QWidget* parent) {
                auto* root = new QWidget(parent);
                auto* layout = new QVBoxLayout(root);
                auto* open = new Button(QStringLiteral("Show quick note"), root);
                open->setFluentStyle(Button::Accent);
                layout->addWidget(open, 0, Qt::AlignLeft);
                QObject::connect(open, &Button::clicked, root, [open] {
                    auto* popup = new Popup(open->window());
                    popup->setThemeSource(open);
                    auto* content = new QVBoxLayout(popup);
                    content->setContentsMargins(24, 22, 24, 22);
                    auto* title = new Label(QStringLiteral("Quick note"), popup);
                    title->setFluentTypography(Typography::FontRole::BodyStrong);
                    content->addWidget(title);
                    auto* text = new Label(QStringLiteral("Click outside or press Escape to close."), popup);
                    content->addWidget(text);
                    popup->setPosition(open, QPoint(0, open->height() + 8));
                    QObject::connect(popup, &Popup::closed, popup, &QObject::deleteLater);
                    popup->open();
                });
                return root;
            }),
        makeSample(QStringLiteral("popup-position-light-dismiss"), QStringLiteral("Choose how a popup closes"),
            QStringLiteral("Popup can be placed relative to a widget; close "
                           "policy controls whether outside clicks dismiss it."),
            QStringLiteral(
                "auto* popup = new Popup(window());\n"
                "popup->setClosePolicy(lightDismiss\n"
                "    ? Popup::ClosePolicy(Popup::CloseOnPressOutside "
                "| Popup::CloseOnEscape)\n"
                "    : Popup::ClosePolicy(Popup::NoAutoClose));\n"
                "popup->setPosition(anchorButton, QPoint(0, "
                "anchorButton->height() + 8));\n"
                "popup->open();"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* row = horizontalGroup(surface, 12);
                auto* button = sampleButton(row, QStringLiteral("Show popup"));
                button->setMinimumWidth(118);
                auto* lightDismiss = new ToggleSwitch(row);
                lightDismiss->setIsOn(true);
                lightDismiss->setOnContent(QStringLiteral("Light dismiss"));
                lightDismiss->setOffContent(QStringLiteral("Sticky"));
                auto* status = makeStatusLabel(surface, QStringLiteral("Popup state: closed"));

                QObject::connect(button, &Button::clicked, button, [button, lightDismiss, status]() {
                    auto* popup = new Popup(button->window());
                    popup->setThemeSource(button);
                    popup->setMinimumSize(360, 186);
                    popup->setClosePolicy(lightDismiss->isOn()
                            ? Popup::ClosePolicy(Popup::CloseOnPressOutside | Popup::CloseOnEscape)
                            : Popup::ClosePolicy(Popup::NoAutoClose));
                    auto* layout = new QVBoxLayout(popup);
                    layout->setContentsMargins(24, 22, 24, 22);
                    layout->setSpacing(10);
                    layout->addWidget(makeTitleLabel(popup, QStringLiteral("Simple popup")));
                    layout->addWidget(makeBodyLabel(popup,
                        lightDismiss->isOn()
                            ? QStringLiteral("Click outside or press Escape to "
                                             "close this popup.")
                            : QStringLiteral("This sticky popup closes only from "
                                             "its own command.")));
                    layout->addStretch(1);
                    auto* closeButton = sampleButton(popup, QStringLiteral("Close"));
                    QObject::connect(closeButton, &Button::clicked, popup, [popup]() { popup->close(); });
                    layout->addWidget(closeButton, 0, Qt::AlignRight);
                    popup->setPosition(button, QPoint(0, button->height() + 8));
                    QObject::connect(popup, &Popup::opened, status, [status, lightDismiss]() {
                        status->setText(lightDismiss->isOn()
                                ? QStringLiteral("Popup state: open, light dismiss on")
                                : QStringLiteral("Popup state: open, sticky"));
                    });
                    QObject::connect(popup, &Popup::closed, status, [status]() {
                        status->setText(QStringLiteral("Popup state: closed"));
                    });
                    QObject::connect(popup, &Popup::closed, popup, &QObject::deleteLater);
                    popup->open();
                });

                boxLayout(row)->addWidget(button);
                boxLayout(row)->addWidget(lightDismiss);
                boxLayout(surface)->addWidget(row);
                boxLayout(surface)->addWidget(status);
                return surface;
            }),
        makeSample(QStringLiteral("popup-modal-dim"), QStringLiteral("Modal and backdrop effects"),
            QStringLiteral("Blur or dim the background, then compare blocked and available background interaction."),
            QStringLiteral(R"sample(using basicinput::Button;
using basicinput::CheckBox;
using basicinput::Slider;
using textfields::Label;
auto* root = new QWidget(this);
auto* layout = new QVBoxLayout(root);
layout->setSpacing(12);
auto* open = new Button(QStringLiteral("Preview background effects"), root);
open->setFluentStyle(Button::Accent);
layout->addWidget(open, 0, Qt::AlignLeft);
auto* hint = new Label(QStringLiteral("Open the overlay to try this action."), root);
hint->setWordWrap(true);
hint->setTextColorRole(Label::TextColorRole::Secondary);
layout->addWidget(hint);
auto* row = new QWidget(root);
auto* rowLayout = new QHBoxLayout(row);
rowLayout->setContentsMargins(0, 0, 0, 0);
rowLayout->setSpacing(12);
auto* counter = new Button(QStringLiteral("Background action"), row);
row->layout()->addWidget(counter);
auto* status = new Label(QStringLiteral("Background clicks: 0"), row);
status->setTextColorRole(Label::TextColorRole::Secondary);
row->layout()->addWidget(status);
layout->addWidget(row);
QObject::connect(counter, &Button::clicked, status, [status, count = 0]() mutable {
    status->setText(QStringLiteral("Background clicks: %1").arg(++count));
});
QObject::connect(open, &Button::clicked, root, [root, open, counter, hint] {
    auto* overlay = new Popup(root);
    overlay->setModal(true);
    overlay->setDim(true);
    overlay->setDimOpacity(0.12);
    overlay->setBackdropBlurRadius(8);
    overlay->setClosePolicy(Popup::CloseOnEscape);
    QWidget* body = overlay;
    overlay->setThemeSource(open);
    overlay->setFixedWidth(400);
    auto* content = new QVBoxLayout(body);
    content->setContentsMargins(28, 26, 28, 26);
    content->setSpacing(10);
    auto* title = new Label(QStringLiteral("Modal and backdrop effects"), body);
    title->setFluentTypography(Typography::FontRole::BodyStrong);
    title->setWordWrap(true);
    content->addWidget(title);
    auto* modal = new CheckBox(QStringLiteral("Modal"), body);
    modal->setChecked(true);
    QObject::connect(modal, &QCheckBox::toggled, overlay, &Popup::setModal);
    const auto explainModal = [hint](bool enabled) {
        hint->setText(enabled
            ? QStringLiteral("Modal on: background blocked.")
            : QStringLiteral("Modal off: background available."));
    };
    QObject::connect(modal, &QCheckBox::toggled, hint, explainModal);
    explainModal(true);
    content->addWidget(modal);
    auto* dimEnabled = new CheckBox(QStringLiteral("Dim background"), body);
    dimEnabled->setChecked(true);
    QObject::connect(dimEnabled, &QCheckBox::toggled, overlay, &Popup::setDim);
    content->addWidget(dimEnabled);
    auto* outside = new CheckBox(QStringLiteral("Close on outside click"), body);
    QObject::connect(outside, &QCheckBox::toggled, overlay, [overlay](bool enabled) {
        Popup::ClosePolicy policy(Popup::CloseOnEscape);
        if (enabled)
            policy |= Popup::CloseOnPressOutside;
        overlay->setClosePolicy(policy);
    });
    content->addWidget(outside);
    auto* blurLabel = new Label(QStringLiteral("Blur: 8 px"), body);
    auto* blur = new Slider(Qt::Horizontal, body);
    blur->setAccessibleName(QStringLiteral("Background blur radius"));
    blur->setRange(0, 24);
    blur->setValue(8);
    QObject::connect(blur, &QSlider::valueChanged, overlay, [overlay, blurLabel](int value) {
        overlay->setBackdropBlurRadius(value);
        blurLabel->setText(QStringLiteral("Blur: %1 px").arg(value));
    });
    content->addWidget(blurLabel);
    content->addWidget(blur);
    auto* dimLabel = new Label(QStringLiteral("Dim: 12%"), body);
    auto* dim = new Slider(Qt::Horizontal, body);
    dim->setAccessibleName(QStringLiteral("Background dim opacity"));
    dim->setRange(0, 60);
    dim->setValue(12);
    QObject::connect(dim, &QSlider::valueChanged, overlay, [overlay, dimLabel](int value) {
        overlay->setDimOpacity(value / 100.0);
        dimLabel->setText(QStringLiteral("Dim: %1%").arg(value));
    });
    content->addWidget(dimLabel);
    content->addWidget(dim);
    auto* close = new Button(QStringLiteral("Close"), body);
    QObject::connect(close, &Button::clicked, overlay, [overlay] { overlay->setIsOpen(false); });
    content->addWidget(close, 0, Qt::AlignRight);

    QObject::connect(counter, &Button::clicked, overlay, &Popup::refreshBackdrop);
    QObject::connect(overlay, &Popup::closed, hint, [hint] {
        hint->setText(QStringLiteral("Open the overlay to try this action."));
    });
    QObject::connect(overlay, &Popup::closed, overlay, &QObject::deleteLater);
    overlay->open();
});)sample"),
            [](QWidget* parent) {
                return samples::overlayEffectsSample<Popup>(parent, QStringLiteral("Gallery.Popup"),
                    QStringLiteral("Preview background effects"));
            }),
    };
    samples::overlayEffectsSourceFiles(samples[2], QStringLiteral("Popup"));
    return samples;
}

QVector<GallerySample> teachingTipSamples()
{
    return {
        makeSample(QStringLiteral("teaching-tip-targeted-action"), QStringLiteral("Targeted tip with close reasons"),
            QStringLiteral("TeachingTip points at a target, can light-dismiss, "
                           "and reports how it was closed."),
            QStringLiteral(
                "auto* tip = new TeachingTip(window());\n"
                "tip->setPreferredPlacement(TeachingTip::Right);\n"
                "tip->setLightDismissEnabled(true);\n"
                "tip->setCardSize(QSize(320, 154));\n"
                "connect(tip, &TeachingTip::closing, status,\n"
                "        [status](TeachingTip::CloseReason reason) { "
                "updateStatus(reason); });\n"
                "tip->showAt(targetButton);"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* row = horizontalGroup(surface, 12);
                auto* button = sampleButton(row, QStringLiteral("Sync settings"));
                button->setMinimumWidth(128);
                auto* status = makeStatusLabel(row, QStringLiteral("Tip state: closed"));

                QObject::connect(button, &Button::clicked, button, [button, status]() {
                    auto* tip = new TeachingTip(button->window());
                    tip->setPreferredPlacement(TeachingTip::Right);
                    tip->setLightDismissEnabled(true);
                    tip->setCardSize(QSize(320, 154));
                    populateTeachingTip(tip, QStringLiteral("Sync is automatic"),
                        QStringLiteral("Changes are saved to the workspace as you edit."), status);
                    QObject::connect(tip, &Popup::opened, status, [status]() {
                        status->setText(QStringLiteral("Tip state: open"));
                    });
                    QObject::connect(tip, &Popup::closed, tip, &QObject::deleteLater);
                    tip->showAt(button);
                });

                boxLayout(row)->addWidget(button);
                boxLayout(row)->addWidget(status);
                boxLayout(surface)->addWidget(row);
                return surface;
            }),
        makeSample(QStringLiteral("teaching-tip-placement-tail"), QStringLiteral("Placement and tail"),
            QStringLiteral("Preferred placement, tail visibility, and card size "
                           "are independent so tips can fit the surrounding UI."),
            QStringLiteral(
                "auto* top = new Button(\"Top\", this);\n"
                "auto* rightTop = new Button(\"RightTop\", this);\n"
                "auto* automatic = new Button(\"Auto\", this);\n"
                "auto* tail = new ToggleSwitch(this);\n"
                "tail->setAccessibleName(\"Show TeachingTip tail\");\n"
                "tail->setIsOn(true);\n"
                "tail->setOnContent(\"Tail\");\n"
                "tail->setOffContent(\"No tail\");\n"
                "auto* status = new Label(\"Placement: none\", this);\n\n"
                "auto showTip = [tail, status](\n"
                "    Button* anchor, TeachingTip::PreferredPlacement placement) {\n"
                "    const QString name = teachingTipPlacementText(placement);\n"
                "    auto* tip = new TeachingTip(anchor->window());\n"
                "    tip->setAccessibleName(name + \" placement tip\");\n"
                "    tip->setPreferredPlacement(placement);\n"
                "    tip->setTailVisible(tail->isOn());\n"
                "    tip->setLightDismissEnabled(true);\n"
                "    tip->setCardSize(QSize(300, 136));\n"
                "    populateTeachingTip(\n"
                "        tip, name + \" placement\",\n"
                "        tail->isOn()\n"
                "            ? \"The tail points back to the control that opened the tip.\"\n"
                "            : \"Hide the tail when the surrounding layout already makes context "
                "clear.\",\n"
                "        status);\n"
                "    QObject::connect(tip, &Popup::opened, status, [=] {\n"
                "        status->setText(\"Placement: \" + name + \", tail \" +\n"
                "                        (tail->isOn() ? \"on\" : \"off\"));\n"
                "    });\n"
                "    QObject::connect(tip, &Popup::closed, tip, &QObject::deleteLater);\n"
                "    tip->showAt(anchor);\n"
                "};\n"
                "QObject::connect(top, &Button::clicked, top, [=] {\n"
                "    showTip(top, TeachingTip::Top);\n"
                "});\n"
                "QObject::connect(rightTop, &Button::clicked, rightTop, [=] {\n"
                "    showTip(rightTop, TeachingTip::RightTop);\n"
                "});\n"
                "QObject::connect(automatic, &Button::clicked, automatic, [=] {\n"
                "    showTip(automatic, TeachingTip::Auto);\n"
                "});"),
            [](QWidget* parent) {
                auto* surface = sampleSurface(parent);
                auto* controls = horizontalGroup(surface, 8);
                auto* top = sampleButton(controls, QStringLiteral("Top"));
                auto* rightTop = sampleButton(controls, QStringLiteral("RightTop"));
                auto* automatic = sampleButton(controls, QStringLiteral("Auto"));
                top->setObjectName(QStringLiteral("teachingTipTopAnchor"));
                rightTop->setObjectName(QStringLiteral("teachingTipRightTopAnchor"));
                automatic->setObjectName(QStringLiteral("teachingTipAutoAnchor"));
                auto* tail = new ToggleSwitch(controls);
                tail->setObjectName(QStringLiteral("teachingTipTailToggle"));
                tail->setAccessibleName(QStringLiteral("Show TeachingTip tail"));
                tail->setIsOn(true);
                tail->setOnContent(QStringLiteral("Tail"));
                tail->setOffContent(QStringLiteral("No tail"));
                auto* status = makeStatusLabel(surface, QStringLiteral("Placement: none"));

                auto showTip = [tail, status](Button* anchor, TeachingTip::PreferredPlacement placement) {
                    auto* tip = new TeachingTip(anchor->window());
                    tip->setObjectName(QStringLiteral("teachingTipPlacementPreview"));
                    tip->setAccessibleName(QStringLiteral("%1 placement tip").arg(teachingTipPlacementText(placement)));
                    tip->setPreferredPlacement(placement);
                    tip->setTailVisible(tail->isOn());
                    tip->setLightDismissEnabled(true);
                    tip->setCardSize(QSize(300, 136));
                    populateTeachingTip(tip, QStringLiteral("%1 placement").arg(teachingTipPlacementText(placement)),
                        tail->isOn()
                            ? QStringLiteral("The tail points back to the control "
                                             "that opened the tip.")
                            : QStringLiteral("Hide the tail when the surrounding "
                                             "layout already makes context clear."),
                        status);
                    QObject::connect(tip, &Popup::opened, status, [status, placement, tail]() {
                        status->setText(QStringLiteral("Placement: %1, tail %2")
                                .arg(teachingTipPlacementText(placement),
                                    tail->isOn() ? QStringLiteral("on") : QStringLiteral("off")));
                    });
                    QObject::connect(tip, &Popup::closed, tip, &QObject::deleteLater);
                    tip->showAt(anchor);
                };

                QObject::connect(top, &Button::clicked, top, [top, showTip]() { showTip(top, TeachingTip::Top); });
                QObject::connect(rightTop, &Button::clicked, rightTop, [rightTop, showTip]() {
                    showTip(rightTop, TeachingTip::RightTop);
                });
                QObject::connect(automatic, &Button::clicked, automatic, [automatic, showTip]() {
                    showTip(automatic, TeachingTip::Auto);
                });

                boxLayout(controls)->addWidget(top);
                boxLayout(controls)->addWidget(rightTop);
                boxLayout(controls)->addWidget(automatic);
                boxLayout(controls)->addWidget(tail);
                boxLayout(surface)->addWidget(controls);
                boxLayout(surface)->addWidget(status);
                return surface;
            })};
}

QVector<GallerySample> coachMarkSamples()
{
    return {makeSample(QStringLiteral("coach-mark-targeted-glide"), QStringLiteral("Targeted coach mark with glide"),
        QStringLiteral(
            "CoachMark overlays its owning window, points a tail "
            "at a target, and glides to a new target when "
            "retargeted while open."),
        QStringLiteral(
            "auto* coach = new CoachMark(window());\n"
            "connect(this, &QObject::destroyed, coach, &QObject::deleteLater);\n"
            "coach->setCardSize(QSize(320, 150));\n"
            "coach->setPlacement(CoachMark::Bottom);\n"
            "coach->setTarget(targetButton);  // glides if already open\n"
            "closeButton->setAccessibleName(\"Close\");\n"
            "connect(closeButton, &Button::clicked, coach, "
            "&CoachMark::close);\n"
            "coach->open();"),
        [](QWidget* parent) {
            auto* surface = sampleSurface(parent);
            auto* row = horizontalGroup(surface, 8);
            auto* bottom = sampleButton(row, QStringLiteral("Bottom"));
            bottom->setObjectName(QStringLiteral("galleryCoachMarkBottom"));
            auto* right = sampleButton(row, QStringLiteral("Right"));
            right->setObjectName(QStringLiteral("galleryCoachMarkRight"));
            auto* top = sampleButton(row, QStringLiteral("Top"));
            top->setObjectName(QStringLiteral("galleryCoachMarkTop"));
            auto* status = makeStatusLabel(surface, QStringLiteral("Coach mark: closed"));

            // One shared coach mark drives all three buttons: retargeting it
            // while open glides the same overlay, and nothing stacks.
            // zh_CN: 三个按钮共用一个 coach mark：打开状态下重定向会让同一个
            // 浮层滑动过去，不会堆叠。
            auto coachRef = std::make_shared<QPointer<CoachMark>>();
            auto titleRef = std::make_shared<QPointer<Label>>();

            auto showCoach = [coachRef, titleRef, surface, status](Button* target, CoachMark::Placement placement) {
                if (!*coachRef) {
                    auto* coach = new CoachMark(target->window());
                    QObject::connect(surface, &QObject::destroyed, coach, &QObject::deleteLater);
                    coach->setCardSize(QSize(320, 150));

                    auto* host = coach->contentHost();
                    auto* layout = new QVBoxLayout(host);
                    layout->setContentsMargins(18, 14, 14, 14);
                    layout->setSpacing(8);

                    auto* titleRow = new QHBoxLayout;
                    titleRow->setSpacing(8);
                    auto* title = makeTitleLabel(host, QStringLiteral("Coach mark"));
                    titleRow->addWidget(title);
                    titleRow->addStretch(1);
                    auto* closeButton = new Button(QString(), host);
                    closeButton->setObjectName(QStringLiteral("galleryCoachMarkDismiss"));
                    closeButton->setAccessibleName(QStringLiteral("Close"));
                    closeButton->setFluentLayout(Button::IconOnly);
                    closeButton->setFluentStyle(Button::Subtle);
                    closeButton->setIconGlyph(Typography::Icons::ChromeClose, Typography::IconSize::Standard);
                    closeButton->setFixedSize(30, 30);
                    QObject::connect(closeButton, &Button::clicked, coach, [coach]() { coach->close(); });
                    titleRow->addWidget(closeButton);
                    layout->addLayout(titleRow);

                    layout->addWidget(makeBodyLabel(host,
                        QStringLiteral(
                            "The tail points back at the control that opened "
                            "this coach mark. Pick another placement to watch "
                            "it glide.")));
                    layout->addStretch(1);

                    auto* gotIt = sampleButton(host, QStringLiteral("Got it"));
                    gotIt->setObjectName(QStringLiteral("galleryCoachMarkClose"));
                    gotIt->setFluentStyle(Button::Accent);
                    QObject::connect(gotIt, &Button::clicked, coach, [coach]() { coach->close(); });
                    layout->addWidget(gotIt, 0, Qt::AlignRight);

                    QObject::connect(coach, &CoachMark::closed, status, [status]() {
                        status->setText(QStringLiteral("Coach mark: closed"));
                    });

                    *coachRef = coach;
                    *titleRef = title;
                }

                const QString placementText = coachMarkPlacementText(placement);
                (*titleRef)->setText(QStringLiteral("%1 placement").arg(placementText));
                (*coachRef)->setPlacement(placement);
                (*coachRef)->setTarget(target);
                (*coachRef)->open();
                status->setText(QStringLiteral("Coach mark: %1").arg(placementText));
            };

            QObject::connect(bottom, &Button::clicked, bottom, [bottom, showCoach]() {
                showCoach(bottom, CoachMark::Bottom);
            });
            QObject::connect(right, &Button::clicked, right, [right, showCoach]() {
                showCoach(right, CoachMark::Right);
            });
            QObject::connect(top, &Button::clicked, top, [top, showCoach]() { showCoach(top, CoachMark::Top); });

            boxLayout(row)->addWidget(bottom);
            boxLayout(row)->addWidget(right);
            boxLayout(row)->addWidget(top);
            boxLayout(surface)->addWidget(row);
            boxLayout(surface)->addWidget(status);
            return surface;
        })};
}

} // namespace

QVector<GallerySample> dialogsFlyoutsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("content-dialog"))
        return contentDialogSamples();
    if (routeId == QStringLiteral("dialog"))
        return dialogSamples();
    if (routeId == QStringLiteral("flyout"))
        return flyoutSamples();
    if (routeId == QStringLiteral("popup"))
        return popupSamples();
    if (routeId == QStringLiteral("teaching-tip"))
        return teachingTipSamples();
    if (routeId == QStringLiteral("coach-mark"))
        return coachMarkSamples();
    return {};
}

} // namespace fluent::gallery
