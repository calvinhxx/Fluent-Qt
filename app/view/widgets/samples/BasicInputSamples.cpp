#include "BasicInputSamples.h"

#include <QLayout>
#include <QPoint>
#include <QUrl>

#include "components/basicinput/Button.h"
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/DropDownButton.h"
#include "components/basicinput/HyperlinkButton.h"
#include "components/basicinput/RadioButton.h"
#include "components/basicinput/RatingControl.h"
#include "components/basicinput/RepeatButton.h"
#include "components/basicinput/Slider.h"
#include "components/basicinput/SplitButton.h"
#include "components/basicinput/ToggleButton.h"
#include "components/basicinput/ToggleSplitButton.h"
#include "components/basicinput/ToggleSwitch.h"
#include "components/menus_toolbars/Menu.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::basicinput::CheckBox;
using fluent::basicinput::ComboBox;
using fluent::basicinput::DropDownButton;
using fluent::basicinput::HyperlinkButton;
using fluent::basicinput::RadioButton;
using fluent::basicinput::RatingControl;
using fluent::basicinput::RepeatButton;
using fluent::basicinput::Slider;
using fluent::basicinput::SplitButton;
using fluent::basicinput::ToggleButton;
using fluent::basicinput::ToggleSplitButton;
using fluent::basicinput::ToggleSwitch;
using fluent::menus_toolbars::FluentMenu;
using fluent::textfields::Label;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

Button* makeButton(QWidget* parent, const QString& text, Button::ButtonStyle style)
{
    auto* button = new Button(text, parent);
    button->setFluentStyle(style);
    button->setFluentSize(Button::StandardSize);
    return button;
}

Label* makeValueLabel(QWidget* parent, const QString& text)
{
    auto* label = new Label(text, parent);
    label->setFluentTypography(Typography::FontRole::Body);
    return label;
}

QVector<GallerySample> buttonSamples()
{
    return {
        makeSample(QStringLiteral("button-standard"),
                   QStringLiteral("Standard button"),
                   QStringLiteral("A button with the default neutral surface."),
                   QStringLiteral("auto* button = new Button(\"Standard\", this);\n"
                                  "button->setFluentStyle(Button::Standard);"),
                   [](QWidget* parent) {
                       return makeButton(parent, QStringLiteral("Standard"), Button::Standard);
                   }),
        makeSample(QStringLiteral("button-accent"),
                   QStringLiteral("Accent button"),
                   QStringLiteral("Use the accent style for the primary action."),
                   QStringLiteral("auto* button = new Button(\"Accent\", this);\n"
                                  "button->setFluentStyle(Button::Accent);"),
                   [](QWidget* parent) {
                       return makeButton(parent, QStringLiteral("Accent"), Button::Accent);
                   }),
        makeSample(QStringLiteral("button-disabled"),
                   QStringLiteral("Disabled button"),
                   QStringLiteral("A button that cannot be invoked while disabled."),
                   QStringLiteral("auto* button = new Button(\"Disabled\", this);\n"
                                  "button->setEnabled(false);"),
                   [](QWidget* parent) {
                       auto* button = makeButton(parent, QStringLiteral("Disabled"), Button::Standard);
                       button->setEnabled(false);
                       return button;
                   }),
        makeSample(QStringLiteral("button-icon"),
                   QStringLiteral("Icon button"),
                   QStringLiteral("Pair an icon glyph with the button label."),
                   QStringLiteral("auto* button = new Button(\"Add item\", this);\n"
                                  "button->setFluentLayout(Button::IconBefore);\n"
                                  "button->setIconGlyph(Typography::Icons::Add);"),
                   [](QWidget* parent) {
                       auto* button = makeButton(parent, QStringLiteral("Add item"), Button::Standard);
                       button->setFluentLayout(Button::IconBefore);
                       button->setIconGlyph(Typography::Icons::Add, Typography::FontSize::Body);
                       return button;
                   })
    };
}

QVector<GallerySample> checkBoxSamples()
{
    return {
        makeSample(QStringLiteral("checkbox-two-state"),
                   QStringLiteral("Two-state CheckBox"),
                   QStringLiteral("A simple on/off choice."),
                   QStringLiteral("auto* checkBox = new CheckBox(\"Accept terms\", this);\n"
                                  "checkBox->setChecked(true);"),
                   [](QWidget* parent) {
                       auto* checkBox = new CheckBox(QStringLiteral("Accept terms"), parent);
                       checkBox->setChecked(true);
                       return checkBox;
                   }),
        makeSample(QStringLiteral("checkbox-three-state"),
                   QStringLiteral("Three-state CheckBox"),
                   QStringLiteral("Tri-state adds an indeterminate middle state for partial selections."),
                   QStringLiteral("auto* checkBox = new CheckBox(\"Select all\", this);\n"
                                  "checkBox->setTristate(true);\n"
                                  "checkBox->setCheckState(Qt::PartiallyChecked);"),
                   [](QWidget* parent) {
                       auto* checkBox = new CheckBox(QStringLiteral("Select all"), parent);
                       checkBox->setTristate(true);
                       checkBox->setCheckState(Qt::PartiallyChecked);
                       return checkBox;
                   })
    };
}

QVector<GallerySample> comboBoxSamples()
{
    return {
        makeSample(QStringLiteral("combobox-basic"),
                   QStringLiteral("ComboBox with items"),
                   QStringLiteral("Pick a single option from a drop-down list."),
                   QStringLiteral("auto* comboBox = new ComboBox(this);\n"
                                  "comboBox->addItems({\"Blue\", \"Green\", \"Red\", \"Yellow\"});\n"
                                  "comboBox->setCurrentIndex(0);"),
                   [](QWidget* parent) {
                       auto* comboBox = new ComboBox(parent);
                       comboBox->addItems({QStringLiteral("Blue"),
                                           QStringLiteral("Green"),
                                           QStringLiteral("Red"),
                                           QStringLiteral("Yellow")});
                       comboBox->setCurrentIndex(0);
                       comboBox->setMinimumWidth(160);
                       return comboBox;
                   })
    };
}

QVector<GallerySample> dropDownButtonSamples()
{
    return {
        makeSample(QStringLiteral("dropdown-button-basic"),
                   QStringLiteral("DropDownButton command group"),
                   QStringLiteral("Use drop-down buttons for commands with related choices."),
                   QStringLiteral("auto* button = new DropDownButton(\"Email\", this);\n"
                                  "button->setFluentLayout(Button::IconBefore);\n"
                                  "button->setIconGlyph(Typography::Icons::Mail);\n"
                                  "auto* menu = new FluentMenu(QString(), button);\n"
                                  "menu->addAction(\"Send\");\n"
                                  "menu->addAction(\"Reply\");\n"
                                  "menu->addAction(\"Forward\");\n"
                                  "button->setMenu(menu);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);

                       auto* emailButton = new DropDownButton(QStringLiteral("Email"), group);
                       emailButton->setFluentLayout(Button::IconBefore);
                       emailButton->setIconGlyph(Typography::Icons::Mail,
                                                 Typography::FontSize::Caption);
                       emailButton->setMinimumWidth(132);
                       auto* emailMenu = new FluentMenu(QString(), emailButton);
                       emailMenu->addAction(QStringLiteral("Send"));
                       emailMenu->addAction(QStringLiteral("Reply"));
                       emailMenu->addAction(QStringLiteral("Forward"));
                       emailButton->setMenu(emailMenu);

                       auto* shareButton = new DropDownButton(QStringLiteral("Share"), group);
                       shareButton->setFluentStyle(Button::Accent);
                       shareButton->setFluentLayout(Button::IconBefore);
                       shareButton->setIconGlyph(Typography::Icons::Share,
                                                 Typography::FontSize::Caption);
                       shareButton->setMinimumWidth(132);
                       auto* shareMenu = new FluentMenu(QString(), shareButton);
                       shareMenu->addAction(QStringLiteral("Copy link"));
                       shareMenu->addAction(QStringLiteral("Email link"));
                       shareMenu->addAction(QStringLiteral("More options"));
                       shareButton->setMenu(shareMenu);

                       auto* moreButton = new DropDownButton(QString(), group);
                       moreButton->setFluentLayout(Button::IconOnly);
                       moreButton->setIconGlyph(Typography::Icons::More,
                                                Typography::FontSize::Caption);
                       moreButton->setChevronOffset(QPoint(8, 0));
                       moreButton->setFixedWidth(58);
                       auto* moreMenu = new FluentMenu(QString(), moreButton);
                       moreMenu->addAction(QStringLiteral("Rename"));
                       moreMenu->addAction(QStringLiteral("Move"));
                       moreMenu->addAction(QStringLiteral("Delete"));
                       moreButton->setMenu(moreMenu);

                       group->layout()->addWidget(emailButton);
                       group->layout()->addWidget(shareButton);
                       group->layout()->addWidget(moreButton);
                       return group;
                   })
    };
}

QVector<GallerySample> hyperlinkButtonSamples()
{
    return {
        makeSample(QStringLiteral("hyperlink-button-basic"),
                   QStringLiteral("HyperlinkButton with a URL"),
                   QStringLiteral("Clicking opens the URL in the default browser."),
                   QStringLiteral("auto* link = new HyperlinkButton(\n"
                                  "    \"calvinhxx/Fluent-QT\",\n"
                                  "    QUrl(\"https://github.com/calvinhxx/Fluent-QT\"), this);"),
                   [](QWidget* parent) {
                       return new HyperlinkButton(
                           QStringLiteral("calvinhxx/Fluent-QT"),
                           QUrl(QStringLiteral("https://github.com/calvinhxx/Fluent-QT")),
                           parent);
                   })
    };
}

QVector<GallerySample> radioButtonSamples()
{
    return {
        makeSample(QStringLiteral("radio-button-group"),
                   QStringLiteral("RadioButton group"),
                   QStringLiteral("Sibling radio buttons are mutually exclusive by default."),
                   QStringLiteral("auto* first = new RadioButton(\"Option 1\", this);\n"
                                  "auto* second = new RadioButton(\"Option 2\", this);\n"
                                  "first->setChecked(true);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* first = new RadioButton(QStringLiteral("Option 1"), group);
                       auto* second = new RadioButton(QStringLiteral("Option 2"), group);
                       auto* third = new RadioButton(QStringLiteral("Option 3"), group);
                       first->setChecked(true);
                       group->layout()->addWidget(first);
                       group->layout()->addWidget(second);
                       group->layout()->addWidget(third);
                       return group;
                   })
    };
}

QVector<GallerySample> ratingControlSamples()
{
    return {
        makeSample(QStringLiteral("rating-control-basic"),
                   QStringLiteral("RatingControl with a caption"),
                   QStringLiteral("Click or drag across the stars to set a rating."),
                   QStringLiteral("auto* rating = new RatingControl(this);\n"
                                  "rating->setValue(3);\n"
                                  "rating->setCaption(\"3 of 5 stars\");"),
                   [](QWidget* parent) {
                       auto* rating = new RatingControl(parent);
                       rating->setValue(3);
                       rating->setCaption(QStringLiteral("3 of 5 stars"));
                       QObject::connect(rating, &RatingControl::valueChanged,
                                        rating, [rating](double value) {
                                            rating->setCaption(QStringLiteral("%1 of 5 stars").arg(value));
                                        });
                       return rating;
                   })
    };
}

QVector<GallerySample> repeatButtonSamples()
{
    return {
        makeSample(QStringLiteral("repeat-button-basic"),
                   QStringLiteral("RepeatButton with a counter"),
                   QStringLiteral("Press and hold: the click event repeats while pressed."),
                   QStringLiteral("auto* button = new RepeatButton(\"Click and hold\", this);\n"
                                  "connect(button, &RepeatButton::clicked,\n"
                                  "        this, [this]() { ++m_count; });"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 12);
                       auto* button = new RepeatButton(QStringLiteral("Click and hold"), group);
                       Label* counter = makeValueLabel(group, QStringLiteral("Clicks: 0"));
                       QObject::connect(button, &RepeatButton::clicked,
                                        counter, [counter]() {
                                            const int count = counter->property("clickCount").toInt() + 1;
                                            counter->setProperty("clickCount", count);
                                            counter->setText(QStringLiteral("Clicks: %1").arg(count));
                                        });
                       group->layout()->addWidget(button);
                       group->layout()->addWidget(counter);
                       return group;
                   })
    };
}

QVector<GallerySample> sliderSamples()
{
    return {
        makeSample(QStringLiteral("slider-basic"),
                   QStringLiteral("Slider with a live value"),
                   QStringLiteral("Drag the handle to pick a value from the range."),
                   QStringLiteral("auto* slider = new Slider(Qt::Horizontal, this);\n"
                                  "slider->setRange(0, 100);\n"
                                  "slider->setValue(30);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 16);
                       auto* slider = new Slider(Qt::Horizontal, group);
                       slider->setRange(0, 100);
                       slider->setValue(30);
                       slider->setFixedWidth(240);
                       Label* value = makeValueLabel(group, QStringLiteral("30"));
                       QObject::connect(slider, &Slider::valueChanged,
                                        value, [value](int newValue) {
                                            value->setText(QString::number(newValue));
                                        });
                       group->layout()->addWidget(slider);
                       group->layout()->addWidget(value);
                       return group;
                   })
    };
}

QVector<GallerySample> splitButtonSamples()
{
    return {
        makeSample(QStringLiteral("split-button-basic"),
                   QStringLiteral("SplitButton with secondary menu"),
                   QStringLiteral("The primary half invokes the action; the chevron opens the menu."),
                   QStringLiteral("auto* button = new SplitButton(\"Save\", this);\n"
                                  "auto* menu = new FluentMenu(QString(), button);\n"
                                  "menu->addAction(\"Save as...\");\n"
                                  "button->setMenu(menu);"),
                   [](QWidget* parent) {
                       auto* button = new SplitButton(QStringLiteral("Save"), parent);
                       auto* menu = new FluentMenu(QString(), button);
                       menu->addAction(QStringLiteral("Save as..."));
                       menu->addAction(QStringLiteral("Save all"));
                       button->setMenu(menu);
                       return button;
                   })
    };
}

QVector<GallerySample> toggleButtonSamples()
{
    return {
        makeSample(QStringLiteral("toggle-button-basic"),
                   QStringLiteral("ToggleButton"),
                   QStringLiteral("Clicking flips between checked and unchecked."),
                   QStringLiteral("auto* toggle = new ToggleButton(\"Bold\", this);\n"
                                  "connect(toggle, &ToggleButton::toggled,\n"
                                  "        this, [](bool checked) { /* apply */ });"),
                   [](QWidget* parent) {
                       return new ToggleButton(QStringLiteral("Bold"), parent);
                   })
    };
}

QVector<GallerySample> toggleSplitButtonSamples()
{
    return {
        makeSample(QStringLiteral("toggle-split-button-basic"),
                   QStringLiteral("ToggleSplitButton with list styles"),
                   QStringLiteral("The primary half toggles; the chevron picks a variant."),
                   QStringLiteral("auto* button = new ToggleSplitButton(\"Bullets\", this);\n"
                                  "auto* menu = new FluentMenu(QString(), button);\n"
                                  "menu->addAction(\"Round bullets\");\n"
                                  "button->setMenu(menu);"),
                   [](QWidget* parent) {
                       auto* button = new ToggleSplitButton(QStringLiteral("Bullets"), parent);
                       auto* menu = new FluentMenu(QString(), button);
                       menu->addAction(QStringLiteral("Round bullets"));
                       menu->addAction(QStringLiteral("Square bullets"));
                       menu->addAction(QStringLiteral("Numbered list"));
                       button->setMenu(menu);
                       return button;
                   })
    };
}

QVector<GallerySample> toggleSwitchSamples()
{
    return {
        makeSample(QStringLiteral("toggle-switch-basic"),
                   QStringLiteral("ToggleSwitch with header and content"),
                   QStringLiteral("The on/off content mirrors the switch state."),
                   QStringLiteral("auto* toggle = new ToggleSwitch(this);\n"
                                  "toggle->setHeader(\"Wi-Fi\");\n"
                                  "toggle->setOnContent(\"On\");\n"
                                  "toggle->setOffContent(\"Off\");\n"
                                  "toggle->setIsOn(true);"),
                   [](QWidget* parent) {
                       auto* toggle = new ToggleSwitch(parent);
                       toggle->setHeader(QStringLiteral("Wi-Fi"));
                       toggle->setOnContent(QStringLiteral("On"));
                       toggle->setOffContent(QStringLiteral("Off"));
                       toggle->setIsOn(true);
                       return toggle;
                   })
    };
}

} // namespace

QVector<GallerySample> basicInputSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("button"))
        return buttonSamples();
    if (routeId == QStringLiteral("checkbox"))
        return checkBoxSamples();
    if (routeId == QStringLiteral("combobox"))
        return comboBoxSamples();
    if (routeId == QStringLiteral("dropdown-button"))
        return dropDownButtonSamples();
    if (routeId == QStringLiteral("hyperlink-button"))
        return hyperlinkButtonSamples();
    if (routeId == QStringLiteral("radio-button"))
        return radioButtonSamples();
    if (routeId == QStringLiteral("rating-control"))
        return ratingControlSamples();
    if (routeId == QStringLiteral("repeat-button"))
        return repeatButtonSamples();
    if (routeId == QStringLiteral("slider"))
        return sliderSamples();
    if (routeId == QStringLiteral("split-button"))
        return splitButtonSamples();
    if (routeId == QStringLiteral("toggle-button"))
        return toggleButtonSamples();
    if (routeId == QStringLiteral("toggle-split-button"))
        return toggleSplitButtonSamples();
    if (routeId == QStringLiteral("toggle-switch"))
        return toggleSwitchSamples();
    return {};
}

} // namespace fluent::gallery
