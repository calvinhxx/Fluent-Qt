#include "TextFieldsSamples.h"

#include "components/textfields/AutoSuggestBox.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/NumberBox.h"
#include "components/textfields/PasswordBox.h"
#include "components/textfields/TextEdit.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::textfields::AutoSuggestBox;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;
using fluent::textfields::NumberBox;
using fluent::textfields::PasswordBox;
using fluent::textfields::TextEdit;
using samples::makeSample;
using samples::verticalGroup;

QVector<GallerySample> autoSuggestBoxSamples()
{
    return {
        makeSample(QStringLiteral("auto-suggest-box-basic"),
                   QStringLiteral("Suggestions while typing"),
                   QStringLiteral("Matching suggestions appear in a flyout as you type."),
                   QStringLiteral("auto* box = new AutoSuggestBox(this);\n"
                                  "box->setHeader(\"Fruit\");\n"
                                  "box->setSuggestions({\"Apple\", \"Apricot\",\n"
                                  "                     \"Banana\", \"Blueberry\"});"),
                   [](QWidget* parent) {
                       auto* box = new AutoSuggestBox(parent);
                       box->setHeader(QStringLiteral("Fruit"));
                       box->setPlaceholderText(QStringLiteral("Type a fruit name"));
                       box->setSuggestions({QStringLiteral("Apple"), QStringLiteral("Apricot"),
                                            QStringLiteral("Banana"), QStringLiteral("Blueberry"),
                                            QStringLiteral("Cherry"), QStringLiteral("Grape"),
                                            QStringLiteral("Orange"), QStringLiteral("Strawberry")});
                       box->setFixedWidth(300);
                       return box;
                   })
    };
}

QVector<GallerySample> labelSamples()
{
    return {
        makeSample(QStringLiteral("label-typography"),
                   QStringLiteral("Typography roles"),
                   QStringLiteral("Labels pick their font from the Fluent type ramp."),
                   QStringLiteral("auto* label = new Label(\"Title\", this);\n"
                                  "label->setFluentTypography(Typography::FontRole::Title);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 6);
                       const QVector<QPair<QString, QString>> roles{
                           {Typography::FontRole::Caption, QStringLiteral("Caption")},
                           {Typography::FontRole::Body, QStringLiteral("Body")},
                           {Typography::FontRole::BodyStrong, QStringLiteral("Body strong")},
                           {Typography::FontRole::Subtitle, QStringLiteral("Subtitle")},
                           {Typography::FontRole::Title, QStringLiteral("Title")}};
                       for (const auto& role : roles) {
                           auto* label = new Label(role.second, group);
                           label->setFluentTypography(role.first);
                           group->layout()->addWidget(label);
                       }
                       return group;
                   })
    };
}

QVector<GallerySample> lineEditSamples()
{
    return {
        makeSample(QStringLiteral("line-edit-basic"),
                   QStringLiteral("LineEdit with clear button"),
                   QStringLiteral("A single-line input; the clear button appears while editing."),
                   QStringLiteral("auto* lineEdit = new LineEdit(this);\n"
                                  "lineEdit->setPlaceholderText(\"Enter your name\");\n"
                                  "lineEdit->setClearButtonEnabled(true);"),
                   [](QWidget* parent) {
                       auto* lineEdit = new LineEdit(parent);
                       lineEdit->setPlaceholderText(QStringLiteral("Enter your name"));
                       lineEdit->setClearButtonEnabled(true);
                       lineEdit->setFixedWidth(280);
                       return lineEdit;
                   })
    };
}

QVector<GallerySample> numberBoxSamples()
{
    return {
        makeSample(QStringLiteral("number-box-basic"),
                   QStringLiteral("NumberBox with inline spin buttons"),
                   QStringLiteral("Step the value with the buttons, the wheel, or typed expressions."),
                   QStringLiteral("auto* numberBox = new NumberBox(this);\n"
                                  "numberBox->setHeader(\"Quantity\");\n"
                                  "numberBox->setRange(0, 100);\n"
                                  "numberBox->setValue(1);\n"
                                  "numberBox->setSpinButtonPlacementMode(\n"
                                  "    NumberBox::SpinButtonPlacementMode::Inline);"),
                   [](QWidget* parent) {
                       auto* numberBox = new NumberBox(parent);
                       numberBox->setHeader(QStringLiteral("Quantity"));
                       numberBox->setRange(0, 100);
                       numberBox->setValue(1);
                       numberBox->setSpinButtonPlacementMode(NumberBox::SpinButtonPlacementMode::Inline);
                       numberBox->setFixedWidth(220);
                       return numberBox;
                   })
    };
}

QVector<GallerySample> passwordBoxSamples()
{
    return {
        makeSample(QStringLiteral("password-box-basic"),
                   QStringLiteral("PasswordBox with reveal button"),
                   QStringLiteral("Input stays concealed; hold the reveal button to peek."),
                   QStringLiteral("auto* passwordBox = new PasswordBox(this);\n"
                                  "passwordBox->setHeader(\"Password\");\n"
                                  "passwordBox->setPassword(\"Fluent123\");"),
                   [](QWidget* parent) {
                       auto* passwordBox = new PasswordBox(parent);
                       passwordBox->setHeader(QStringLiteral("Password"));
                       passwordBox->setPassword(QStringLiteral("Fluent123"));
                       passwordBox->setFixedWidth(280);
                       return passwordBox;
                   })
    };
}

QVector<GallerySample> textEditSamples()
{
    return {
        makeSample(QStringLiteral("text-edit-basic"),
                   QStringLiteral("Multi-line TextEdit"),
                   QStringLiteral("A multi-line surface with Fluent borders and scrollbars."),
                   QStringLiteral("auto* textEdit = new TextEdit(this);\n"
                                  "textEdit->setPlaceholderText(\"Type your notes here\");\n"
                                  "textEdit->setMinVisibleLines(4);"),
                   [](QWidget* parent) {
                       auto* textEdit = new TextEdit(parent);
                       textEdit->setPlaceholderText(QStringLiteral("Type your notes here"));
                       textEdit->setMinVisibleLines(4);
                       textEdit->setFixedWidth(360);
                       return textEdit;
                   })
    };
}

} // namespace

QVector<GallerySample> textFieldsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("auto-suggest-box"))
        return autoSuggestBoxSamples();
    if (routeId == QStringLiteral("label"))
        return labelSamples();
    if (routeId == QStringLiteral("line-edit"))
        return lineEditSamples();
    if (routeId == QStringLiteral("number-box"))
        return numberBoxSamples();
    if (routeId == QStringLiteral("password-box"))
        return passwordBoxSamples();
    if (routeId == QStringLiteral("text-edit"))
        return textEditSamples();
    return {};
}

} // namespace fluent::gallery
