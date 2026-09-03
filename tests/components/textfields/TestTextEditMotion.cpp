#include "components/foundation/MotionPolicy.h"
#include "components/scrolling/ScrollBar.h"
#include "components/textfields/TextEdit.h"

#include <QApplication>
#include <QInputMethodEvent>
#include <QTextCursor>
#include <QTextEdit>
#include <QVariantAnimation>
#include <QWidget>
#include <QtTest/QTest>
#include <gtest/gtest.h>

namespace {

using fluent::MotionPolicy;
using fluent::textfields::TextEdit;

class MotionModeRestorer {
public:
    MotionModeRestorer() : m_previous(MotionPolicy::instance().mode())
    {
        MotionPolicy::instance().setMode(MotionPolicy::Mode::Full);
    }

    ~MotionModeRestorer() { MotionPolicy::instance().setMode(m_previous); }

private:
    MotionPolicy::Mode m_previous;
};

QTextEdit* innerTextEdit(TextEdit* edit)
{
    return edit ? edit->findChild<QTextEdit*>() : nullptr;
}

QVariantAnimation* heightAnimation(TextEdit* edit)
{
    return edit ? edit->findChild<QVariantAnimation*>(
                      QStringLiteral("fluentTextEditHeightAnimation"), Qt::FindDirectChildrenOnly)
                : nullptr;
}

void showFocusedEditor(QWidget& window, TextEdit& edit)
{
    edit.setGeometry(20, 20, 360, edit.height());
    window.resize(420, 260);
    window.show();
    edit.show();
    QApplication::processEvents();

    QTextEdit* inner = innerTextEdit(&edit);
    ASSERT_NE(inner, nullptr);
    inner->setFocus(Qt::OtherFocusReason);
    QApplication::processEvents();
    ASSERT_TRUE(inner->hasFocus());
}

void insertParagraph(QTextEdit* inner)
{
    ASSERT_NE(inner, nullptr);
    QTextCursor cursor = inner->textCursor();
    cursor.insertBlock();
    inner->setTextCursor(cursor);
    QApplication::processEvents();
}

void deletePreviousCharacter(QTextEdit* inner)
{
    ASSERT_NE(inner, nullptr);
    QTextCursor cursor = inner->textCursor();
    cursor.deletePreviousChar();
    inner->setTextCursor(cursor);
    QApplication::processEvents();
}

} // namespace

TEST(TextEditMotionTest, Contract_FocusedEditsAnimateRetargetAndCollapseVisibleLineHeight)
{
    MotionModeRestorer restoreMotionMode;
    QWidget window;
    TextEdit edit(&window);
    edit.setLineHeight(32);
    edit.setMinVisibleLines(1);
    edit.setMaxVisibleLines(4);
    showFocusedEditor(window, edit);

    QTextEdit* inner = innerTextEdit(&edit);
    QVariantAnimation* animation = heightAnimation(&edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(animation, nullptr);
    const int oneLineHeight = edit.height();
    ASSERT_GT(oneLineHeight, 0);

    insertParagraph(inner);
    const int twoLineHeight = animation->endValue().toInt();

    EXPECT_EQ(inner->toPlainText(), QStringLiteral("\n"));
    EXPECT_TRUE(edit.isVisible());
    EXPECT_TRUE(inner->hasFocus());
    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_EQ(animation->duration(), edit.themeAnimation().fast);
    EXPECT_GT(twoLineHeight, oneLineHeight);
    EXPECT_LT(edit.height(), twoLineHeight);

    insertParagraph(inner);
    const int threeLineHeight = animation->endValue().toInt();

    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_GT(threeLineHeight, twoLineHeight);
    QTRY_COMPARE_WITH_TIMEOUT(edit.height(), threeLineHeight, 1000);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);

    deletePreviousCharacter(inner);

    EXPECT_EQ(inner->toPlainText(), QStringLiteral("\n"));
    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_EQ(animation->endValue().toInt(), twoLineHeight);
    EXPECT_GT(edit.height(), twoLineHeight);
    QTRY_COMPARE_WITH_TIMEOUT(edit.height(), twoLineHeight, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(animation->state(), QAbstractAnimation::Stopped, 1000);
}

TEST(TextEditMotionTest, Contract_ReducedAndDisabledMotionResolveEditedHeight)
{
    MotionModeRestorer restoreMotionMode;
    QWidget window;
    TextEdit edit(&window);
    edit.setLineHeight(32);
    edit.setMinVisibleLines(1);
    edit.setMaxVisibleLines(4);
    showFocusedEditor(window, edit);

    QTextEdit* inner = innerTextEdit(&edit);
    QVariantAnimation* animation = heightAnimation(&edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(animation, nullptr);
    const int oneLineHeight = edit.height();

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Reduced);
    insertParagraph(inner);
    const int twoLineHeight = animation->endValue().toInt();

    EXPECT_EQ(inner->toPlainText(), QStringLiteral("\n"));
    EXPECT_TRUE(edit.isVisible());
    EXPECT_TRUE(inner->hasFocus());
    EXPECT_EQ(animation->duration(), 50);
    EXPECT_GT(twoLineHeight, oneLineHeight);
    QTRY_COMPARE_WITH_TIMEOUT(edit.height(), twoLineHeight, 1000);

    edit.clear();
    QApplication::processEvents();
    ASSERT_EQ(edit.height(), oneLineHeight);

    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    insertParagraph(inner);

    EXPECT_EQ(animation->duration(), 0);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    EXPECT_EQ(edit.height(), twoLineHeight);
}

TEST(TextEditMotionTest, Contract_ProgrammaticHeightChangesRemainSynchronous)
{
    MotionModeRestorer restoreMotionMode;
    QWidget window;
    TextEdit edit(&window);
    edit.setLineHeight(32);
    edit.setMinVisibleLines(1);
    edit.setMaxVisibleLines(4);
    showFocusedEditor(window, edit);

    QVariantAnimation* animation = heightAnimation(&edit);
    ASSERT_NE(animation, nullptr);
    const int oneLineHeight = edit.height();

    edit.setPlainText(QStringLiteral("Alpha\nBeta\nGamma"));
    const int threeLineHeight = edit.height();

    EXPECT_GT(threeLineHeight, oneLineHeight);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);

    edit.clear();
    EXPECT_EQ(edit.height(), oneLineHeight);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);

    edit.setMinVisibleLines(3);
    EXPECT_EQ(edit.height(), threeLineHeight);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
}

TEST(TextEditMotionTest, Contract_ProgrammaticSameTargetUpdateStopsActiveUserTransition)
{
    MotionModeRestorer restoreMotionMode;
    QWidget window;
    TextEdit edit(&window);
    edit.setLineHeight(32);
    edit.setMinVisibleLines(1);
    edit.setMaxVisibleLines(4);
    showFocusedEditor(window, edit);

    QTextEdit* inner = innerTextEdit(&edit);
    QVariantAnimation* animation = heightAnimation(&edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(animation, nullptr);
    const int oneLineHeight = edit.height();

    insertParagraph(inner);
    const int twoLineHeight = animation->endValue().toInt();

    ASSERT_EQ(animation->state(), QAbstractAnimation::Running);
    ASSERT_GT(twoLineHeight, oneLineHeight);
    ASSERT_LT(edit.height(), twoLineHeight);

    edit.setPlainText(QStringLiteral("Alpha\nBeta"));

    EXPECT_EQ(edit.toPlainText(), QStringLiteral("Alpha\nBeta"));
    EXPECT_EQ(edit.height(), twoLineHeight);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
}

TEST(TextEditMotionTest, Contract_InputMethodPreeditKeepsHeightStableUntilCommit)
{
    MotionModeRestorer restoreMotionMode;
    QWidget window;
    TextEdit edit(&window);
    edit.setLineHeight(32);
    edit.setMinVisibleLines(1);
    edit.setMaxVisibleLines(4);
    edit.setFixedWidth(140);
    showFocusedEditor(window, edit);

    QTextEdit* inner = innerTextEdit(&edit);
    QVariantAnimation* animation = heightAnimation(&edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(animation, nullptr);
    const int oneLineHeight = edit.height();
    const QString committedText(28, QChar(0x4e2d));

    for (const int characterCount : {12, 28, 56}) {
        QInputMethodEvent preedit(QString(characterCount, QChar(0x4e2d)), {});
        QApplication::sendEvent(inner, &preedit);
        QTest::qWait(30);
        QApplication::processEvents();

        EXPECT_TRUE(edit.toPlainText().isEmpty());
        EXPECT_EQ(edit.height(), oneLineHeight);
        EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    }

    QInputMethodEvent cancel;
    QApplication::sendEvent(inner, &cancel);
    QTest::qWait(30);
    QApplication::processEvents();

    EXPECT_TRUE(edit.toPlainText().isEmpty());
    EXPECT_EQ(edit.height(), oneLineHeight);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);

    QInputMethodEvent committedPreedit(QString(28, QChar(0x4e2d)), {});
    QApplication::sendEvent(inner, &committedPreedit);
    QTest::qWait(30);
    QApplication::processEvents();

    EXPECT_TRUE(edit.toPlainText().isEmpty());
    EXPECT_EQ(edit.height(), oneLineHeight);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);

    QInputMethodEvent commit;
    commit.setCommitString(committedText);
    QApplication::sendEvent(inner, &commit);
    QApplication::processEvents();
    const int committedHeight = animation->endValue().toInt();

    EXPECT_EQ(edit.toPlainText(), committedText);
    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_GT(committedHeight, oneLineHeight);
    EXPECT_LT(edit.height(), committedHeight);
    QTRY_COMPARE_WITH_TIMEOUT(edit.height(), committedHeight, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(animation->state(), QAbstractAnimation::Stopped, 1000);
}

TEST(TextEditMotionTest, Contract_CrossingVisibleLineLimitKeepsHeightTransitionRunning)
{
    MotionModeRestorer restoreMotionMode;
    QWidget window;
    TextEdit edit(&window);
    edit.setLineHeight(32);
    edit.setMinVisibleLines(1);
    edit.setMaxVisibleLines(4);
    edit.setFixedWidth(360);
    edit.setPlainText(QStringLiteral("Alpha\nBeta\nGamma"));
    showFocusedEditor(window, edit);

    QTextEdit* inner = innerTextEdit(&edit);
    QVariantAnimation* animation = heightAnimation(&edit);
    ASSERT_NE(inner, nullptr);
    ASSERT_NE(animation, nullptr);
    const int threeLineHeight = edit.height();

    QTextCursor cursor = inner->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(QStringLiteral("\nDelta\nEpsilon"));
    inner->setTextCursor(cursor);
    QApplication::processEvents();
    const int fourLineHeight = animation->endValue().toInt();

    ASSERT_TRUE(edit.verticalScrollBar()->isVisible());
    ASSERT_EQ(animation->state(), QAbstractAnimation::Running);
    ASSERT_GT(fourLineHeight, threeLineHeight);

    QTest::qWait(40);
    QApplication::processEvents();

    EXPECT_EQ(animation->state(), QAbstractAnimation::Running);
    EXPECT_GT(edit.height(), threeLineHeight);
    EXPECT_LT(edit.height(), fourLineHeight);
    EXPECT_LT(inner->width(), edit.width());
    QTRY_COMPARE_WITH_TIMEOUT(edit.height(), fourLineHeight, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(animation->state(), QAbstractAnimation::Stopped, 1000);
}
