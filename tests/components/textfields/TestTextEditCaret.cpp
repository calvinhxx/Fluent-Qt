#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QScrollBar>
#include <QTextEdit>
#include <QTest>
#include "components/textfields/TextEdit.h"

using fluent::textfields::TextEdit;

TEST(TextEditCaretTest, Contract_RectUsesOuterCoordinatesAndScrollOffsets)
{
    TextEdit edit;
    edit.setMinVisibleLines(2);
    edit.setMaxVisibleLines(2);
    edit.resize(300, edit.height());
    edit.setContentMargins(QMargins(17, 6, 13, 8));
    edit.setPlainText("First\nSecond\nThird\nFourth\nFifth");
    edit.show();
    auto* inner = qobject_cast<QTextEdit*>(edit.focusProxy());
    ASSERT_NE(inner, nullptr);
    auto cursor = edit.textCursor();
    cursor.movePosition(QTextCursor::End);
    edit.setTextCursor(cursor);
    QCoreApplication::processEvents();
    EXPECT_EQ(edit.cursorRect(), inner->cursorRect().translated(inner->viewport()->mapTo(&edit, QPoint())));
    ASSERT_GT(inner->verticalScrollBar()->maximum(), 0);
    QSignalSpy changed(&edit, &TextEdit::cursorRectChanged);
    const QRect before = edit.cursorRect();
    inner->verticalScrollBar()->setValue(0);
    ASSERT_TRUE(QTest::qWaitFor([&]() { return !changed.isEmpty(); }, 1000));
    EXPECT_GT(edit.cursorRect().top(), before.top());
    EXPECT_EQ(edit.cursorRect(), inner->cursorRect().translated(inner->viewport()->mapTo(&edit, QPoint())));
}

TEST(TextEditCaretTest, Contract_LayoutAndNoOpNotifications)
{
    TextEdit edit;
    edit.setMinVisibleLines(4);
    edit.setMaxVisibleLines(4);
    edit.resize(400, edit.height());
    edit.setPlainText("A wrapping line with enough content to move the current caret when resized.");
    edit.show();
    auto cursor = edit.textCursor();
    cursor.movePosition(QTextCursor::End);
    edit.setTextCursor(cursor);
    QCoreApplication::processEvents();
    QSignalSpy changed(&edit, &TextEdit::cursorRectChanged);
    const QRect wide = edit.cursorRect();
    edit.resize(180, edit.height());
    ASSERT_TRUE(QTest::qWaitFor([&]() { return edit.cursorRect() != wide && !changed.isEmpty(); }, 1000));
    QCoreApplication::processEvents();
    changed.clear();
    edit.setTextCursor(edit.textCursor());
    edit.setContentMargins(edit.contentMargins());
    edit.resize(edit.size());
    QCoreApplication::processEvents();
    EXPECT_EQ(changed.count(), 0);
    EXPECT_TRUE(edit.metaObject()->indexOfProperty("cursorRect") >= 0);
}

TEST(TextEditCaretTest, Contract_QueuedGeometryUpdateIsCancelledWithOwner)
{
    auto* edit = new TextEdit;
    edit->setPlainText("A new caret position");
    auto cursor = edit->textCursor();
    cursor.movePosition(QTextCursor::End);
    edit->setTextCursor(cursor);
    delete edit;
    QCoreApplication::processEvents();
    SUCCEED();
}
