#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include <QApplication>
#include <QTest>

#include "components/basicinput/Button.h"
#include "components/foundation/FormController.h"
#include "components/layout/Field.h"
#include "components/navigation/SelectorBar.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "view/widgets/GalleryCodeBlock.h"
#include "view/widgets/GallerySampleCard.h"
#include "view/widgets/GallerySampleCatalog.h"

TEST(GalleryFormTest, Contract_ConnectionSourceKeepsUsageAndFullExampleInBothLanguages)
{
    using namespace fluent::gallery;
    const auto samples = gallerySamplesForRoute(QStringLiteral("field"));
    const auto it = std::find_if(samples.cbegin(), samples.cend(), [](const auto& sample) {
        return sample.id == QStringLiteral("field-connection-form");
    });
    ASSERT_NE(it, samples.cend());
    GallerySampleCard card(QStringLiteral("field"), *it);
    auto* block = card.codeBlock();
    ASSERT_NE(block, nullptr);
    auto* selector = block->findChild<fluent::navigation::SelectorBar*>(
        QStringLiteral("galleryCodeBlockSourceSelector"));
    ASSERT_NE(selector, nullptr);
    for (const auto language : {GalleryCodeLanguage::Cpp, GalleryCodeLanguage::Python}) {
        block->setCodeLanguage(language);
        const auto files = block->sourceFileBundle();
        ASSERT_EQ(files.size(), 1);
        const auto& file = files.first();
        EXPECT_FALSE(file.usageSnippet.isEmpty());
        EXPECT_LE(file.usageSnippet.count(QLatin1Char('\n')), 80);
        EXPECT_EQ(block->code(), file.usageSnippet);
        selector->setSelectedIndex(1);
        EXPECT_EQ(block->code(), file.code);
        EXPECT_TRUE(block->code().contains(QStringLiteral("class ConnectionForm")));
        EXPECT_TRUE(block->code().contains(QStringLiteral("completeValidation")));
        selector->setSelectedIndex(0);
        EXPECT_EQ(block->code(), file.usageSnippet);
    }
}

TEST(GalleryFormTest, Contract_ConnectionValidationSaveAndLateResponse)
{
    const auto samples = fluent::gallery::gallerySamplesForRoute(QStringLiteral("field"));
    auto it = std::find_if(samples.begin(), samples.end(), [](const auto& sample) {
        return sample.id == QStringLiteral("field-connection-form");
    });
    ASSERT_NE(it, samples.end());
    EXPECT_TRUE(it->codeSnippet.contains(QStringLiteral("class ConnectionForm")));
    EXPECT_TRUE(it->codeSnippet.contains(QStringLiteral("completeValidation")));
    std::unique_ptr<QWidget> root(it->createPreview(nullptr));
    root->resize(560, root->sizeHint().height());
    root->show();
    QApplication::processEvents();
    auto* form = root->findChild<fluent::FormController*>();
    auto* address = root->findChild<fluent::textfields::LineEdit*>(QStringLiteral("connectionInput_address"));
    auto* token = root->findChild<fluent::textfields::LineEdit*>(QStringLiteral("connectionInput_token"));
    auto* save = root->findChild<fluent::basicinput::Button*>(QStringLiteral("connectionSave"));
    ASSERT_NE(form, nullptr);
    ASSERT_NE(address, nullptr);
    ASSERT_NE(token, nullptr);
    ASSERT_NE(save, nullptr);
    token->setText(QStringLiteral("demo-token"));
    address->setText(QStringLiteral("https://offline.example"));
    form->validate();
    EXPECT_TRUE(form->isValidating());
    address->setText(QStringLiteral("https://api.example.com/v1"));
    ASSERT_FALSE(form->isValidating());
    ASSERT_TRUE(QTest::qWaitFor([&]() { return save->isEnabled(); }, 1000));
    QTest::qWait(700);
    EXPECT_TRUE(form->errorMessages().isEmpty());
    save->click();
    EXPECT_TRUE(form->isSubmitting());
    ASSERT_TRUE(QTest::qWaitFor([&]() { return !form->isSubmitting(); }, 2500));
    EXPECT_TRUE(form->isValid());
    EXPECT_FALSE(form->isDirty());
    token->clear();
    save->click();
    ASSERT_TRUE(QTest::qWaitFor([&]() { return !form->isSubmitting(); }, 2000));
    EXPECT_EQ(form->invalidFieldKeys(), QStringList{QStringLiteral("token")});
    EXPECT_EQ(root->focusWidget(), token);
    root->resize(340, root->sizeHint().height());
    QApplication::processEvents();
    for (auto* field : root->findChildren<fluent::layout::Field*>()) {
        EXPECT_LE(field->geometry().right(), root->width());
        EXPECT_GE(field->geometry().left(), 0);
        EXPECT_LE(field->editor()->width(), field->width());
    }
}
