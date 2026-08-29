#include <gtest/gtest.h>

#include <QAbstractListModel>
#include <QAccessible>
#include <QApplication>
#include <QImage>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QTimer>
#include <QWheelEvent>
#include <QtTest/QTest>

#include "components/basicinput/Button.h"
#include "components/basicinput/CheckBox.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/collections/ListView.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "QtTestEnvironment.h"

using fluent::basicinput::CheckBox;
using fluent::basicinput::MultiSelectComboBox;

namespace {

class MultiSelectComboBoxTestWindow final : public QWidget,
                                            public fluent::FluentElement {
public:
  using QWidget::QWidget;

  void onThemeUpdated() override {
    const auto &colors = themeColors();
    setStyleSheet(
        QStringLiteral("background-color: %1;").arg(colors.bgCanvas.name()));
  }
};

class CountingListModel final : public QAbstractListModel {
public:
  explicit CountingListModel(int rows, QObject *parent = nullptr)
      : QAbstractListModel(parent), m_rows(rows) {}

  int rowCount(const QModelIndex &parent = QModelIndex()) const override {
    return parent.isValid() ? 0 : m_rows;
  }

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows)
      return {};
    if (role == Qt::DisplayRole) {
      ++displayRequests;
      return QStringLiteral("Option %1").arg(index.row());
    }
    return {};
  }

  Qt::ItemFlags flags(const QModelIndex &index) const override {
    ++flagRequests;
    return QAbstractListModel::flags(index);
  }

  mutable int displayRequests = 0;
  mutable int flagRequests = 0;

private:
  int m_rows = 0;
};

fluent::dialogs_flyouts::Flyout *openPopup(MultiSelectComboBox &box,
                                           QWidget &window) {
  window.show();
  box.show();
  QApplication::processEvents();
  box.open();
  QApplication::processEvents();
  return window.findChild<fluent::dialogs_flyouts::Flyout *>(
      QStringLiteral("MultiSelectComboBox.Popup"));
}

QRect visiblePopupGeometry(const QWidget &popup, const QWidget &window) {
  const QRect visible =
      fluent::overlay::visibleCardRect(QRect(QPoint(), popup.size()));
  return QRect(popup.mapTo(&window, visible.topLeft()), visible.size());
}

bool sendWheel(QWidget *target, QPoint pixelDelta, QPoint angleDelta,
               Qt::ScrollPhase phase) {
  const QPoint local = target->rect().center();
  QWheelEvent event(QPointF(local), QPointF(target->mapToGlobal(local)),
                    pixelDelta, angleDelta, Qt::NoButton, Qt::NoModifier, phase,
                    false);
  QApplication::sendEvent(target, &event);
  return event.isAccepted();
}

bool sendWheel(QWidget *target, int angleDeltaY) {
  return sendWheel(target, QPoint(), QPoint(0, angleDeltaY), Qt::NoScrollPhase);
}

} // namespace

class MultiSelectComboBoxTest : public ::testing::Test {
protected:
  void SetUp() override {
    window = new MultiSelectComboBoxTestWindow;
    window->onThemeUpdated();
    window->resize(640, 480);
  }

  void TearDown() override { delete window; }

  MultiSelectComboBoxTestWindow *window = nullptr;
};

TEST_F(MultiSelectComboBoxTest, DefaultContract) {
  MultiSelectComboBox box(window);
  EXPECT_EQ(box.model(), nullptr);
  ASSERT_NE(box.selectionModel(), nullptr);
  EXPECT_EQ(box.selectionModel()->model(), nullptr);
  EXPECT_EQ(box.modelColumn(), 0);
  EXPECT_FALSE(box.rootModelIndex().isValid());
  EXPECT_TRUE(box.placeholderText().isEmpty());
  EXPECT_FALSE(box.isSearchEnabled());
  EXPECT_TRUE(box.isSelectAllVisible());
  EXPECT_EQ(box.maximumVisibleItems(), 6);
  EXPECT_FALSE(box.isOpen());
  EXPECT_EQ(box.selectedCount(), 0);
  EXPECT_TRUE(box.selectedRows().isEmpty());
  EXPECT_EQ(box.focusPolicy(), Qt::StrongFocus);
  EXPECT_EQ(box.sizeHint().height(), Spacing::ControlHeight::Standard);
}

TEST_F(MultiSelectComboBoxTest, EmptyModelDisablesSelectAllHeader) {
  MultiSelectComboBox box(window);
  box.setGeometry(40, 40, 260, Spacing::ControlHeight::Standard);

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  auto *selectAll = popup->findChild<CheckBox *>(
      QStringLiteral("MultiSelectComboBox.SelectAll"));
  ASSERT_NE(selectAll, nullptr);
  EXPECT_FALSE(selectAll->isEnabled());
  EXPECT_EQ(selectAll->checkState(), Qt::Unchecked);
}

TEST_F(MultiSelectComboBoxTest, ReplacesClearsAndSelectsRows) {
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                          QStringLiteral("Gamma")});
  MultiSelectComboBox box(window);
  box.setModel(&model);

  QSignalSpy countSpy(&box, &MultiSelectComboBox::selectedCountChanged);
  box.setSelectedRows({2, 0, 2});
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 2}));
  EXPECT_EQ(box.selectedCount(), 2);
  EXPECT_TRUE(box.isRowSelected(0));
  EXPECT_FALSE(box.isRowSelected(1));
  EXPECT_EQ(countSpy.count(), 1);

  box.clearSelection();
  EXPECT_TRUE(box.selectedRows().isEmpty());
  EXPECT_EQ(box.selectedCount(), 0);

  box.selectAll();
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 1, 2}));
  EXPECT_EQ(box.selectedCount(), 3);
}

TEST_F(MultiSelectComboBoxTest, SharedModelSelectionsStayIndependent) {
  QStringListModel model(
      {QStringLiteral("North America"), QStringLiteral("Europe"),
       QStringLiteral("Asia Pacific"), QStringLiteral("Latin America"),
       QStringLiteral("Middle East and Africa"), QString::fromUtf8("中国大陆"),
       QString::fromUtf8("日本")});
  MultiSelectComboBox first(window);
  MultiSelectComboBox second(window);
  first.setModel(&model);
  second.setModel(&model);

  ASSERT_NE(first.selectionModel(), second.selectionModel());
  first.setSelectedRows({0, 1, 2, 3, 4});
  second.setSelectedRows({5, 6});

  EXPECT_EQ(first.selectedRows(), (QList<int>{0, 1, 2, 3, 4}));
  EXPECT_EQ(second.selectedRows(), (QList<int>{5, 6}));
  EXPECT_EQ(first.selectedCount(), 5);
  EXPECT_EQ(second.selectedCount(), 2);
}

TEST_F(MultiSelectComboBoxTest, PopupFollowsOwnerLayoutDirection) {
  QStringListModel model(
      {QString::fromUtf8("الشرق الأوسط"), QString::fromUtf8("شمال أفريقيا")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 80, 260, Spacing::ControlHeight::Standard);
  box.setLayoutDirection(Qt::RightToLeft);
  box.setSearchEnabled(true);
  box.setModel(&model);

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  auto *search = popup->findChild<fluent::textfields::LineEdit *>(
      QStringLiteral("MultiSelectComboBox.Search"));
  auto *selectAll = popup->findChild<CheckBox *>(
      QStringLiteral("MultiSelectComboBox.SelectAll"));
  auto *list = popup->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(search, nullptr);
  ASSERT_NE(selectAll, nullptr);
  ASSERT_NE(list, nullptr);

  EXPECT_EQ(popup->layoutDirection(), Qt::RightToLeft);
  EXPECT_EQ(search->layoutDirection(), Qt::RightToLeft);
  EXPECT_EQ(selectAll->layoutDirection(), Qt::RightToLeft);
  EXPECT_EQ(list->layoutDirection(), Qt::RightToLeft);
}

TEST_F(MultiSelectComboBoxTest, DisabledRowsStayUnselectedByActions) {
  QStandardItemModel model;
  model.appendRow(new QStandardItem(QStringLiteral("Alpha")));
  auto *disabled = new QStandardItem(QStringLiteral("Disabled"));
  disabled->setFlags(disabled->flags() & ~Qt::ItemIsEnabled &
                     ~Qt::ItemIsSelectable);
  model.appendRow(disabled);
  model.appendRow(new QStandardItem(QStringLiteral("Gamma")));

  MultiSelectComboBox box(window);
  box.setModel(&model);
  box.setSelectedRows({0, 1, 2});
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 2}));

  box.clearSelection();
  box.selectAll();
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 2}));
}

TEST_F(MultiSelectComboBoxTest, ExternalSelectionModelRemainsCallerOwned) {
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta")});
  QStringListModel otherModel({QStringLiteral("Other")});
  QItemSelectionModel external(&model);
  QItemSelectionModel mismatched(&otherModel);
  QObject *const originalParent = external.parent();
  MultiSelectComboBox box(window);
  box.setModel(&model);

  box.setSelectionModel(&external);
  EXPECT_EQ(box.selectionModel(), &external);
  EXPECT_EQ(external.parent(), originalParent);

  box.setSelectionModel(&mismatched);
  EXPECT_EQ(box.selectionModel(), &external);

  box.setSelectedRows({1});
  EXPECT_TRUE(external.isSelected(model.index(1, 0)));

  box.setSelectionModel(nullptr);
  EXPECT_NE(box.selectionModel(), &external);
  EXPECT_EQ(box.selectionModel()->model(), &model);
  EXPECT_EQ(external.parent(), originalParent);
}

TEST_F(MultiSelectComboBoxTest, RootSelectionPreservesOtherRoots) {
  QStandardItemModel model;
  auto *firstRoot = new QStandardItem(QStringLiteral("First"));
  firstRoot->appendRow(new QStandardItem(QStringLiteral("A")));
  firstRoot->appendRow(new QStandardItem(QStringLiteral("B")));
  auto *secondRoot = new QStandardItem(QStringLiteral("Second"));
  secondRoot->appendRow(new QStandardItem(QStringLiteral("C")));
  model.appendRow(firstRoot);
  model.appendRow(secondRoot);

  MultiSelectComboBox box(window);
  box.setModel(&model);
  const QModelIndex firstRootIndex = model.index(0, 0);
  const QModelIndex secondRootIndex = model.index(1, 0);

  box.setRootModelIndex(firstRootIndex);
  box.setSelectedRows({1});
  EXPECT_EQ(box.selectedRows(), (QList<int>{1}));

  box.selectionModel()->select(model.index(0, 0, secondRootIndex),
                               QItemSelectionModel::Select |
                                   QItemSelectionModel::Rows);
  box.clearSelection();
  EXPECT_TRUE(box.selectedRows().isEmpty());
  EXPECT_TRUE(
      box.selectionModel()->isSelected(model.index(0, 0, secondRootIndex)));
}

TEST_F(MultiSelectComboBoxTest, DestroyingModelRestoresEmptyInternalState) {
  MultiSelectComboBox box(window);
  auto *model =
      new QStringListModel({QStringLiteral("Alpha"), QStringLiteral("Beta")});
  box.setModel(model);
  box.setSelectedRows({1});
  ASSERT_EQ(box.selectedCount(), 1);

  delete model;
  QApplication::processEvents();
  EXPECT_EQ(box.model(), nullptr);
  ASSERT_NE(box.selectionModel(), nullptr);
  EXPECT_EQ(box.selectionModel()->model(), nullptr);
  EXPECT_EQ(box.selectedCount(), 0);
}

TEST_F(MultiSelectComboBoxTest, OpenModelDestructionClosesAndDetachesView) {
  auto *model =
      new QStringListModel({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                            QStringLiteral("Gamma")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 80, 260, Spacing::ControlHeight::Standard);
  box.setModel(model);
  box.setSelectedRows({0, 2});

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(list, nullptr);
  ASSERT_TRUE(box.isOpen());
  ASSERT_EQ(list->model(), model);

  delete model;
  QApplication::processEvents();

  EXPECT_FALSE(box.isOpen());
  EXPECT_EQ(box.model(), nullptr);
  EXPECT_EQ(list->model(), nullptr);
  ASSERT_NE(box.selectionModel(), nullptr);
  EXPECT_EQ(box.selectionModel()->model(), nullptr);
  EXPECT_EQ(box.selectedCount(), 0);
}

TEST_F(MultiSelectComboBoxTest, OpenFilteredModelDestructionClosesSafely) {
  auto *model =
      new QStringListModel({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                            QStringLiteral("Gamma")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 80, 260, Spacing::ControlHeight::Standard);
  box.setSearchEnabled(true);
  box.setModel(model);

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(list, nullptr);
  ASSERT_TRUE(box.isOpen());
  ASSERT_NE(list->model(), nullptr);
  ASSERT_NE(list->model(), model);

  delete model;
  QApplication::processEvents();

  EXPECT_FALSE(box.isOpen());
  EXPECT_EQ(box.model(), nullptr);
  EXPECT_EQ(list->model(), nullptr);
  ASSERT_NE(box.selectionModel(), nullptr);
  EXPECT_EQ(box.selectionModel()->model(), nullptr);
}

TEST_F(MultiSelectComboBoxTest, DestroyingHostWithPopupOpenDoesNotCrash) {
  auto *model = new QStringListModel(
      {QStringLiteral("Alpha"), QStringLiteral("Beta")}, window);
  auto *box = new MultiSelectComboBox(window);
  box->setGeometry(40, 80, 260, Spacing::ControlHeight::Standard);
  box->setModel(model);

  ASSERT_NE(openPopup(*box, *window), nullptr);
  ASSERT_TRUE(box->isOpen());

  delete window;
  window = nullptr;
}

TEST_F(MultiSelectComboBoxTest, PopupUsesRoomierSideBeforeClipping) {
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                          QStringLiteral("Gamma"), QStringLiteral("Delta"),
                          QStringLiteral("Epsilon"), QStringLiteral("Zeta"),
                          QStringLiteral("Eta"), QStringLiteral("Theta")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 300, 260, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  box.setMaximumVisibleItems(6);

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(list, nullptr);

  const QRect card = visiblePopupGeometry(*popup, *window);
  const QRect surface = fluent::overlay::overlaySurfaceRect(window);
  EXPECT_EQ(popup->placement(), fluent::dialogs_flyouts::Flyout::Top);
  EXPECT_EQ(card.bottom() + 1, box.geometry().top());
  EXPECT_GE(card.top(), surface.top() + 4);
  EXPECT_EQ(list->height(),
            box.maximumVisibleItems() *
                (Spacing::ControlHeight::Standard + Spacing::XSmall));
}

TEST_F(MultiSelectComboBoxTest, ConstrainedPopupAcceptsWheelAndTrackpadScroll) {
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                          QStringLiteral("Gamma"), QStringLiteral("Delta"),
                          QStringLiteral("Epsilon"), QStringLiteral("Zeta"),
                          QStringLiteral("Eta"), QStringLiteral("Theta")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 190, 260, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  box.setMaximumVisibleItems(8);

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(list, nullptr);

  QScrollBar *scrollBar = list->verticalScrollBar();
  ASSERT_NE(scrollBar, nullptr);
  ASSERT_GT(scrollBar->maximum(), scrollBar->minimum());

  const int beforeWheel = scrollBar->value();
  EXPECT_TRUE(sendWheel(list->viewport(), -120));
  const int afterDispatch = scrollBar->value();
  EXPECT_GT(afterDispatch, beforeWheel);
  QTest::qWait(20);
  EXPECT_EQ(scrollBar->value(), afterDispatch);

  scrollBar->setValue(scrollBar->minimum());
  sendWheel(list->viewport(), QPoint(), QPoint(), Qt::ScrollBegin);
  EXPECT_TRUE(
      sendWheel(list->viewport(), QPoint(0, -24), QPoint(), Qt::ScrollUpdate));
  QTest::qWait(20);
  EXPECT_GT(scrollBar->value(), scrollBar->minimum());
}

TEST_F(MultiSelectComboBoxTest, PopupContentStaysInsideVisibleCard) {
  QStringListModel model(
      {QStringLiteral("North America"), QStringLiteral("Europe"),
       QStringLiteral("Asia Pacific"), QStringLiteral("Latin America"),
       QStringLiteral("Middle East and Africa"), QString::fromUtf8("中国大陆"),
       QString::fromUtf8("日本")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 80, 300, Spacing::ControlHeight::Standard);
  box.setSearchEnabled(true);
  box.setModel(&model);
  box.setMaximumVisibleItems(6);

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  auto *search = window->findChild<fluent::textfields::LineEdit *>(
      QStringLiteral("MultiSelectComboBox.Search"));
  auto *selectAll = window->findChild<CheckBox *>(
      QStringLiteral("MultiSelectComboBox.SelectAll"));
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(search, nullptr);
  ASSERT_NE(selectAll, nullptr);
  ASSERT_NE(list, nullptr);
  ASSERT_NE(list->viewport(), nullptr);
  EXPECT_EQ(selectAll->boxMargin(), Spacing::Medium);
  EXPECT_EQ(selectAll->textGap(), Spacing::Small);
  EXPECT_EQ(selectAll->height(),
            Spacing::ControlHeight::Standard + Spacing::XSmall);
  EXPECT_EQ(popup->anchorOffset(), 0);
  EXPECT_EQ(list->geometry().top(), selectAll->geometry().bottom() + 1);
  EXPECT_FALSE(list->backgroundVisible());
  EXPECT_TRUE(list->property("fluentPreserveParentSurface").toBool());
  EXPECT_TRUE(
      list->viewport()->property("fluentPreserveParentSurface").toBool());
  EXPECT_FALSE(list->viewport()->testAttribute(Qt::WA_NoSystemBackground));

  popup->setPopupProgress(1.0);
  QImage composite(window->size(), QImage::Format_ARGB32_Premultiplied);
  composite.fill(Qt::transparent);
  window->render(&composite);
  const QRect restingRow =
      static_cast<QListView *>(list)->visualRect(list->model()->index(1, 0));
  ASSERT_TRUE(restingRow.isValid());
  EXPECT_EQ(restingRow.height(),
            Spacing::ControlHeight::Standard + Spacing::XSmall);
  const QPoint surfaceSample = list->viewport()->mapTo(
      window, QPoint(restingRow.center().x(), restingRow.top() + 1));
  ASSERT_TRUE(composite.rect().contains(surfaceSample));
  const QColor actualSurface = composite.pixelColor(surfaceSample).toRgb();
  const QColor expectedSurface = popup->themeColorsRef().bgLayer.toRgb();
  EXPECT_NEAR(actualSurface.red(), expectedSurface.red(), 1);
  EXPECT_NEAR(actualSurface.green(), expectedSurface.green(), 1);
  EXPECT_NEAR(actualSurface.blue(), expectedSurface.blue(), 1);

  const QRect card =
      fluent::overlay::visibleCardRect(QRect(QPoint(), popup->size()));
  const QRect contentBounds = card.adjusted(Spacing::XSmall, Spacing::XSmall,
                                            -Spacing::XSmall, -Spacing::XSmall);
  for (QWidget *child :
       {static_cast<QWidget *>(search), static_cast<QWidget *>(selectAll),
        static_cast<QWidget *>(list)}) {
    SCOPED_TRACE(child->objectName().toStdString());
    EXPECT_TRUE(contentBounds.contains(child->geometry()))
        << "content=" << contentBounds.x() << ',' << contentBounds.y() << ' '
        << contentBounds.width() << 'x' << contentBounds.height()
        << " child=" << child->geometry().x() << ',' << child->geometry().y()
        << ' ' << child->geometry().width() << 'x'
        << child->geometry().height();
  }
}

TEST_F(MultiSelectComboBoxTest, OpenPopupReflowsAfterHostResize) {
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                          QStringLiteral("Gamma"), QStringLiteral("Delta"),
                          QStringLiteral("Epsilon"), QStringLiteral("Zeta"),
                          QStringLiteral("Eta"), QStringLiteral("Theta")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 150, 260, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  box.setMaximumVisibleItems(6);

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(list, nullptr);
  const int initialListHeight = list->height();

  window->resize(640, 300);
  QApplication::processEvents();
  QApplication::processEvents();

  const QRect card = visiblePopupGeometry(*popup, *window);
  const QRect surface = fluent::overlay::overlaySurfaceRect(window);
  EXPECT_EQ(popup->placement(), fluent::dialogs_flyouts::Flyout::Top);
  EXPECT_LT(list->height(), initialListHeight);
  EXPECT_EQ(card.bottom() + 1, box.geometry().top());
  EXPECT_GE(card.top(), surface.top() + 4);
}

TEST_F(MultiSelectComboBoxTest, SearchAndKeyboardToggleKeepPopupOpen) {
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                          QStringLiteral("Gamma")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 40, 260, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  box.setSearchEnabled(true);
  box.setSelectedRows({0, 1});

  auto *popup = openPopup(box, *window);
  ASSERT_NE(popup, nullptr);
  ASSERT_TRUE(box.isOpen());
  auto *search = window->findChild<fluent::textfields::LineEdit *>(
      QStringLiteral("MultiSelectComboBox.Search"));
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(search, nullptr);
  ASSERT_NE(list, nullptr);

  search->setText(QStringLiteral("mm"));
  QApplication::processEvents();
  ASSERT_NE(list->model(), nullptr);
  EXPECT_EQ(list->model()->rowCount(list->rootIndex()), 1);
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 1}));

  const QModelIndex only = list->model()->index(0, 0, list->rootIndex());
  list->selectionModel()->setCurrentIndex(only, QItemSelectionModel::NoUpdate);
  list->setFocus(Qt::OtherFocusReason);
  QTest::keyClick(list, Qt::Key_Return);
  QApplication::processEvents();
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 1, 2}));
  EXPECT_TRUE(box.isOpen());

  QTest::keyClick(list, Qt::Key_Escape);
  QApplication::processEvents();
  EXPECT_FALSE(box.isOpen());
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 1, 2}));
}

TEST_F(MultiSelectComboBoxTest, SelectAllHeaderTargetsFilteredRows) {
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                          QStringLiteral("Gamma")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 40, 260, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  box.setSearchEnabled(true);

  ASSERT_NE(openPopup(box, *window), nullptr);
  auto *search = window->findChild<fluent::textfields::LineEdit *>(
      QStringLiteral("MultiSelectComboBox.Search"));
  auto *selectAll = window->findChild<CheckBox *>(
      QStringLiteral("MultiSelectComboBox.SelectAll"));
  ASSERT_NE(search, nullptr);
  ASSERT_NE(selectAll, nullptr);

  search->setText(QStringLiteral("mm"));
  QApplication::processEvents();
  EXPECT_EQ(selectAll->checkState(), Qt::Unchecked);
  ASSERT_TRUE(selectAll->isEnabled());
  selectAll->click();
  QApplication::processEvents();
  EXPECT_EQ(box.selectedRows(), (QList<int>{2}));
  EXPECT_EQ(selectAll->checkState(), Qt::Checked);

  search->clear();
  QApplication::processEvents();
  EXPECT_EQ(selectAll->checkState(), Qt::PartiallyChecked);

  selectAll->click();
  QApplication::processEvents();
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 1, 2}));
  EXPECT_EQ(selectAll->checkState(), Qt::Checked);

#if QT_CONFIG(accessibility)
  box.clearSelection();
  box.setSelectedRows({0});
  QApplication::processEvents();
  ASSERT_EQ(selectAll->checkState(), Qt::PartiallyChecked);
  QAccessibleInterface *selectAllInterface =
      QAccessible::queryAccessibleInterface(selectAll);
  ASSERT_NE(selectAllInterface, nullptr);
  EXPECT_TRUE(selectAllInterface->state().checkStateMixed);
  QAccessibleActionInterface *selectAllActions =
      selectAllInterface->actionInterface();
  ASSERT_NE(selectAllActions, nullptr);
  EXPECT_TRUE(selectAllActions->actionNames().contains(
      QAccessibleActionInterface::pressAction()));
  selectAllActions->doAction(QAccessibleActionInterface::pressAction());
  QApplication::processEvents();
  EXPECT_EQ(box.selectedRows(), (QList<int>{0, 1, 2}));
  EXPECT_EQ(selectAll->checkState(), Qt::Checked);
#endif
}

TEST_F(MultiSelectComboBoxTest, PointerCannotSelectDisabledPopupRow) {
  QStandardItemModel model;
  model.appendRow(new QStandardItem(QStringLiteral("Alpha")));
  auto *disabled = new QStandardItem(QStringLiteral("Disabled"));
  disabled->setFlags(disabled->flags() & ~Qt::ItemIsEnabled &
                     ~Qt::ItemIsSelectable);
  model.appendRow(disabled);

  MultiSelectComboBox box(window);
  box.setGeometry(40, 40, 260, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  ASSERT_NE(openPopup(box, *window), nullptr);
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(list, nullptr);
  const QModelIndex disabledIndex = model.index(1, 0);
  const QRect target =
      static_cast<QListView *>(list)->visualRect(disabledIndex);
  ASSERT_TRUE(target.isValid());
  QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                    target.center());
  QApplication::processEvents();
  EXPECT_TRUE(box.selectedRows().isEmpty());
  EXPECT_TRUE(box.isOpen());
}

TEST_F(MultiSelectComboBoxTest, PopupFocusVisualTracksInputModality) {
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 40, 260, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  box.setFocus(Qt::OtherFocusReason);

  ASSERT_NE(openPopup(box, *window), nullptr);
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(list, nullptr);
  EXPECT_FALSE(list->property("fluentKeyboardFocusVisible").toBool());

  const QRect firstRow =
      static_cast<QListView *>(list)->visualRect(model.index(0, 0));
  ASSERT_TRUE(firstRow.isValid());
  QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                    firstRow.center());
  QApplication::processEvents();
  EXPECT_FALSE(list->property("fluentKeyboardFocusVisible").toBool());

  QTest::keyClick(list, Qt::Key_Down);
  QApplication::processEvents();
  EXPECT_TRUE(list->property("fluentKeyboardFocusVisible").toBool());
}

TEST_F(MultiSelectComboBoxTest, ClosedKeyboardContractOpensAndCloses) {
  QStringListModel model({QStringLiteral("Alpha")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 40, 240, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  window->show();
  box.show();
  box.setFocus(Qt::OtherFocusReason);
  QApplication::processEvents();

  QTest::keyClick(&box, Qt::Key_F4);
  QApplication::processEvents();
  EXPECT_TRUE(box.isOpen());
  auto *list = window->findChild<fluent::collections::ListView *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  ASSERT_NE(list, nullptr);
  EXPECT_TRUE(list->property("fluentKeyboardFocusVisible").toBool());
  box.close();
  QApplication::processEvents();
  EXPECT_FALSE(box.isOpen());
  EXPECT_TRUE(box.hasFocus());
}

TEST_F(MultiSelectComboBoxTest, AccessibleRootExposesValueAndPopupAction) {
#if !QT_CONFIG(accessibility)
  GTEST_SKIP() << "Qt accessibility support is disabled";
#else
  QStringListModel model({QStringLiteral("Alpha"), QStringLiteral("Beta"),
                          QStringLiteral("Gamma")});
  MultiSelectComboBox box(window);
  box.setGeometry(40, 40, 260, Spacing::ControlHeight::Standard);
  box.setAccessibleName(QStringLiteral("Included regions"));
  box.setModel(&model);
  box.setSearchEnabled(true);
  box.setSelectedRows({0, 2});
  MultiSelectComboBox other(window);
  other.setGeometry(40, 96, 260, Spacing::ControlHeight::Standard);
  other.setModel(&model);
  window->show();
  box.show();
  other.show();
  QApplication::processEvents();

  QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(&box);
  ASSERT_NE(interface, nullptr);
  EXPECT_EQ(interface->role(), QAccessible::ButtonMenu);
  EXPECT_EQ(interface->text(QAccessible::Name),
            QStringLiteral("Included regions"));
  EXPECT_TRUE(
      interface->text(QAccessible::Value).contains(QStringLiteral("Alpha")));
  EXPECT_TRUE(
      interface->text(QAccessible::Value).contains(QStringLiteral("Gamma")));
  EXPECT_TRUE(interface->state().hasPopup);
  EXPECT_TRUE(interface->state().collapsed);
  EXPECT_EQ(interface->childCount(), 0);

  QAccessibleActionInterface *actions = interface->actionInterface();
  ASSERT_NE(actions, nullptr);
  EXPECT_TRUE(actions->actionNames().contains(
      QAccessibleActionInterface::showMenuAction()));
  actions->doAction(QAccessibleActionInterface::showMenuAction());
  QApplication::processEvents();
  EXPECT_TRUE(box.isOpen());
  EXPECT_TRUE(interface->state().expanded);

  QWidget *list = window->findChild<QWidget *>(
      QStringLiteral("MultiSelectComboBox.ListView"));
  QWidget *trigger = window->findChild<QWidget *>(
      QStringLiteral("MultiSelectComboBox.Trigger"));
  QWidget *search = window->findChild<QWidget *>(
      QStringLiteral("MultiSelectComboBox.Search"));
  ASSERT_NE(list, nullptr);
  ASSERT_NE(trigger, nullptr);
  ASSERT_NE(search, nullptr);
  for (QWidget *namedWidget : {trigger, search, list}) {
    QAccessibleInterface *namedInterface =
        QAccessible::queryAccessibleInterface(namedWidget);
    ASSERT_NE(namedInterface, nullptr);
    EXPECT_FALSE(namedInterface->text(QAccessible::Name).isEmpty())
        << namedWidget->objectName().toStdString();
  }
  QAccessibleInterface *listInterface =
      QAccessible::queryAccessibleInterface(list);
  ASSERT_NE(listInterface, nullptr);
  EXPECT_EQ(listInterface->role(), QAccessible::List);
  EXPECT_EQ(listInterface->childCount(), 3);

  bool controlsList = false;
  for (const auto &relation : interface->relations(QAccessible::Controller)) {
    controlsList =
        controlsList || (relation.first == listInterface &&
                         relation.second.testFlag(QAccessible::Controller));
  }
  EXPECT_TRUE(controlsList);

  QAccessibleInterface *alpha = listInterface->child(0);
  QAccessibleInterface *beta = listInterface->child(1);
  ASSERT_NE(alpha, nullptr);
  ASSERT_NE(beta, nullptr);
  EXPECT_EQ(alpha->role(), QAccessible::ListItem);
  EXPECT_EQ(alpha->text(QAccessible::Name), QStringLiteral("Alpha"));
  EXPECT_TRUE(alpha->state().selected);
  EXPECT_FALSE(beta->state().selected);
  QAccessibleActionInterface *betaActions = beta->actionInterface();
  ASSERT_NE(betaActions, nullptr);
  EXPECT_TRUE(betaActions->actionNames().contains(
      QAccessibleActionInterface::pressAction()));
  betaActions->doAction(QAccessibleActionInterface::pressAction());
  QApplication::processEvents();
  EXPECT_TRUE(beta->state().selected);
  EXPECT_TRUE(box.isOpen());

  QAccessibleInterface *otherInterface =
      QAccessible::queryAccessibleInterface(&other);
  ASSERT_NE(otherInterface, nullptr);
  EXPECT_TRUE(otherInterface->relations(QAccessible::Controller).isEmpty());
#endif
}

TEST_F(MultiSelectComboBoxTest, DefaultOpenDoesNotVisitEveryRow) {
  CountingListModel model(10000);
  MultiSelectComboBox box(window);
  box.setGeometry(40, 40, 260, Spacing::ControlHeight::Standard);
  box.setModel(&model);
  model.displayRequests = 0;
  model.flagRequests = 0;

  ASSERT_NE(openPopup(box, *window), nullptr);
  QApplication::processEvents();
  EXPECT_LT(model.displayRequests, 100);
  EXPECT_LT(model.flagRequests, 100);
}

TEST_F(MultiSelectComboBoxTest, VisualCheck) {
  if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST")) {
    GTEST_SKIP() << "Set SKIP_VISUAL_TEST=1 to skip visual tests";
  }

  using Edge = fluent::AnchorLayout::Edge;
  auto *layout = new fluent::AnchorLayout(window);
  window->setMinimumSize(660, 420);
  window->resize(700, 440);

  auto *title = new fluent::textfields::Label(
      QStringLiteral("MultiSelectComboBox"), window);
  title->setFluentTypography(Typography::FontRole::Subtitle);
  title->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
  title->anchors()->top = {window, Edge::Top, 28};
  title->anchors()->left = {window, Edge::Left, 36};
  layout->addWidget(title);

  auto *basicLabel =
      new fluent::textfields::Label(QStringLiteral("Regions"), window);
  basicLabel->setFluentTypography(Typography::FontRole::BodyStrong);
  basicLabel->setTextColorRole(
      fluent::textfields::Label::TextColorRole::Primary);
  basicLabel->anchors()->top = {title, Edge::Bottom, 24};
  basicLabel->anchors()->left = {window, Edge::Left, 36};
  layout->addWidget(basicLabel);

  auto *basicModel = new QStringListModel(
      {QStringLiteral("North America"), QStringLiteral("Europe"),
       QStringLiteral("Asia Pacific"), QStringLiteral("Latin America"),
       QStringLiteral("Middle East and Africa"), QString::fromUtf8("中国大陆"),
       QString::fromUtf8("日本")},
      window);
  auto *basic = new MultiSelectComboBox(window);
  basic->setAccessibleName(QStringLiteral("Regions"));
  basic->setPlaceholderText(QStringLiteral("Choose regions"));
  basic->setSearchEnabled(true);
  basic->setModel(basicModel);
  basic->setSelectedRows({1, 2});
  basic->setMaximumVisibleItems(4);
  basic->setFixedWidth(300);
  basic->anchors()->top = {basicLabel, Edge::Bottom, 8};
  basic->anchors()->left = {window, Edge::Left, 36};
  layout->addWidget(basic);

  auto *narrowLabel =
      new fluent::textfields::Label(QStringLiteral("Compact summary"), window);
  narrowLabel->setFluentTypography(Typography::FontRole::BodyStrong);
  narrowLabel->setTextColorRole(
      fluent::textfields::Label::TextColorRole::Primary);
  narrowLabel->anchors()->top = {basicLabel, Edge::Top, 0};
  narrowLabel->anchors()->left = {basic, Edge::Right, 48};
  layout->addWidget(narrowLabel);

  auto *narrow = new MultiSelectComboBox(window);
  narrow->setModel(basicModel);
  narrow->setSelectedRows({0, 1, 2, 3, 4});
  narrow->setFixedWidth(180);
  narrow->anchors()->top = {narrowLabel, Edge::Bottom, 8};
  narrow->anchors()->left = {narrowLabel, Edge::Left, 0};
  layout->addWidget(narrow);

  auto *rtlLabel = new fluent::textfields::Label(
      QStringLiteral("Arabic (right-to-left)"), window);
  rtlLabel->setFluentTypography(Typography::FontRole::BodyStrong);
  rtlLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
  rtlLabel->anchors()->top = {narrow, Edge::Bottom, 24};
  rtlLabel->anchors()->left = {narrowLabel, Edge::Left, 0};
  layout->addWidget(rtlLabel);

  auto *rtl = new MultiSelectComboBox(window);
  rtl->setLayoutDirection(Qt::RightToLeft);
  auto *rtlModel = new QStringListModel({QString::fromUtf8("الشرق الأوسط"),
                                         QString::fromUtf8("شمال أفريقيا"),
                                         QString::fromUtf8("الخليج العربي")},
                                        window);
  rtl->setModel(rtlModel);
  rtl->setSelectedRows({0});
  rtl->setFixedWidth(240);
  rtl->anchors()->top = {rtlLabel, Edge::Bottom, 8};
  rtl->anchors()->left = {narrowLabel, Edge::Left, 0};
  layout->addWidget(rtl);

  auto *disabledLabel =
      new fluent::textfields::Label(QStringLiteral("Disabled"), window);
  disabledLabel->setFluentTypography(Typography::FontRole::BodyStrong);
  disabledLabel->setTextColorRole(
      fluent::textfields::Label::TextColorRole::Primary);
  disabledLabel->anchors()->top = {rtl, Edge::Bottom, 24};
  disabledLabel->anchors()->left = {narrowLabel, Edge::Left, 0};
  layout->addWidget(disabledLabel);

  auto *disabled = new MultiSelectComboBox(window);
  disabled->setPlaceholderText(QStringLiteral("Unavailable"));
  disabled->setEnabled(false);
  disabled->setFixedWidth(220);
  disabled->anchors()->top = {disabledLabel, Edge::Bottom, 8};
  disabled->anchors()->left = {narrowLabel, Edge::Left, 0};
  layout->addWidget(disabled);

  auto *themeButton =
      new fluent::basicinput::Button(QStringLiteral("Dark theme"), window);
  themeButton->setFixedSize(112, Spacing::ControlHeight::Standard);
  themeButton->anchors()->top = {disabled, Edge::Bottom, 24};
  themeButton->anchors()->left = {narrowLabel, Edge::Left, 0};
  layout->addWidget(themeButton);
  QObject::connect(
      themeButton, &fluent::basicinput::Button::clicked, [themeButton]() {
        fluent::FluentElement::setTheme(fluent::FluentElement::currentTheme() ==
                                                fluent::FluentElement::Light
                                            ? fluent::FluentElement::Dark
                                            : fluent::FluentElement::Light);
        themeButton->setText(fluent::FluentElement::currentTheme() ==
                                     fluent::FluentElement::Light
                                 ? QStringLiteral("Dark theme")
                                 : QStringLiteral("Light theme"));
      });

  window->show();
  if (tests::support::shouldCaptureVisualSnapshot()) {
    basic->open();
    QApplication::processEvents();

    tests::support::VisualSnapshotOptions light;
    light.windowSize = QSize(700, 440);
    light.variant = QStringLiteral("light");
    light.theme = tests::support::VisualSnapshotTheme::Light;
    ASSERT_TRUE(tests::support::captureVisualSnapshot(window, light));

    tests::support::VisualSnapshotOptions dark = light;
    dark.variant = QStringLiteral("dark");
    dark.theme = tests::support::VisualSnapshotTheme::Dark;
    ASSERT_TRUE(tests::support::captureVisualSnapshot(window, dark));

    basic->close();
    QApplication::processEvents();
    narrow->open();
    QApplication::processEvents();

    tests::support::VisualSnapshotOptions noSearchLight = light;
    noSearchLight.variant = QStringLiteral("no-search-light");
    ASSERT_TRUE(tests::support::captureVisualSnapshot(window, noSearchLight));

    tests::support::VisualSnapshotOptions noSearchDark = dark;
    noSearchDark.variant = QStringLiteral("no-search-dark");
    ASSERT_TRUE(tests::support::captureVisualSnapshot(window, noSearchDark));
    return;
  }
  QTimer::singleShot(0, basic, [basic]() { basic->open(); });
  qApp->exec();
}
