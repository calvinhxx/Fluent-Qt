#include <gtest/gtest.h>
#include <QAbstractItemModelTester>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <future>
#include <cmath>
#include <limits>
#include "components/charts/ChartModel.h"

using namespace fluent::charts;

TEST(ChartModelTest, Contract_TableSignalsValidationAndCallerSnapshots)
{
    ChartModel model;
    QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::Fatal);
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy inserts(&model, &QAbstractItemModel::rowsInserted);
    EXPECT_TRUE(model.setPoints({{0, 2}, {1, 4}}, {"A", "B"}));
    EXPECT_EQ(resets.count(), 1);
    EXPECT_EQ(model.data(model.index(1, 1)).toDouble(), 4);
    EXPECT_EQ(model.headerData(0, Qt::Vertical).toString(), "A");
    const auto snapshot = model.dataSnapshot();
    EXPECT_FALSE(model.setPoints({{2, 4}, {1, 3}}));
    EXPECT_FALSE(model.appendPoints({{0, 9}}));
    EXPECT_FALSE(model.setPoints({{0, 1}}, {"A", "B"}));
    EXPECT_EQ(model.rowCount(), 2);
    EXPECT_EQ(resets.count(), 1);
    EXPECT_TRUE(model.appendPoints({{2, -8}, {3, 12}}, {"C", "D"}));
    EXPECT_EQ(inserts.count(), 1);
    EXPECT_EQ(inserts.first()[1].toInt(), 2);
    EXPECT_EQ(inserts.first()[2].toInt(), 3);
    EXPECT_EQ(snapshot.size(), 2);
    EXPECT_EQ(snapshot.maximumY(), 4);
    EXPECT_EQ(model.minimumY(), -8);
    EXPECT_EQ(model.maximumY(), 12);
    QSignalSpy names(&model, &ChartModel::nameChanged);
    model.setName("Requests");
    model.setName("Requests");
    EXPECT_EQ(names.count(), 1);
    EXPECT_EQ(model.headerData(1, Qt::Horizontal).toString(), "Requests");
}

TEST(ChartModelTest, Contract_MillionPointsKeepPeaksAndBoundEveryProjection)
{
    auto future = std::async(std::launch::async, [] {
        QVector<QPointF> points;
        points.reserve(1000000);
        for (int i = 0; i < 1000000; ++i)
            points.append({double(i), std::sin(i * 0.01)});
        points[500123].setY(10000);
        points[700012].setY(-9000);
        return ChartData::fromPoints(points);
    });
    const auto snapshot = future.get();
    ASSERT_TRUE(snapshot.isValid());
    ChartModel model;
    ASSERT_TRUE(model.setDataSnapshot(snapshot));
    QElapsedTimer timer;
    timer.start();
    for (int budget : {16, 32, 100, 1000, 4096}) {
        const auto rows = model.sampledRows(0, model.rowCount(), budget);
        EXPECT_LE(rows.size(), budget);
        EXPECT_TRUE(rows.contains(500123));
        EXPECT_TRUE(rows.contains(700012));
        EXPECT_EQ(rows.first(), 0);
        EXPECT_EQ(rows.last(), 999999);
    }
    const auto rows = model.sampledRows(model.lowerBound(500000), model.upperBound(501000), 200);
    EXPECT_TRUE(rows.contains(500123));
    EXPECT_FALSE(rows.contains(700012));
    for (int row : rows)
        EXPECT_TRUE(row >= 500000 && row <= 501000);
    ::testing::Test::RecordProperty("million_point_queries_us", timer.nsecsElapsed() / 1000);
}

TEST(ChartModelTest, Contract_AppendUpdatesIndexAcrossBlockAndTreeBoundaries)
{
    ChartModel model;
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy inserts(&model, &QAbstractItemModel::rowsInserted);
    for (int batch = 0; batch < 160; ++batch) {
        QVector<QPointF> points;
        for (int j = 0; j < 73; ++j) {
            const int row = batch * 73 + j;
            points.append({double(row), double(row % 2 ? -row : row)});
        }
        ASSERT_TRUE(model.appendPoints(points));
        const auto all = model.dataSnapshot();
        const auto sampled = all.sampledRows(0, all.size(), 64);
        EXPECT_TRUE(sampled.contains(all.size() - 1));
        EXPECT_DOUBLE_EQ(all.maximumY(), all.size() % 2 ? all.size() - 1 : all.size() - 2);
        EXPECT_DOUBLE_EQ(all.minimumY(), all.size() % 2 ? -(all.size() - 2) : -(all.size() - 1));
    }
    EXPECT_EQ(resets.count(), 0);
    EXPECT_EQ(inserts.count(), 160);
}

TEST(ChartModelTest, Contract_GapsExtremeValuesAndAggregates)
{
    QVector<QPointF> points;
    for (int i = 0; i < 10000; ++i)
        points.append(
            {double(i), i % 31 ? double(i % 7) : std::numeric_limits<double>::quiet_NaN()});
    const auto data = ChartData::fromPoints(points);
    for (int budget : {16, 24, 32, 128, 1024}) {
        const auto rows = data.sampledRows(0, data.size(), budget);
        EXPECT_LE(rows.size(), budget);
        EXPECT_TRUE(rows.contains(-1));
    }
    const auto huge = ChartData::fromPoints({{0, 1e308}, {1, 1e308}, {2, -1e308}});
    EXPECT_NEAR(huge.mean(0, 3) / 1e308, 1.0 / 3, 1e-12);
    EXPECT_DOUBLE_EQ(huge.positiveFraction(0, 1), 0.5);
    EXPECT_DOUBLE_EQ(huge.positiveFraction(2, 3), 0);
    EXPECT_TRUE(std::isnan(data.mean(0, 0)));
    EXPECT_FALSE(ChartData::fromPoints({{std::numeric_limits<double>::infinity(), 2}}).isValid());
    EXPECT_FALSE(ChartData::fromPoints({{0, std::numeric_limits<double>::infinity()}}).isValid());
}

TEST(ChartModelTest, Contract_ReentrantMutationsCannotNestQtModelTransactions)
{
    ChartModel model;
    bool nested = true;
    QObject::connect(&model, &QAbstractItemModel::rowsAboutToBeInserted, &model, [&] {
        nested = model.appendPoints({{1, 3}});
    });
    ASSERT_TRUE(model.appendPoints({{0, 2}}));
    EXPECT_FALSE(nested);
    EXPECT_EQ(model.rowCount(), 1);
    QObject::connect(&model, &QAbstractItemModel::modelAboutToBeReset, &model, [&] {
        nested = model.setPoints({{1, 7}});
    });
    ASSERT_TRUE(model.setPoints({{2, 4}}));
    EXPECT_FALSE(nested);
    EXPECT_EQ(model.pointAt(0), QPointF(2, 4));
    QSignalSpy headers(&model, &QAbstractItemModel::headerDataChanged);
    model.setName("Values");
    model.setName("Values");
    EXPECT_EQ(headers.count(), 1);
    EXPECT_EQ(model.headerData(1, Qt::Horizontal).toString(), "Values");
}
