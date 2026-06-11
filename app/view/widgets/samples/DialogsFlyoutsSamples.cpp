#include "DialogsFlyoutsSamples.h"

#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/ContentDialog.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/dialogs_flyouts/TeachingTip.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::dialogs_flyouts::ContentDialog;
using fluent::dialogs_flyouts::Dialog;
using fluent::dialogs_flyouts::Flyout;
using fluent::dialogs_flyouts::Popup;
using fluent::dialogs_flyouts::TeachingTip;
using fluent::textfields::Label;
using samples::makeSample;

Label* makeBodyLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    label->setWordWrap(true);
    return label;
}

QVector<GallerySample> contentDialogSamples()
{
    return {
        makeSample(QStringLiteral("content-dialog-basic"),
                   QStringLiteral("Modal dialog with commit buttons"),
                   QStringLiteral("ContentDialog blocks the window until one of the buttons is invoked."),
                   QStringLiteral("auto* dialog = new ContentDialog(window());\n"
                                  "dialog->setTitle(\"Save your work?\");\n"
                                  "dialog->setContent(bodyLabel);\n"
                                  "dialog->setPrimaryButtonText(\"Save\");\n"
                                  "dialog->setCloseButtonText(\"Cancel\");\n"
                                  "dialog->exec();"),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Show dialog"), parent);
                       QObject::connect(button, &Button::clicked, button, [button]() {
                           auto* dialog = new ContentDialog(button->window());
                           dialog->setTitle(QStringLiteral("Save your work?"));
                           dialog->setContent(makeBodyLabel(
                               nullptr, QStringLiteral("Lorem ipsum dolor sit amet, adipisicing elit.")));
                           dialog->setPrimaryButtonText(QStringLiteral("Save"));
                           dialog->setSecondaryButtonText(QStringLiteral("Don't save"));
                           dialog->setCloseButtonText(QStringLiteral("Cancel"));
                           dialog->exec();
                           dialog->deleteLater();
                       });
                       return button;
                   })
    };
}

QVector<GallerySample> dialogSamples()
{
    return {
        makeSample(QStringLiteral("dialog-basic"),
                   QStringLiteral("Dialog hosting custom content"),
                   QStringLiteral("Dialog provides the modal surface; you own the content layout."),
                   QStringLiteral("auto* dialog = new Dialog(window());\n"
                                  "auto* layout = new QVBoxLayout(dialog);\n"
                                  "layout->addWidget(contentLabel);\n"
                                  "layout->addWidget(closeButton);\n"
                                  "dialog->exec();"),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Open dialog"), parent);
                       QObject::connect(button, &Button::clicked, button, [button]() {
                           auto* dialog = new Dialog(button->window());
                           auto* layout = new QVBoxLayout(dialog);
                           layout->setSpacing(16);
                           auto* title = new Label(QStringLiteral("A Fluent dialog"), dialog);
                           title->setFluentTypography(Typography::FontRole::Subtitle);
                           layout->addWidget(title);
                           layout->addWidget(makeBodyLabel(
                               dialog, QStringLiteral("This modal surface hosts any widget content.")));
                           auto* closeButton = new Button(QStringLiteral("Close"), dialog);
                           closeButton->setFluentStyle(Button::Accent);
                           QObject::connect(closeButton, &Button::clicked,
                                            dialog, [dialog]() { dialog->done(0); });
                           layout->addWidget(closeButton, 0, Qt::AlignRight);
                           dialog->exec();
                           dialog->deleteLater();
                       });
                       return button;
                   })
    };
}

QVector<GallerySample> flyoutSamples()
{
    return {
        makeSample(QStringLiteral("flyout-basic"),
                   QStringLiteral("Flyout anchored to a button"),
                   QStringLiteral("The flyout opens above its anchor and light-dismisses on outside click."),
                   QStringLiteral("auto* flyout = new Flyout(window());\n"
                                  "auto* layout = new QVBoxLayout(flyout);\n"
                                  "layout->addWidget(messageLabel);\n"
                                  "layout->addWidget(confirmButton);\n"
                                  "flyout->showAt(anchorButton);"),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Empty cart"), parent);
                       QObject::connect(button, &Button::clicked, button, [button]() {
                           auto* flyout = new Flyout(button->window());
                           auto* layout = new QVBoxLayout(flyout);
                           layout->setSpacing(12);
                           layout->addWidget(makeBodyLabel(
                               flyout, QStringLiteral("All items will be removed. Do you want to continue?")));
                           auto* confirm = new Button(QStringLiteral("Yes, empty my cart"), flyout);
                           confirm->setFluentStyle(Button::Accent);
                           QObject::connect(confirm, &Button::clicked,
                                            flyout, [flyout]() { flyout->close(); });
                           layout->addWidget(confirm, 0, Qt::AlignLeft);
                           QObject::connect(flyout, &Popup::closed,
                                            flyout, &QObject::deleteLater);
                           flyout->showAt(button);
                       });
                       return button;
                   })
    };
}

QVector<GallerySample> popupSamples()
{
    return {
        makeSample(QStringLiteral("popup-basic"),
                   QStringLiteral("Popup surface"),
                   QStringLiteral("Popup floats above the window content and closes on outside click."),
                   QStringLiteral("auto* popup = new Popup(window());\n"
                                  "auto* layout = new QVBoxLayout(popup);\n"
                                  "layout->addWidget(contentLabel);\n"
                                  "popup->open();"),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Open popup"), parent);
                       QObject::connect(button, &Button::clicked, button, [button]() {
                           auto* popup = new Popup(button->window());
                           auto* layout = new QVBoxLayout(popup);
                           layout->setSpacing(12);
                           auto* title = new Label(QStringLiteral("Popup"), popup);
                           title->setFluentTypography(Typography::FontRole::BodyStrong);
                           layout->addWidget(title);
                           layout->addWidget(makeBodyLabel(
                               popup, QStringLiteral("A floating surface positioned over app content.")));
                           QObject::connect(popup, &Popup::closed,
                                            popup, &QObject::deleteLater);
                           popup->open();
                       });
                       return button;
                   })
    };
}

QVector<GallerySample> teachingTipSamples()
{
    return {
        makeSample(QStringLiteral("teaching-tip-basic"),
                   QStringLiteral("TeachingTip pointing at its target"),
                   QStringLiteral("The tip anchors to the target with a tail and light-dismisses."),
                   QStringLiteral("auto* tip = new TeachingTip(window());\n"
                                  "auto* layout = new QVBoxLayout(tip->contentHost());\n"
                                  "layout->addWidget(tipLabel);\n"
                                  "tip->setCardSize(QSize(300, 120));\n"
                                  "tip->showAt(targetButton);"),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Show teaching tip"), parent);
                       QObject::connect(button, &Button::clicked, button, [button]() {
                           auto* tip = new TeachingTip(button->window());
                           auto* layout = new QVBoxLayout(tip->contentHost());
                           layout->setSpacing(8);
                           auto* title = new Label(QStringLiteral("Saving automatically"), tip->contentHost());
                           title->setFluentTypography(Typography::FontRole::BodyStrong);
                           layout->addWidget(title);
                           layout->addWidget(makeBodyLabel(
                               tip->contentHost(),
                               QStringLiteral("We save your changes as you go, so you never have to.")));
                           tip->setCardSize(QSize(300, 120));
                           QObject::connect(tip, &Popup::closed,
                                            tip, &QObject::deleteLater);
                           tip->showAt(button);
                       });
                       return button;
                   })
    };
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
    return {};
}

} // namespace fluent::gallery
