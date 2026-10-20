#include <gtest/gtest.h>

#include <QApplication>
#include <QPointer>
#include <QSignalSpy>
#include <QTimer>
#include <QtTest>

#include "components/foundation/FormController.h"
#include "components/layout/Field.h"
#include "components/textfields/LineEdit.h"

using fluent::FormController;
using fluent::layout::Field;
using fluent::textfields::LineEdit;

TEST(FormControllerTest, Contract_BorrowedFieldsAndIndependentEditState)
{
    QWidget owner;
    Field name(&owner), address(&owner);
    {
        FormController form;
        EXPECT_FALSE(form.addField(QString(), &name));
        ASSERT_TRUE(form.addField(QStringLiteral("name"), &name));
        ASSERT_TRUE(form.addField(QStringLiteral("address"), &address));
        EXPECT_FALSE(form.addField(QStringLiteral("name"), &address));
        EXPECT_FALSE(form.addField(QStringLiteral("duplicate"), &name));
        EXPECT_EQ(name.parentWidget(), &owner);
        EXPECT_FALSE(form.isDirty());
        EXPECT_FALSE(form.isValid());
        form.markFieldChanged(QStringLiteral("name"));
        form.setFieldTouched(QStringLiteral("address"));
        EXPECT_TRUE(form.isDirty());
        EXPECT_TRUE(form.isTouched());
        EXPECT_FALSE(form.isFieldTouched(QStringLiteral("name")));
        EXPECT_FALSE(form.isFieldDirty(QStringLiteral("address")));
        form.markClean();
        EXPECT_FALSE(form.isDirty());
        EXPECT_TRUE(form.isTouched());
    }
    EXPECT_EQ(name.parentWidget(), &owner);
}

TEST(FormControllerTest, Contract_LateResultsCannotOverwriteNewInputOrReplacement)
{
    FormController form;
    Field address, replacement;
    LineEdit editor;
    editor.setText(QStringLiteral("https://new.example"));
    address.setEditor(&editor);
    form.addField(QStringLiteral("address"), &address);
    const auto oldToken = form.validateField(QStringLiteral("address"));
    EXPECT_TRUE(form.isValidating());
    form.markFieldChanged(QStringLiteral("address"));
    EXPECT_FALSE(form.completeValidation(QStringLiteral("address"), oldToken, false, QStringLiteral("Old failure")));
    const auto current = form.validateField(QStringLiteral("address"));
    EXPECT_TRUE(form.completeValidation(QStringLiteral("address"), current, true));
    EXPECT_TRUE(form.isValid());
    EXPECT_TRUE(address.validationMessage().isEmpty());
    EXPECT_EQ(editor.text(), QStringLiteral("https://new.example"));
    EXPECT_FALSE(form.completeValidation(QStringLiteral("address"), current, false));
    const auto removed = form.validateField(QStringLiteral("address"));
    form.removeField(QStringLiteral("address"));
    form.addField(QStringLiteral("address"), &replacement);
    EXPECT_FALSE(form.completeValidation(QStringLiteral("address"), removed, false));
    EXPECT_TRUE(replacement.validationMessage().isEmpty());
}

TEST(FormControllerTest, Contract_SubmissionWaitsForAllValidatorsAndRejectsDoubleSubmit)
{
    FormController form;
    Field name, address;
    form.addField(QStringLiteral("name"), &name);
    form.addField(QStringLiteral("address"), &address);
    form.markFieldChanged(QStringLiteral("address"));
    quint64 addressToken = 0;
    QObject::connect(&form, &FormController::validationRequested, &form, [&](const QString& key, quint64 token) {
        if (key == QStringLiteral("name"))
            form.completeValidation(key, token, true);
        else
            addressToken = token;
    });
    QSignalSpy submitted(&form, &FormController::submitRequested);
    ASSERT_TRUE(form.submit());
    EXPECT_TRUE(form.isSubmitting());
    EXPECT_FALSE(form.submit());
    EXPECT_EQ(submitted.count(), 0);
    ASSERT_TRUE(form.completeValidation(QStringLiteral("address"), addressToken, true));
    ASSERT_EQ(submitted.count(), 1);
    const quint64 token = submitted.at(0).at(0).toULongLong();
    EXPECT_FALSE(form.completeSubmit(token + 1, true));
    EXPECT_TRUE(form.completeSubmit(token, false, QStringLiteral("Server unavailable")));
    EXPECT_TRUE(form.isDirty());
    EXPECT_EQ(form.submissionError(), QStringLiteral("Server unavailable"));
    ASSERT_TRUE(form.submit());
    ASSERT_TRUE(form.completeValidation(QStringLiteral("address"), addressToken, true));
    ASSERT_EQ(submitted.count(), 2);
    EXPECT_TRUE(form.completeSubmit(submitted.at(1).at(0).toULongLong(), true));
    EXPECT_FALSE(form.isDirty());
    EXPECT_FALSE(form.isSubmitting());
    EXPECT_TRUE(form.submissionError().isEmpty());
}

TEST(FormControllerTest, Contract_SynchronousValidationCannotSubmitBeforeLaterFields)
{
    FormController form;
    Field first, last;
    first.setLabelText(QStringLiteral("Name"));
    last.setLabelText(QStringLiteral("Address"));
    form.addField(QStringLiteral("first"), &first);
    form.addField(QStringLiteral("last"), &last);
    QObject::connect(&form, &FormController::validationRequested, &form, [&](const QString& key, quint64 token) {
        form.completeValidation(key, token, key == QStringLiteral("first"), QStringLiteral("Required"));
    });
    QSignalSpy submitted(&form, &FormController::submitRequested);
    QSignalSpy finished(&form, &FormController::submissionFinished);
    form.submit();
    EXPECT_EQ(submitted.count(), 0);
    EXPECT_EQ(finished.count(), 1);
    EXPECT_FALSE(form.isSubmitting());
    EXPECT_EQ(form.invalidFieldKeys(), QStringList{QStringLiteral("last")});
    EXPECT_EQ(form.errorMessages(), QStringList{QStringLiteral("Address: Required")});
    EXPECT_TRUE(form.isFieldTouched(QStringLiteral("first")));
}

TEST(FormControllerTest, Contract_EditOrResetInvalidatesPendingSaveAndDoesNotWriteValues)
{
    FormController form;
    Field field;
    auto* editor = new LineEdit;
    field.setEditor(editor, fluent::WidgetOwnership::Owned);
    editor->setText(QStringLiteral("keep"));
    form.addField(QStringLiteral("value"), &field);
    QObject::connect(&form, &FormController::validationRequested, &form, [&](const QString& key, quint64 token) {
        form.completeValidation(key, token, true);
    });
    QSignalSpy submitted(&form, &FormController::submitRequested);
    form.submit();
    ASSERT_EQ(submitted.count(), 1);
    form.markFieldChanged(QStringLiteral("value"));
    EXPECT_FALSE(form.completeSubmit(submitted.at(0).at(0).toULongLong(), true));
    EXPECT_TRUE(form.isDirty());
    form.reset();
    EXPECT_FALSE(form.isDirty());
    EXPECT_FALSE(form.isTouched());
    EXPECT_EQ(editor->text(), QStringLiteral("keep"));
    EXPECT_EQ(field.validationState(), Field::ValidationState::None);
}

TEST(FormControllerTest, Contract_DestructionAndReentrantResetDuringValidation)
{
    FormController form;
    auto* field = new Field;
    form.addField(QStringLiteral("value"), field);
    auto token = form.validateField(QStringLiteral("value"));
    delete field;
    EXPECT_TRUE(form.fieldKeys().isEmpty());
    EXPECT_FALSE(form.completeValidation(QStringLiteral("value"), token, true));
    Field next;
    form.addField(QStringLiteral("next"), &next);
    QObject::connect(&form, &FormController::validationRequested, &form, [&](const QString&, quint64) {
        form.reset();
    });
    QSignalSpy submitted(&form, &FormController::submitRequested);
    form.submit();
    EXPECT_FALSE(form.isSubmitting());
    EXPECT_FALSE(form.isValidating());
    EXPECT_TRUE(next.validationMessage().isEmpty());
    EXPECT_EQ(submitted.count(), 0);
    QPointer<FormController> destroyed = new FormController;
    destroyed->addField(QStringLiteral("next"), &next);
    QObject::connect(destroyed, &FormController::validationRequested, &next, [&]() { delete destroyed; });
    destroyed->validate();
    EXPECT_TRUE(destroyed.isNull());
}

TEST(FormControllerTest, Contract_ErrorFocusFollowsRegistrationOrder)
{
    QWidget host;
    Field first(&host), second(&host);
    auto* a = new LineEdit;
    auto* b = new LineEdit;
    first.setEditor(a, fluent::WidgetOwnership::Owned);
    second.setEditor(b, fluent::WidgetOwnership::Owned);
    first.setGeometry(0, 0, 240, 100);
    second.setGeometry(0, 110, 240, 100);
    host.show();
    QApplication::processEvents();
    FormController form;
    form.addField(QStringLiteral("a"), &first);
    form.addField(QStringLiteral("b"), &second);
    const auto bToken = form.validateField(QStringLiteral("b"));
    form.completeValidation(QStringLiteral("b"), bToken, false);
    const auto aToken = form.validateField(QStringLiteral("a"));
    form.completeValidation(QStringLiteral("a"), aToken, false);
    EXPECT_TRUE(form.focusFirstError());
    EXPECT_EQ(host.focusWidget(), a);
    a->setEnabled(false);
    EXPECT_TRUE(form.focusFirstError());
    EXPECT_EQ(host.focusWidget(), b);
}
