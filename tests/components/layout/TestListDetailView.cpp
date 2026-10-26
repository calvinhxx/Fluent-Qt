#include <QLineEdit>
#include <gtest/gtest.h>
#include "QtTestEnvironment.h"

#include <QApplication>
#include <QPointer>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTest>
#include <QTextEdit>
#include <QVBoxLayout>

#include "components/collections/ListView.h"
#include "components/layout/ListDetailView.h"
#include "components/textfields/LineEdit.h"
#include "components/textfields/TextEdit.h"

using fluent::layout::ListDetailView;

TEST(ListDetailViewTest, Contract_CompactCallbackCanDestroyHost)
{
    QPointer<ListDetailView> view = new ListDetailView;
    view->resize(800, 400);
    QObject::connect(view, &ListDetailView::compactChanged, view, [view] { delete view.data(); });
    view->setCompactThreshold(1000);
    EXPECT_TRUE(view.isNull());
}

TEST(ListDetailViewTest, Contract_ReentrantThresholdDoesNotPublishSupersededValues)
{
    for (bool roundTrip : {false, true}) {
        SCOPED_TRACE(roundTrip);
        ListDetailView view;
        view.resize(800, 400);
        QSignalSpy changes(&view, &ListDetailView::compactThresholdChanged);
        bool entered = false;
        QObject::connect(&view, &ListDetailView::compactChanged, &view, [&](bool compact) {
            if (!compact || entered)
                return;
            entered = true;
            view.setCompactThreshold(600);
            if (roundTrip)
                view.setCompactThreshold(1000);
        });
        view.setCompactThreshold(1000);
        ASSERT_EQ(changes.count(), roundTrip ? 2 : 1);
        EXPECT_EQ(changes.first().first().toInt(), 600);
        EXPECT_EQ(changes.last().first().toInt(), view.compactThreshold());
        EXPECT_EQ(view.isCompact(), roundTrip);
        EXPECT_EQ(view.detailPane()->isHidden(), roundTrip);
    }
}

TEST(ListDetailViewTest, Contract_FocusCallbackCanReplaceTheRequestedPane)
{
    ListDetailView view;
    auto* first = new fluent::textfields::LineEdit(view.listPane());
    auto* second = new fluent::textfields::LineEdit(view.detailPane());
    view.resize(360, 300);
    ASSERT_TRUE(tests::support::showAndFocusWidget(first));
    QSignalSpy panes(&view, &ListDetailView::currentPaneChanged);
    QObject::connect(qApp, &QApplication::focusChanged, &view, [&](QWidget*, QWidget* now) {
        if (now == second)
            view.showList();
    });
    view.showDetail();
    EXPECT_EQ(view.currentPane(), ListDetailView::List);
    EXPECT_FALSE(view.listPane()->isHidden());
    EXPECT_TRUE(view.detailPane()->isHidden());
    ASSERT_EQ(panes.count(), 1);
    EXPECT_EQ(panes.first().first().value<ListDetailView::Pane>(), ListDetailView::List);
}

TEST(ListDetailViewTest, Contract_ThresholdNavigationAndRtlGeometry)
{
    ListDetailView view;
    view.resize(800, 400);
    view.show();
    QApplication::processEvents();
    EXPECT_FALSE(view.isCompact());
    EXPECT_EQ(view.listPane()->width(), 240);
    EXPECT_EQ(view.detailPane()->geometry(), QRect(241, 0, 559, 400));
    view.setLayoutDirection(Qt::RightToLeft);
    EXPECT_EQ(view.listPane()->geometry(), QRect(560, 0, 240, 400));
    EXPECT_EQ(view.detailPane()->geometry(), QRect(0, 0, 559, 400));

    QSignalSpy mode(&view, &ListDetailView::compactChanged);
    view.resize(640, 400);
    EXPECT_FALSE(view.isCompact());
    view.resize(639, 400);
    EXPECT_TRUE(view.isCompact());
    EXPECT_TRUE(view.detailPane()->isHidden());
    EXPECT_EQ(view.listPane()->geometry(), view.rect());
    view.showDetail();
    EXPECT_EQ(view.currentPane(), ListDetailView::Detail);
    EXPECT_TRUE(view.listPane()->isHidden());
    EXPECT_EQ(view.detailPane()->geometry(), view.rect());
    view.resize(800, 400);
    EXPECT_FALSE(view.listPane()->isHidden());
    EXPECT_FALSE(view.detailPane()->isHidden());
    view.resize(360, 400);
    EXPECT_TRUE(view.listPane()->isHidden());
    view.showList();
    EXPECT_FALSE(view.listPane()->isHidden());
    EXPECT_EQ(mode.count(), 3);
}

TEST(ListDetailViewTest, Contract_IdempotentPropertiesAndNarrowHost)
{
    ListDetailView view;
    QSignalSpy threshold(&view, &ListDetailView::compactThresholdChanged);
    QSignalSpy width(&view, &ListDetailView::listPaneWidthChanged);
    QSignalSpy pane(&view, &ListDetailView::currentPaneChanged);
    view.setCompactThreshold(640);
    view.setListPaneWidth(240);
    view.showList();
    EXPECT_EQ(threshold.count() + width.count() + pane.count(), 0);
    view.setCompactThreshold(-10);
    view.setListPaneWidth(-10);
    EXPECT_EQ(view.compactThreshold(), 1);
    EXPECT_EQ(view.listPaneWidth(), 1);
    view.setCurrentPane(static_cast<ListDetailView::Pane>(42));
    EXPECT_EQ(pane.count(), 0);
    view.setCompactThreshold(640);
    view.resize(200, 200);
    view.show();
    QApplication::processEvents();
    EXPECT_EQ(view.width(), 200);
    EXPECT_TRUE(view.isCompact());
}

TEST(ListDetailViewTest, Contract_SelectionScrollDocumentAndUndoSurviveSwitches)
{
    ListDetailView view;
    auto* list = new fluent::collections::ListView(view.listPane());
    auto* left = new QVBoxLayout(view.listPane());
    left->setContentsMargins(0, 0, 0, 0);
    left->addWidget(list);
    QStandardItemModel model;
    for (int row = 0; row < 100; ++row)
        model.appendRow(new QStandardItem(QStringLiteral("File %1.md").arg(row)));
    list->setModel(&model);
    auto* edit = new fluent::textfields::TextEdit(view.detailPane());
    edit->setMinVisibleLines(12);
    edit->setMaxVisibleLines(12);
    auto* right = new QVBoxLayout(view.detailPane());
    right->addWidget(edit);
    auto* native = edit->findChild<QTextEdit*>();
    ASSERT_NE(native, nullptr);
    edit->setPlainText(QStringLiteral("Original\n").repeated(100));
    view.resize(800, 400);
    view.show();
    QApplication::processEvents();
    list->setCurrentIndex(model.index(70, 0));
    list->verticalScrollBar()->setValue(500);
    QTextCursor cursor = native->textCursor();
    cursor.setPosition(3);
    native->setTextCursor(cursor);
    native->insertPlainText(QStringLiteral("draft"));
    cursor = native->textCursor();
    cursor.setPosition(2);
    cursor.setPosition(7, QTextCursor::KeepAnchor);
    native->setTextCursor(cursor);
    native->verticalScrollBar()->setValue(100);
    const int listOffset = list->verticalScrollBar()->value();
    const int editOffset = native->verticalScrollBar()->value();
    const QString draft = edit->toPlainText();
    QWidget* listParent = list->parentWidget();
    QWidget* editParent = edit->parentWidget();
    for (int count = 0; count < 4; ++count) {
        view.showDetail();
        view.resize(360, 400);
        QApplication::processEvents();
        view.showList();
        QApplication::processEvents();
        view.showDetail();
        view.resize(800, 400);
        QApplication::processEvents();
    }
    EXPECT_EQ(list->currentIndex(), model.index(70, 0));
    EXPECT_EQ(list->verticalScrollBar()->value(), listOffset);
    EXPECT_EQ(native->verticalScrollBar()->value(), editOffset);
    EXPECT_EQ(native->textCursor().position(), cursor.position());
    EXPECT_EQ(native->textCursor().anchor(), cursor.anchor());
    EXPECT_EQ(edit->toPlainText(), draft);
    EXPECT_EQ(list->parentWidget(), listParent);
    EXPECT_EQ(edit->parentWidget(), editParent);
    EXPECT_TRUE(native->document()->isUndoAvailable());
    native->undo();
    EXPECT_EQ(edit->toPlainText(), QStringLiteral("Original\n").repeated(100));
}

TEST(ListDetailViewTest, Contract_FocusReturnsToPreviouslyFocusedControl)
{
    ListDetailView view;
    auto* first = new fluent::textfields::LineEdit(view.listPane());
    auto* second = new fluent::textfields::LineEdit(view.detailPane());
    auto* third = new fluent::textfields::LineEdit(view.detailPane());
    third->move(0, 50);
    view.resize(360, 300);
    ASSERT_TRUE(tests::support::showAndFocusWidget(first));
    view.showDetail();
    EXPECT_TRUE(second->hasFocus());
    third->setFocus();
    view.showList();
    EXPECT_TRUE(first->hasFocus());
    view.showDetail();
    EXPECT_TRUE(third->hasFocus());
    view.resize(800, 300);
    EXPECT_TRUE(third->hasFocus());
}

TEST(ListDetailViewTest, Contract_PanesAndChildrenLiveUntilHostDestruction)
{
    auto* view = new ListDetailView;
    QPointer<QWidget> pane = view->detailPane();
    QPointer<QWidget> content = new QWidget(pane);
    view->showDetail();
    view->showList();
    EXPECT_FALSE(content.isNull());
    delete view;
    EXPECT_TRUE(pane.isNull());
    EXPECT_TRUE(content.isNull());
}

TEST(ListDetailViewTest, Contract_CompactPaneFocusDoesNotVisitOutsideControls)
{
    QWidget host;
    auto* body = new QVBoxLayout(&host);
    auto* view = new ListDetailView(&host);
    view->setCompactThreshold(1000);
    body->addWidget(view);
    auto* first = new QLineEdit(view->listPane());
    auto* second = new QLineEdit(view->detailPane());
    first->setGeometry(10, 10, 150, 32);
    second->setGeometry(10, 10, 150, 32);
    auto* outside = new QLineEdit(&host);
    body->addWidget(outside);
    host.resize(420, 300);
    host.show();
    host.activateWindow();
    first->setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return first->hasFocus(); }));
    int outsideVisits = 0;
    const auto connection = QObject::connect(qApp, &QApplication::focusChanged, &host, [&](QWidget*, QWidget* next) {
        outsideVisits += next == outside;
    });
    view->showDetail();
    EXPECT_TRUE(second->hasFocus());
    view->showList();
    EXPECT_TRUE(first->hasFocus());
    EXPECT_EQ(outsideVisits, 0);
    QObject::disconnect(connection);
}
