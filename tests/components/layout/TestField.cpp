#include <gtest/gtest.h>

#include <QApplication>
#include <QPointer>
#include <QSignalSpy>
#include <type_traits>

#include "components/foundation/FluentElement.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/QMLPlus.h"
#include "components/layout/Field.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"

using fluent::WidgetOwnership;
using fluent::layout::Field;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;

namespace {

class FluentTestWindow : public QWidget, public fluent::FluentElement {
public:
    using QWidget::QWidget;
    void onThemeUpdated() override
    {
        const auto& colors = themeColors();
        setStyleSheet(
            QStringLiteral("background-color: %1;").arg(colors.bgCanvas.name()));
    }
};

} // namespace

class FieldTest : public ::testing::Test {
protected:
    static void SetUpTestSuite()
    {
        qRegisterMetaType<Field::ValidationState>(
            "fluent::layout::Field::ValidationState");
        qRegisterMetaType<WidgetOwnership>("fluent::WidgetOwnership");
    }
};

TEST_F(FieldTest, Contract_DefaultsAndInheritance)
{
    static_assert(std::is_base_of<QWidget, Field>::value,
                  "Field must be a QWidget composition shell");
    static_assert(std::is_base_of<fluent::FluentElement, Field>::value,
                  "Field must mix in FluentElement");
    static_assert(std::is_base_of<fluent::QMLPlus, Field>::value,
                  "Field must mix in QMLPlus");

    Field field;
    EXPECT_TRUE(field.labelText().isEmpty());
    EXPECT_FALSE(field.isRequired());
    EXPECT_TRUE(field.helperText().isEmpty());
    EXPECT_TRUE(field.validationMessage().isEmpty());
    EXPECT_EQ(field.validationState(), Field::ValidationState::None);
    EXPECT_EQ(field.editor(), nullptr);
    EXPECT_EQ(field.editorOwnership(), WidgetOwnership::Borrowed);
    EXPECT_EQ(field.objectName(), QStringLiteral("fluentField"));
    EXPECT_EQ(field.focusPolicy(), Qt::NoFocus);
    EXPECT_EQ(field.focusProxy(), nullptr);
    EXPECT_NE(field.findChild<Label*>(QStringLiteral("fluentFieldCaption")),
              nullptr);
    EXPECT_NE(field.findChild<QWidget*>(QStringLiteral("fluentFieldEditorHost")),
              nullptr);
}

TEST_F(FieldTest, Contract_SettersAreNoOpsOnUnchangedValues)
{
    Field field;
    QSignalSpy labelSpy(&field, &Field::labelTextChanged);
    QSignalSpy requiredSpy(&field, &Field::requiredChanged);
    QSignalSpy helperSpy(&field, &Field::helperTextChanged);
    QSignalSpy messageSpy(&field, &Field::validationMessageChanged);
    QSignalSpy stateSpy(&field, &Field::validationStateChanged);

    field.setLabelText(QStringLiteral("Email"));
    field.setLabelText(QStringLiteral("Email"));
    field.setRequired(true);
    field.setRequired(true);
    field.setHelperText(QStringLiteral("Used for recovery"));
    field.setHelperText(QStringLiteral("Used for recovery"));
    field.setValidationMessage(QStringLiteral("Required"));
    field.setValidationMessage(QStringLiteral("Required"));
    field.setValidationState(Field::ValidationState::Error);
    field.setValidationState(Field::ValidationState::Error);

    EXPECT_EQ(labelSpy.count(), 1);
    EXPECT_EQ(requiredSpy.count(), 1);
    EXPECT_EQ(helperSpy.count(), 1);
    EXPECT_EQ(messageSpy.count(), 1);
    EXPECT_EQ(stateSpy.count(), 1);
}

TEST_F(FieldTest, Contract_BorrowedEditorIsDetached)
{
    QPointer<QWidget> editor = new LineEdit;
    {
        Field field;
        ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Borrowed));
        EXPECT_EQ(editor->parentWidget(),
                  field.findChild<QWidget*>(
                      QStringLiteral("fluentFieldEditorHost")));
    }

    ASSERT_FALSE(editor.isNull());
    EXPECT_EQ(editor->parentWidget(), nullptr);
    delete editor;
}

TEST_F(FieldTest, Contract_ReparentedEditorReturnsToOriginalParent)
{
    QWidget owner;
    auto* editor = new LineEdit(&owner);
    {
        Field field;
        ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Reparented));
        EXPECT_NE(editor->parentWidget(), &owner);
    }
    EXPECT_EQ(editor->parentWidget(), &owner);
}

TEST_F(FieldTest, Contract_OwnedEditorIsDestroyed)
{
    QPointer<QWidget> editor = new LineEdit;
    auto* field = new Field;
    ASSERT_TRUE(field->setEditor(editor, WidgetOwnership::Owned));
    delete field;
    EXPECT_TRUE(editor.isNull());
}

TEST_F(FieldTest, Contract_TakeEditorTransfersWithoutDeleting)
{
    Field field;
    auto* editor = new LineEdit;
    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Owned));

    QWidget* taken = field.takeEditor();
    EXPECT_EQ(taken, editor);
    EXPECT_EQ(taken->parentWidget(), nullptr);
    EXPECT_EQ(field.editor(), nullptr);
    EXPECT_EQ(field.editorOwnership(), WidgetOwnership::Borrowed);
    delete taken;
}

TEST_F(FieldTest, Contract_ReleaseEditorAppliesOwnershipPolicy)
{
    QPointer<QWidget> borrowed = new LineEdit;
    {
        Field field;
        ASSERT_TRUE(field.setEditor(borrowed, WidgetOwnership::Borrowed));
        field.releaseEditor();
        EXPECT_EQ(field.editor(), nullptr);
        ASSERT_FALSE(borrowed.isNull());
        EXPECT_EQ(borrowed->parentWidget(), nullptr);
    }
    delete borrowed;

    QWidget owner;
    auto* restored = new LineEdit(&owner);
    Field reparentedHost;
    ASSERT_TRUE(reparentedHost.setEditor(restored, WidgetOwnership::Reparented));
    reparentedHost.releaseEditor();
    EXPECT_EQ(restored->parentWidget(), &owner);

    QPointer<QWidget> owned = new LineEdit;
    Field ownedHost;
    ASSERT_TRUE(ownedHost.setEditor(owned, WidgetOwnership::Owned));
    ownedHost.releaseEditor();
    EXPECT_TRUE(owned.isNull());
}

TEST_F(FieldTest, Contract_RejectsSelfAndAncestorEditor)
{
    QWidget ancestor;
    Field field(&ancestor);
    EXPECT_FALSE(field.setEditor(&field, WidgetOwnership::Borrowed));

    EXPECT_FALSE(field.setEditor(&ancestor, WidgetOwnership::Borrowed));
    EXPECT_EQ(field.editor(), nullptr);
}

TEST_F(FieldTest, Contract_OwnershipModeChangeRequiresTakeEditor)
{
    Field field;
    auto* editor = new LineEdit;
    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Borrowed));
    QSignalSpy ownershipSpy(&field, &Field::editorOwnershipChanged);

    EXPECT_FALSE(field.setEditor(editor, WidgetOwnership::Owned));
    EXPECT_EQ(field.editor(), editor);
    EXPECT_EQ(field.editorOwnership(), WidgetOwnership::Borrowed);
    EXPECT_EQ(ownershipSpy.count(), 0);

    ASSERT_EQ(field.takeEditor(), editor);
    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Owned));
}

TEST_F(FieldTest, Contract_HostingPreservesEditorSizePolicy)
{
    Field field;
    auto* editor = new LineEdit;
    const QSizePolicy policy(QSizePolicy::Fixed, QSizePolicy::Minimum);
    editor->setSizePolicy(policy);

    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Borrowed));
    EXPECT_EQ(editor->sizePolicy(), policy);

    ASSERT_EQ(field.takeEditor(), editor);
    EXPECT_EQ(editor->sizePolicy(), policy);
    delete editor;
}

TEST_F(FieldTest, Contract_ValidationPresentationDoesNotMutateEditorValue)
{
    Field field;
    auto* editor = new LineEdit;
    editor->setText(QStringLiteral("keep-me"));
    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Owned));

    field.setValidationState(Field::ValidationState::Error);
    field.setValidationMessage(QStringLiteral("Too short"));
    field.setValidationState(Field::ValidationState::Warning);
    field.setValidationState(Field::ValidationState::Success);
    field.setValidationMessage(QStringLiteral("Looks good"));

    EXPECT_EQ(editor->text(), QStringLiteral("keep-me"));
    auto* status = field.findChild<Label*>(QStringLiteral("fluentFieldStatus"));
    ASSERT_NE(status, nullptr);
    EXPECT_EQ(status->text(), QStringLiteral("Looks good"));
    EXPECT_FALSE(status->isHidden());
    auto* icon = field.findChild<fluent::FontIcon*>(
        QStringLiteral("fluentFieldStatusIcon"));
    ASSERT_NE(icon, nullptr);
    EXPECT_FALSE(icon->isHidden());
    EXPECT_EQ(icon->glyph(), Typography::Icons::CheckmarkBadge12);
}

TEST_F(FieldTest, Contract_FocusProxyAndCaptionBuddyFollowEditor)
{
    Field field;
    field.setLabelText(QStringLiteral("Name"));
    auto* editor = new LineEdit;
    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Owned));

    EXPECT_EQ(field.focusProxy(), editor);
    auto* caption = field.findChild<Label*>(QStringLiteral("fluentFieldCaption"));
    ASSERT_NE(caption, nullptr);
    EXPECT_EQ(caption->buddy(), editor);

    QWidget* taken = field.takeEditor();
    EXPECT_EQ(field.focusProxy(), nullptr);
    EXPECT_EQ(caption->buddy(), nullptr);
    delete taken;
}

TEST_F(FieldTest, Contract_AccessibleNameAndDescription)
{
    Field field;
    field.setLabelText(QStringLiteral("Email"));
    field.setHelperText(QStringLiteral("Used for recovery"));
    field.setValidationMessage(QStringLiteral("Enter a valid address"));

    EXPECT_EQ(field.accessibleName(), QStringLiteral("Email"));
    EXPECT_EQ(field.accessibleDescription(),
              QStringLiteral("Used for recovery\nEnter a valid address"));
}

TEST_F(FieldTest, Contract_ManagedEditorAccessibleNameFollowsAndRestores)
{
    Field field;
    field.setLabelText(QStringLiteral("Email"));
    auto* editor = new LineEdit;
    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Borrowed));
    EXPECT_EQ(editor->accessibleName(), QStringLiteral("Email"));

    field.setLabelText(QStringLiteral("Work email"));
    EXPECT_EQ(editor->accessibleName(), QStringLiteral("Work email"));

    QWidget* taken = field.takeEditor();
    ASSERT_EQ(taken, editor);
    EXPECT_TRUE(editor->accessibleName().isEmpty());

    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Borrowed));
    editor->setAccessibleName(QStringLiteral("Custom editor name"));
    field.setHelperText(QStringLiteral("Caller-owned accessibility"));
    field.setLabelText(QStringLiteral("Account email"));
    EXPECT_EQ(editor->accessibleName(), QStringLiteral("Custom editor name"));

    taken = field.takeEditor();
    ASSERT_EQ(taken, editor);
    EXPECT_EQ(editor->accessibleName(), QStringLiteral("Custom editor name"));
    delete editor;

    auto* namedEditor = new LineEdit;
    namedEditor->setAccessibleName(QStringLiteral("Preset editor name"));
    ASSERT_TRUE(field.setEditor(namedEditor, WidgetOwnership::Borrowed));
    field.setLabelText(QStringLiteral("Ignored generated name"));
    EXPECT_EQ(
        namedEditor->accessibleName(), QStringLiteral("Preset editor name"));
    delete field.takeEditor();
}

TEST_F(FieldTest, Contract_RequiredIndicator)
{
    Field field;
    field.setLabelText(QStringLiteral("Password"));
    auto* mark = field.findChild<Label*>(QStringLiteral("fluentFieldRequired"));
    ASSERT_NE(mark, nullptr);
    EXPECT_FALSE(mark->isVisible());

    field.setRequired(true);
    EXPECT_FALSE(mark->isHidden());
    EXPECT_EQ(mark->text(), QStringLiteral("*"));
}

TEST_F(FieldTest, Contract_RequiredIndicatorNeedsVisibleLabel)
{
    Field field;
    field.setRequired(true);

    auto* mark = field.findChild<Label*>(QStringLiteral("fluentFieldRequired"));
    ASSERT_NE(mark, nullptr);
    EXPECT_TRUE(mark->isHidden());

    field.setLabelText(QStringLiteral("Account name"));
    EXPECT_FALSE(mark->isHidden());
}

TEST_F(FieldTest, Contract_LongLabelsWrap)
{
    Field field;
    field.setLabelText(QStringLiteral(
        "A long account label that must wrap instead of clipping"));
    auto* editor = new LineEdit;
    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Owned));

    auto* caption = field.findChild<Label*>(QStringLiteral("fluentFieldCaption"));
    ASSERT_NE(caption, nullptr);
    EXPECT_TRUE(caption->wordWrap());
    EXPECT_TRUE(field.hasHeightForWidth());
    EXPECT_GT(field.heightForWidth(180), field.heightForWidth(420));
}

TEST_F(FieldTest, Contract_ThemeAndDisabledDoNotDropEditor)
{
    Field field;
    auto* editor = new LineEdit;
    ASSERT_TRUE(field.setEditor(editor, WidgetOwnership::Owned));

    field.onThemeUpdated();
    field.setEnabled(false);
    EXPECT_EQ(field.editor(), editor);
    field.setEnabled(true);
    EXPECT_EQ(field.editor(), editor);
}

TEST_F(FieldTest, VisualCheck_FieldComposition)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";

    using Edge = fluent::AnchorLayout::Edge;

    FluentTestWindow window;
    window.setFixedSize(560, 420);
    window.setWindowTitle(QStringLiteral("Field VisualCheck"));
    auto* layout = new fluent::AnchorLayout(&window);
    window.setLayout(layout);
    window.onThemeUpdated();

    auto* header = new Label(QStringLiteral("Field composition shell"), &window);
    header->anchors()->top = {&window, Edge::Top, 24};
    header->anchors()->left = {&window, Edge::Left, 32};
    layout->addWidget(header);

    auto* helperField = new Field(&window);
    helperField->setLabelText(QStringLiteral("Email"));
    helperField->setHelperText(QStringLiteral("We'll only use this for recovery."));
    auto* helperEditor = new LineEdit;
    helperEditor->setPlaceholderText(QStringLiteral("name@example.com"));
    ASSERT_TRUE(
        helperField->setEditor(helperEditor, WidgetOwnership::Owned));
    helperField->anchors()->top = {header, Edge::Bottom, 16};
    helperField->anchors()->left = {&window, Edge::Left, 32};
    helperField->anchors()->right = {&window, Edge::Right, -32};
    layout->addWidget(helperField);

    auto* errorField = new Field(&window);
    errorField->setLabelText(QStringLiteral("Password"));
    errorField->setRequired(true);
    errorField->setValidationState(Field::ValidationState::Error);
    errorField->setValidationMessage(
        QStringLiteral("Password must be at least 8 characters."));
    auto* errorEditor = new LineEdit;
    errorEditor->setText(QStringLiteral("1234"));
    ASSERT_TRUE(errorField->setEditor(errorEditor, WidgetOwnership::Owned));
    errorField->anchors()->top = {helperField, Edge::Bottom, 16};
    errorField->anchors()->left = {&window, Edge::Left, 32};
    errorField->anchors()->right = {&window, Edge::Right, -32};
    layout->addWidget(errorField);

    auto* successField = new Field(&window);
    successField->setLabelText(QStringLiteral("Display name"));
    successField->setValidationState(Field::ValidationState::Success);
    successField->setValidationMessage(QStringLiteral("Looks good"));
    auto* successEditor = new LineEdit;
    successEditor->setText(QStringLiteral("Alex"));
    ASSERT_TRUE(
        successField->setEditor(successEditor, WidgetOwnership::Owned));
    successField->anchors()->top = {errorField, Edge::Bottom, 16};
    successField->anchors()->left = {&window, Edge::Left, 32};
    successField->anchors()->right = {&window, Edge::Right, -32};
    layout->addWidget(successField);

    window.show();
    qApp->exec();
}
