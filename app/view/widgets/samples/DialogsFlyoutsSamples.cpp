#include "DialogsFlyoutsSamples.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/ContentDialog.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/dialogs_flyouts/TeachingTip.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
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
using fluent::textfields::LineEdit;
using samples::horizontalGroup;
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
                   QStringLiteral("ContentDialog blocks the window until a button is invoked; the label echoes the result."),
                   QStringLiteral("auto* dialog = new ContentDialog(window());\n"
                                  "dialog->setTitle(\"Save your work?\");\n"
                                  "dialog->setContent(bodyLabel);\n"
                                  "dialog->setPrimaryButtonText(\"Save\");\n"
                                  "dialog->setSecondaryButtonText(\"Don't save\");\n"
                                  "dialog->setCloseButtonText(\"Cancel\");\n"
                                  "connect(dialog, &ContentDialog::primaryButtonClicked,\n"
                                  "        this, [] { /* save */ });\n"
                                  "dialog->exec();"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 16);
                       auto* button = new Button(QStringLiteral("Show dialog"), group);
                       auto* result = makeBodyLabel(group, QStringLiteral("Result: —"));
                       QObject::connect(button, &Button::clicked, button, [button, result]() {
                           auto* dialog = new ContentDialog(button->window());
                           dialog->setTitle(QStringLiteral("Save your work?"));
                           dialog->setContent(makeBodyLabel(
                               nullptr,
                               QStringLiteral("Unsaved changes in \"Quarterly report\" will be "
                                              "lost unless you save them.")));
                           dialog->setPrimaryButtonText(QStringLiteral("Save"));
                           dialog->setSecondaryButtonText(QStringLiteral("Don't save"));
                           dialog->setCloseButtonText(QStringLiteral("Cancel"));
                           QObject::connect(dialog, &ContentDialog::primaryButtonClicked,
                                            result, [result]() {
                                                result->setText(QStringLiteral("Result: Save"));
                                            });
                           QObject::connect(dialog, &ContentDialog::secondaryButtonClicked,
                                            result, [result]() {
                                                result->setText(QStringLiteral("Result: Don't save"));
                                            });
                           QObject::connect(dialog, &ContentDialog::closeButtonClicked,
                                            result, [result]() {
                                                result->setText(QStringLiteral("Result: Cancel"));
                                            });
                           dialog->exec();
                           dialog->deleteLater();
                       });
                       group->layout()->addWidget(button);
                       group->layout()->addWidget(result);
                       return group;
                   })
    };
}

QVector<GallerySample> dialogSamples()
{
    return {
        makeSample(QStringLiteral("dialog-basic"),
                   QStringLiteral("Dialog hosting a small form"),
                   QStringLiteral("Dialog provides the modal surface; you own the content layout."),
                   QStringLiteral("auto* dialog = new Dialog(window());\n"
                                  "auto* layout = new QVBoxLayout(dialog);\n"
                                  "layout->addWidget(titleLabel);\n"
                                  "layout->addWidget(nameEdit);\n"
                                  "layout->addWidget(buttonRow);\n"
                                  "dialog->exec();"),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Open dialog"), parent);
                       QObject::connect(button, &Button::clicked, button, [button]() {
                           auto* dialog = new Dialog(button->window());
                           auto* layout = new QVBoxLayout(dialog);
                           layout->setSpacing(16);

                           auto* title = new Label(QStringLiteral("Rename project"), dialog);
                           title->setFluentTypography(Typography::FontRole::Subtitle);
                           layout->addWidget(title);
                           layout->addWidget(makeBodyLabel(
                               dialog, QStringLiteral("Choose a new name for \"Northwind\".")));

                           auto* nameEdit = new LineEdit(dialog);
                           nameEdit->setText(QStringLiteral("Northwind"));
                           layout->addWidget(nameEdit);

                           auto* buttonRow = new QHBoxLayout;
                           buttonRow->setSpacing(8);
                           buttonRow->addStretch(1);
                           auto* renameButton = new Button(QStringLiteral("Rename"), dialog);
                           renameButton->setFluentStyle(Button::Accent);
                           auto* cancelButton = new Button(QStringLiteral("Cancel"), dialog);
                           QObject::connect(renameButton, &Button::clicked,
                                            dialog, [dialog]() { dialog->done(1); });
                           QObject::connect(cancelButton, &Button::clicked,
                                            dialog, [dialog]() { dialog->done(0); });
                           buttonRow->addWidget(renameButton);
                           buttonRow->addWidget(cancelButton);
                           layout->addLayout(buttonRow);

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
                   QStringLiteral("Popup floats above the window content; close it from inside or by clicking outside."),
                   QStringLiteral("auto* popup = new Popup(window());\n"
                                  "auto* layout = new QVBoxLayout(popup);\n"
                                  "layout->addWidget(contentLabel);\n"
                                  "layout->addWidget(closeButton);\n"
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
                           auto* closeButton = new Button(QStringLiteral("Close"), popup);
                           QObject::connect(closeButton, &Button::clicked,
                                            popup, [popup]() { popup->close(); });
                           layout->addWidget(closeButton, 0, Qt::AlignRight);
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
                   QStringLiteral("The tip anchors to the target with a tail; close it with the X or the action button."),
                   QStringLiteral("auto* tip = new TeachingTip(window());\n"
                                  "auto* layout = new QVBoxLayout(tip->contentHost());\n"
                                  "layout->addWidget(titleRowWithCloseButton);\n"
                                  "layout->addWidget(tipLabel);\n"
                                  "layout->addWidget(gotItButton);\n"
                                  "tip->setCardSize(QSize(320, 150));\n"
                                  "tip->showAt(targetButton);"),
                   [](QWidget* parent) {
                       auto* button = new Button(QStringLiteral("Show teaching tip"), parent);
                       QObject::connect(button, &Button::clicked, button, [button]() {
                           auto* tip = new TeachingTip(button->window());
                           auto* layout = new QVBoxLayout(tip->contentHost());
                           layout->setSpacing(8);

                           // Title row: heading on the left, X close button on the right.
                           // zh_CN: 标题行：左侧标题，右侧 X 关闭按钮。
                           auto* titleRow = new QHBoxLayout;
                           titleRow->setSpacing(8);
                           auto* title = new Label(QStringLiteral("Saving automatically"),
                                                   tip->contentHost());
                           title->setFluentTypography(Typography::FontRole::BodyStrong);
                           titleRow->addWidget(title);
                           titleRow->addStretch(1);
                           auto* closeButton = new Button(QString(), tip->contentHost());
                           closeButton->setFluentLayout(Button::IconOnly);
                           closeButton->setIconGlyph(Typography::Icons::ChromeClose,
                                                     Typography::FontSize::Caption);
                           closeButton->setFixedSize(28, 28);
                           QObject::connect(closeButton, &Button::clicked, tip, [tip]() {
                               tip->closeWithReason(TeachingTip::CloseButton);
                           });
                           titleRow->addWidget(closeButton);
                           layout->addLayout(titleRow);

                           layout->addWidget(makeBodyLabel(
                               tip->contentHost(),
                               QStringLiteral("We save your changes as you go, so you never have to.")));

                           auto* gotIt = new Button(QStringLiteral("Got it"), tip->contentHost());
                           gotIt->setFluentStyle(Button::Accent);
                           QObject::connect(gotIt, &Button::clicked, tip, [tip]() {
                               tip->closeWithReason(TeachingTip::ActionButton);
                           });
                           layout->addWidget(gotIt, 0, Qt::AlignRight);

                           tip->setCardSize(QSize(320, 150));
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
