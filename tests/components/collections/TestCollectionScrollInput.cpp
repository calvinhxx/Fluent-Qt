#include <memory>

#include <QApplication>
#include <QHeaderView>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QWheelEvent>
#include <QtTest/QTest>
#include <gtest/gtest.h>

#include "components/collections/FileListView.h"
#include "components/collections/FlowView.h"
#include "components/collections/GridView.h"
#include "components/collections/ListView.h"
#include "components/collections/Timeline.h"
#include "components/collections/TreeView.h"
#include "components/scrolling/OverscrollController.h"
#include "components/scrolling/ScrollBar.h"
#include "compatibility/EventConstructionCompat.h"

namespace {
using namespace fluent::collections;
using fluent::scrolling::OverscrollController;

class CollectionScrollInput : public ::testing::TestWithParam<const char*> {
protected:
    std::unique_ptr<QAbstractItemView> view;
    OverscrollController* overscroll = nullptr;

    void SetUp() override
    {
        const QString kind = QString::fromLatin1(GetParam());
        if (kind == QStringLiteral("FileListView"))
            view = std::make_unique<FileListView>();
        else if (kind == QStringLiteral("GridView"))
            view = std::make_unique<GridView>();
        else if (kind == QStringLiteral("FlowView"))
            view = std::make_unique<FlowView>();
        else if (kind == QStringLiteral("TreeView"))
            view = std::make_unique<TreeView>();
        else if (kind == QStringLiteral("Timeline"))
            view = std::make_unique<Timeline>();
        else
            view = std::make_unique<ListView>();

        auto* model = new QStandardItemModel(view.get());
        for (int row = 0; row < 60; ++row) {
            auto* item = new QStandardItem(QStringLiteral("Item %1").arg(row));
            item->setSizeHint(QSize(120, 48));
            model->appendRow(item);
        }
        view->setModel(model);
        view->resize(320, 200);
        view->setAttribute(Qt::WA_DontShowOnScreen);
        view->show();
        view->doItemsLayout();
        ASSERT_TRUE(QTest::qWaitFor([&] { return bar()->maximum() > 0; }));
        overscroll = view->findChild<OverscrollController*>();
        ASSERT_NE(overscroll, nullptr);
    }

    QScrollBar* bar() const { return view->verticalScrollBar(); }

    void wheel(int pixels, Qt::ScrollPhase phase = Qt::NoScrollPhase)
    {
        auto* viewport = view->viewport();
        const QPointF pos(viewport->rect().center());
        QWheelEvent event(pos, viewport->mapToGlobal(pos.toPoint()), QPoint(0, pixels), {}, Qt::NoButton,
            Qt::NoModifier, phase, false);
        QApplication::sendEvent(viewport, &event);
        EXPECT_TRUE(event.isAccepted());
    }
};

TEST_P(CollectionScrollInput, LargePixelBoundaryInputStaysBounded)
{
    for (const auto phase : {Qt::NoScrollPhase, Qt::ScrollUpdate}) {
        for (const int direction : {1, -1}) {
            SCOPED_TRACE(::testing::Message() << phase << ":" << direction);
            overscroll->cancel();
            bar()->setValue(direction > 0 ? bar()->minimum() : bar()->maximum());
            const int boundary = bar()->value();
            wheel(direction * 1200, phase);
            EXPECT_GT(qAbs(overscroll->value()), 0);
            EXPECT_LE(qAbs(overscroll->value()), 100);
            EXPECT_EQ(bar()->value(), boundary);
        }
    }
}

TEST_P(CollectionScrollInput, UnscrollableHorizontalAxisPreservesVerticalRecovery)
{
    view->horizontalScrollBar()->setRange(0, 0);
    for (const bool pixel : {false, true}) {
        overscroll->cancel();
        bar()->setValue(bar()->minimum());
        wheel(160);
        const qreal stretched = overscroll->value();
        ASSERT_GT(stretched, 0);
        const QPointF pos(view->viewport()->rect().center());
        QWheelEvent event(pos, view->viewport()->mapToGlobal(pos.toPoint()), pixel ? QPoint(40, 0) : QPoint(),
            pixel ? QPoint() : QPoint(120, 0), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(view->viewport(), &event);
        EXPECT_FALSE(event.isAccepted());
        EXPECT_EQ(overscroll->value(), stretched);
        EXPECT_EQ(bar()->value(), bar()->minimum());
        ASSERT_TRUE(QTest::qWaitFor([&] { return qFuzzyIsNull(overscroll->value()); }, 1000));
    }
}

TEST_P(CollectionScrollInput, ReversePixelInputImmediatelyResumesContentScroll)
{
    for (const bool bouncing : {false, true}) {
        for (const int direction : {1, -1}) {
            overscroll->cancel();
            bar()->setValue(direction > 0 ? bar()->minimum() : bar()->maximum());
            const int boundary = bar()->value();
            wheel(direction * 100);
            const qreal stretched = qAbs(overscroll->value());
            if (bouncing) {
                ASSERT_TRUE(QTest::qWaitFor([&] {
                    return qAbs(overscroll->value()) > 1 && qAbs(overscroll->value()) < stretched;
                }));
            }
            wheel(-direction * 40);
            EXPECT_EQ(overscroll->value(), 0);
            EXPECT_EQ(bar()->value(), boundary + direction * 40);
        }
    }
}

TEST_P(CollectionScrollInput, FingerReversalReleasesTravelWithoutDampingOrLostRemainder)
{
    bar()->setValue(bar()->minimum());
    wheel(160, Qt::ScrollUpdate);
    ASSERT_EQ(overscroll->value(), 80);
    wheel(-30, Qt::ScrollUpdate);
    EXPECT_EQ(overscroll->value(), 50);
    EXPECT_EQ(bar()->value(), bar()->minimum());
    wheel(-80, Qt::ScrollUpdate);
    EXPECT_EQ(overscroll->value(), 0);
    EXPECT_EQ(bar()->value(), bar()->minimum() + 30);
}

TEST_P(CollectionScrollInput, MomentumTailDoesNotRestartReleaseAnimation)
{
    bar()->setValue(bar()->minimum());
    wheel(160, Qt::ScrollUpdate);
    wheel(0, Qt::ScrollEnd);
    for (int i = 0; i < 20; ++i) {
        wheel(12, Qt::ScrollMomentum);
        QTest::qWait(20);
    }
    EXPECT_EQ(overscroll->value(), 0);
    EXPECT_EQ(bar()->value(), bar()->minimum());
}

TEST_P(CollectionScrollInput, DraggingOverlayThumbDiscardsElasticOffset)
{
    bar()->setValue(bar()->minimum());
    wheel(160);
    ASSERT_GT(overscroll->value(), 0);
    fluent::scrolling::ScrollBar* overlay = nullptr;
    for (auto* candidate : view->findChildren<fluent::scrolling::ScrollBar*>()) {
        if (candidate->orientation() == Qt::Vertical && candidate->isVisible()) {
            overlay = candidate;
            break;
        }
    }
    ASSERT_NE(overlay, nullptr);
    QStyleOptionSlider option;
    option.initFrom(overlay);
    option.orientation = Qt::Vertical;
    option.minimum = overlay->minimum();
    option.maximum = overlay->maximum();
    option.pageStep = overlay->pageStep();
    option.sliderPosition = overlay->sliderPosition();
    const QRect thumb = overlay->style()->subControlRect(QStyle::CC_ScrollBar, &option, QStyle::SC_ScrollBarSlider,
        overlay);
    const QPoint end = overlay->rect().center();
    QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, thumb.center());
    ASSERT_TRUE(overlay->isSliderDown());
    FLUENT_MAKE_MOUSE_EVENT(move, QEvent::MouseMove, overlay, end, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(overlay, &move);
    EXPECT_GT(bar()->value(), bar()->minimum());
    EXPECT_EQ(overlay->value(), bar()->value());
    EXPECT_EQ(overscroll->value(), 0);
    const int draggedValue = bar()->value();
    QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, end);
    QTest::qWait(400);
    EXPECT_EQ(bar()->value(), draggedValue);
    EXPECT_EQ(overscroll->value(), 0);
}

INSTANTIATE_TEST_SUITE_P(SharedViews,
    CollectionScrollInput,
    ::testing::Values("ListView", "FileListView", "GridView", "FlowView", "TreeView", "Timeline"),
    [](const ::testing::TestParamInfo<const char*>& info) { return info.param; });
TEST(CollectionSecondaryScroll, WideTreeAcceptsHorizontalPixelAndAngleInput)
{
    TreeView view;
    QStandardItemModel model;
    model.appendRow(new QStandardItem(QStringLiteral("Wide row")));
    view.setModel(&model);
    view.header()->setStretchLastSection(false);
    view.setColumnWidth(0, 900);
    view.resize(320, 200);
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.show();
    QApplication::processEvents();
    auto* horizontal = view.horizontalScrollBar();
    ASSERT_GT(horizontal->maximum(), 0);
    ASSERT_EQ(view.verticalScrollBar()->maximum(), 0);
    for (const bool pixel : {true, false}) {
        horizontal->setValue(0);
        auto* viewport = view.viewport();
        const QPointF pos(viewport->rect().center());
        QWheelEvent event(pos, viewport->mapToGlobal(pos.toPoint()), pixel ? QPoint(-60, 0) : QPoint(),
            pixel ? QPoint() : QPoint(-120, 0), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(viewport, &event);
        EXPECT_TRUE(event.isAccepted());
        EXPECT_GT(horizontal->value(), 0);
        EXPECT_EQ(view.verticalScrollBar()->value(), 0);
    }
}
} // namespace
