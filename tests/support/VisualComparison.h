#ifndef FLUENT_QT_VISUAL_COMPARISON_H
#define FLUENT_QT_VISUAL_COMPARISON_H

#include <QImage>
#include <QJsonObject>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include <optional>

namespace tests::support {

struct VisualComparisonPolicy {
    int channelThreshold = 0;
    std::optional<qint64> maxDifferentPixels = qint64(0);
    std::optional<double> maxDifferentRatio;
    int translationSearchRadius = 4;
    std::optional<int> maxTranslation = 0;
    int edgeThreshold = 12;
};

struct VisualTranslationEstimate {
    QPoint offset;
    bool confident = false;
    int baselineEdgePixels = 0;
    int actualEdgePixels = 0;
    double zeroOffsetScore = 0.0;
    double bestScore = 0.0;
    double improvement = 0.0;
};

struct VisualComparisonMetrics {
    QSize baselineSize;
    QSize actualSize;
    qint64 totalPixels = 0;
    qint64 differentPixels = 0;
    double differentRatio = 0.0;
    int maxChannelDelta = 0;
    double meanMaxChannelDelta = 0.0;
    QRect differenceBounds;
    VisualTranslationEstimate translation;
};

struct VisualComparisonResult {
    bool valid = false;
    bool sizeMatches = false;
    bool pixelLimitsPass = false;
    bool translationLimitPass = false;
    bool passed = false;
    QString error;
    VisualComparisonPolicy policy;
    VisualComparisonMetrics metrics;
};

VisualComparisonResult analyzeVisualDifference(
    const QImage& baseline,
    const QImage& actual,
    const VisualComparisonPolicy& policy = VisualComparisonPolicy{});

QImage renderVisualDifference(
    const QImage& baseline,
    const QImage& actual,
    int channelThreshold = 0);

QJsonObject visualComparisonReport(const VisualComparisonResult& result);
QString visualComparisonSummary(const VisualComparisonResult& result);

} // namespace tests::support

#endif // FLUENT_QT_VISUAL_COMPARISON_H
