#include <gtest/gtest.h>
#include <QAccessible>
#include <QItemSelectionModel>
#include <QImage>
#include <QPointer>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTest>
#include "components/basicinput/Button.h"
#include "components/collections/ListView.h"
#include "components/collections/CollectionViewBackdrop_p.h"
#include "components/collections/Transfer.h"
#include "components/layout/Card.h"
#include "components/textfields/AutoSuggestBox.h"

using fluent::collections::Transfer;

namespace {
void appendTransferItem(QStandardItemModel& model, const QString& key, const QString& label, bool enabled = true)
{
    auto* item = new QStandardItem(label);
    item->setData(key, Qt::UserRole);
    item->setData(QStringLiteral("Design"), Qt::UserRole + 1);
    item->setEnabled(enabled);
    model.appendRow(item);
}
void selectTransferRow(fluent::collections::ListView* list, int row)
{
    list->selectionModel()->select(list->model()->index(row, 0),
        QItemSelectionModel::Select | QItemSelectionModel::Rows);
}
} // namespace

TEST(TransferTest, Contract_ModelBorrowingAndNormalizedMembership)
{
    QStandardItemModel model;
    appendTransferItem(model, "a", "Alpha");
    appendTransferItem(model, "b", "Beta");
    appendTransferItem(model, "c", "Gamma", false);
    Transfer transfer;
    transfer.setModel(&model);
    QSignalSpy changes(&transfer, &Transfer::selectedKeysChanged);
    transfer.setSelectedKeys({"b", "b", "missing", "c"});
    EXPECT_EQ(transfer.selectedKeys(), (QStringList{"b", "c"}));
    EXPECT_EQ(transfer.availableCount(), 1);
    EXPECT_EQ(transfer.selectedCount(), 2);
    EXPECT_EQ(model.rowCount(), 3);
    EXPECT_EQ(model.parent(), nullptr);
    EXPECT_EQ(changes.count(), 1);
    transfer.setSelectedKeys({"b", "c"});
    EXPECT_EQ(changes.count(), 1);
    transfer.setModel(nullptr);
    EXPECT_TRUE(transfer.selectedKeys().isEmpty());
    EXPECT_EQ(transfer.sourceView()->model()->rowCount(), 0);
    EXPECT_EQ(model.rowCount(), 3);
}

TEST(TransferTest, Contract_MoveSearchAndDisabledItems)
{
    QStandardItemModel model;
    appendTransferItem(model, "a", "Alpha");
    appendTransferItem(model, "b", "Beta");
    appendTransferItem(model, "c", "Gamma", false);
    Transfer transfer;
    transfer.setModel(&model);
    transfer.setDescriptionRole(Qt::UserRole + 1);
    transfer.setSourceFilter("ALPHA design");
    ASSERT_EQ(transfer.sourceView()->model()->rowCount(), 1);
    selectTransferRow(transfer.sourceView(), 0);
    transfer.addSelected();
    EXPECT_EQ(transfer.selectedKeys(), QStringList{"a"});
    EXPECT_EQ(transfer.sourceView()->model()->rowCount(), 0);
    transfer.setSourceFilter(QString());
    selectTransferRow(transfer.sourceView(), 0);
    selectTransferRow(transfer.sourceView(), 1);
    transfer.addSelected();
    EXPECT_EQ(transfer.selectedKeys(), (QStringList{"a", "b"}));
    transfer.setTargetFilter("beta");
    ASSERT_EQ(transfer.targetView()->model()->rowCount(), 1);
    selectTransferRow(transfer.targetView(), 0);
    transfer.removeSelected();
    EXPECT_EQ(transfer.selectedKeys(), QStringList{"a"});
    EXPECT_EQ(model.item(0)->data(Qt::UserRole).toString(), QStringLiteral("a"));
    EXPECT_EQ(model.rowCount(), 3);
}

TEST(TransferTest, Contract_ModelChangesReconcileKeysAndLifetime)
{
    auto* model = new QStandardItemModel;
    appendTransferItem(*model, "a", "Alpha");
    appendTransferItem(*model, "b", "Beta");
    Transfer transfer;
    transfer.setModel(model);
    transfer.setSelectedKeys({"b"});
    model->insertRow(0, new QStandardItem("Invalid key"));
    EXPECT_EQ(transfer.availableCount(), 1);
    appendTransferItem(*model, "c", "Charlie");
    EXPECT_EQ(transfer.availableCount(), 2);
    model->sort(0, Qt::DescendingOrder);
    EXPECT_EQ(transfer.selectedKeys(), QStringList{"b"});
    for (int row = 0; row < model->rowCount(); ++row) {
        if (model->item(row)->data(Qt::UserRole).toString() == "b") {
            model->removeRow(row);
            break;
        }
    }
    EXPECT_TRUE(transfer.selectedKeys().isEmpty());
    QSignalSpy changed(&transfer, &Transfer::modelChanged);
    delete model;
    EXPECT_EQ(transfer.model(), nullptr);
    EXPECT_EQ(transfer.availableCount(), 0);
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(transfer.targetView()->model()->rowCount(), 0);
}

TEST(TransferTest, Contract_DuplicateKeysAndRoleChanges)
{
    QStandardItemModel model;
    appendTransferItem(model, "a", "Alpha");
    appendTransferItem(model, "a", "Another Alpha");
    appendTransferItem(model, "b", "Beta");
    Transfer transfer;
    transfer.setModel(&model);
    EXPECT_EQ(transfer.availableCount(), 1);
    model.item(1)->setData("c", Qt::UserRole);
    EXPECT_EQ(transfer.availableCount(), 3);
    transfer.setSelectedKeys({"a"});
    model.item(0)->setData("d", Qt::UserRole);
    EXPECT_TRUE(transfer.selectedKeys().isEmpty());
    transfer.setKeyRole(Qt::DisplayRole);
    transfer.setSelectedKeys({"Beta"});
    EXPECT_EQ(transfer.selectedCount(), 1);
}

TEST(TransferTest, Contract_KeyboardAndAccessibleChildControls)
{
    QStandardItemModel model;
    appendTransferItem(model, "a", "Alpha");
    appendTransferItem(model, "b", "Beta");
    Transfer transfer;
    transfer.setSourceHeader("Available members");
    transfer.setTargetHeader("Project members");
    transfer.setModel(&model);
    transfer.resize(740, 320);
    transfer.show();
    QCoreApplication::processEvents();
    auto* view = transfer.sourceView();
    view->setFocus();
    view->setCurrentIndex(view->model()->index(0, 0));
    view->selectionModel()->clearSelection();
    QTest::keyClick(view, Qt::Key_Space);
    EXPECT_FALSE(view->selectionModel()->selectedRows().isEmpty());
    auto* add = transfer.findChild<fluent::basicinput::Button*>("Transfer.Add");
    ASSERT_NE(add, nullptr);
    EXPECT_TRUE(add->isEnabled());
    QTest::keyClick(add, Qt::Key_Space);
    EXPECT_EQ(transfer.selectedCount(), 1);
    auto* accessible = QAccessible::queryAccessibleInterface(view);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->text(QAccessible::Name), QStringLiteral("Available members"));
    EXPECT_EQ(accessible->role(), QAccessible::List);
    transfer.setEnabled(false);
    const auto before = transfer.selectedKeys();
    transfer.addSelected();
    transfer.removeSelected();
    EXPECT_EQ(transfer.selectedKeys(), before);
}

TEST(TransferTest, Contract_NarrowLayoutAndBorrowedModelSurviveTeardown)
{
    QStandardItemModel model;
    appendTransferItem(model, "a", "Alpha");
    auto* transfer = new Transfer;
    transfer->setModel(&model);
    transfer->resize(320, transfer->heightForWidth(320));
    transfer->show();
    QCoreApplication::processEvents();
    EXPECT_GT(transfer->targetView()->mapTo(transfer, QPoint()).y(),
        transfer->sourceView()->mapTo(transfer, QPoint()).y());
    EXPECT_LE(transfer->sourceView()->mapTo(transfer, transfer->sourceView()->rect().bottomRight()).x(),
        transfer->width());
    transfer->setLayoutDirection(Qt::RightToLeft);
    delete transfer;
    appendTransferItem(model, "b", "Beta");
    EXPECT_EQ(model.rowCount(), 2);
}

TEST(TransferTest, Contract_SelectionKeepsPaneGeometryAndDirectionalActionsStable)
{
    QStandardItemModel model;
    for (int i = 0; i < 24; ++i)
        appendTransferItem(model, QString::number(i), QStringLiteral("Member %1").arg(i));
    Transfer transfer;
    transfer.setModel(&model);
    QStringList selected;
    for (int i = 12; i < 24; ++i)
        selected.append(QString::number(i));
    transfer.setSelectedKeys(selected);
    auto* forward = transfer.findChild<fluent::basicinput::Button*>("Transfer.Add");
    auto* backward = transfer.findChild<fluent::basicinput::Button*>("Transfer.Remove");
    ASSERT_NE(forward, nullptr);
    ASSERT_NE(backward, nullptr);
    transfer.show();
    for (auto direction : {Qt::LeftToRight, Qt::RightToLeft}) {
        transfer.setLayoutDirection(direction);
        for (int width : {740, 520, 320}) {
            SCOPED_TRACE(width);
            transfer.sourceView()->clearSelection();
            transfer.targetView()->clearSelection();
            transfer.resize(width, transfer.heightForWidth(width));
            QTest::qWait(50);
            const QRect source = transfer.sourceView()->parentWidget()->geometry();
            const QRect target = transfer.targetView()->parentWidget()->geometry();
            const QSize buttons = forward->size();
            const int heightHint = transfer.heightForWidth(width);
            for (int i = 0; i < 12; ++i) {
                selectTransferRow(transfer.targetView(), i);
                selectTransferRow(transfer.sourceView(), i);
                QCoreApplication::processEvents();
                EXPECT_EQ(transfer.sourceView()->parentWidget()->geometry(), source);
                EXPECT_EQ(transfer.targetView()->parentWidget()->geometry(), target);
                EXPECT_EQ(forward->size(), buttons);
                EXPECT_EQ(backward->size(), buttons);
                EXPECT_EQ(transfer.heightForWidth(width), heightHint);
            }
            EXPECT_TRUE(forward->text().isEmpty());
            EXPECT_TRUE(backward->text().isEmpty());
            EXPECT_EQ(forward->fluentLayout(), fluent::basicinput::Button::IconOnly);
            EXPECT_EQ(backward->fluentLayout(), fluent::basicinput::Button::IconOnly);
        }
    }
    transfer.setSourceHeader("Candidates");
    transfer.setTargetHeader("Reviewers");
    auto* accessible = QAccessible::queryAccessibleInterface(forward);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->text(QAccessible::Name), QStringLiteral("Move 12 selected items to Reviewers"));
    EXPECT_EQ(backward->toolTip(), QStringLiteral("Move 12 selected items to Candidates"));
}

TEST(TransferTest, Contract_PanesPreserveCardSurfaceInCompositedWindow)
{
    QWidget host;
    host.setAttribute(Qt::WA_TranslucentBackground);
    host.resize(740, 320);
    fluent::windowing::BackdropState state;
    state.surfaceMode = fluent::windowing::BackdropSurfaceMode::CompositedTransparent;
    fluent::windowing::publishWindowBackdropState(&host, state);
    Transfer transfer(&host);
    transfer.setGeometry(host.rect());
    host.show();
    QCoreApplication::processEvents();
    for (auto* view : {transfer.sourceView(), transfer.targetView()}) {
        EXPECT_FALSE(fluent::collections::detail::shouldClearCompositedViewport(view));
        auto* card = qobject_cast<fluent::layout::Card*>(view->parentWidget());
        ASSERT_NE(card, nullptr);
        const QPoint point = view->viewport()->mapTo(&host, QPoint(12, view->viewport()->height() - 12));
        QImage image(host.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        host.render(&image);
        EXPECT_EQ(image.pixelColor(point), card->themeColors().bgLayer);
    }
}

TEST(TransferTest, Contract_NotificationsPermitOwnerDeletionAndReentrantMembership)
{
    QStandardItemModel model;
    appendTransferItem(model, "a", "Alpha");
    appendTransferItem(model, "b", "Beta");
    QPointer<Transfer> transfer = new Transfer;
    transfer->setModel(&model);
    QObject::connect(transfer, &Transfer::countsChanged, transfer, [transfer]() { delete transfer; });
    transfer->setSelectedKeys({"a"});
    EXPECT_TRUE(transfer.isNull());
    EXPECT_EQ(model.rowCount(), 2);

    Transfer reentrant;
    reentrant.setModel(&model);
    QSignalSpy changes(&reentrant, &Transfer::selectedKeysChanged);
    QObject::connect(&reentrant, &Transfer::countsChanged, &reentrant, [&]() { reentrant.setSelectedKeys({"b"}); });
    reentrant.setSelectedKeys({"a"});
    EXPECT_EQ(reentrant.selectedKeys(), QStringList{"b"});
    ASSERT_EQ(changes.count(), 1);
    EXPECT_EQ(changes.first().first().toStringList(), QStringList{"b"});
}
