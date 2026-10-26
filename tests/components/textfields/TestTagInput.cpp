#include <gtest/gtest.h>
#include "QtTestEnvironment.h"
#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QInputMethodEvent>
#include <QMimeData>
#include <QPointer>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#include "components/basicinput/Button.h"
#include "components/textfields/TagInput.h"
#include "components/textfields/Label.h"
#include <memory>

using fluent::textfields::TagInput;
using fluent::basicinput::Button;

namespace {
class RestoreClipboard {
public:
    RestoreClipboard() : m_previous(new QMimeData)
    {
        if (const auto* data = QApplication::clipboard()->mimeData())
            for (const auto& format : data->formats())
                m_previous->setData(format, data->data(format));
    }
    ~RestoreClipboard() { QApplication::clipboard()->setMimeData(m_previous.release()); }

private:
    std::unique_ptr<QMimeData> m_previous;
};
QVariant member(const QString& key, const QString& text)
{
    return QVariantMap{{QStringLiteral("key"), key}, {QStringLiteral("text"), text}};
}
QList<Button*> tags(TagInput& input)
{
    QList<Button*> result;
    for (auto* button : input.findChildren<Button*>(QStringLiteral("TagInputRemoveButton")))
        if (!button->isHidden())
            result.append(button);
    return result;
}
QRect tagBounds(Button* close, const TagInput& input)
{
    auto* tag = close->parentWidget();
    return QRect(tag->mapTo(&input, QPoint()), tag->size());
}
void commit(TagInput& input, const QString& text)
{
    QInputMethodEvent event;
    event.setCommitString(text);
    QApplication::sendEvent(&input, &event);
}
} // namespace

TEST(TagInputTest, ValueSnapshotsNormalizeAndKeepStableIdentity)
{
    TagInput input;
    QSignalSpy changes(&input, &TagInput::selectionChanged);
    input.setSelectedItems({member("design-chen", QStringLiteral("陈晨")),
        member("engineer-chen", QStringLiteral("陈晨")), member("design-chen", "Duplicate"), QVariantMap{}});
    EXPECT_EQ(input.selectedKeys(), QStringList({"design-chen", "engineer-chen"}));
    EXPECT_EQ(input.selectedTexts(), QStringList({QStringLiteral("陈晨"), QStringLiteral("陈晨")}));
    EXPECT_EQ(changes.count(), 1);
    input.setSelectedItems(input.selectedItems());
    EXPECT_FALSE(input.addItem("design-chen", "Duplicate"));
    EXPECT_FALSE(input.addItem(" ", "Empty"));
    EXPECT_EQ(changes.count(), 1);
    EXPECT_TRUE(input.removeItem("engineer-chen"));
    EXPECT_FALSE(input.removeItem("missing"));
    input.clearSelection();
    input.clearSelection();
    EXPECT_EQ(changes.count(), 3);
}

TEST(TagInputTest, RtlTagsMirrorWithoutLosingQueryAndUseVisualArrowDirection)
{
    TagInput input;
    input.resize(420, 40);
    input.setQueryIconVisible(false);
    input.setSelectedItems({member("1", "Alpha"), member("2", "Beta")});
    ASSERT_TRUE(tests::support::showAndFocusWidget(&input));
    const auto buttons = tags(input);
    ASSERT_EQ(buttons.size(), 2);
    const QRect first = tagBounds(buttons.first(), input);
    const QRect second = tagBounds(buttons.last(), input);
    input.setLayoutDirection(Qt::RightToLeft);
    QApplication::processEvents();
    EXPECT_EQ(tagBounds(buttons.first(), input).x(), input.width() - first.right() - 1);
    EXPECT_EQ(tagBounds(buttons.last(), input).x(), input.width() - second.right() - 1);
    EXPECT_GT(tagBounds(buttons.first(), input).x(), tagBounds(buttons.last(), input).x());
    input.setFocus();
    QTest::keyClick(&input, Qt::Key_Right);
    EXPECT_TRUE(buttons.last()->hasFocus());
    QTest::keyClick(buttons.last(), Qt::Key_Right);
    EXPECT_TRUE(buttons.first()->hasFocus());
    QTest::keyClick(buttons.first(), Qt::Key_Left);
    EXPECT_TRUE(buttons.last()->hasFocus());
    QTest::keyClick(buttons.last(), Qt::Key_Left);
    EXPECT_TRUE(input.hasFocus());
    EXPECT_EQ(input.selectedKeys(), QStringList({"1", "2"}));
}

TEST(TagInputTest, QueryButtonPlacementKeepsTagsAndInputClear)
{
    TagInput input;
    input.resize(420, 40);
    input.setSelectedItems({member("1", "Alpha"), member("2", "Beta")});
    input.setQueryIconVisible(true);
    input.show();
    for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        input.setLayoutDirection(direction);
        for (auto placement : {TagInput::QueryButtonPlacement::Left, TagInput::QueryButtonPlacement::Right}) {
            input.setQueryButtonPlacement(placement);
            QApplication::processEvents();
            auto* query = input.findChild<Button*>("AutoSuggestBoxQueryButton");
            ASSERT_NE(query, nullptr);
            for (auto* tag : tags(input))
                EXPECT_FALSE(tagBounds(tag, input).intersects(query->geometry()));
        }
    }
}

TEST(TagInputTest, WrapsWithoutLosingQuerySelectionOrTags)
{
    TagInput input;
    input.resize(420, 40);
    input.setSelectedItems({member("1", QStringLiteral("陈晨")), member("2", "Alex Chen"),
        member("3", QStringLiteral("王小明")), member("4", QStringLiteral("林晓 · 研发"))});
    input.setText(QStringLiteral("中文查询"));
    input.setSelection(1, 2);
    const auto selected = input.selectedText();
    const int wide = input.height();
    input.resize(280, input.height());
    input.show();
    QApplication::processEvents();
    EXPECT_GE(input.height(), wide);
    EXPECT_EQ(input.text(), QStringLiteral("中文查询"));
    EXPECT_EQ(input.selectedText(), selected);
    const auto buttons = tags(input);
    ASSERT_EQ(buttons.size(), 4);
    for (int i = 0; i < buttons.size(); ++i) {
        EXPECT_TRUE(input.rect().contains(tagBounds(buttons[i], input)));
        for (int j = 0; j < i; ++j)
            EXPECT_FALSE(tagBounds(buttons[i], input).intersects(tagBounds(buttons[j], input)));
    }
    input.resize(420, input.height());
    QApplication::processEvents();
    EXPECT_EQ(input.selectedCount(), 4);
    EXPECT_EQ(input.selectedText(), selected);
}

TEST(TagInputTest, Contract_AdditionsKeepLinearWidgetCountAndExistingIdentities)
{
    TagInput input;
    input.addItem("first", "First");
    auto* first = tags(input).first();
    for (int i = 1; i < 100; ++i)
        ASSERT_TRUE(input.addItem(QString::number(i), QStringLiteral("Member %1").arg(i)));
    // Count before processing DeferredDelete: rebuilding old tags must fail here.
    EXPECT_EQ(input.findChildren<QWidget*>("TagInputTag").size(), 100);
    EXPECT_EQ(input.findChildren<Button*>("TagInputRemoveButton").size(), 100);
    EXPECT_EQ(tags(input).first(), first);
}

TEST(TagInputTest, Contract_ReorderingRenamingAndReadonlyKeepTagFocusAndIdentity)
{
    TagInput input;
    input.resize(420, 40);
    input.setSelectedItems({member("a", "Alex"), member("b", "Bao")});
    ASSERT_TRUE(tests::support::showAndFocusWidget(&input));
    auto* alex = tags(input).first();
    auto* bao = tags(input).last();
    bao->setFocus();
    input.setSelectedItems({member("b", "Bao renamed"), member("a", "Alex"), member("c", "Chen")});
    EXPECT_TRUE(bao->hasFocus());
    EXPECT_EQ(tags(input).first(), bao);
    EXPECT_EQ(bao->accessibleName(), "Remove Bao renamed");
    EXPECT_EQ(input.findChildren<QWidget*>("TagInputTag").size(), 3);
    EXPECT_LT(tagBounds(bao, input).left(), tagBounds(alex, input).left());
    input.setReadOnly(true);
    EXPECT_TRUE(input.hasFocus());
    EXPECT_TRUE(tags(input).isEmpty());
    input.setReadOnly(false);
    EXPECT_EQ(input.findChildren<QWidget*>("TagInputTag").size(), 3);
    EXPECT_TRUE(tags(input).contains(alex));
    EXPECT_TRUE(tags(input).contains(bao));
    input.removeItem("c");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_EQ(input.findChildren<QWidget*>("TagInputTag").size(), 2);
    EXPECT_EQ(input.selectedKeys(), QStringList({"b", "a"}));
}

TEST(TagInputTest, PointerRemovalIsConfinedToCloseButton)
{
    TagInput input;
    input.resize(400, 40);
    input.addItem("member", "A long member name with a team description");
    ASSERT_TRUE(tests::support::showAndFocusWidget(&input));
    QPointer<Button> close = tags(input).first();
    auto* chip = close->parentWidget();
    EXPECT_EQ(close->size(), QSize(24, 24));
    EXPECT_GT(chip->width(), close->width() * 4);
    auto* label = chip->findChild<fluent::textfields::Label*>();
    ASSERT_NE(label, nullptr);
    EXPECT_FALSE(label->isTextElided());
    input.resize(160, input.height());
    QApplication::processEvents();
    ASSERT_TRUE(tests::support::waitForStableLayouts({chip}));
    EXPECT_TRUE(label->isTextElided());
    input.resize(400, input.height());
    QApplication::processEvents();
    ASSERT_TRUE(tests::support::waitForStableLayouts({chip}));
    EXPECT_FALSE(label->isTextElided());
    QSignalSpy selection(&input, &TagInput::selectionChanged);
    QTest::mouseClick(chip, Qt::LeftButton, Qt::NoModifier, QPoint(12, chip->height() / 2));
    EXPECT_EQ(input.selectedCount(), 1);
    EXPECT_EQ(selection.count(), 0);
    EXPECT_TRUE(close->hasFocus());
    QTest::mousePress(chip, Qt::LeftButton, Qt::NoModifier, QPoint(12, chip->height() / 2));
    QTest::mouseRelease(chip, Qt::LeftButton, Qt::NoModifier, close->geometry().center());
    EXPECT_EQ(input.selectedCount(), 1);
    auto* accessible = QAccessible::queryAccessibleInterface(close);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->rect().size(), close->size());
    QTest::mouseClick(close, Qt::LeftButton);
    EXPECT_EQ(input.selectedCount(), 0);
    EXPECT_EQ(selection.count(), 1);
    EXPECT_TRUE(input.hasFocus());
}

TEST(TagInputTest, KeyboardFocusRemovalAndTabExit)
{
    QWidget window;
    QVBoxLayout layout(&window);
    TagInput input;
    Button after("After");
    layout.addWidget(&input);
    layout.addWidget(&after);
    input.setSelectedItems({member("a", "Alex"), member("b", QStringLiteral("林晓"))});
    ASSERT_TRUE(tests::support::showAndFocusWidget(&input));
    QTest::keyClick(&input, Qt::Key_Backspace);
    EXPECT_EQ(input.selectedCount(), 2);
    ASSERT_EQ(QApplication::focusWidget(), tags(input).last());
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Backspace);
    EXPECT_EQ(input.selectedKeys(), QStringList({"a"}));
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Right);
    EXPECT_TRUE(input.hasFocus());
    QTest::keyClick(&input, Qt::Key_Left);
    QTest::keyClick(QApplication::focusWidget(), Qt::Key_Tab);
    EXPECT_TRUE(after.hasFocus());
}

TEST(TagInputTest, InputMethodPreeditDoesNotSearchSelectOrDelete)
{
    TagInput input;
    input.resize(360, 40);
    input.addItem("a", "Alex");
    input.setAsyncSuggestionsEnabled(true);
    input.setDebounceInterval(5);
    QSignalSpy requests(&input, &TagInput::suggestionsRequested);
    QSignalSpy submitted(&input, &TagInput::tokensSubmitted);
    QSignalSpy chosen(&input, &TagInput::suggestionChosen);
    input.show();
    input.activateWindow();
    input.setFocus();
    QApplication::processEvents();
    QInputMethodEvent preedit(QStringLiteral("chen"), {});
    QApplication::sendEvent(&input, &preedit);
    QTest::keyClick(&input, Qt::Key_Down);
    QTest::keyClick(&input, Qt::Key_Return);
    QTest::keyClick(&input, Qt::Key_Backspace);
    QTest::qWait(20);
    EXPECT_EQ(requests.count(), 0);
    EXPECT_EQ(submitted.count(), 0);
    EXPECT_EQ(chosen.count(), 0);
    EXPECT_EQ(input.selectedCount(), 1);
    commit(input, QStringLiteral("陈"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return requests.count() == 1; }, 300));
    EXPECT_EQ(requests.at(0).at(0).toString(), QStringLiteral("陈"));
    EXPECT_EQ(input.text(), QStringLiteral("陈"));
}

TEST(TagInputTest, InputMethodFromFocusedTagKeepsTheFirstSyllableInComposition)
{
    TagInput input;
    input.addItem("alex", "Alex");
    input.resize(360, 40);
    input.setAsyncSuggestionsEnabled(true);
    input.setDebounceInterval(1);
    ASSERT_TRUE(tests::support::showAndFocusWidget(&input));
    QSignalSpy requests(&input, &TagInput::suggestionsRequested);
    QTest::keyClick(&input, Qt::Key_Backspace);
    auto* tag = tags(input).first();
    ASSERT_EQ(QApplication::focusWidget(), tag);
    EXPECT_TRUE(tag->testAttribute(Qt::WA_InputMethodEnabled));
    const auto editorCursor = input.inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    const auto tagCursor = tag->inputMethodQuery(Qt::ImCursorRectangle).toRectF();
    EXPECT_EQ(tagCursor.translated(tag->mapToGlobal(QPoint())), editorCursor.translated(input.mapToGlobal(QPoint())));
    QInputMethodEvent preedit(QStringLiteral("chen"), {});
    QApplication::sendEvent(tag, &preedit);
    QTest::keyClick(tag, Qt::Key_Backspace);
    QTest::qWait(20);
    EXPECT_TRUE(input.text().isEmpty());
    EXPECT_EQ(input.selectedCount(), 1);
    EXPECT_EQ(requests.count(), 0);
    QInputMethodEvent committed;
    committed.setCommitString(QStringLiteral("陈"));
    QApplication::sendEvent(tag, &committed);
    ASSERT_TRUE(QTest::qWaitFor([&] { return input.hasFocus(); }, 300));
    EXPECT_EQ(input.text(), QStringLiteral("陈"));
    EXPECT_EQ(input.selectedCount(), 1);
    ASSERT_TRUE(QTest::qWaitFor([&] { return requests.count() == 1; }, 300));
}

TEST(TagInputTest, InputMethodCommitDoesNotStealFocusFromApplicationCallbacks)
{
    QWidget window;
    QVBoxLayout layout(&window);
    TagInput input;
    Button after("After");
    layout.addWidget(&input);
    layout.addWidget(&after);
    input.addItem("alex", "Alex");
    ASSERT_TRUE(tests::support::showAndFocusWidget(&input));
    QTest::keyClick(&input, Qt::Key_Backspace);
    auto* tag = tags(input).first();
    QInputMethodEvent preedit(QStringLiteral("chen"), {});
    QApplication::sendEvent(tag, &preedit);
    QObject::connect(&input, &QLineEdit::textChanged, &after, [&after] { after.setFocus(); });
    QInputMethodEvent committed;
    committed.setCommitString(QStringLiteral("陈"));
    QApplication::sendEvent(tag, &committed);
    QApplication::processEvents();
    EXPECT_TRUE(after.hasFocus());
    EXPECT_EQ(input.text(), QStringLiteral("陈"));
}

TEST(TagInputTest, AsyncResultsAndResetsPreserveSelectedMembers)
{
    TagInput input;
    QStandardItemModel model;
    input.setSuggestionModel(&model);
    input.setAsyncSuggestionsEnabled(true);
    input.setDebounceInterval(1);
    input.addItem("design", QStringLiteral("陈晨"));
    input.resize(360, 40);
    input.show();
    input.activateWindow();
    input.setFocus();
    QApplication::processEvents();
    QSignalSpy requests(&input, &TagInput::suggestionsRequested);
    commit(input, QStringLiteral("陈"));
    ASSERT_TRUE(QTest::qWaitFor([&] { return requests.count() == 1; }, 300));
    const auto token = requests.at(0).at(1).toULongLong();
    auto* row = new QStandardItem(QStringLiteral("陈晨 · 研发团队"));
    row->setData(member("engineering", QStringLiteral("陈晨")), Qt::UserRole);
    model.appendRow(row);
    ASSERT_TRUE(input.completeSuggestionRequest(token));
    QTest::keyClick(&input, Qt::Key_Down);
    EXPECT_EQ(input.selectedCount(), 1);
    QTest::keyClick(&input, Qt::Key_Return);
    EXPECT_EQ(input.selectedKeys(), QStringList({"design", "engineering"}));
    EXPECT_TRUE(input.text().isEmpty());
    model.clear();
    EXPECT_EQ(input.selectedCount(), 2);
    EXPECT_FALSE(input.completeSuggestionRequest(token));
}

TEST(TagInputTest, PasteSubmitsExactTokensAndReadOnlyBlocksUserMutations)
{
    RestoreClipboard clipboard;
    TagInput input;
    input.addItem("a", "Alex");
    input.resize(360, 40);
    input.show();
    input.activateWindow();
    input.setFocus();
    QApplication::processEvents();
    QSignalSpy submitted(&input, &TagInput::tokensSubmitted);
    QApplication::clipboard()->setText(QStringLiteral("chen@example.com； lin@example.com; chen@example.com"));
    input.paste();
    ASSERT_TRUE(QTest::qWaitFor([&] { return submitted.count() == 1; }, 300));
    EXPECT_EQ(submitted.at(0).at(0).toStringList(), QStringList({"chen@example.com", "lin@example.com"}));
    EXPECT_EQ(input.selectedCount(), 1);
    input.clear();
    input.setReadOnly(true);
    QTest::keyClick(&input, Qt::Key_Backspace);
    QTest::keyClick(&input, Qt::Key_Down);
    QTest::keyClick(&input, Qt::Key_Return);
    EXPECT_TRUE(tags(input).isEmpty());
    QList<fluent::textfields::Label*> labels;
    for (auto* label : input.findChildren<fluent::textfields::Label*>())
        if (label->isVisible())
            labels.append(label);
    ASSERT_EQ(labels.size(), 1);
    EXPECT_EQ(labels[0]->textColorRole(), fluent::textfields::Label::TextColorRole::Primary);
    auto* iface = QAccessible::queryAccessibleInterface(labels[0]);
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->role(), QAccessible::StaticText);
    EXPECT_EQ(iface->text(QAccessible::Name), QStringLiteral("Alex"));
    EXPECT_EQ(input.selectedCount(), 1);
    EXPECT_EQ(submitted.count(), 1);
    input.setEnabled(false);
    EXPECT_EQ(labels[0]->textColorRole(), fluent::textfields::Label::TextColorRole::Disabled);
    input.setEnabled(true);
    input.setReadOnly(false);
    ASSERT_EQ(tags(input).size(), 1);
    tags(input).first()->click();
    EXPECT_EQ(input.selectedCount(), 0);
}

TEST(TagInputTest, AccessibleSelectedValuesAndDeleteCallbacks)
{
    auto* input = new TagInput;
    input->setAccessibleName("Project members");
    input->addItem("alex", "Alex");
    auto* iface = QAccessible::queryAccessibleInterface(input);
    ASSERT_NE(iface, nullptr);
    EXPECT_EQ(iface->role(), QAccessible::EditableText);
    const auto buttons = tags(*input);
    ASSERT_EQ(buttons.size(), 1);
    auto* tagIface = QAccessible::queryAccessibleInterface(buttons[0]);
    ASSERT_NE(tagIface, nullptr);
    EXPECT_EQ(tagIface->text(QAccessible::Name), QStringLiteral("Remove Alex"));
    QPointer<TagInput> guard(input);
    QObject::connect(input, &TagInput::selectionChanged, input, [input] { delete input; });
    input->addItem("lin", QStringLiteral("林晓"));
    EXPECT_TRUE(guard.isNull());
}

TEST(TagInputTest, Contract_TagActivationDoesNotInsertQueryText)
{
    for (auto key : {Qt::Key_Space, Qt::Key_Return, Qt::Key_Enter}) {
        TagInput input;
        input.resize(360, 80);
        input.addItem("alex", "Alex");
        input.show();
        input.activateWindow();
        auto* button = input.findChild<Button*>("TagInputRemoveButton");
        ASSERT_TRUE(button);
        button->setFocus();
        ASSERT_TRUE(QTest::qWaitFor([&] { return button->hasFocus(); }));
        QTest::keyClick(button, key);
        EXPECT_EQ(input.selectedCount(), 0);
        EXPECT_TRUE(input.text().isEmpty());
    }
}

TEST(TagInputTest, Contract_RetiredTagDoesNotShadowAReplacementBeforeDeferredDeletion)
{
    TagInput input;
    input.addItem("same", "Before");
    QPointer<QWidget> retired = input.findChild<QWidget*>("TagInputTag");
    input.removeItem("same");
    input.addItem("same", "After");
    const auto tags = input.findChildren<QWidget*>("TagInputTag");
    ASSERT_EQ(tags.size(), 1);
    EXPECT_NE(tags.first(), retired.data());
    EXPECT_EQ(tags.first()->toolTip(), "After");
    ASSERT_TRUE(retired);
    EXPECT_FALSE(retired->property("tagKey").isValid());
}

TEST(TagInputTest, Contract_TextCommitSpaceActivatesTagOnlyOutsideComposition)
{
    for (bool composing : {false, true}) {
        TagInput input;
        input.resize(360, 80);
        input.addItem("alex", "Alex");
        input.show();
        input.activateWindow();
        auto* button = input.findChild<Button*>("TagInputRemoveButton");
        ASSERT_TRUE(button);
        button->setFocus();
        ASSERT_TRUE(QTest::qWaitFor([&] { return button->hasFocus(); }));
        if (composing) {
            QInputMethodEvent preedit(QStringLiteral("pinyin"), {});
            QApplication::sendEvent(button, &preedit);
        }
        QInputMethodEvent space;
        space.setCommitString(QStringLiteral(" "));
        QApplication::sendEvent(button, &space);
        EXPECT_EQ(input.selectedCount(), composing ? 1 : 0);
        EXPECT_EQ(input.text(), composing ? QStringLiteral(" ") : QString());
    }
}
