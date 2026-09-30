#include <gtest/gtest.h>
#include <QAccessible>
#include <QPointer>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTest>
#include <QVariantAnimation>
#include "components/collections/Timeline.h"
#include "components/foundation/MotionPolicy.h"

using fluent::collections::Timeline;

namespace {
class CountingTimelineModel final : public QStandardItemModel {
public:
    explicit CountingTimelineModel(int rows) : QStandardItemModel(rows, 1) {}
    mutable int titleReads = 0;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (role == Qt::DisplayRole) {
            ++titleReads;
            return QStringLiteral("Node %1").arg(index.row());
        }
        return QStandardItemModel::data(index, role);
    }
};

class StatusHeightDelegate final : public QStyledItemDelegate {
public:
    explicit StatusHeightDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override
    {
        return QSize(320, index.data(Timeline::StatusRole).toInt() == Timeline::Active ? 120 : 48);
    }
};
} // namespace

TEST(TimelineTest, Contract_StatusUpdatesReuseLayoutButTextAndCustomDelegatesReflow)
{
    CountingTimelineModel model(3000);
    Timeline view;
    view.setAnimationEnabled(false);
    view.setLayoutMode(QListView::SinglePass);
    view.resize(320, 240);
    view.setModel(&model);
    auto* base = static_cast<QAbstractItemView*>(&view);
    view.show();
    QTest::qWait(30);
    view.grab();
    model.titleReads = 0;
    model.setData(model.index(0, 0), Timeline::Active, Timeline::StatusRole);
    QTest::qWait(30);
    RecordProperty("title_reads_after_status_update", model.titleReads);
    EXPECT_LT(model.titleReads, 100) << "A color update must not remeasure 3000 rows";
    const int height = base->visualRect(model.index(0, 0)).height();
    model.setData(model.index(0, 0), QStringLiteral("Supporting text ").repeated(60),
                  Timeline::DescriptionRole);
    QTest::qWait(30);
    EXPECT_GT(base->visualRect(model.index(0, 0)).height(), height);

    QStandardItemModel customModel(1, 1);
    Timeline custom;
    custom.setItemDelegate(new StatusHeightDelegate(&custom));
    custom.resize(320, 240);
    custom.setModel(&customModel);
    auto* customBase = static_cast<QAbstractItemView*>(&custom);
    custom.show();
    QTest::qWait(20);
    EXPECT_EQ(customBase->visualRect(customModel.index(0, 0)).height(), 48);
    customModel.setData(customModel.index(0, 0), Timeline::Active, Timeline::StatusRole);
    QTest::qWait(20);
    EXPECT_EQ(customBase->visualRect(customModel.index(0, 0)).height(), 120);
}

TEST(TimelineTest, Contract_ResizePreservesVisibleNodeTransitionAndReflowsText)
{
    const auto previous = fluent::MotionPolicy::instance().mode();
    QStandardItemModel model(1, 1);
    model.setData(model.index(0, 0), QStringLiteral("Node with supporting content"));
    model.setData(model.index(0, 0), QStringLiteral("Supporting content ").repeated(12),
                  Timeline::DescriptionRole);
    Timeline view;
    view.resize(320, 240);
    view.setModel(&model);
    auto* base = static_cast<QAbstractItemView*>(&view);
    view.show();
    QTest::qWait(30);
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    view.grab();
    const int narrowHeight = base->visualRect(model.index(0, 0)).height();
    model.setData(model.index(0, 0), Timeline::Active, Timeline::StatusRole);
    QTest::qWait(20);
    QPointer<QVariantAnimation> motion =
        view.findChild<QVariantAnimation*>(QStringLiteral("timelineMarkerTransition"));
    ASSERT_TRUE(motion);
    const int elapsed = motion->currentTime();
    view.resize(600, 260);
    QTest::qWait(10);
    EXPECT_TRUE(motion);
    if (motion) {
        EXPECT_EQ(motion->state(), QAbstractAnimation::Running);
        EXPECT_GE(motion->currentTime(), elapsed);
    }
    EXPECT_LT(base->visualRect(model.index(0, 0)).height(), narrowHeight);
    fluent::MotionPolicy::instance().setMode(previous);
}

TEST(TimelineTest, Contract_ModelOwnershipAndPropertyNoOps)
{
    QObject owner;
    auto* model = new QStandardItemModel(3, 1, &owner);
    {
        Timeline view;
        view.setModel(model);
        EXPECT_EQ(model->parent(), &owner);
        QSignalSpy layout(&view, &Timeline::nodeAlignmentChanged);
        view.setNodeAlignment(Timeline::Alternate);
        view.setNodeAlignment(Timeline::Alternate);
        EXPECT_EQ(layout.count(), 1);
        QSignalSpy motion(&view, &Timeline::animationEnabledChanged);
        view.setAnimationEnabled(false);
        view.setAnimationEnabled(false);
        EXPECT_EQ(motion.count(), 1);
        EXPECT_EQ(view.selectionMode(), Timeline::SelectionMode::None);
    }
    EXPECT_EQ(model->rowCount(), 3);
}

TEST(TimelineTest, Contract_WrappedRowsAndNoPersistentItemWidgets)
{
    QStandardItemModel model(3000, 1);
    model.setData(model.index(0, 0),
                  QStringLiteral("A long title that must wrap in a narrow timeline"));
    model.setData(model.index(0, 0),
                  QStringLiteral("Description with additional text and mixed Unicode 示例文字"),
                  Timeline::DescriptionRole);
    Timeline view;
    view.resize(320, 240);
    view.setModel(&model);
    view.show();
    QTest::qWait(30);
    auto* base = static_cast<QAbstractItemView*>(&view);
    const int children = view.findChildren<QWidget*>().size();
    const int leadingHeight = base->visualRect(model.index(0, 0)).height();
    EXPECT_GT(leadingHeight, 48);
    view.setNodeAlignment(Timeline::Alternate);
    QTest::qWait(30);
    EXPECT_GT(base->visualRect(model.index(0, 0)).height(), leadingHeight);
    EXPECT_EQ(view.findChildren<QWidget*>().size(), children);
    EXPECT_LT(children, 30);
    view.scrollTo(model.index(2999, 0));
    QTest::qWait(30);
    EXPECT_EQ(view.findChildren<QWidget*>().size(), children);
}

TEST(TimelineTest, Contract_AccessibilityIncludesCallerTextTimestampAndStatus)
{
    QStandardItemModel model(2, 1);
    model.setData(model.index(0, 0), QStringLiteral("Node"));
    model.setData(model.index(0, 0), QStringLiteral("Time label"), Timeline::TimestampRole);
    model.setData(model.index(0, 0), QStringLiteral("Description"), Timeline::DescriptionRole);
    model.setData(model.index(0, 0), Timeline::Warning, Timeline::StatusRole);
    model.setData(model.index(1, 0), QStringLiteral("Caller name"), Qt::AccessibleTextRole);
    Timeline view;
    view.setAccessibleName(QStringLiteral("Caller timeline"));
    view.setModel(&model);
    auto* root = QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(root->role(), QAccessible::List);
    ASSERT_EQ(root->childCount(), 2);
    EXPECT_EQ(root->text(QAccessible::Name), QStringLiteral("Caller timeline"));
    EXPECT_EQ(root->child(0)->text(QAccessible::Name), QStringLiteral("Node"));
    const auto description = root->child(0)->text(QAccessible::Description);
    EXPECT_TRUE(description.contains(QStringLiteral("Time label")));
    EXPECT_TRUE(description.contains(QStringLiteral("Description")));
    EXPECT_TRUE(description.contains(QStringLiteral("Warning")));
    EXPECT_EQ(root->child(1)->text(QAccessible::Name), QStringLiteral("Caller name"));
    model.setData(model.index(0, 0), QStringLiteral("Custom detail"),
                  Qt::AccessibleDescriptionRole);
    EXPECT_EQ(root->child(0)->text(QAccessible::Description), QStringLiteral("Custom detail"));
    model.insertRow(0);
    EXPECT_EQ(root->childCount(), 3);
    model.removeRow(0);
    EXPECT_EQ(root->childCount(), 2);
}

TEST(TimelineTest, Contract_InheritedPointerSignalsRemainExactOnce)
{
    QStandardItemModel model(1, 1);
    model.setData(model.index(0, 0), QStringLiteral("Node"));
    Timeline view;
    view.resize(400, 160);
    view.setModel(&model);
    view.show();
    QTest::qWait(20);
    QSignalSpy clicked(&view, &QAbstractItemView::clicked),
        pressed(&view, &QAbstractItemView::pressed);
    QSignalSpy itemClicked(&view, &Timeline::itemClicked);
    auto* base = static_cast<QAbstractItemView*>(&view);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      base->visualRect(model.index(0, 0)).center());
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(pressed.count(), 1);
    EXPECT_EQ(itemClicked.count(), 1);
    EXPECT_EQ(model.data(model.index(0, 0), Timeline::StatusRole), QVariant());
}

TEST(TimelineTest, Contract_MotionReversalPolicyAndModelDestruction)
{
    const auto previous = fluent::MotionPolicy::instance().mode();
    auto* model = new QStandardItemModel(1, 1);
    model->setData(model->index(0, 0), QStringLiteral("Node"));
    Timeline view;
    view.resize(400, 180);
    view.setModel(model);
    view.show();
    QTest::qWait(20);
    view.grab();
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    view.grab();
    model->setData(model->index(0, 0), Timeline::Active, Timeline::StatusRole);
    QTest::qWait(30);
    model->setData(model->index(0, 0), Timeline::Error, Timeline::StatusRole);
    EXPECT_FALSE(view.findChildren<QVariantAnimation*>(QStringLiteral("timelineMarkerTransition"))
                     .isEmpty());
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    QTest::qWait(10);
    for (auto* animation :
         view.findChildren<QVariantAnimation*>(QStringLiteral("timelineMarkerTransition")))
        EXPECT_EQ(animation->state(), QAbstractAnimation::Stopped);
    delete model;
    EXPECT_EQ(view.model(), nullptr);
    view.setModel(new QStandardItemModel(2, 1, &view));
    fluent::MotionPolicy::instance().setMode(previous);
}

TEST(TimelineTest, Contract_NodeTransitionRendersAnIntermediateFrame)
{
    const auto previous = fluent::MotionPolicy::instance().mode();
    QStandardItemModel model(1, 1);
    model.setData(model.index(0, 0), QStringLiteral("Node"));
    Timeline view;
    view.resize(400, 160);
    view.setModel(&model);
    view.show();
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Full);
    QTest::qWait(30);
    const auto before = view.viewport()->grab().toImage();
    model.setData(model.index(0, 0), Timeline::Active, Timeline::StatusRole);
    QTest::qWait(35);
    const auto intermediate = view.viewport()->grab().toImage();
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    const auto final = view.viewport()->grab().toImage();
    EXPECT_NE(before, intermediate);
    EXPECT_NE(intermediate, final);
    fluent::MotionPolicy::instance().setMode(previous);
}

TEST(TimelineTest, Contract_DisabledMarkerPresentationRestoresAfterReenable)
{
    QStandardItemModel model(1, 1);
    model.setData(model.index(0, 0), Timeline::Active, Timeline::StatusRole);
    Timeline view;
    view.resize(400, 160);
    view.setModel(&model);
    view.show();
    QTest::qWait(20);
    const QRect markerArea(14, 16, 12, 12);
    const auto enabled = view.viewport()->grab(markerArea).toImage();
    view.setEnabled(false);
    const auto disabled = view.viewport()->grab(markerArea).toImage();
    EXPECT_NE(enabled, disabled);
    view.setEnabled(true);
    EXPECT_EQ(view.viewport()->grab(markerArea).toImage(), enabled);
    EXPECT_EQ(model.index(0, 0).data(Timeline::StatusRole).toInt(), Timeline::Active);
}
