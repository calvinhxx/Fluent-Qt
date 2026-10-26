#include <gtest/gtest.h>
#include "QtTestEnvironment.h"
#include <QAccessible>
#include <QLayout>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <FluentQt/FluentQt.h>

using namespace fluent;
using namespace fluent::layout;

namespace {
PropertyDefinition definition(QString key, PropertyDefinition::Type type, QVariant value)
{
    PropertyDefinition p;
    p.key = key;
    p.label = key;
    p.type = type;
    p.defaultValue = value;
    p.minimum = 0;
    p.maximum = 100;
    p.choices = {QStringLiteral("Line"), QStringLiteral("Bar")};
    return p;
}
} // namespace

TEST(PropertyPanelTest, Contract_AdaptiveLayoutKeepsEditorsAndQuietResetActions)
{
    PropertyPanel panel;
    ASSERT_TRUE(panel.setProperties({definition("Title", PropertyDefinition::Text, QString("Before")),
        definition("Color", PropertyDefinition::Color, QColor("#0078d4")),
        definition("Maximum", PropertyDefinition::Number, 80.0)}));
    panel.resize(420, 260);
    ASSERT_TRUE(tests::support::showAndFocusWidget(panel.editor("Title")));
    QPointer<QWidget> editor = panel.editor("Title");
    EXPECT_EQ(panel.field("Title")->labelPosition(), Field::LabelPosition::Leading);
    EXPECT_TRUE(panel.searchField()->isHidden());
    auto* reset = panel.findChild<basicinput::Button*>("propertyReset_Title");
    ASSERT_TRUE(reset);
    EXPECT_TRUE(reset->isHidden());
    auto* color = qobject_cast<basicinput::Button*>(panel.editor("Color"));
    ASSERT_TRUE(color);
    EXPECT_FALSE(color->icon().isNull());
    EXPECT_EQ(color->text(), "#0078D4");
    panel.setValue("Title", QString("Changed"));
    EXPECT_FALSE(reset->isHidden());
    EXPECT_EQ(panel.modifiedPropertyKeys(), QStringList{"Title"});
    panel.resize(240, 380);
    QApplication::processEvents();
    ASSERT_EQ(panel.width(), 240);
    EXPECT_EQ(panel.field("Title")->labelPosition(), Field::LabelPosition::Top);
    EXPECT_EQ(panel.editor("Title"), editor);
    EXPECT_EQ(panel.value("Title").toString(), "Changed");
    panel.setLayoutDirection(Qt::RightToLeft);
    for (int width : {240, 360, 480}) {
        panel.resize(width, 380);
        QApplication::processEvents();
        auto* scroll = panel.findChild<scrolling::ScrollView*>();
        ASSERT_TRUE(scroll);
        QWidget* content = scroll->widget();
        ASSERT_TRUE(content);
        QList<QWidget*> layoutHosts{content};
        for (auto* group : content->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
            if (group->isVisible() && group->layout())
                layoutHosts.append(group);
        }
        ASSERT_TRUE(tests::support::waitForStableLayouts(layoutHosts));
        EXPECT_EQ(scroll->scrollableWidth(), 0) << width;
    }
    panel.setLayoutMode(PropertyPanel::LayoutMode::Stacked);
    panel.resize(480, 380);
    QApplication::processEvents();
    EXPECT_EQ(panel.field("Title")->labelPosition(), Field::LabelPosition::Top);
    panel.resetAll();
    EXPECT_EQ(panel.value("Title").toString(), "Before");
    EXPECT_TRUE(reset->isHidden());
    EXPECT_TRUE(panel.modifiedPropertyKeys().isEmpty());
}

TEST(PropertyPanelTest, Contract_ResetActionsAlignWithEditorsAcrossReflowAndValidation)
{
    PropertyPanel panel;
    auto title = definition("title", PropertyDefinition::Text, QString("Default"));
    title.label = QStringLiteral("Document title with a longer caption");
    title.description = QStringLiteral("A helper message that can wrap on narrow layouts.");
    ASSERT_TRUE(panel.setProperties({title, definition("number", PropertyDefinition::Number, 80.0),
        definition("enabled", PropertyDefinition::Boolean, true),
        definition("kind", PropertyDefinition::Choice, QString("Line")),
        definition("color", PropertyDefinition::Color, QColor(Qt::blue))}));
    panel.resize(520, 760);
    panel.show();
    QApplication::processEvents();
    const int initialWidth = panel.editor("title")->width();
    panel.setValue("title", QString("Edited"));
    panel.setValue("number", 50.0);
    panel.setValue("enabled", false);
    panel.setValue("kind", QString("Bar"));
    panel.setValue("color", QColor(Qt::red));
    QApplication::processEvents();
    EXPECT_EQ(panel.editor("title")->width(), initialWidth);
    for (const auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        panel.setLayoutDirection(direction);
        for (const auto mode : {PropertyPanel::LayoutMode::Automatic, PropertyPanel::LayoutMode::Stacked,
                 PropertyPanel::LayoutMode::Inline}) {
            panel.setLayoutMode(mode);
            for (const int width : {260, 360, 520}) {
                panel.resize(width, 760);
                panel.field("title")->setValidationMessage(
                    width < 400 ? QStringLiteral("Choose a shorter title for this document.") : QString());
                QApplication::processEvents();
                for (const auto& key : panel.propertyKeys()) {
                    auto* editor = panel.editor(key);
                    auto* reset = panel.findChild<basicinput::Button*>("propertyReset_" + key);
                    ASSERT_TRUE(reset);
                    ASSERT_TRUE(reset->isVisible());
                    EXPECT_EQ(panel.field(key)->editor(), editor);
                    const QRect editorRect(editor->mapTo(&panel, QPoint()), editor->size());
                    const QRect resetRect(reset->mapTo(&panel, QPoint()), reset->size());
                    EXPECT_LE(qAbs(resetRect.center().y() - editorRect.center().y()), 1) << key.toStdString();
                    EXPECT_FALSE(editorRect.intersects(resetRect)) << key.toStdString();
                    if (direction == Qt::LeftToRight)
                        EXPECT_GT(resetRect.left(), editorRect.right());
                    else
                        EXPECT_LT(resetRect.right(), editorRect.left());
                }
            }
        }
    }
}

TEST(PropertyPanelTest, Contract_SearchPolicyAndResetAllRespectReadOnlyFields)
{
    PropertyPanel panel;
    QVector<PropertyDefinition> fields;
    for (int i = 0; i < 8; ++i)
        fields.append(definition(QString::number(i), PropertyDefinition::Text, QString("Default")));
    fields[1].readOnly = true;
    panel.setProperties(fields);
    panel.show();
    EXPECT_FALSE(panel.searchField()->isHidden());
    panel.setSearchVisibility(PropertyPanel::SearchVisibility::Hidden);
    EXPECT_TRUE(panel.searchField()->isHidden());
    panel.setSearchVisibility(PropertyPanel::SearchVisibility::Visible);
    EXPECT_FALSE(panel.searchField()->isHidden());
    panel.setValue("0", QString("Editable"));
    panel.setValue("1", QString("Locked"));
    panel.setSearchText("7");
    panel.resetAll();
    EXPECT_EQ(panel.value("0").toString(), "Default");
    EXPECT_EQ(panel.value("1").toString(), "Locked");
    EXPECT_EQ(panel.modifiedPropertyKeys(), QStringList{"1"});
}

TEST(PropertyPanelTest, Contract_TypedEditorsResetAndSilentApplicationUpdates)
{
    PropertyPanel panel;
    const auto fields = QVector<PropertyDefinition>{definition("title", PropertyDefinition::Text, QString("Latency")),
        definition("limit", PropertyDefinition::Number, 80.0), definition("legend", PropertyDefinition::Boolean, true),
        definition("kind", PropertyDefinition::Choice, QString("Line")),
        definition("color", PropertyDefinition::Color, QColor(Qt::red))};
    ASSERT_TRUE(panel.setProperties(fields));
    EXPECT_EQ(panel.propertyKeys().size(), 5);
    EXPECT_FALSE(panel.formController()->isDirty());
    QSignalSpy edited(&panel, &PropertyPanel::valueEdited);
    EXPECT_TRUE(panel.setValue("limit", 50.0));
    EXPECT_TRUE(panel.formController()->isDirty());
    EXPECT_EQ(edited.count(), 0);
    EXPECT_FALSE(panel.setValue("limit", 101.0));
    EXPECT_FALSE(panel.setValue("limit", QStringLiteral("80")));
    EXPECT_FALSE(panel.setValue("kind", "Unknown"));
    EXPECT_FALSE(panel.setValue("legend", "false"));
    EXPECT_TRUE(panel.resetProperty("limit"));
    EXPECT_DOUBLE_EQ(panel.value("limit").toDouble(), 80.0);
    ASSERT_EQ(edited.count(), 1);
    EXPECT_TRUE(panel.resetProperty("limit"));
    EXPECT_EQ(edited.count(), 1);
    auto* number = qobject_cast<textfields::NumberBox*>(panel.editor("limit"));
    ASSERT_TRUE(number);
    EXPECT_TRUE(number->isScrubbingEnabled());
    number->setValue(45);
    EXPECT_EQ(edited.count(), 2);
    panel.setReadOnly(true);
    EXPECT_TRUE(number->isReadOnly());
    EXPECT_FALSE(panel.resetProperty("limit"));
    EXPECT_TRUE(panel.setValue("limit", 60.0));
    EXPECT_EQ(edited.count(), 2);
    EXPECT_DOUBLE_EQ(panel.value("limit").toDouble(), 60.0);
}

TEST(PropertyPanelTest, Contract_InvalidReplacementPreservesFieldsAndValidReplacementReleasesThem)
{
    PropertyPanel panel;
    auto p = definition("limit", PropertyDefinition::Number, 10.0);
    ASSERT_TRUE(panel.setProperties({p}));
    QPointer<QWidget> oldEditor = panel.editor("limit");
    EXPECT_TRUE(panel.setProperties({p}));
    EXPECT_EQ(panel.editor("limit"), oldEditor.data());
    EXPECT_FALSE(panel.setProperties({p, p}));
    EXPECT_EQ(panel.editor("limit"), oldEditor.data());
    p.minimum = 50;
    EXPECT_FALSE(panel.setProperties({p}));
    p = definition("name", PropertyDefinition::Text, QString("First"));
    ASSERT_TRUE(panel.setProperties({p}));
    EXPECT_TRUE(oldEditor.isNull());
    EXPECT_EQ(panel.formController()->fieldKeys(), QStringList{"name"});
    EXPECT_FALSE(panel.formController()->isDirty());
}

TEST(PropertyPanelTest, Contract_SearchKeepsValuesAndValidationRejectsStaleResults)
{
    PropertyPanel panel;
    auto p = definition("limit", PropertyDefinition::Number, 10.0);
    p.group = "Axes";
    ASSERT_TRUE(panel.setProperties({p, definition("name", PropertyDefinition::Text, QString("Chart"))}));
    panel.setSearchText("axes");
    EXPECT_FALSE(panel.field("limit")->parentWidget()->isHidden());
    EXPECT_TRUE(panel.field("name")->parentWidget()->isHidden());
    panel.setSearchText(QString());
    const auto token = panel.formController()->validateField("limit");
    ASSERT_NE(token, 0u);
    panel.setValue("limit", 20);
    EXPECT_FALSE(panel.formController()->completeValidation("limit", token, true));
    EXPECT_EQ(panel.value("name").toString(), "Chart");
}

TEST(PropertyPanelTest, Contract_AccessibilityNamesAndKeyboardEditors)
{
    PropertyPanel panel;
    ASSERT_TRUE(panel.setProperties({definition("Title", PropertyDefinition::Text, QString("Before"))}));
    auto* input = qobject_cast<textfields::LineEdit*>(panel.editor("Title"));
    ASSERT_TRUE(input);
    auto* accessible = QAccessible::queryAccessibleInterface(input);
    ASSERT_TRUE(accessible);
    EXPECT_EQ(accessible->text(QAccessible::Name), "Title");
    panel.resize(360, 400);
    panel.show();
    input->setFocus();
    input->selectAll();
    QSignalSpy edited(&panel, &PropertyPanel::valueEdited);
    QTest::keyClicks(input, "After");
    EXPECT_EQ(panel.value("Title").toString(), "After");
    EXPECT_GT(edited.count(), 0);
}

TEST(PropertyPanelTest, Contract_ThemeSurfaceFollowsLightDarkAndContrast)
{
    QWidget host;
    host.setObjectName("propertyHost");
    host.resize(400, 560);
    PropertyPanel panel(&host);
    panel.resize(360, 500);
    panel.setProperties({definition("title", PropertyDefinition::Text, QString("Example"))});
    auto* scroll = panel.findChild<fluent::scrolling::ScrollView*>();
    ASSERT_TRUE(scroll);
    for (const auto theme : {FluentElement::Light, FluentElement::Dark, FluentElement::HighContrast}) {
        FluentElement::setTheme(theme);
        panel.onThemeUpdated();
        host.setStyleSheet("#propertyHost { border: none; }");
        host.show();
        QApplication::processEvents();
        const QImage image = scroll->contentWidget()->grab().toImage();
        ASSERT_FALSE(image.isNull());
        EXPECT_EQ(image.pixelColor(image.width() / 2, image.height() - 8), panel.themeColors().bgLayer);
        EXPECT_FALSE(scroll->contentWidget()->autoFillBackground());
    }
    FluentElement::setTheme(FluentElement::Light);
}

TEST(PropertyPanelTest, Contract_CallbackCanDestroyPanel)
{
    QPointer<PropertyPanel> panel = new PropertyPanel;
    QObject::connect(panel, &PropertyPanel::propertiesChanged, panel, [panel] { delete panel; });
    panel->setProperties({definition("Title", PropertyDefinition::Text, QString("Value"))});
    EXPECT_TRUE(panel.isNull());
    panel = new PropertyPanel;
    panel->setProperties({definition("limit", PropertyDefinition::Number, 10.0)});
    QObject::connect(panel, &PropertyPanel::valueEdited, panel, [panel] { delete panel; });
    qobject_cast<textfields::NumberBox*>(panel->editor("limit"))->setValue(20);
    EXPECT_TRUE(panel.isNull());
}

TEST(PropertyPanelTest, Contract_ReflowToleratesDefinitionReplacementFromFieldNotification)
{
    PropertyPanel panel;
    panel.setProperties({definition("old", PropertyDefinition::Text, QString("Before"))});
    panel.setLayoutMode(PropertyPanel::LayoutMode::Inline);
    QPointer<Field> previous = panel.field("old");
    QObject::connect(previous, &Field::labelPositionChanged, &panel, [&] {
        panel.setProperties({definition("new", PropertyDefinition::Text, QString("After"))});
    });
    panel.setLayoutMode(PropertyPanel::LayoutMode::Stacked);
    EXPECT_TRUE(previous.isNull());
    EXPECT_EQ(panel.propertyKeys(), QStringList{"new"});
    EXPECT_EQ(panel.value("new").toString(), "After");
}

TEST(PropertyPanelTest, Contract_OwnedColorFlyoutDiesWithPanelAfterReparenting)
{
    for (bool closeFirst : {false, true}) {
        QWidget window;
        window.resize(500, 400);
        auto* panel = new PropertyPanel(&window);
        panel->setGeometry(20, 20, 360, 250);
        ASSERT_TRUE(panel->setProperties({definition("color", PropertyDefinition::Color, QColor(Qt::red))}));
        QPointer<dialogs_flyouts::Flyout> flyout = panel->findChild<dialogs_flyouts::Flyout*>();
        ASSERT_TRUE(flyout);
        flyout->setAnimationEnabled(false);
        window.show();
        QApplication::processEvents();
        qobject_cast<basicinput::Button*>(panel->editor("color"))->click();
        ASSERT_TRUE(flyout->isOpen());
        ASSERT_NE(flyout->parentWidget(), panel);
        if (closeFirst)
            flyout->close();
        delete panel;
        EXPECT_TRUE(flyout.isNull());
    }
}

TEST(PropertyPanelTest, Contract_DefaultComparisonUsesColorAndNumericValues)
{
    PropertyPanel panel;
    auto number = definition("number", PropertyDefinition::Number, 0.3);
    number.step = 0.1;
    ASSERT_TRUE(panel.setProperties({definition("color", PropertyDefinition::Color, QString("#0078d4")), number}));
    EXPECT_TRUE(panel.modifiedPropertyKeys().isEmpty());
    EXPECT_EQ(panel.value("color").value<QColor>(), QColor("#0078d4"));
    auto* editor = qobject_cast<textfields::NumberBox*>(panel.editor("number"));
    ASSERT_TRUE(editor);
    editor->setValue(0);
    for (int i = 0; i < 3; ++i)
        QTest::keyClick(editor, Qt::Key_Up);
    EXPECT_DOUBLE_EQ(editor->value(), 0.1 + 0.1 + 0.1);
    EXPECT_TRUE(panel.modifiedPropertyKeys().isEmpty());
    EXPECT_TRUE(panel.findChild<basicinput::Button*>("propertyReset_number")->isHidden());
    QSignalSpy edits(&panel, &PropertyPanel::valueEdited);
    EXPECT_TRUE(panel.resetProperty("number"));
    EXPECT_EQ(edits.count(), 0);
    panel.setValue("number", 0.300001);
    EXPECT_EQ(panel.modifiedPropertyKeys(), QStringList{"number"});
    panel.resetAll();
    EXPECT_TRUE(panel.modifiedPropertyKeys().isEmpty());
    EXPECT_EQ(edits.count(), 1);
    number.defaultValue = 3e-20;
    number.step = 1e-20;
    ASSERT_TRUE(panel.setProperties({number}));
    panel.setValue("number", 4e-20);
    EXPECT_EQ(panel.modifiedPropertyKeys(), QStringList{"number"});
    number.defaultValue = 80.0;
    number.step = 0.1;
    ASSERT_TRUE(panel.setProperties({number}));
    editor = qobject_cast<textfields::NumberBox*>(panel.editor("number"));
    ASSERT_TRUE(editor);
    editor->setValue(79.7);
    for (int i = 0; i < 3; ++i)
        QTest::keyClick(editor, Qt::Key_Up);
    EXPECT_TRUE(panel.modifiedPropertyKeys().isEmpty());
    number.defaultValue = 1e18;
    number.maximum = 1e20;
    number.step = 128;
    ASSERT_TRUE(panel.setProperties({number}));
    panel.setValue("number", 1e18 + 128);
    EXPECT_EQ(panel.modifiedPropertyKeys(), QStringList{"number"});
}
