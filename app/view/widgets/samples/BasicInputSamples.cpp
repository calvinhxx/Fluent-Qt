#include "BasicInputSamples.h"

#include <QLayout>
#include <QPoint>
#include <QSignalBlocker>
#include <QStringList>
#include <QUrl>

#include "components/basicinput/Button.h"
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/ColorPicker.h"
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
#include "compatibility/QtCompat.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::Button;
using fluent::basicinput::CheckBox;
using fluent::basicinput::ColorPicker;
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
    // These status/value labels sit on the sample preview surface, which carries a style sheet — so a
    // palette-based text color is ignored and they rendered near-black in dark theme. An explicit role
    // colors them through the label's own style sheet, surviving the ancestor style sheet and theme
    // changes. zh_CN: 这些状态/取值标签位于带样式表的示例预览面上,palette 上色会被忽略、深色下渲染近黑;指定角色用
    // 标签自身样式表上色,可越过祖先样式表并跟随主题切换。
    label->setTextColorRole(Label::TextColorRole::Primary);
    return label;
}

QString checkStateText(Qt::CheckState state)
{
    if (state == Qt::Checked)
        return QStringLiteral("Checked");
    if (state == Qt::PartiallyChecked)
        return QStringLiteral("Mixed");
    return QStringLiteral("Unchecked");
}

template <typename Func>
void connectCheckBoxState(CheckBox* checkBox, QObject* context, Func callback)
{
    fluentConnectCheckStateChanged(checkBox, context, callback);
}

QString ratingText(double value)
{
    return value < 0.0 ? QStringLiteral("Unset")
                       : QStringLiteral("%1 stars").arg(value, 0, 'f', 1);
}

ComboBox* makeComboBox(QWidget* parent,
                       const QStringList& items,
                       int currentIndex,
                       int width = 200)
{
    auto* comboBox = new ComboBox(parent);
    comboBox->addItems(items);
    comboBox->setCurrentIndex(currentIndex);
    comboBox->setFixedWidth(width);
    return comboBox;
}

FluentMenu* makeMenu(QWidget* owner,
                     const QString& title,
                     const QStringList& actions)
{
    auto* menu = new FluentMenu(title, owner);
    for (const QString& action : actions)
        menu->addAction(action);
    return menu;
}

void setSwatchColor(QWidget* swatch, const QColor& color)
{
    swatch->setStyleSheet(
        QStringLiteral("background-color: %1; border: 1px solid rgba(0, 0, 0, 48); "
                       "border-radius: 6px;")
            .arg(color.name(QColor::HexArgb)));
}

QWidget* makeColorSwatch(QWidget* parent, const QColor& color)
{
    auto* swatch = new QWidget(parent);
    swatch->setFixedSize(64, 40);
    setSwatchColor(swatch, color);
    return swatch;
}

void setStableStatusWidth(Label* label, const QString& widestText)
{
    label->setMinimumWidth(label->fontMetrics().horizontalAdvance(widestText));
}

QVector<GallerySample> buttonSamples()
{
    return {
        makeSample(QStringLiteral("button-styles"),
                   QStringLiteral("Button styles"),
                   QStringLiteral("Standard, accent, and subtle styles cover neutral, primary, and lightweight commands."),
                   QStringLiteral("auto* standard = new Button(\"Standard\", this);\n"
                                  "standard->setFluentStyle(Button::Standard);\n\n"
                                  "auto* accent = new Button(\"Accent\", this);\n"
                                  "accent->setFluentStyle(Button::Accent);\n\n"
                                  "auto* subtle = new Button(\"Subtle\", this);\n"
                                  "subtle->setFluentStyle(Button::Subtle);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       group->layout()->addWidget(makeButton(group, QStringLiteral("Standard"), Button::Standard));
                       group->layout()->addWidget(makeButton(group, QStringLiteral("Accent"), Button::Accent));
                       group->layout()->addWidget(makeButton(group, QStringLiteral("Subtle"), Button::Subtle));
                       return group;
                   }),
        makeSample(QStringLiteral("button-sizes"),
                   QStringLiteral("Button sizes"),
                   QStringLiteral("Size presets change button height and padding without changing the command semantics."),
                   QStringLiteral("auto* small = new Button(\"Small\", this);\n"
                                  "small->setFluentSize(Button::Small);\n\n"
                                  "auto* standard = new Button(\"Standard\", this);\n"
                                  "standard->setFluentSize(Button::StandardSize);\n\n"
                                  "auto* large = new Button(\"Large\", this);\n"
                                  "large->setFluentSize(Button::Large);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       auto* small = makeButton(group, QStringLiteral("Small"), Button::Standard);
                       small->setFluentSize(Button::Small);
                       auto* standard = makeButton(group, QStringLiteral("Standard"), Button::Standard);
                       standard->setFluentSize(Button::StandardSize);
                       auto* large = makeButton(group, QStringLiteral("Large"), Button::Standard);
                       large->setFluentSize(Button::Large);
                       group->layout()->addWidget(small);
                       group->layout()->addWidget(standard);
                       group->layout()->addWidget(large);
                       return group;
                   }),
        makeSample(QStringLiteral("button-icon-layouts"),
                   QStringLiteral("Icon layouts"),
                   QStringLiteral("Icon glyphs can sit before text, after text, or become the whole button content."),
                   QStringLiteral("auto* before = new Button(\"Icon before\", this);\n"
                                  "before->setFluentLayout(Button::IconBefore);\n"
                                  "before->setIconGlyph(Typography::Icons::Add);\n\n"
                                  "auto* iconOnly = new Button(QString(), this);\n"
                                  "iconOnly->setFluentLayout(Button::IconOnly);\n"
                                  "iconOnly->setIconGlyph(Typography::Icons::More);\n\n"
                                  "auto* after = new Button(\"Next\", this);\n"
                                  "after->setFluentLayout(Button::IconAfter);\n"
                                  "after->setIconGlyph(Typography::Icons::ChevronRight);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       auto* before = makeButton(group, QStringLiteral("Icon before"), Button::Standard);
                       before->setFluentLayout(Button::IconBefore);
                       before->setIconGlyph(Typography::Icons::Add, Typography::IconSize::Standard);

                       auto* iconOnly = makeButton(group, QString(), Button::Standard);
                       iconOnly->setFluentLayout(Button::IconOnly);
                       iconOnly->setIconGlyph(Typography::Icons::More, Typography::IconSize::Standard);
                       iconOnly->setFixedSize(40, 40);

                       auto* after = makeButton(group, QStringLiteral("Next"), Button::Standard);
                       after->setFluentLayout(Button::IconAfter);
                       after->setIconGlyph(Typography::Icons::ChevronRight, Typography::IconSize::Standard);

                       group->layout()->addWidget(before);
                       group->layout()->addWidget(iconOnly);
                       group->layout()->addWidget(after);
                       return group;
                   }),
        makeSample(QStringLiteral("button-interaction-state"),
                   QStringLiteral("Interaction state preview"),
                   QStringLiteral("Forced interaction states make hover, pressed, and disabled visuals easy to compare."),
                   QStringLiteral("auto* hover = new Button(\"Hover\", this);\n"
                                  "hover->setInteractionState(Button::Hover);\n\n"
                                  "auto* pressed = new Button(\"Pressed\", this);\n"
                                  "pressed->setInteractionState(Button::Pressed);\n\n"
                                  "auto* disabled = new Button(\"Disabled\", this);\n"
                                  "disabled->setInteractionState(Button::Disabled);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       auto* rest = makeButton(group, QStringLiteral("Rest"), Button::Standard);
                       auto* hover = makeButton(group, QStringLiteral("Hover"), Button::Standard);
                       hover->setInteractionState(Button::Hover);
                       auto* pressed = makeButton(group, QStringLiteral("Pressed"), Button::Standard);
                       pressed->setInteractionState(Button::Pressed);
                       auto* disabled = makeButton(group, QStringLiteral("Disabled"), Button::Standard);
                       disabled->setInteractionState(Button::Disabled);
                       group->layout()->addWidget(rest);
                       group->layout()->addWidget(hover);
                       group->layout()->addWidget(pressed);
                       group->layout()->addWidget(disabled);
                       return group;
                   }),
        makeSample(QStringLiteral("button-critical-hover"),
                   QStringLiteral("Critical hover action"),
                   QStringLiteral("Critical hover uses the danger color for destructive commands while keeping the button neutral at rest."),
                   QStringLiteral("auto* deleteButton = new Button(\"Delete\", this);\n"
                                  "deleteButton->setFluentLayout(Button::IconBefore);\n"
                                  "deleteButton->setIconGlyph(Typography::Icons::Delete);\n"
                                  "deleteButton->setCriticalOnHover(true);\n"
                                  "deleteButton->setInteractionState(Button::Hover);"),
                   [](QWidget* parent) {
                       auto* button = makeButton(parent, QStringLiteral("Delete"), Button::Standard);
                       button->setFluentLayout(Button::IconBefore);
                       button->setIconGlyph(Typography::Icons::Delete, Typography::IconSize::Standard);
                       button->setCriticalOnHover(true);
                       button->setInteractionState(Button::Hover);
                       return button;
                   })
    };
}

QVector<GallerySample> checkBoxSamples()
{
    return {
        makeSample(QStringLiteral("checkbox-two-state"),
                   QStringLiteral("Two-state CheckBox"),
                   QStringLiteral("A two-state checkbox represents a simple on/off choice and reports the current state."),
                   QStringLiteral("auto* checkBox = new CheckBox(\"Accept terms\", this);\n"
                                  "auto* status = new Label(\"State: Unchecked\", this);\n"
                                  "connect(checkBox, &CheckBox::checkStateChanged,\n"
                                  "        status, [status](Qt::CheckState state) {\n"
                                  "            status->setText(state == Qt::Checked\n"
                                  "                ? \"State: Checked\"\n"
                                  "                : \"State: Unchecked\");\n"
                                  "        });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* checkBox = new CheckBox(QStringLiteral("Accept terms"), group);
                       auto* status = makeValueLabel(group, QStringLiteral("State: Unchecked"));
                       setStableStatusWidth(status, QStringLiteral("State: Unchecked"));
                       connectCheckBoxState(checkBox, status, [status](Qt::CheckState state) {
                                            status->setText(state == Qt::Checked
                                                ? QStringLiteral("State: Checked")
                                                : QStringLiteral("State: Unchecked"));
                                        });
                       group->layout()->addWidget(checkBox);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("checkbox-three-state"),
                   QStringLiteral("Three-state CheckBox"),
                   QStringLiteral("Tri-state adds a mixed state for partially selected sets."),
                   QStringLiteral("auto* checkBox = new CheckBox(\"Enable selected items\", this);\n"
                                  "checkBox->setTristate(true);\n"
                                  "checkBox->setCheckState(Qt::PartiallyChecked);\n"
                                  "connect(checkBox, &CheckBox::checkStateChanged,\n"
                                  "        status, [status](Qt::CheckState state) { /* show Checked/Mixed/Unchecked */ });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* checkBox = new CheckBox(QStringLiteral("Enable selected items"), group);
                       checkBox->setTristate(true);
                       checkBox->setCheckState(Qt::PartiallyChecked);
                       auto* status = makeValueLabel(group, QStringLiteral("State: Mixed"));
                       setStableStatusWidth(status, QStringLiteral("State: Unchecked"));
                       connectCheckBoxState(checkBox, status, [status](Qt::CheckState state) {
                                            status->setText(QStringLiteral("State: %1").arg(
                                                checkStateText(state)));
                                        });
                       group->layout()->addWidget(checkBox);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("checkbox-select-all"),
                   QStringLiteral("Select all pattern"),
                   QStringLiteral("A parent three-state checkbox can summarize and control child selections."),
                   QStringLiteral("auto* selectAll = new CheckBox(\"Select all\", this);\n"
                                  "selectAll->setTristate(true);\n"
                                  "auto* mail = new CheckBox(\"Mail\", this);\n"
                                  "auto* calendar = new CheckBox(\"Calendar\", this);\n"
                                  "auto* people = new CheckBox(\"People\", this);\n\n"
                                  "auto updateSelectAll = [=]() {\n"
                                  "    const int checked = int(mail->isChecked())\n"
                                  "        + int(calendar->isChecked()) + int(people->isChecked());\n"
                                  "    selectAll->setCheckState(checked == 0 ? Qt::Unchecked\n"
                                  "        : checked == 3 ? Qt::Checked : Qt::PartiallyChecked);\n"
                                  "};\n"
                                  "connect(selectAll, &CheckBox::clicked, this, [=]() {\n"
                                  "    const bool checked = selectAll->checkState() == Qt::Checked;\n"
                                  "    mail->setChecked(checked);\n"
                                  "    calendar->setChecked(checked);\n"
                                  "    people->setChecked(checked);\n"
                                  "});\n"
                                  "connect(mail, &CheckBox::clicked, this, updateSelectAll);\n"
                                  "connect(calendar, &CheckBox::clicked, this, updateSelectAll);\n"
                                  "connect(people, &CheckBox::clicked, this, updateSelectAll);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* selectAll = new CheckBox(QStringLiteral("Select all"), group);
                       selectAll->setTristate(true);

                       QWidget* children = verticalGroup(group, 4);
                       children->layout()->setContentsMargins(28, 0, 0, 0);
                       auto* mail = new CheckBox(QStringLiteral("Mail"), children);
                       auto* calendar = new CheckBox(QStringLiteral("Calendar"), children);
                       auto* people = new CheckBox(QStringLiteral("People"), children);
                       mail->setChecked(true);
                       calendar->setChecked(true);
                       children->layout()->addWidget(mail);
                       children->layout()->addWidget(calendar);
                       children->layout()->addWidget(people);

                       auto updateSelectAll = [selectAll, mail, calendar, people]() {
                           const int checked = int(mail->isChecked()) + int(calendar->isChecked()) +
                                               int(people->isChecked());
                           const QSignalBlocker blocker(selectAll);
                           selectAll->setCheckState(checked == 0 ? Qt::Unchecked
                               : checked == 3 ? Qt::Checked : Qt::PartiallyChecked);
                       };
                       updateSelectAll();

                       QObject::connect(selectAll, &CheckBox::clicked,
                                        group, [selectAll, mail, calendar, people]() {
                                            const bool checked = selectAll->checkState() == Qt::Checked;
                                            mail->setChecked(checked);
                                            calendar->setChecked(checked);
                                            people->setChecked(checked);
                                        });
                       QObject::connect(mail, &CheckBox::clicked, group, updateSelectAll);
                       QObject::connect(calendar, &CheckBox::clicked, group, updateSelectAll);
                       QObject::connect(people, &CheckBox::clicked, group, updateSelectAll);

                       group->layout()->addWidget(selectAll);
                       group->layout()->addWidget(children);
                       return group;
                   }),
        makeSample(QStringLiteral("checkbox-metrics"),
                   QStringLiteral("CheckBox metrics"),
                   QStringLiteral("Box size, leading margin, text gap, and hover background can be tuned for dense layouts."),
                   QStringLiteral("auto* large = new CheckBox(\"Larger box\", this);\n"
                                  "large->setBoxSize(24);\n\n"
                                  "auto* compact = new CheckBox(\"Compact spacing\", this);\n"
                                  "compact->setBoxSize(16);\n"
                                  "compact->setBoxMargin(4);\n"
                                  "compact->setTextGap(6);\n\n"
                                  "auto* hoverRow = new CheckBox(\"Hover row background\", this);\n"
                                  "hoverRow->setHoverBackgroundEnabled(true);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* large = new CheckBox(QStringLiteral("Larger box"), group);
                       large->setBoxSize(24);
                       auto* compact = new CheckBox(QStringLiteral("Compact spacing"), group);
                       compact->setBoxSize(16);
                       compact->setBoxMargin(4);
                       compact->setTextGap(6);
                       auto* hoverRow = new CheckBox(QStringLiteral("Hover row background"), group);
                       hoverRow->setHoverBackgroundEnabled(true);
                       hoverRow->setCheckState(Qt::Checked);
                       group->layout()->addWidget(large);
                       group->layout()->addWidget(compact);
                       group->layout()->addWidget(hoverRow);
                       return group;
                   })
    };
}

QVector<GallerySample> colorPickerSamples()
{
    return {
        makeSample(QStringLiteral("color-picker-rgba"),
                   QStringLiteral("RGBA color value"),
                   QStringLiteral("The picker synchronizes spectrum, channel inputs, alpha, and the QColor value."),
                   QStringLiteral("auto* picker = new ColorPicker(this);\n"
                                  "picker->setColor(QColor(0, 120, 212, 180));\n"
                                  "connect(picker, &ColorPicker::colorChanged,\n"
                                  "        this, [](const QColor& color) { /* apply color */ });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 12);
                       const QColor initial(0, 120, 212, 180);
                       auto* picker = new ColorPicker(group);
                       picker->setColor(initial);
                       picker->setMinimumSize(420, 480);

                       QWidget* statusRow = horizontalGroup(group, 10);
                       auto* swatch = makeColorSwatch(statusRow, initial);
                       auto* status = makeValueLabel(statusRow,
                           QStringLiteral("Color: %1").arg(initial.name(QColor::HexArgb).toUpper()));
                       QObject::connect(picker, &ColorPicker::colorChanged,
                                        statusRow, [swatch, status](const QColor& color) {
                                            setSwatchColor(swatch, color);
                                            status->setText(QStringLiteral("Color: %1").arg(
                                                color.name(QColor::HexArgb).toUpper()));
                                        });
                       statusRow->layout()->addWidget(swatch);
                       statusRow->layout()->addWidget(status);

                       group->layout()->addWidget(picker);
                       group->layout()->addWidget(statusRow);
                       return group;
                   }),
        makeSample(QStringLiteral("color-picker-opaque"),
                   QStringLiteral("Opaque color picker"),
                   QStringLiteral("Disabling alpha hides the transparency channel while keeping RGB and hex editing."),
                   QStringLiteral("auto* picker = new ColorPicker(this);\n"
                                  "picker->setAlphaEnabled(false);\n"
                                  "picker->setColor(QColor(16, 124, 16));"),
                   [](QWidget* parent) {
                       auto* picker = new ColorPicker(parent);
                       picker->setAlphaEnabled(false);
                       picker->setColor(QColor(16, 124, 16));
                       picker->setMinimumSize(420, 420);
                       return picker;
                   })
    };
}

QVector<GallerySample> comboBoxSamples()
{
    return {
        makeSample(QStringLiteral("combobox-selection"),
                   QStringLiteral("ComboBox selection"),
                   QStringLiteral("Selecting an item updates the current index and current text."),
                   QStringLiteral("auto* comboBox = new ComboBox(this);\n"
                                  "comboBox->addItems({\"Blue\", \"Green\", \"Red\", \"Yellow\"});\n"
                                  "comboBox->setCurrentIndex(0);\n"
                                  "connect(comboBox, qOverload<int>(&ComboBox::currentIndexChanged),\n"
                                  "        this, [=](int index) {\n"
                                  "            status->setText(comboBox->currentText());\n"
                                  "        });"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 12);
                       auto* comboBox = makeComboBox(group,
                           {QStringLiteral("Blue"), QStringLiteral("Green"),
                            QStringLiteral("Red"), QStringLiteral("Yellow")},
                           0);
                       auto* status = makeValueLabel(group, QStringLiteral("Selected: Blue"));
                       setStableStatusWidth(status, QStringLiteral("Selected: Yellow"));
                       QObject::connect(comboBox, qOverload<int>(&ComboBox::currentIndexChanged),
                                        status, [comboBox, status](int) {
                                            status->setText(QStringLiteral("Selected: %1").arg(comboBox->currentText()));
                                        });
                       group->layout()->addWidget(comboBox);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("combobox-editable"),
                   QStringLiteral("Editable ComboBox"),
                   QStringLiteral("Editable mode lets users type a value while still offering known choices."),
                   QStringLiteral("auto* comboBox = new ComboBox(this);\n"
                                  "comboBox->addItems({\"8\", \"9\", \"10\", \"11\", \"12\", \"14\", \"16\"});\n"
                                  "comboBox->setEditable(true);\n"
                                  "comboBox->setCurrentIndex(4);"),
                   [](QWidget* parent) {
                       auto* comboBox = makeComboBox(parent,
                           {QStringLiteral("8"), QStringLiteral("9"), QStringLiteral("10"),
                            QStringLiteral("11"), QStringLiteral("12"), QStringLiteral("14"),
                            QStringLiteral("16")},
                           4);
                       comboBox->setEditable(true);
                       return comboBox;
                   }),
        makeSample(QStringLiteral("combobox-many-items"),
                   QStringLiteral("Popup with many items"),
                   QStringLiteral("Long option lists use the ComboBox popup ListView and keep the selected item visible."),
                   QStringLiteral("auto* comboBox = new ComboBox(this);\n"
                                  "QStringList items;\n"
                                  "for (int i = 1; i <= 20; ++i)\n"
                                  "    items << QString(\"Item %1\").arg(i);\n"
                                  "comboBox->addItems(items);\n"
                                  "comboBox->setCurrentIndex(5);"),
                   [](QWidget* parent) {
                       QStringList items;
                       for (int i = 1; i <= 20; ++i)
                           items << QStringLiteral("Item %1").arg(i);
                       return makeComboBox(parent, items, 5);
                   }),
        makeSample(QStringLiteral("combobox-appearance"),
                   QStringLiteral("Typography and chevron"),
                   QStringLiteral("Font role, padding, chevron glyph, and popup offset are exposed for compact field layouts."),
                   QStringLiteral("auto* comboBox = new ComboBox(this);\n"
                                  "comboBox->addItems({\"Compact\", \"Comfortable\", \"Spacious\"});\n"
                                  "comboBox->setFontRole(Typography::FontRole::Caption);\n"
                                  "comboBox->setContentPaddingH(12);\n"
                                  "comboBox->setChevronGlyph(Typography::Icons::ChevronDown);\n"
                                  "comboBox->setPopupOffset(8);"),
                   [](QWidget* parent) {
                       auto* comboBox = makeComboBox(parent,
                           {QStringLiteral("Compact"), QStringLiteral("Comfortable"), QStringLiteral("Spacious")},
                           1,
                           180);
                       comboBox->setFontRole(Typography::FontRole::Caption);
                       comboBox->setContentPaddingH(12);
                       comboBox->setChevronGlyph(Typography::Icons::ChevronDown);
                       comboBox->setPopupOffset(8);
                       return comboBox;
                   })
    };
}

QVector<GallerySample> dropDownButtonSamples()
{
    return {
        makeSample(QStringLiteral("dropdown-button-menu"),
                   QStringLiteral("DropDownButton menu"),
                   QStringLiteral("A drop-down button opens a menu of related commands without invoking a primary action."),
                   QStringLiteral("auto* button = new DropDownButton(\"Options\", this);\n"
                                  "auto* menu = new FluentMenu(\"Options\", button);\n"
                                  "menu->addAction(\"Edit profile\");\n"
                                  "menu->addAction(\"Account settings\");\n"
                                  "menu->addAction(\"Sign out\");\n"
                                  "button->setMenu(menu);"),
                   [](QWidget* parent) {
                       auto* button = new DropDownButton(QStringLiteral("Options"), parent);
                       button->setMinimumWidth(140);
                       button->setMenu(makeMenu(button, QStringLiteral("Options"),
                           {QStringLiteral("Edit profile"), QStringLiteral("Account settings"),
                            QStringLiteral("Sign out")}));
                       return button;
                   }),
        makeSample(QStringLiteral("dropdown-button-accent"),
                   QStringLiteral("Accent DropDownButton"),
                   QStringLiteral("Accent style marks the menu entry as the primary command group."),
                   QStringLiteral("auto* button = new DropDownButton(\"Primary action\", this);\n"
                                  "button->setFluentStyle(Button::Accent);\n"
                                  "auto* menu = new FluentMenu(\"Primary action\", button);\n"
                                  "menu->addAction(\"Confirm selection\");\n"
                                  "menu->addAction(\"Review changes\");\n"
                                  "button->setMenu(menu);"),
                   [](QWidget* parent) {
                       auto* button = new DropDownButton(QStringLiteral("Primary action"), parent);
                       button->setFluentStyle(Button::Accent);
                       button->setMinimumWidth(168);
                       button->setMenu(makeMenu(button, QStringLiteral("Primary action"),
                           {QStringLiteral("Confirm selection"), QStringLiteral("Review changes")}));
                       return button;
                   }),
        makeSample(QStringLiteral("dropdown-button-chevron"),
                   QStringLiteral("Chevron glyphs"),
                   QStringLiteral("Chevron glyph, size, and offset can adapt the affordance to different command shapes."),
                   QStringLiteral("auto* up = new DropDownButton(\"Chevron up\", this);\n"
                                  "up->setChevronGlyph(Typography::Icons::ChevronUp);\n"
                                  "up->setChevronSize(16);\n\n"
                                  "auto* more = new DropDownButton(\"More actions\", this);\n"
                                  "more->setChevronGlyph(Typography::Icons::More);\n"
                                  "more->setChevronOffset(QPoint(16, 3));"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       auto* up = new DropDownButton(QStringLiteral("Chevron up"), group);
                       up->setChevronGlyph(Typography::Icons::ChevronUp);
                       up->setChevronSize(16);
                       up->setMinimumWidth(150);
                       auto* more = new DropDownButton(QStringLiteral("More actions"), group);
                       more->setChevronGlyph(Typography::Icons::More);
                       more->setChevronOffset(QPoint(16, 3));
                       more->setMinimumWidth(160);
                       group->layout()->addWidget(up);
                       group->layout()->addWidget(more);
                       return group;
                   }),
        makeSample(QStringLiteral("dropdown-button-icon-layout"),
                   QStringLiteral("Icon DropDownButton"),
                   QStringLiteral("The inherited Button layout controls text/icon placement while the chevron stays reserved on the right."),
                   QStringLiteral("auto* iconOnly = new DropDownButton(QString(), this);\n"
                                  "iconOnly->setFluentLayout(Button::IconOnly);\n"
                                  "iconOnly->setIconGlyph(Typography::Icons::Send);\n"
                                  "iconOnly->setChevronOffset(QPoint(10, 0));\n\n"
                                  "auto* iconText = new DropDownButton(\"More actions\", this);\n"
                                  "iconText->setFluentLayout(Button::IconBefore);\n"
                                  "iconText->setIconGlyph(Typography::Icons::More);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       auto* iconOnly = new DropDownButton(QString(), group);
                       iconOnly->setFluentLayout(Button::IconOnly);
                       iconOnly->setIconGlyph(Typography::Icons::Send, Typography::IconSize::Standard);
                       iconOnly->setChevronOffset(QPoint(10, 0));
                       iconOnly->setFixedSize(58, 34);
                       auto* iconText = new DropDownButton(QStringLiteral("More actions"), group);
                       iconText->setFluentLayout(Button::IconBefore);
                       iconText->setIconGlyph(Typography::Icons::More, Typography::IconSize::Standard);
                       iconText->setMinimumWidth(170);
                       group->layout()->addWidget(iconOnly);
                       group->layout()->addWidget(iconText);
                       return group;
                   })
    };
}

QVector<GallerySample> hyperlinkButtonSamples()
{
    return {
        makeSample(QStringLiteral("hyperlink-button-url"),
                   QStringLiteral("HyperlinkButton with URL"),
                   QStringLiteral("A URL-backed hyperlink opens its target when invoked."),
                   QStringLiteral("auto* link = new HyperlinkButton(\"calvinhxx/Fluent-Qt\", this);\n"
                                  "link->setUrl(QUrl(\"https://github.com/calvinhxx/Fluent-Qt\"));"),
                   [](QWidget* parent) {
                       auto* link = new HyperlinkButton(QStringLiteral("calvinhxx/Fluent-Qt"), parent);
                       link->setUrl(QUrl(QStringLiteral("https://github.com/calvinhxx/Fluent-Qt")));
                       return link;
                   }),
        makeSample(QStringLiteral("hyperlink-button-underline"),
                   QStringLiteral("Underline feedback"),
                   QStringLiteral("Underline can stay enabled for links that need stronger affordance."),
                   QStringLiteral("auto* link = new HyperlinkButton(\"Show underline\", this);\n"
                                  "link->setShowUnderline(true);"),
                   [](QWidget* parent) {
                       auto* link = new HyperlinkButton(QStringLiteral("Show underline"), parent);
                       link->setShowUnderline(true);
                       return link;
                   })
    };
}

QVector<GallerySample> radioButtonSamples()
{
    return {
        makeSample(QStringLiteral("radio-button-group"),
                   QStringLiteral("RadioButton group"),
                   QStringLiteral("Sibling radio buttons are mutually exclusive and update the selected option."),
                   QStringLiteral("auto* low = new RadioButton(\"Low\", this);\n"
                                  "auto* medium = new RadioButton(\"Medium\", this);\n"
                                  "auto* high = new RadioButton(\"High\", this);\n"
                                  "medium->setChecked(true);\n"
                                  "connect(high, &RadioButton::toggled,\n"
                                  "        this, [=](bool checked) { /* update selected option */ });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* low = new RadioButton(QStringLiteral("Low"), group);
                       auto* medium = new RadioButton(QStringLiteral("Medium"), group);
                       auto* high = new RadioButton(QStringLiteral("High"), group);
                       medium->setChecked(true);
                       auto* status = makeValueLabel(group, QStringLiteral("Selected: Medium"));
                       auto connectStatus = [status](RadioButton* radio) {
                           QObject::connect(radio, &RadioButton::toggled,
                                            status, [radio, status](bool checked) {
                                                if (checked)
                                                    status->setText(QStringLiteral("Selected: %1").arg(radio->text()));
                                            });
                       };
                       connectStatus(low);
                       connectStatus(medium);
                       connectStatus(high);
                       group->layout()->addWidget(low);
                       group->layout()->addWidget(medium);
                       group->layout()->addWidget(high);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("radio-button-metrics"),
                   QStringLiteral("RadioButton metrics"),
                   QStringLiteral("Circle size, text gap, and font can be tuned for dense or emphasized choices."),
                   QStringLiteral("auto* compact = new RadioButton(\"Compact\", this);\n"
                                  "compact->setCircleSize(16);\n"
                                  "compact->setTextGap(6);\n\n"
                                  "auto* large = new RadioButton(\"Large\", this);\n"
                                  "large->setCircleSize(24);\n"
                                  "large->setTextGap(12);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* compact = new RadioButton(QStringLiteral("Compact"), group);
                       compact->setCircleSize(16);
                       compact->setTextGap(6);
                       compact->setChecked(true);
                       auto* large = new RadioButton(QStringLiteral("Large"), group);
                       large->setCircleSize(24);
                       large->setTextGap(12);
                       group->layout()->addWidget(compact);
                       group->layout()->addWidget(large);
                       return group;
                   }),
        makeSample(QStringLiteral("radio-button-disabled"),
                   QStringLiteral("Disabled states"),
                   QStringLiteral("Disabled radios preserve checked and unchecked state while blocking interaction."),
                   QStringLiteral("auto* disabledOff = new RadioButton(\"Disabled off\", this);\n"
                                  "disabledOff->setEnabled(false);\n\n"
                                  "auto* disabledOn = new RadioButton(\"Disabled on\", this);\n"
                                  "disabledOn->setChecked(true);\n"
                                  "disabledOn->setEnabled(false);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 16);
                       auto* disabledOff = new RadioButton(QStringLiteral("Disabled off"), group);
                       disabledOff->setEnabled(false);
                       auto* disabledOn = new RadioButton(QStringLiteral("Disabled on"), group);
                       disabledOn->setChecked(true);
                       disabledOn->setEnabled(false);
                       group->layout()->addWidget(disabledOff);
                       group->layout()->addWidget(disabledOn);
                       return group;
                   })
    };
}

QVector<GallerySample> ratingControlSamples()
{
    return {
        makeSample(QStringLiteral("rating-control-value"),
                   QStringLiteral("Rating value"),
                   QStringLiteral("The value signal can update both a caption and an external status label."),
                   QStringLiteral("auto* rating = new RatingControl(this);\n"
                                  "rating->setCaption(\"312 ratings\");\n"
                                  "connect(rating, &RatingControl::valueChanged,\n"
                                  "        this, [=](double value) {\n"
                                  "            rating->setCaption(\"Your rating\");\n"
                                  "            status->setText(QString(\"Value: %1\").arg(value));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* rating = new RatingControl(group);
                       rating->setCaption(QStringLiteral("312 ratings"));
                       auto* status = makeValueLabel(group, QStringLiteral("Value: Unset"));
                       QObject::connect(rating, &RatingControl::valueChanged,
                                        status, [rating, status](double value) {
                                            rating->setCaption(QStringLiteral("Your rating"));
                                            status->setText(QStringLiteral("Value: %1").arg(ratingText(value)));
                                        });
                       group->layout()->addWidget(rating);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("rating-control-placeholder"),
                   QStringLiteral("Placeholder value"),
                   QStringLiteral("A placeholder previews a suggested rating before the user commits a value."),
                   QStringLiteral("auto* rating = new RatingControl(this);\n"
                                  "rating->setPlaceholderValue(3.5);\n"
                                  "rating->setCaption(\"Suggested rating\");"),
                   [](QWidget* parent) {
                       auto* rating = new RatingControl(parent);
                       rating->setPlaceholderValue(3.5);
                       rating->setCaption(QStringLiteral("Suggested rating"));
                       return rating;
                   }),
        makeSample(QStringLiteral("rating-control-readonly"),
                   QStringLiteral("Read-only and disabled"),
                   QStringLiteral("Read-only keeps the value visible without allowing edits; disabled also changes the visual state."),
                   QStringLiteral("auto* readOnly = new RatingControl(this);\n"
                                  "readOnly->setValue(4.0);\n"
                                  "readOnly->setIsReadOnly(true);\n\n"
                                  "auto* disabled = new RatingControl(this);\n"
                                  "disabled->setValue(2.5);\n"
                                  "disabled->setEnabled(false);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* readOnly = new RatingControl(group);
                       readOnly->setValue(4.0);
                       readOnly->setIsReadOnly(true);
                       readOnly->setCaption(QStringLiteral("Read-only"));
                       auto* disabled = new RatingControl(group);
                       disabled->setValue(2.5);
                       disabled->setEnabled(false);
                       disabled->setCaption(QStringLiteral("Disabled"));
                       group->layout()->addWidget(readOnly);
                       group->layout()->addWidget(disabled);
                       return group;
                   }),
        makeSample(QStringLiteral("rating-control-max-size"),
                   QStringLiteral("Max rating and star size"),
                   QStringLiteral("Max rating and star size change the visible scale while preserving the same value API."),
                   QStringLiteral("auto* rating = new RatingControl(this);\n"
                                  "rating->setMaxRating(10);\n"
                                  "rating->setStarSize(20);\n"
                                  "rating->setPlaceholderValue(7.0);"),
                   [](QWidget* parent) {
                       auto* rating = new RatingControl(parent);
                       rating->setMaxRating(10);
                       rating->setStarSize(20);
                       rating->setPlaceholderValue(7.0);
                       rating->setCaption(QStringLiteral("10 point scale"));
                       return rating;
                   })
    };
}

QVector<GallerySample> repeatButtonSamples()
{
    return {
        makeSample(QStringLiteral("repeat-button-counter"),
                   QStringLiteral("RepeatButton counter"),
                   QStringLiteral("Press and hold: the click signal repeats after the initial delay."),
                   QStringLiteral("auto* button = new RepeatButton(\"Click and hold\", this);\n"
                                  "auto* counter = new Label(\"Clicks: 0\", this);\n"
                                  "connect(button, &RepeatButton::clicked,\n"
                                  "        counter, [counter]() { ++count; });"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 12);
                       auto* button = new RepeatButton(QStringLiteral("Click and hold"), group);
                       auto* counter = makeValueLabel(group, QStringLiteral("Clicks: 0"));
                       setStableStatusWidth(counter, QStringLiteral("Clicks: 8888"));
                       QObject::connect(button, &RepeatButton::clicked,
                                        counter, [counter]() {
                                            const int count = counter->property("clickCount").toInt() + 1;
                                            counter->setProperty("clickCount", count);
                                            counter->setText(QStringLiteral("Clicks: %1").arg(count));
                                        });
                       group->layout()->addWidget(button);
                       group->layout()->addWidget(counter);
                       return group;
                   }),
        makeSample(QStringLiteral("repeat-button-timing"),
                   QStringLiteral("Repeat timing"),
                   QStringLiteral("Delay and interval tune when repeated clicks start and how quickly they fire."),
                   QStringLiteral("auto* normal = new RepeatButton(\"Normal\", this);\n"
                                  "normal->setDelay(300);\n"
                                  "normal->setInterval(100);\n\n"
                                  "auto* fast = new RepeatButton(\"Fast\", this);\n"
                                  "fast->setDelay(150);\n"
                                  "fast->setInterval(30);\n"
                                  "fast->setFluentStyle(Button::Accent);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       auto* normal = new RepeatButton(QStringLiteral("Normal"), group);
                       normal->setDelay(300);
                       normal->setInterval(100);
                       auto* fast = new RepeatButton(QStringLiteral("Fast"), group);
                       fast->setDelay(150);
                       fast->setInterval(30);
                       fast->setFluentStyle(Button::Accent);
                       group->layout()->addWidget(normal);
                       group->layout()->addWidget(fast);
                       return group;
                   })
    };
}

QVector<GallerySample> sliderSamples()
{
    return {
        makeSample(QStringLiteral("slider-live-value"),
                   QStringLiteral("Slider live value"),
                   QStringLiteral("Drag the handle to select a value and mirror it in external UI."),
                   QStringLiteral("auto* slider = new Slider(Qt::Horizontal, this);\n"
                                  "slider->setRange(0, 100);\n"
                                  "slider->setValue(32);\n"
                                  "connect(slider, &Slider::valueChanged,\n"
                                  "        status, [status](int value) {\n"
                                  "            status->setText(QString(\"Value: %1\").arg(value));\n"
                                  "        });"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 16);
                       auto* slider = new Slider(Qt::Horizontal, group);
                       slider->setRange(0, 100);
                       slider->setValue(32);
                       slider->setFixedWidth(260);
                       auto* status = makeValueLabel(group, QStringLiteral("Value: 32"));
                       setStableStatusWidth(status, QStringLiteral("Value: 100"));
                       QObject::connect(slider, &Slider::valueChanged,
                                        status, [status](int value) {
                                            status->setText(QStringLiteral("Value: %1").arg(value));
                                        });
                       group->layout()->addWidget(slider);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("slider-range-steps"),
                   QStringLiteral("Range and steps"),
                   QStringLiteral("Minimum, maximum, single step, and page step define the slider's numeric contract."),
                   QStringLiteral("auto* slider = new Slider(Qt::Horizontal, this);\n"
                                  "slider->setRange(500, 1000);\n"
                                  "slider->setSingleStep(10);\n"
                                  "slider->setPageStep(50);\n"
                                  "slider->setValue(800);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 16);
                       auto* slider = new Slider(Qt::Horizontal, group);
                       slider->setRange(500, 1000);
                       slider->setSingleStep(10);
                       slider->setPageStep(50);
                       slider->setValue(800);
                       slider->setFixedWidth(260);
                       auto* status = makeValueLabel(group, QStringLiteral("Value: 800"));
                       setStableStatusWidth(status, QStringLiteral("Value: 1000"));
                       QObject::connect(slider, &Slider::valueChanged,
                                        status, [status](int value) {
                                            status->setText(QStringLiteral("Value: %1").arg(value));
                                        });
                       group->layout()->addWidget(slider);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("slider-tick-marks"),
                   QStringLiteral("Tick marks"),
                   QStringLiteral("Tick interval and tick position expose discrete stops along the track."),
                   QStringLiteral("auto* slider = new Slider(Qt::Horizontal, this);\n"
                                  "slider->setRange(0, 10);\n"
                                  "slider->setTickInterval(1);\n"
                                  "slider->setTickPosition(QSlider::TicksBelow);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 16);
                       auto* slider = new Slider(Qt::Horizontal, group);
                       slider->setRange(0, 10);
                       slider->setTickInterval(1);
                       slider->setTickPosition(QSlider::TicksBelow);
                       slider->setValue(4);
                       slider->setFixedWidth(260);
                       auto* status = makeValueLabel(group, QStringLiteral("Value: 4"));
                       QObject::connect(slider, &Slider::valueChanged,
                                        status, [status](int value) {
                                            status->setText(QStringLiteral("Value: %1").arg(value));
                                        });
                       group->layout()->addWidget(slider);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("slider-vertical"),
                   QStringLiteral("Vertical Slider"),
                   QStringLiteral("Vertical orientation uses the same range and value API with a vertical track."),
                   QStringLiteral("auto* slider = new Slider(Qt::Vertical, this);\n"
                                  "slider->setRange(0, 100);\n"
                                  "slider->setValue(25);\n"
                                  "slider->setFixedHeight(160);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 20);
                       auto* slider = new Slider(Qt::Vertical, group);
                       slider->setRange(0, 100);
                       slider->setValue(25);
                       slider->setFixedHeight(160);
                       auto* status = makeValueLabel(group, QStringLiteral("Value: 25"));
                       setStableStatusWidth(status, QStringLiteral("Value: 100"));
                       QObject::connect(slider, &Slider::valueChanged,
                                        status, [status](int value) {
                                            status->setText(QStringLiteral("Value: %1").arg(value));
                                        });
                       group->layout()->addWidget(slider);
                       group->layout()->addWidget(status);
                       return group;
                   })
    };
}

QVector<GallerySample> splitButtonSamples()
{
    return {
        makeSample(QStringLiteral("split-button-primary-menu"),
                   QStringLiteral("Primary action and menu"),
                   QStringLiteral("The primary region invokes the default action; the secondary region opens the menu."),
                   QStringLiteral("auto* button = new SplitButton(\"Choose color\", this);\n"
                                  "auto* menu = new FluentMenu(\"Colors\", button);\n"
                                  "menu->addAction(\"Red\");\n"
                                  "menu->addAction(\"Green\");\n"
                                  "menu->addAction(\"Blue\");\n"
                                  "button->setMenu(menu);\n"
                                  "connect(button, &SplitButton::clicked,\n"
                                  "        status, []() { /* primary clicked */ });"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 12);
                       auto* button = new SplitButton(QStringLiteral("Choose color"), group);
                       button->setMenu(makeMenu(button, QStringLiteral("Colors"),
                           {QStringLiteral("Red"), QStringLiteral("Green"), QStringLiteral("Blue")}));
                       auto* status = makeValueLabel(group, QStringLiteral("Status: Ready"));
                       setStableStatusWidth(status, QStringLiteral("Status: Primary clicked"));
                       QObject::connect(button, &SplitButton::clicked,
                                        status, [status]() {
                                            status->setText(QStringLiteral("Status: Primary clicked"));
                                        });
                       group->layout()->addWidget(button);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("split-button-accent"),
                   QStringLiteral("Accent SplitButton"),
                   QStringLiteral("Accent style marks the primary split action as the recommended path."),
                   QStringLiteral("auto* button = new SplitButton(\"Submit\", this);\n"
                                  "button->setFluentStyle(Button::Accent);\n"
                                  "auto* menu = new FluentMenu(\"Actions\", button);\n"
                                  "menu->addAction(\"Submit and close\");\n"
                                  "menu->addAction(\"Submit and notify\");\n"
                                  "button->setMenu(menu);"),
                   [](QWidget* parent) {
                       auto* button = new SplitButton(QStringLiteral("Submit"), parent);
                       button->setFluentStyle(Button::Accent);
                       button->setMenu(makeMenu(button, QStringLiteral("Actions"),
                           {QStringLiteral("Submit and close"), QStringLiteral("Submit and notify")}));
                       return button;
                   }),
        makeSample(QStringLiteral("split-button-sizes"),
                   QStringLiteral("SplitButton sizes"),
                   QStringLiteral("Split buttons inherit Button size presets and keep the secondary region stable."),
                   QStringLiteral("auto* small = new SplitButton(\"Small\", this);\n"
                                  "small->setFluentSize(Button::Small);\n\n"
                                  "auto* standard = new SplitButton(\"Standard\", this);\n"
                                  "standard->setFluentSize(Button::StandardSize);\n\n"
                                  "auto* large = new SplitButton(\"Large\", this);\n"
                                  "large->setFluentSize(Button::Large);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       auto* small = new SplitButton(QStringLiteral("Small"), group);
                       small->setFluentSize(Button::Small);
                       auto* standard = new SplitButton(QStringLiteral("Standard"), group);
                       standard->setFluentSize(Button::StandardSize);
                       auto* large = new SplitButton(QStringLiteral("Large"), group);
                       large->setFluentSize(Button::Large);
                       group->layout()->addWidget(small);
                       group->layout()->addWidget(standard);
                       group->layout()->addWidget(large);
                       return group;
                   })
    };
}

QVector<GallerySample> toggleButtonSamples()
{
    return {
        makeSample(QStringLiteral("toggle-button-state"),
                   QStringLiteral("ToggleButton state"),
                   QStringLiteral("A toggle button keeps its checked state visible after invocation."),
                   QStringLiteral("auto* toggle = new ToggleButton(\"Bold\", this);\n"
                                  "connect(toggle, &ToggleButton::toggled,\n"
                                  "        status, [status](bool checked) {\n"
                                  "            status->setText(checked ? \"Checked\" : \"Unchecked\");\n"
                                  "        });"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 12);
                       auto* toggle = new ToggleButton(QStringLiteral("Bold"), group);
                       auto* status = makeValueLabel(group, QStringLiteral("State: Unchecked"));
                       setStableStatusWidth(status, QStringLiteral("State: Unchecked"));
                       QObject::connect(toggle, &ToggleButton::toggled,
                                        status, [status](bool checked) {
                                            status->setText(checked
                                                ? QStringLiteral("State: Checked")
                                                : QStringLiteral("State: Unchecked"));
                                        });
                       group->layout()->addWidget(toggle);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("toggle-button-three-state"),
                   QStringLiteral("Three-state ToggleButton"),
                   QStringLiteral("Three-state mode cycles unchecked, checked, and mixed states."),
                   QStringLiteral("auto* toggle = new ToggleButton(\"Three state\", this);\n"
                                  "toggle->setThreeState(true);\n"
                                  "connect(toggle, &ToggleButton::checkStateChanged,\n"
                                  "        status, [status](Qt::CheckState state) {\n"
                                  "            const QString text = state == Qt::Checked ? \"Checked\"\n"
                                  "                : state == Qt::PartiallyChecked ? \"Mixed\" : \"Unchecked\";\n"
                                  "            status->setText(text);\n"
                                  "        });"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 12);
                       auto* toggle = new ToggleButton(QStringLiteral("Three state"), group);
                       toggle->setThreeState(true);
                       auto* status = makeValueLabel(group, QStringLiteral("State: Unchecked"));
                       setStableStatusWidth(status, QStringLiteral("State: Unchecked"));
                       QObject::connect(toggle, &ToggleButton::checkStateChanged,
                                        status, [status](Qt::CheckState state) {
                                            status->setText(QStringLiteral("State: %1").arg(checkStateText(state)));
                                        });
                       group->layout()->addWidget(toggle);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("toggle-button-disabled"),
                   QStringLiteral("Disabled ToggleButton"),
                   QStringLiteral("Disabled toggle buttons preserve checked and unchecked visuals without accepting input."),
                   QStringLiteral("auto* off = new ToggleButton(\"Disabled off\", this);\n"
                                  "off->setEnabled(false);\n\n"
                                  "auto* on = new ToggleButton(\"Disabled on\", this);\n"
                                  "on->setChecked(true);\n"
                                  "on->setEnabled(false);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 10);
                       auto* off = new ToggleButton(QStringLiteral("Disabled off"), group);
                       off->setEnabled(false);
                       auto* on = new ToggleButton(QStringLiteral("Disabled on"), group);
                       on->setChecked(true);
                       on->setEnabled(false);
                       group->layout()->addWidget(off);
                       group->layout()->addWidget(on);
                       return group;
                   })
    };
}

QVector<GallerySample> toggleSplitButtonSamples()
{
    return {
        makeSample(QStringLiteral("toggle-split-button-menu"),
                   QStringLiteral("ToggleSplitButton with menu"),
                   QStringLiteral("The primary region toggles selection; the secondary region opens related choices."),
                   QStringLiteral("auto* button = new ToggleSplitButton(\"List options\", this);\n"
                                  "button->setFluentLayout(Button::IconBefore);\n"
                                  "button->setIconGlyph(Typography::Icons::List);\n"
                                  "auto* menu = new FluentMenu(\"Styles\", button);\n"
                                  "menu->addAction(\"None\");\n"
                                  "menu->addAction(\"Bulleted\");\n"
                                  "menu->addAction(\"Numbered\");\n"
                                  "button->setMenu(menu);\n"
                                  "connect(button, &ToggleSplitButton::toggled,\n"
                                  "        status, [status](bool checked) { /* show state */ });"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 12);
                       auto* button = new ToggleSplitButton(QStringLiteral("List options"), group);
                       button->setFluentLayout(Button::IconBefore);
                       button->setIconGlyph(Typography::Icons::List, Typography::IconSize::Standard);
                       button->setMinimumWidth(160);
                       button->setMenu(makeMenu(button, QStringLiteral("Styles"),
                           {QStringLiteral("None"), QStringLiteral("Bulleted"), QStringLiteral("Numbered")}));
                       auto* status = makeValueLabel(group, QStringLiteral("State: Unchecked"));
                       setStableStatusWidth(status, QStringLiteral("State: Unchecked"));
                       QObject::connect(button, &ToggleSplitButton::toggled,
                                        status, [status](bool checked) {
                                            status->setText(checked
                                                ? QStringLiteral("State: Checked")
                                                : QStringLiteral("State: Unchecked"));
                                        });
                       group->layout()->addWidget(button);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("toggle-split-button-icon-only"),
                   QStringLiteral("Icon-only ToggleSplitButton"),
                   QStringLiteral("Icon-only layout keeps compact tool commands selectable while preserving the split menu affordance."),
                   QStringLiteral("auto* button = new ToggleSplitButton(QString(), this);\n"
                                  "button->setFluentLayout(Button::IconOnly);\n"
                                  "button->setIconGlyph(Typography::Icons::Settings);\n"
                                  "button->setFixedSize(64, 34);"),
                   [](QWidget* parent) {
                       auto* button = new ToggleSplitButton(QString(), parent);
                       button->setFluentLayout(Button::IconOnly);
                       button->setIconGlyph(Typography::Icons::Settings, Typography::IconSize::Standard);
                       button->setFixedSize(64, 34);
                       return button;
                   })
    };
}

QVector<GallerySample> toggleSwitchSamples()
{
    return {
        makeSample(QStringLiteral("toggle-switch-state"),
                   QStringLiteral("ToggleSwitch state"),
                   QStringLiteral("The toggled signal mirrors the on/off state into surrounding UI."),
                   QStringLiteral("auto* toggle = new ToggleSwitch(this);\n"
                                  "auto* status = new Label(\"State: Off\", this);\n"
                                  "connect(toggle, &ToggleSwitch::toggled,\n"
                                  "        status, [status](bool on) {\n"
                                  "            status->setText(on ? \"State: On\" : \"State: Off\");\n"
                                  "        });"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 8);
                       auto* toggle = new ToggleSwitch(group);
                       auto* status = makeValueLabel(group, QStringLiteral("State: Off"));
                       setStableStatusWidth(status, QStringLiteral("State: Off"));
                       QObject::connect(toggle, &ToggleSwitch::toggled,
                                        status, [status](bool on) {
                                            status->setText(on
                                                ? QStringLiteral("State: On")
                                                : QStringLiteral("State: Off"));
                                        });
                       group->layout()->addWidget(toggle);
                       group->layout()->addWidget(status);
                       return group;
                   }),
        makeSample(QStringLiteral("toggle-switch-content"),
                   QStringLiteral("External label and content"),
                   QStringLiteral("Applications provide the field label while the switch owns on/off content text."),
                   QStringLiteral("auto* label = new Label(\"Wi-Fi\", this);\n"
                                  "auto* toggle = new ToggleSwitch(this);\n"
                                  "toggle->setOnContent(\"Connected\");\n"
                                  "toggle->setOffContent(\"Disconnected\");\n"
                                  "toggle->setIsOn(true);"),
                   [](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 6);
                       auto* label = makeValueLabel(group, QStringLiteral("Wi-Fi"));
                       auto* toggle = new ToggleSwitch(group);
                       toggle->setOnContent(QStringLiteral("Connected"));
                       toggle->setOffContent(QStringLiteral("Disconnected"));
                       toggle->setIsOn(true);
                       group->layout()->addWidget(label);
                       group->layout()->addWidget(toggle);
                       return group;
                   }),
        makeSample(QStringLiteral("toggle-switch-disabled"),
                   QStringLiteral("Disabled ToggleSwitch"),
                   QStringLiteral("Disabled switches preserve both off and on visuals while blocking interaction."),
                   QStringLiteral("auto* off = new ToggleSwitch(this);\n"
                                  "off->setEnabled(false);\n\n"
                                  "auto* on = new ToggleSwitch(this);\n"
                                  "on->setIsOn(true);\n"
                                  "on->setEnabled(false);"),
                   [](QWidget* parent) {
                       QWidget* group = horizontalGroup(parent, 24);
                       auto* off = new ToggleSwitch(group);
                       off->setEnabled(false);
                       auto* on = new ToggleSwitch(group);
                       on->setIsOn(true);
                       on->setEnabled(false);
                       group->layout()->addWidget(off);
                       group->layout()->addWidget(on);
                       return group;
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
    if (routeId == QStringLiteral("color-picker"))
        return colorPickerSamples();
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
