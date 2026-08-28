#include "VisualComparison.h"

#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QProcess>
#include <QTemporaryDir>

#include <gtest/gtest.h>

#ifndef FLUENT_QT_VISUAL_COMPARE_PATH
#define FLUENT_QT_VISUAL_COMPARE_PATH ""
#endif

namespace {

QImage rectangleImage(int offsetX = 0)
{
    QImage image(64, 40, QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.fillRect(QRect(12 + offsetX, 10, 28, 18), Qt::black);
    return image;
}

TEST(VisualComparisonTest, ExactImagesPassStrictPolicy)
{
    const QImage baseline = rectangleImage();
    const auto result = tests::support::analyzeVisualDifference(baseline, baseline);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.sizeMatches);
    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.metrics.differentPixels, 0);
    EXPECT_FALSE(result.metrics.differenceBounds.isValid());
    EXPECT_EQ(result.metrics.translation.baselineEdgePixels, 0);
}

TEST(VisualComparisonTest, SingleChangedPixelFailsAndReportsTightBounds)
{
    QImage baseline(24, 16, QImage::Format_ARGB32);
    baseline.fill(qRgb(100, 100, 100));
    QImage actual = baseline;
    actual.setPixel(7, 9, qRgb(101, 100, 100));

    const auto result = tests::support::analyzeVisualDifference(baseline, actual);

    EXPECT_FALSE(result.passed);
    EXPECT_EQ(result.metrics.differentPixels, 1);
    EXPECT_EQ(result.metrics.maxChannelDelta, 1);
    EXPECT_EQ(result.metrics.differenceBounds, QRect(7, 9, 1, 1));
}

TEST(VisualComparisonTest, ChannelThresholdCanIgnoreKnownColorNoise)
{
    QImage baseline(24, 16, QImage::Format_ARGB32);
    baseline.fill(qRgb(100, 100, 100));
    QImage actual = baseline;
    actual.setPixel(7, 9, qRgb(101, 100, 100));

    tests::support::VisualComparisonPolicy policy;
    policy.channelThreshold = 1;
    const auto result =
        tests::support::analyzeVisualDifference(baseline, actual, policy);

    EXPECT_TRUE(result.passed);
    EXPECT_EQ(result.metrics.differentPixels, 0);
    EXPECT_EQ(result.metrics.maxChannelDelta, 1);
}

TEST(VisualComparisonTest, DetectsTwoPixelPaintedGeometryTranslation)
{
    tests::support::VisualComparisonPolicy policy;
    policy.maxDifferentPixels.reset();
    policy.maxDifferentRatio = 1.0;
    policy.maxTranslation = 0;
    policy.translationSearchRadius = 4;

    const auto result = tests::support::analyzeVisualDifference(
        rectangleImage(), rectangleImage(2), policy);

    EXPECT_TRUE(result.metrics.translation.confident);
    EXPECT_EQ(result.metrics.translation.offset, QPoint(2, 0));
    EXPECT_FALSE(result.translationLimitPass);
    EXPECT_FALSE(result.passed);
}

TEST(VisualComparisonTest, SizeMismatchIsAComparisonFailure)
{
    QImage baseline(24, 16, QImage::Format_ARGB32);
    baseline.fill(Qt::white);
    QImage actual(25, 16, QImage::Format_ARGB32);
    actual.fill(Qt::white);

    const auto result = tests::support::analyzeVisualDifference(baseline, actual);

    EXPECT_TRUE(result.valid);
    EXPECT_FALSE(result.sizeMatches);
    EXPECT_FALSE(result.passed);
    EXPECT_TRUE(result.error.contains(QStringLiteral("dimensions")));
}

TEST(VisualComparisonTest, DifferenceRendererPreservesNativeResolution)
{
    const QImage baseline = rectangleImage();
    const QImage actual = rectangleImage(1);
    const QImage difference =
        tests::support::renderVisualDifference(baseline, actual);

    EXPECT_FALSE(difference.isNull());
    EXPECT_EQ(difference.size(), baseline.size());
    EXPECT_GT(qRed(difference.pixel(12, 12)),
              qGreen(difference.pixel(12, 12)));
}

TEST(VisualComparisonCliTest, WritesMachineReadableFailureEvidence)
{
    QTemporaryDir temporary;
    ASSERT_TRUE(temporary.isValid());
    const QString baselinePath = temporary.filePath(QStringLiteral("baseline.png"));
    const QString actualPath = temporary.filePath(QStringLiteral("actual.png"));
    const QString reportPath =
        temporary.filePath(QStringLiteral("evidence/report.json"));
    const QString diffPath =
        temporary.filePath(QStringLiteral("evidence/diff.png"));
    ASSERT_TRUE(rectangleImage().save(baselinePath, "PNG"));
    ASSERT_TRUE(rectangleImage(1).save(actualPath, "PNG"));

    QProcess process;
    process.setProgram(QString::fromUtf8(FLUENT_QT_VISUAL_COMPARE_PATH));
    process.setArguments({QStringLiteral("--baseline"), baselinePath,
                          QStringLiteral("--actual"), actualPath,
                          QStringLiteral("--report"), reportPath,
                          QStringLiteral("--diff"), diffPath,
                          QStringLiteral("--quiet")});
    process.start();
    ASSERT_TRUE(process.waitForStarted(10000))
        << process.errorString().toStdString();
    ASSERT_TRUE(process.waitForFinished(30000))
        << process.errorString().toStdString();
    EXPECT_EQ(process.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(process.exitCode(), 1)
        << process.readAllStandardError().toStdString();

    QFile reportFile(reportPath);
    ASSERT_TRUE(reportFile.open(QIODevice::ReadOnly));
    const QJsonDocument document = QJsonDocument::fromJson(reportFile.readAll());
    ASSERT_TRUE(document.isObject());
    const QJsonObject report = document.object();
    EXPECT_EQ(report.value(QStringLiteral("status")).toString(),
              QStringLiteral("fail"));
    EXPECT_GT(report.value(QStringLiteral("metrics"))
                  .toObject()
                  .value(QStringLiteral("different_pixels"))
                  .toDouble(),
              0.0);
    EXPECT_TRUE(QFileInfo(diffPath).isFile());
}

} // namespace
