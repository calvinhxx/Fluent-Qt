#include <gtest/gtest.h>
#include <QAccessible>
#include <QApplication>
#include <QDialog>
#include <QElapsedTimer>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QSignalSpy>
#include <QTest>
#include <cmath>
#include <limits>
#include <vector>
#include <FluentQt/BasicInput.h>
#include <FluentQt/Charts.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/TextFields.h>
#include "QtTestEnvironment.h"

using namespace fluent::charts;

namespace {
class CountingModel : public ChartModel {
public:
    mutable int queries = 0, reads = 0, first = -1, last = -1;
    QPointF pointAt(int row) const override
    {
        ++reads;
        return ChartModel::pointAt(row);
    }
    QVector<int> sampledRows(int begin, int end, int budget) const override
    {
        ++queries;
        first = begin;
        last = end;
        return ChartModel::sampledRows(begin, end, budget);
    }
};
QVector<QPointF> wave(int count)
{
    QVector<QPointF> points;
    points.reserve(count);
    for (int i = 0; i < count; ++i)
        points.append({double(i), 50 + 20 * std::sin(i * 0.02)});
    return points;
}
void movePointer(QWidget& target, const QPoint& position)
{
    QMouseEvent event(QEvent::MouseMove, position, target.mapToGlobal(position), Qt::NoButton,
                      Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&target, &event);
}
} // namespace

TEST(ChartViewTest, Contract_BorrowedModelsShareDataAndSurviveDestruction)
{
    QWidget owner;
    auto* model = new ChartModel(&owner);
    model->setPoints(wave(100));
    ChartView first, second;
    first.setModel(model);
    second.setModel(model);
    EXPECT_EQ(model->parent(), &owner);
    QSignalSpy changed(&first, &ChartView::modelChanged);
    first.setModel(model);
    first.addSeries(model);
    EXPECT_EQ(first.seriesCount(), 1);
    EXPECT_EQ(changed.count(), 0);
    first.setCurrentPoint(0, 50);
    delete model;
    EXPECT_EQ(first.model(), nullptr);
    EXPECT_EQ(second.model(), nullptr);
    EXPECT_EQ(first.currentRow(), -1);
    first.resize(640, 320);
    EXPECT_FALSE(first.grab().isNull());
}

TEST(ChartViewTest, Contract_ChildModelsCanOutliveViewPrivateState)
{
    // Gallery samples parent their models to the chart or its composition host.
    for (int i = 0; i < 12; ++i) {
        auto* view = new ChartView;
        QPointer<ChartModel> model = new ChartModel(view);
        model->setPoints(wave(100));
        view->setModel(model);
        view->setCurrentPoint(0, 5);
        view->resize(400, 220);
        view->grab();
        delete view;
        EXPECT_TRUE(model.isNull());
    }
}

TEST(ChartViewTest, Contract_MillionPointPaintAndHoverNeverRescanHistory)
{
    CountingModel model;
    model.setPoints(wave(1000000));
    ChartView view;
    view.resize(1000, 400);
    view.setModel(&model);
    view.show();
    QElapsedTimer timer;
    timer.start();
    view.grab();
    ::testing::Test::RecordProperty("million_first_projection_us", timer.nsecsElapsed() / 1000);
    EXPECT_EQ(model.queries, 1);
    EXPECT_LE(model.reads, view.maximumPointCount());
    EXPECT_LE(view.renderedPointCount(), view.maximumPointCount());
    const int reads = model.reads;
    for (int i = 0; i < 10; ++i) {
        view.grab();
        QTest::mouseMove(&view, QPoint(300 + i, 130));
    }
    EXPECT_EQ(model.queries, 1);
    EXPECT_EQ(model.reads, reads);
    view.setXRange(450000, 450100);
    view.grab();
    EXPECT_EQ(model.queries, 2);
    EXPECT_GE(model.first, 449999);
    EXPECT_LE(model.last, 450102);
    EXPECT_LE(view.renderedPointCount(), 104);
}

TEST(ChartViewTest, Contract_StreamCoalescesAndHiddenViewsDoNoProjectionWork)
{
    CountingModel model;
    model.setPoints(wave(1000));
    ChartView view;
    view.resize(800, 320);
    view.setModel(&model);
    view.show();
    view.grab();
    const auto builds = view.projectionBuildCount();
    for (int i = 0; i < 100; ++i)
        model.appendPoints({{1000.0 + i, 50.0}});
    view.grab();
    EXPECT_EQ(view.projectionBuildCount(), builds);
    ASSERT_TRUE(QTest::qWaitFor(
        [&] {
            view.grab();
            return view.projectionBuildCount() > builds;
        },
        1000));
    EXPECT_EQ(view.projectionBuildCount(), builds + 1);
    view.hide();
    const int queries = model.queries;
    model.appendPoints({{1100, 55}});
    QTest::qWait(45);
    EXPECT_EQ(model.queries, queries);
    view.show();
    view.grab();
    EXPECT_EQ(model.queries, queries + 1);
}

TEST(ChartViewTest, Contract_BoundsQueriesDoNotChangeTheCachedFrame)
{
    ChartModel model;
    model.setPoints({{0, 10}, {1, 20}, {2, 15}});
    LineChart view;
    view.setModel(&model);
    view.resize(640, 320);
    view.show();
    view.grab();
    const auto builds = view.projectionBuildCount();
    model.appendPoints({{3, 500}});
    const auto cached = view.grab().toImage();
    EXPECT_GE(view.maximumY(), 500);
    EXPECT_LE(view.minimumY(), 10);
    EXPECT_EQ(view.projectionBuildCount(), builds);
    EXPECT_TRUE(view.grab().toImage() == cached);
}

TEST(ChartViewTest, Contract_SharedCursorNeverMislabelsDifferentSampleTimes)
{
    const auto render = [](bool aligned, const QString& comparisonName) {
        CountingModel primary, comparison;
        primary.setName("Current");
        primary.setPoints({{0, 3}, {1, 5}, {2, 4}});
        comparison.setName(comparisonName);
        comparison.setPoints(aligned ? QVector<QPointF>{{0, 3}, {1, 4}, {2, 2}}
                                     : QVector<QPointF>{{.5, 3}, {1.5, 4}, {2.5, 2}});
        LineChart view;
        view.resize(640, 320);
        view.setLegendVisible(false);
        view.setModel(&primary);
        view.addSeries(&comparison);
        view.setXRange(0, 3);
        view.setYRange(0, 10);
        view.show();
        view.grab();
        view.setCurrentPoint(0, 1);
        const int reads = primary.reads + comparison.reads;
        const auto image = view.grab().toImage();
        EXPECT_EQ(primary.reads + comparison.reads, reads);
        return image;
    };
    // With the legend hidden, only a valid comparison readout exposes its name.
    EXPECT_TRUE(render(false, "Alpha") == render(false, "Bravo"));
    EXPECT_FALSE(render(true, "Alpha") == render(true, "Bravo"));
}

TEST(ChartViewTest, Contract_TypesThemesRangesAndBudgets)
{
    const auto oldTheme = fluent::FluentElement::currentTheme();
    ChartModel model;
    model.setPoints(wave(10000));
    model.setName("Sample values");
    ChartView view;
    view.setModel(&model);
    view.setTitle("Illustrative measurements");
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark,
                       fluent::FluentElement::HighContrast}) {
        fluent::FluentElement::setTheme(theme);
        for (int type = ChartView::Line; type <= ChartView::Sparkline; ++type) {
            view.setChartType(static_cast<ChartView::ChartType>(type));
            for (QSize size : {QSize(760, 360), QSize(260, 220)}) {
                view.resize(size);
                const auto image = view.grab().toImage();
                EXPECT_FALSE(image.isNull());
                EXPECT_GT(view.renderedPointCount(), 0);
                EXPECT_LE(view.renderedPointCount(), view.maximumPointCount());
                EXPECT_TRUE(view.rect().contains(view.plotRect().toRect()));
            }
        }
    }
    fluent::FluentElement::setTheme(oldTheme);
    QSignalSpy ranges(&view, &ChartView::xRangeChanged);
    view.setXRange(10, 20);
    view.setXRange(10, 20);
    view.setXRange(20, 10);
    EXPECT_EQ(ranges.count(), 1);
    EXPECT_FALSE(view.isAutoRange());
    view.resetXRange();
    view.resetXRange();
    EXPECT_EQ(ranges.count(), 2);
}

TEST(ChartViewTest, Contract_ReadoutsDismissWithoutClearingSelectionAcrossTypes)
{
    for (int type = ChartView::Line; type <= ChartView::Sparkline; ++type) {
        SCOPED_TRACE(type);
        CountingModel model;
        model.setName(QStringLiteral("Measurements"));
        model.setPoints({{0, 2}, {1, 5}, {2, 9}});
        ChartView view;
        view.setChartType(static_cast<ChartView::ChartType>(type));
        view.resize(640, 400);
        view.setLegendVisible(false);
        view.setModel(&model);
        view.setXRange(0, 2);
        view.setYRange(0, 10);
        view.show();
        view.grab();
        const auto builds = view.projectionBuildCount();
        const auto plot = view.plotRect();
        QPointF point = plot.center();
        int expectedRow = 1;
        if (type == ChartView::Bar)
            point.setY(plot.bottom() - plot.height() * .25);
        else if (type == ChartView::HorizontalBar)
            point.setX(plot.left() + plot.width() * .25);
        else if (type == ChartView::Pie || type == ChartView::Donut) {
            const double angle = 67.5 * std::acos(-1.0) / 180;
            const double radius = std::min(plot.width(), plot.height()) * .4;
            point += QPointF(radius * std::cos(angle), -radius * std::sin(angle));
            expectedRow = 0;
        }
        QSignalSpy selected(&view, &ChartView::currentPointChanged);
        QSignalSpy activated(&view, &ChartView::rangeActivated);
        movePointer(view, point.toPoint());
        auto* readout = view.findChild<fluent::dialogs_flyouts::Popup*>();
        if (type == ChartView::Sparkline) {
            EXPECT_EQ(readout, nullptr);
            continue;
        }
        ASSERT_NE(readout, nullptr);
        EXPECT_TRUE(readout->isOpen());
        EXPECT_TRUE(readout->testAttribute(Qt::WA_TransparentForMouseEvents));
        EXPECT_FALSE(readout->isModal());
        EXPECT_FALSE(readout->isDim());
        EXPECT_EQ(readout->focusPolicy(), Qt::NoFocus);
        EXPECT_EQ(view.currentRow(), -1);

        movePointer(view, QPoint(8, 8));
        EXPECT_FALSE(readout->isVisible());
        EXPECT_EQ(selected.count(), 0);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, point.toPoint());
        EXPECT_TRUE(readout->isOpen());
        EXPECT_EQ(view.currentRow(), expectedRow);
        EXPECT_EQ(selected.count(), 1);
        EXPECT_EQ(activated.count(), 1);

        movePointer(view, QPoint(8, 8));
        EXPECT_FALSE(readout->isOpen());
        const auto selectionOnly = view.grab().toImage();
        movePointer(view, point.toPoint());
        EXPECT_TRUE(readout->isOpen());
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(&view, &leave);
        EXPECT_FALSE(readout->isVisible());
        EXPECT_TRUE(view.grab().toImage() == selectionOnly);
        EXPECT_EQ(view.currentRow(), expectedRow);
        EXPECT_EQ(selected.count(), 1);
        EXPECT_EQ(activated.count(), 1);
        EXPECT_EQ(view.findChildren<fluent::dialogs_flyouts::Popup*>().size(), 1);
        EXPECT_EQ(view.projectionBuildCount(), builds);
    }
}

TEST(ChartViewTest, Contract_ReadoutKeyboardFocusAndLifetime)
{
    QWidget window;
    window.resize(640, 440);
    ChartModel model;
    model.setName(QStringLiteral("<b>Raw name</b>"));
    model.setPoints({{0, 2}, {1, 5}, {2, 9}}, {"First", "Middle", "Last"});
    auto* view = new LineChart(&window);
    view->setGeometry(0, 0, 640, 360);
    view->setModel(&model);
    view->setXRange(0, 2);
    view->setYRange(0, 10);
    fluent::basicinput::Button focusSink(QStringLiteral("Focus sink"), &window);
    focusSink.setGeometry(20, 390, 120, 32);
    window.show();
    if (!tests::support::isHeadlessPlatform()) {
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland")))
            window.activateWindow();
    }
    view->setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return view->hasFocus(); }, 1000));
    view->grab();
    movePointer(*view, view->plotRect().center().toPoint());
    QPointer<fluent::dialogs_flyouts::Popup> readout =
        window.findChild<fluent::dialogs_flyouts::Popup*>();
    ASSERT_TRUE(readout);
    ASSERT_TRUE(readout->isOpen());
    EXPECT_TRUE(view->hasFocus());
    QSignalSpy selected(view, &ChartView::currentPointChanged);
    QTest::keyClick(view, Qt::Key_End);
    EXPECT_EQ(view->currentRow(), 2);
    bool showsLast = false, showsRawName = false;
    for (auto* label : readout->findChildren<fluent::textfields::Label*>()) {
        showsLast |= label->text() == QStringLiteral("Last");
        showsRawName |= label->text() == model.name() && label->textFormat() == Qt::PlainText;
    }
    EXPECT_TRUE(showsLast); // Keyboard selection replaces a stationary pointer's hover.
    EXPECT_TRUE(showsRawName);
    EXPECT_TRUE(view->hasFocus());
    EXPECT_EQ(selected.count(), 1);

    focusSink.setFocus();
    ASSERT_TRUE(QTest::qWaitFor([&] { return focusSink.hasFocus(); }, 1000));
    EXPECT_FALSE(readout->isVisible());
    EXPECT_EQ(view->currentRow(), 2);
    EXPECT_TRUE(view->currentPointText().contains(QStringLiteral("Last")));
    view->setFocus();
    QTest::keyClick(view, Qt::Key_End); // Same row reopens without another selection signal.
    EXPECT_TRUE(readout->isOpen());
    EXPECT_EQ(selected.count(), 1);

    view->setEnabled(false);
    EXPECT_FALSE(readout->isVisible());
    view->setEnabled(true);
    EXPECT_FALSE(readout->isVisible());
    view->setCurrentPoint(0, 2);
    EXPECT_TRUE(readout->isOpen());
    view->hide();
    EXPECT_FALSE(readout->isVisible());
    view->show();
    view->grab();
    EXPECT_FALSE(readout->isVisible());
    view->setCurrentPoint(0, 2);
    EXPECT_TRUE(readout->isOpen());
    QEvent deactivate(QEvent::WindowDeactivate);
    QApplication::sendEvent(&window, &deactivate);
    EXPECT_FALSE(readout->isVisible());
    view->setCurrentPoint(0, 2);
    EXPECT_TRUE(readout->isOpen());
    EXPECT_EQ(selected.count(), 1);
    delete view; // Popup is hosted by the window, but its lifetime belongs to the chart.
    EXPECT_TRUE(readout.isNull());
}

TEST(ChartViewTest, Contract_ReadoutDetailsStayCompleteAtNarrowWidths)
{
    struct Example {
        int width;
        QString name, heading, suffix;
        bool stacked;
    };
    const QVector<Example> examples = {
        {640, "Sample activity", "Tue", "", false},
        {640, "North America production response time for returning customers",
         "Tuesday, September 17, 2026, 10:00–11:00", " ms", false},
        {260, QString::fromUtf8("华东地区生产环境平均请求响应时间"),
         QString::fromUtf8("2026 年 9 月 17 日上午 10 点"), " milliseconds", true},
        {260, "ProductionResponseTimeForReturningCustomers", "Tue", " ms", false},
    };
    for (const auto& example : examples) {
        SCOPED_TRACE(example.name.toStdString());
        ChartModel model;
        model.setName(example.name);
        model.setPoints({{0, 2}, {1, 32}, {2, 9}}, {"Mon", example.heading, "Wed"});
        LineChart view;
        view.resize(example.width, 560);
        view.setLegendVisible(false);
        view.setValueSuffix(example.suffix);
        view.setModel(&model);
        view.show();
        view.grab();
        view.setCurrentPoint(0, 1);
        auto* readout = view.findChild<fluent::dialogs_flyouts::Popup*>();
        ASSERT_NE(readout, nullptr);
        ASSERT_TRUE(readout->isOpen());
        const QString value = view.locale().toString(32.0, 'g', 5) + example.suffix;
        fluent::textfields::Label *nameLabel = nullptr, *valueLabel = nullptr;
        int detailLabels = 0;
        for (auto* label : readout->findChildren<fluent::textfields::Label*>()) {
            if (!label->isVisible() || label->text().isEmpty())
                continue;
            ++detailLabels;
            // Verify what QLabel actually paints, not just Label's retained full text.
            EXPECT_EQ(static_cast<QLabel*>(label)->text(), label->text());
            EXPECT_FALSE(label->isTextElided());
            EXPECT_TRUE(readout->contentsRect().contains(label->geometry()));
            EXPECT_GE(label->height(), label->heightForWidth(label->width()));
            if (label->text() == example.name)
                nameLabel = label;
            if (label->text() == value)
                valueLabel = label;
        }
        EXPECT_EQ(detailLabels, 3);
        ASSERT_NE(nameLabel, nullptr);
        ASSERT_NE(valueLabel, nullptr);
        EXPECT_FALSE(nameLabel->geometry().intersects(valueLabel->geometry()));
        EXPECT_TRUE(view.rect().contains(readout->geometry()));
        if (example.stacked)
            EXPECT_GT(valueLabel->y(), nameLabel->geometry().bottom());
        else
            EXPECT_EQ(valueLabel->y(), nameLabel->y());
        if (nameLabel->fontMetrics().horizontalAdvance(example.name) > nameLabel->width())
            EXPECT_GT(nameLabel->height(), nameLabel->fontMetrics().height());
    }
}

TEST(ChartViewTest, Contract_WrappedComparisonsFitAndReportOverflow)
{
    std::vector<std::unique_ptr<ChartModel>> models;
    LineChart view;
    view.resize(300, 320);
    view.setLegendVisible(false);
    for (int i = 0; i < 12; ++i) {
        auto model = std::make_unique<ChartModel>();
        model->setName(QStringLiteral("Regional production response time comparison %1").arg(i));
        model->setPoints({{0, 2}, {1, 32}, {2, 9}});
        view.addSeries(model.get());
        models.push_back(std::move(model));
    }
    view.show();
    view.grab();
    view.setCurrentPoint(0, 1);
    auto* readout = view.findChild<fluent::dialogs_flyouts::Popup*>();
    ASSERT_NE(readout, nullptr);
    EXPECT_TRUE(view.rect().contains(readout->geometry()));
    int names = 0;
    QString overflow;
    for (auto* label : readout->findChildren<fluent::textfields::Label*>()) {
        if (!label->isVisible())
            continue;
        EXPECT_TRUE(readout->contentsRect().contains(label->geometry()));
        EXPECT_FALSE(label->isTextElided());
        if (label->text().startsWith(QStringLiteral("Regional production")))
            ++names;
        if (label->text().endsWith(QStringLiteral(" more series")))
            overflow = label->text();
    }
    EXPECT_GT(names, 0);
    EXPECT_LT(names, int(models.size()));
    EXPECT_EQ(overflow, QStringLiteral("%1 more series").arg(int(models.size()) - names));
}

TEST(ChartViewTest, Contract_FocusRingFollowsKeyboardRatherThanMouse)
{
    QWidget window;
    window.resize(640, 420);
    ChartModel model;
    model.setPoints({{0, 2}, {1, 5}, {2, 9}});
    LineChart view(&window);
    view.setGeometry(0, 0, 640, 360);
    view.setModel(&model);
    view.setXRange(0, 2);
    view.setYRange(0, 10);
    fluent::basicinput::Button focusSink(QStringLiteral("Focus sink"), &window);
    focusSink.setGeometry(20, 380, 120, 32);
    QWidget::setTabOrder(&focusSink, &view);
    window.show();
    if (!tests::support::isHeadlessPlatform()) {
        ASSERT_TRUE(QTest::qWaitForWindowExposed(&window));
        if (!QGuiApplication::platformName().startsWith(QStringLiteral("wayland")))
            window.activateWindow();
    }
    focusSink.setFocus(Qt::OtherFocusReason);
    ASSERT_TRUE(QTest::qWaitFor([&] { return focusSink.hasFocus(); }, 1000));

    // Compare the empty top edge: selection/readouts cannot change this region.
    const auto topEdge = [&] {
        const auto image = view.grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        return image.copy(qRound(view.width() * .25 * dpr), 0, qRound(view.width() * .5 * dpr),
                          qRound(8 * dpr));
    };
    const auto unfocused = topEdge();
    const auto point = view.plotRect().center().toPoint();
    QSignalSpy activated(&view, &ChartView::rangeActivated);
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, point);
    ASSERT_TRUE(QTest::qWaitFor([&] { return view.hasFocus(); }, 1000));
    EXPECT_TRUE(topEdge() == unfocused);
    EXPECT_EQ(view.currentRow(), 1);
    EXPECT_EQ(activated.count(), 1);

    QTest::keyClick(&view, Qt::Key_Right);
    EXPECT_EQ(view.currentRow(), 2);
    EXPECT_FALSE(topEdge() == unfocused);

    // A click must clear the ring even when the chart already owns focus.
    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_TRUE(view.hasFocus());
    EXPECT_TRUE(topEdge() == unfocused);
    EXPECT_EQ(view.currentRow(), 1);
    EXPECT_EQ(activated.count(), 2);

    focusSink.setFocus(Qt::OtherFocusReason);
    QTest::keyClick(&focusSink, Qt::Key_Tab);
    ASSERT_TRUE(QTest::qWaitFor([&] { return view.hasFocus(); }, 1000));
    EXPECT_FALSE(topEdge() == unfocused);
    QTest::keyClick(&view, Qt::Key_Tab, Qt::ShiftModifier);
    ASSERT_TRUE(QTest::qWaitFor([&] { return focusSink.hasFocus(); }, 1000));
    EXPECT_TRUE(topEdge() == unfocused);
}

TEST(ChartViewTest, Contract_EscapeResetsZoomThenReachesParentDialog)
{
    // Exercise Qt's native child-to-dialog key propagation as well as Fluent below.
    QDialog dialog;
    LineChart view(&dialog);
    dialog.resize(640, 420);
    view.setGeometry(dialog.rect());
    QSignalSpy rejected(&dialog, &QDialog::rejected);
    dialog.show();
    view.setFocus();
    QApplication::processEvents();

    QTest::keyClick(&view, Qt::Key_Escape);
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(rejected.count(), 1);

    dialog.show();
    view.setFocus();
    view.setXRange(10, 20);
    QSignalSpy ranges(&view, &ChartView::xRangeChanged);
    QApplication::processEvents();
    QTest::keyClick(&view, Qt::Key_Escape);
    EXPECT_TRUE(view.isAutoRange());
    EXPECT_EQ(ranges.count(), 1);
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_EQ(rejected.count(), 1);

    QTest::keyClick(&view, Qt::Key_Escape);
    EXPECT_FALSE(dialog.isVisible());
    EXPECT_EQ(rejected.count(), 2);
    EXPECT_EQ(ranges.count(), 1);
}

TEST(ChartViewTest, Contract_RadialEscapeDoesNotConsumeAnIgnoredXRange)
{
    for (auto type : {ChartView::Pie, ChartView::Donut}) {
        SCOPED_TRACE(type);
        QDialog dialog;
        ChartView view(&dialog);
        view.setChartType(type);
        view.setXRange(10, 20);
        dialog.resize(640, 420);
        view.setGeometry(dialog.rect());
        QSignalSpy ranges(&view, &ChartView::xRangeChanged);
        dialog.show();
        view.setFocus();
        QApplication::processEvents();
        QTest::keyClick(&view, Qt::Key_Escape);
        EXPECT_FALSE(dialog.isVisible());
        EXPECT_FALSE(view.isAutoRange());
        EXPECT_EQ(ranges.count(), 0);
    }
}

TEST(ChartViewTest, Contract_EscapeRespectsFluentDialogClosePolicy)
{
    using fluent::dialogs_flyouts::Dialog;
    QWidget window;
    window.resize(800, 600);
    Dialog dialog(&window);
    dialog.setAnimationEnabled(false);
    dialog.setClosePolicy(Dialog::NoAutoClose);
    dialog.resize(640, 420);
    LineChart view(&dialog);
    view.setGeometry(24, 24, 592, 372);
    window.show();
    dialog.open();
    view.setFocus();
    QApplication::processEvents();
    QSignalSpy closed(&dialog, &Dialog::closed);

    QTest::keyClick(&view, Qt::Key_Escape);
    EXPECT_TRUE(dialog.isOpen());
    EXPECT_EQ(closed.count(), 0);

    dialog.setClosePolicy(Dialog::CloseOnEscape);
    view.setXRange(10, 20);
    QTest::keyClick(&view, Qt::Key_Escape);
    EXPECT_TRUE(view.isAutoRange());
    EXPECT_TRUE(dialog.isOpen());
    EXPECT_EQ(closed.count(), 0);

    QTest::keyClick(&view, Qt::Key_Escape);
    EXPECT_FALSE(dialog.isOpen());
    EXPECT_EQ(closed.count(), 1);
}

TEST(ChartViewTest, Contract_AccessibilityAndKeyboardExposeRawData)
{
    ChartModel model;
    model.setName("Requests");
    model.setPoints({{0, 2}, {1, 5}, {2, 9}});
    ChartView view;
    view.setModel(&model);
    view.setAccessibleName("Usage history");
    auto* accessible = QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->role(), QAccessible::Chart);
    EXPECT_EQ(accessible->text(QAccessible::Name), "Usage history");
    EXPECT_TRUE(accessible->text(QAccessible::Description).contains("3"));
    QSignalSpy changes(&view, &ChartView::currentPointChanged);
    QTest::keyClick(&view, Qt::Key_Right);
    EXPECT_EQ(view.currentRow(), 0);
    QTest::keyClick(&view, Qt::Key_Right);
    EXPECT_EQ(view.currentRow(), 1);
    EXPECT_TRUE(accessible->text(QAccessible::Value).contains("5"));
    view.setCurrentPoint(0, 1);
    EXPECT_EQ(changes.count(), 2);
    QSignalSpy activated(&view, &ChartView::rangeActivated);
    QTest::keyClick(&view, Qt::Key_Return);
    ASSERT_EQ(activated.count(), 1);
    EXPECT_EQ(activated.first()[1].toInt(), 1);
    EXPECT_EQ(activated.first()[2].toInt(), 2);
    QTest::keyClick(&view, Qt::Key_End);
    EXPECT_EQ(view.currentRow(), 2);
    QPointer<ChartView> dying = new ChartView;
    dying->setModel(&model);
    QObject::connect(dying, &ChartView::currentPointChanged, dying, [dying] { delete dying; });
    dying->setCurrentPoint(0, 0);
    EXPECT_TRUE(dying.isNull());
}

TEST(ChartViewTest, Contract_DataInkAndLegendKeepContrastOnNeutralSurfaces)
{
    const auto oldTheme = fluent::FluentElement::currentTheme();
    const auto luminance = [](const QColor& c) {
        const auto linear = [](double v) {
            return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
        };
        return 0.2126 * linear(c.redF()) + 0.7152 * linear(c.greenF()) + 0.0722 * linear(c.blueF());
    };
    for (auto theme : {fluent::FluentElement::Light, fluent::FluentElement::Dark,
                       fluent::FluentElement::HighContrast}) {
        fluent::FluentElement::setTheme(theme);
        ChartModel model;
        QVector<QPointF> points;
        for (int i = 0; i < 12; ++i)
            points.append({double(i), 1});
        model.setPoints(points);
        ChartView view;
        view.setModel(&model);
        view.setChartType(ChartView::Donut);
        view.resize(960, 400);
        const auto background = view.themeColorsRef().bgLayerAlt;
        auto palette = view.palette();
        palette.setColor(QPalette::Window, background);
        view.setPalette(palette);
        view.setAutoFillBackground(true);
        const auto image = view.grab().toImage();
        const double dpr = image.devicePixelRatio();
        const auto pixel = [&](QPointF p) {
            return image.pixelColor(qRound(p.x() * dpr), qRound(p.y() * dpr));
        };
        const auto plot = view.plotRect();
        const double radius = std::min(plot.width(), plot.height()) * 0.43;
        const auto legendRect = view.legendRect();
        const double cell = std::min(36.0, (legendRect.height() - 28) / 12);
        for (int i = 0; i < 12; ++i) {
            const QColor legend =
                pixel({legendRect.left() + 4, legendRect.top() + 28 + i * cell + 9});
            const double fg = luminance(legend) + 0.05, bg = luminance(background) + 0.05;
            EXPECT_GE(std::max(fg, bg) / std::min(fg, bg), 3.0)
                << "theme=" << int(theme) << " slice=" << i;
            const double angle = (90 - (i + 0.5) * 30) * 3.141592653589793 / 180;
            const auto ink =
                pixel(plot.center() + QPointF(std::cos(angle) * radius, -std::sin(angle) * radius));
            EXPECT_EQ(legend.rgb(), ink.rgb());
        }
    }
    fluent::FluentElement::setTheme(oldTheme);
}

TEST(ChartViewTest, Contract_DenseExtremaRasterizationAtTwoX)
{
    ChartModel model;
    model.setPoints(wave(1000000));
    ChartView view;
    view.setModel(&model);
    view.resize(1000, 400);
    // A 2x backing store catches pathological Qt path stroking/filling that
    // a source-read budget alone cannot detect. Keep a generous stall limit.
    QImage image(2000, 800, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(2);
    for (auto type : {ChartView::Line, ChartView::Area}) {
        view.setChartType(type);
        image.fill(Qt::white);
        QElapsedTimer timer;
        timer.start();
        {
            QPainter painter(&image);
            view.render(&painter);
        }
        const auto elapsed = timer.elapsed();
        ::testing::Test::RecordProperty(
            type == ChartView::Line ? "dense_line_2x_ms" : "dense_area_2x_ms", elapsed);
        EXPECT_LT(elapsed, 1000) << "Dense projection must not stall for a second";
        EXPECT_GT(view.renderedPointCount(), 0);
        EXPECT_LE(view.renderedPointCount(), view.maximumPointCount());
    }
}

TEST(ChartViewTest, Contract_MissingValuesKeepIsolatedSamplesVisible)
{
    ChartModel model;
    model.setPoints({{0, 3}, {1, std::numeric_limits<double>::quiet_NaN()}, {2, 4}});
    ChartView view;
    view.setModel(&model);
    view.setChartType(ChartView::Sparkline);
    view.resize(240, 120);
    auto palette = view.palette();
    palette.setColor(QPalette::Window, Qt::white);
    view.setPalette(palette);
    view.setAutoFillBackground(true);
    const auto image = view.grab().toImage();
    const double scale = image.devicePixelRatio();
    EXPECT_EQ(view.renderedPointCount(), 2);
    EXPECT_NE(image.pixelColor(qRound(5 * scale), qRound(115 * scale)), QColor(Qt::white));
    EXPECT_NE(image.pixelColor(qRound(235 * scale), qRound(5 * scale)), QColor(Qt::white));
    EXPECT_EQ(image.pixelColor(qRound(120 * scale), qRound(60 * scale)), QColor(Qt::white));
}

TEST(ChartViewTest, VisualCheck)
{
    if (qEnvironmentVariableIsSet("SKIP_VISUAL_TEST"))
        GTEST_SKIP();
    ChartView view;
    ChartModel model;
    model.setName("Sample signal");
    model.setPoints(wave(1000000));
    view.setModel(&model);
    view.setTitle("One million points · sample data");
    view.resize(1000, 440);
    view.show();
    if (tests::support::shouldCaptureVisualSnapshot()) {
        EXPECT_TRUE(tests::support::captureVisualSnapshot(&view));
        return;
    }
    qApp->exec();
}

TEST(ChartViewTest, Contract_DedicatedComponentsKeepTypeAndBorrowOneSharedModel)
{
    ChartModel model;
    model.setPoints(wave(1000000));
    std::vector<std::unique_ptr<ChartView>> views;
    views.emplace_back(new LineChart);
    views.emplace_back(new AreaChart);
    views.emplace_back(new BarChart);
    views.emplace_back(new HorizontalBarChart);
    views.emplace_back(new PieChart);
    views.emplace_back(new DonutChart);
    views.emplace_back(new ScatterChart);
    views.emplace_back(new Sparkline);
    for (int i = 0; i < int(views.size()); ++i) {
        auto& view = *views[i];
        view.setModel(&model);
        EXPECT_EQ(view.chartType(), ChartView::ChartType(i));
        view.setChartType(ChartView::ChartType((i + 1) % 8));
        EXPECT_EQ(view.chartType(), ChartView::ChartType(i));
        view.resize(676, 420);
        view.grab();
        EXPECT_GT(view.renderedPointCount(), 0);
        EXPECT_LE(view.renderedPointCount(), view.maximumPointCount());
        auto* accessible = QAccessible::queryAccessibleInterface(&view);
        ASSERT_NE(accessible, nullptr);
        EXPECT_EQ(accessible->role(), QAccessible::Chart);
    }
    EXPECT_EQ(model.parent(), nullptr);
}

TEST(ChartViewTest, Contract_PercentageTracksAndResponsiveRadialLegend)
{
    ChartModel model;
    model.setPoints({{0, 48.2}, {1, 26.8}, {2, 16.4}, {3, 8.6}},
                    {"Direct", "Search", "Referral", "Campaign"});
    HorizontalBarChart bar;
    bar.setModel(&model);
    bar.setYRange(0, 100);
    bar.setValueSuffix("%");
    bar.resize(676, 420);
    const auto image = bar.grab().toImage();
    const auto plot = bar.plotRect();
    const auto pixel = [&](double fraction) {
        const QPointF p(plot.left() + plot.width() * fraction, plot.top() + plot.height() / 8);
        return image.pixelColor(qRound(p.x() * image.devicePixelRatio()),
                                qRound(p.y() * image.devicePixelRatio()));
    };
    EXPECT_EQ(pixel(.1), pixel(.45));
    EXPECT_NE(pixel(.45), pixel(.6));
    QSignalSpy range(&bar, &ChartView::yRangeChanged);
    bar.setYRange(0, 100);
    bar.setYRange(100, 0);
    EXPECT_EQ(range.count(), 0);
    EXPECT_EQ(bar.minimumY(), 0);
    EXPECT_EQ(bar.maximumY(), 100);
    DonutChart donut;
    donut.setModel(&model);
    donut.setTitle("Traffic sources");
    donut.setSubtitle("Share of sessions");
    donut.resize(676, 420);
    donut.grab();
    EXPECT_GT(donut.legendRect().left(), donut.plotRect().right());
    donut.resize(360, 620);
    donut.grab();
    EXPECT_GT(donut.legendRect().top(), donut.plotRect().bottom());
    EXPECT_TRUE(QRectF(donut.rect()).contains(donut.legendRect()));
    QSignalSpy loading(&donut, &ChartView::loadingChanged);
    donut.setLoading(true);
    donut.setLoading(true);
    donut.grab();
    EXPECT_EQ(loading.count(), 1);
    EXPECT_EQ(donut.renderedPointCount(), 0);
    donut.setLoading(false);
    donut.grab();
    EXPECT_EQ(donut.renderedPointCount(), 4);
}
