#include "VisualComparison.h"

#include <QColor>
#include <QJsonValue>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <limits>

namespace tests::support {
namespace {

QImage normalizedImage(const QImage& image)
{
    if (image.isNull() || image.format() == QImage::Format_ARGB32)
        return image;
    return image.convertToFormat(QImage::Format_ARGB32);
}

int channelDelta(QRgb left, QRgb right)
{
    return std::max({std::abs(qRed(left) - qRed(right)),
                     std::abs(qGreen(left) - qGreen(right)),
                     std::abs(qBlue(left) - qBlue(right)),
                     std::abs(qAlpha(left) - qAlpha(right))});
}

int luminance(QRgb pixel)
{
    return (77 * qRed(pixel) + 150 * qGreen(pixel) + 29 * qBlue(pixel)) >> 8;
}

QVector<quint8> edgeMap(const QImage& image, int threshold, int& edgePixels)
{
    const int width = image.width();
    const int height = image.height();
    QVector<quint8> edges(width * height, 0);
    edgePixels = 0;

    for (int y = 0; y < height; ++y) {
        const auto* line = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        const auto* previous = y > 0
            ? reinterpret_cast<const QRgb*>(image.constScanLine(y - 1))
            : nullptr;
        for (int x = 0; x < width; ++x) {
            const QRgb pixel = line[x];
            int gradient = 0;
            if (x > 0) {
                gradient = std::max(
                    std::abs(luminance(pixel) - luminance(line[x - 1])),
                    std::abs(qAlpha(pixel) - qAlpha(line[x - 1])));
            }
            if (previous) {
                gradient = std::max(
                    gradient,
                    std::max(
                        std::abs(luminance(pixel) - luminance(previous[x])),
                        std::abs(qAlpha(pixel) - qAlpha(previous[x]))));
            }
            if (gradient >= threshold) {
                edges[y * width + x] = 1;
                ++edgePixels;
            }
        }
    }
    return edges;
}

double edgeAlignmentScore(const QVector<quint8>& baselineEdges,
                          const QVector<quint8>& actualEdges,
                          int width,
                          int height,
                          int baselineEdgePixels,
                          int actualEdgePixels,
                          int dx,
                          int dy)
{
    const int denominator = baselineEdgePixels + actualEdgePixels;
    if (denominator == 0)
        return 0.0;

    int intersection = 0;
    const int left = std::max(0, -dx);
    const int right = std::min(width, width - dx);
    const int top = std::max(0, -dy);
    const int bottom = std::min(height, height - dy);
    for (int y = top; y < bottom; ++y) {
        const int actualY = y + dy;
        for (int x = left; x < right; ++x) {
            if (baselineEdges[y * width + x]
                && actualEdges[actualY * width + x + dx]) {
                ++intersection;
            }
        }
    }
    return (2.0 * intersection) / denominator;
}

VisualTranslationEstimate estimateTranslation(const QImage& baseline,
                                               const QImage& actual,
                                               const VisualComparisonPolicy& policy)
{
    VisualTranslationEstimate estimate;
    if (policy.translationSearchRadius <= 0 || baseline.size() != actual.size())
        return estimate;

    const QVector<quint8> baselineEdges =
        edgeMap(baseline, policy.edgeThreshold, estimate.baselineEdgePixels);
    const QVector<quint8> actualEdges =
        edgeMap(actual, policy.edgeThreshold, estimate.actualEdgePixels);
    if (estimate.baselineEdgePixels + estimate.actualEdgePixels < 8)
        return estimate;

    estimate.zeroOffsetScore = edgeAlignmentScore(
        baselineEdges, actualEdges, baseline.width(), baseline.height(),
        estimate.baselineEdgePixels, estimate.actualEdgePixels, 0, 0);
    estimate.bestScore = estimate.zeroOffsetScore;

    constexpr double kTieEpsilon = 1e-9;
    for (int dy = -policy.translationSearchRadius;
         dy <= policy.translationSearchRadius; ++dy) {
        for (int dx = -policy.translationSearchRadius;
             dx <= policy.translationSearchRadius; ++dx) {
            const double score = edgeAlignmentScore(
                baselineEdges, actualEdges, baseline.width(), baseline.height(),
                estimate.baselineEdgePixels, estimate.actualEdgePixels, dx, dy);
            const int candidateDistance = std::max(std::abs(dx), std::abs(dy));
            const int bestDistance = std::max(
                std::abs(estimate.offset.x()), std::abs(estimate.offset.y()));
            if (score > estimate.bestScore + kTieEpsilon
                || (std::abs(score - estimate.bestScore) <= kTieEpsilon
                    && candidateDistance < bestDistance)) {
                estimate.bestScore = score;
                estimate.offset = QPoint(dx, dy);
            }
        }
    }

    estimate.improvement = estimate.bestScore - estimate.zeroOffsetScore;
    estimate.confident = !estimate.offset.isNull()
        && estimate.bestScore >= 0.5
        && estimate.improvement >= 0.02;
    return estimate;
}

QJsonValue optionalInteger(const std::optional<qint64>& value)
{
    return value ? QJsonValue(static_cast<double>(*value)) : QJsonValue(QJsonValue::Null);
}

QJsonValue optionalInteger(const std::optional<int>& value)
{
    return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
}

QJsonValue optionalRatio(const std::optional<double>& value)
{
    return value ? QJsonValue(*value) : QJsonValue(QJsonValue::Null);
}

QJsonObject sizeObject(const QSize& size)
{
    return {{QStringLiteral("width"), size.width()},
            {QStringLiteral("height"), size.height()}};
}

QJsonValue boundsValue(const QRect& bounds)
{
    if (!bounds.isValid())
        return QJsonValue(QJsonValue::Null);
    return QJsonObject{{QStringLiteral("x"), bounds.x()},
                       {QStringLiteral("y"), bounds.y()},
                       {QStringLiteral("width"), bounds.width()},
                       {QStringLiteral("height"), bounds.height()}};
}

} // namespace

VisualComparisonResult analyzeVisualDifference(const QImage& baseline,
                                               const QImage& actual,
                                               const VisualComparisonPolicy& policy)
{
    VisualComparisonResult result;
    result.policy = policy;
    result.metrics.baselineSize = baseline.size();
    result.metrics.actualSize = actual.size();

    if (baseline.isNull() || actual.isNull()) {
        result.error = baseline.isNull()
            ? QStringLiteral("Baseline image is null.")
            : QStringLiteral("Actual image is null.");
        return result;
    }
    if (policy.channelThreshold < 0 || policy.channelThreshold > 255
        || policy.translationSearchRadius < 0
        || policy.edgeThreshold < 1 || policy.edgeThreshold > 255
        || (policy.maxDifferentPixels && *policy.maxDifferentPixels < 0)
        || (policy.maxDifferentRatio
            && (*policy.maxDifferentRatio < 0.0 || *policy.maxDifferentRatio > 1.0))
        || (policy.maxTranslation && *policy.maxTranslation < 0)) {
        result.error = QStringLiteral("Visual comparison policy is outside its valid range.");
        return result;
    }

    result.valid = true;
    result.sizeMatches = baseline.size() == actual.size();
    if (!result.sizeMatches) {
        result.error = QStringLiteral("Image dimensions differ.");
        return result;
    }

    const QImage baselineArgb = normalizedImage(baseline);
    const QImage actualArgb = normalizedImage(actual);
    result.metrics.totalPixels =
        static_cast<qint64>(baselineArgb.width()) * baselineArgb.height();

    qint64 summedMaxChannelDelta = 0;
    int minX = baselineArgb.width();
    int minY = baselineArgb.height();
    int maxX = -1;
    int maxY = -1;
    for (int y = 0; y < baselineArgb.height(); ++y) {
        const auto* baselineLine =
            reinterpret_cast<const QRgb*>(baselineArgb.constScanLine(y));
        const auto* actualLine =
            reinterpret_cast<const QRgb*>(actualArgb.constScanLine(y));
        for (int x = 0; x < baselineArgb.width(); ++x) {
            const int delta = channelDelta(baselineLine[x], actualLine[x]);
            summedMaxChannelDelta += delta;
            result.metrics.maxChannelDelta =
                std::max(result.metrics.maxChannelDelta, delta);
            if (delta <= policy.channelThreshold)
                continue;
            ++result.metrics.differentPixels;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (result.metrics.totalPixels > 0) {
        result.metrics.differentRatio =
            static_cast<double>(result.metrics.differentPixels)
            / static_cast<double>(result.metrics.totalPixels);
        result.metrics.meanMaxChannelDelta =
            static_cast<double>(summedMaxChannelDelta)
            / static_cast<double>(result.metrics.totalPixels);
    }
    if (maxX >= minX && maxY >= minY) {
        result.metrics.differenceBounds =
            QRect(QPoint(minX, minY), QPoint(maxX, maxY));
    }

    if (result.metrics.differentPixels > 0) {
        result.metrics.translation =
            estimateTranslation(baselineArgb, actualArgb, policy);
    }
    result.pixelLimitsPass = true;
    if (policy.maxDifferentPixels) {
        result.pixelLimitsPass = result.pixelLimitsPass
            && result.metrics.differentPixels <= *policy.maxDifferentPixels;
    }
    if (policy.maxDifferentRatio) {
        result.pixelLimitsPass = result.pixelLimitsPass
            && result.metrics.differentRatio <= *policy.maxDifferentRatio;
    }

    result.translationLimitPass = true;
    if (policy.maxTranslation && result.metrics.translation.confident) {
        const int displacement = std::max(
            std::abs(result.metrics.translation.offset.x()),
            std::abs(result.metrics.translation.offset.y()));
        result.translationLimitPass = displacement <= *policy.maxTranslation;
    }
    result.passed = result.pixelLimitsPass && result.translationLimitPass;
    return result;
}

QImage renderVisualDifference(const QImage& baseline,
                              const QImage& actual,
                              int channelThreshold)
{
    if (baseline.isNull() || actual.isNull() || baseline.size() != actual.size())
        return {};

    const QImage baselineArgb = normalizedImage(baseline);
    const QImage actualArgb = normalizedImage(actual);
    QImage difference(baselineArgb.size(), QImage::Format_ARGB32);
    for (int y = 0; y < baselineArgb.height(); ++y) {
        const auto* baselineLine =
            reinterpret_cast<const QRgb*>(baselineArgb.constScanLine(y));
        const auto* actualLine =
            reinterpret_cast<const QRgb*>(actualArgb.constScanLine(y));
        auto* differenceLine = reinterpret_cast<QRgb*>(difference.scanLine(y));
        for (int x = 0; x < baselineArgb.width(); ++x) {
            const int delta = channelDelta(baselineLine[x], actualLine[x]);
            if (delta <= channelThreshold) {
                const QColor dim = QColor::fromRgba(baselineLine[x]);
                differenceLine[x] = qRgba(
                    dim.red() / 4, dim.green() / 4, dim.blue() / 4, 255);
                continue;
            }
            differenceLine[x] = qRgb(
                std::min(255, 96 + delta * 2),
                std::min(72, delta / 2),
                std::min(72, delta / 2));
        }
    }
    return difference;
}

QJsonObject visualComparisonReport(const VisualComparisonResult& result)
{
    const auto& translation = result.metrics.translation;
    const QJsonObject policy{
        {QStringLiteral("channel_threshold"), result.policy.channelThreshold},
        {QStringLiteral("max_different_pixels"),
         optionalInteger(result.policy.maxDifferentPixels)},
        {QStringLiteral("max_different_ratio"),
         optionalRatio(result.policy.maxDifferentRatio)},
        {QStringLiteral("translation_search_radius"),
         result.policy.translationSearchRadius},
        {QStringLiteral("max_translation"),
         optionalInteger(result.policy.maxTranslation)},
        {QStringLiteral("edge_threshold"), result.policy.edgeThreshold}};
    const QJsonObject translationObject{
        {QStringLiteral("dx"), translation.offset.x()},
        {QStringLiteral("dy"), translation.offset.y()},
        {QStringLiteral("confident"), translation.confident},
        {QStringLiteral("baseline_edge_pixels"), translation.baselineEdgePixels},
        {QStringLiteral("actual_edge_pixels"), translation.actualEdgePixels},
        {QStringLiteral("zero_offset_score"), translation.zeroOffsetScore},
        {QStringLiteral("best_score"), translation.bestScore},
        {QStringLiteral("improvement"), translation.improvement}};
    const QJsonObject metrics{
        {QStringLiteral("total_pixels"),
         static_cast<double>(result.metrics.totalPixels)},
        {QStringLiteral("different_pixels"),
         static_cast<double>(result.metrics.differentPixels)},
        {QStringLiteral("different_ratio"), result.metrics.differentRatio},
        {QStringLiteral("max_channel_delta"), result.metrics.maxChannelDelta},
        {QStringLiteral("mean_max_channel_delta"),
         result.metrics.meanMaxChannelDelta},
        {QStringLiteral("difference_bounds"),
         boundsValue(result.metrics.differenceBounds)},
        {QStringLiteral("estimated_translation"), translationObject}};
    return {
        {QStringLiteral("schema_version"), 1},
        {QStringLiteral("tool"), QStringLiteral("FluentQt Visual Compare")},
        {QStringLiteral("status"),
         !result.valid ? QStringLiteral("error")
                       : result.passed ? QStringLiteral("pass")
                                       : QStringLiteral("fail")},
        {QStringLiteral("error"),
         result.error.isEmpty() ? QJsonValue(QJsonValue::Null)
                                : QJsonValue(result.error)},
        {QStringLiteral("baseline_size"), sizeObject(result.metrics.baselineSize)},
        {QStringLiteral("actual_size"), sizeObject(result.metrics.actualSize)},
        {QStringLiteral("checks"),
         QJsonObject{{QStringLiteral("size_matches"), result.sizeMatches},
                     {QStringLiteral("pixel_limits_pass"), result.pixelLimitsPass},
                     {QStringLiteral("translation_limit_pass"),
                      result.translationLimitPass}}},
        {QStringLiteral("policy"), policy},
        {QStringLiteral("metrics"), metrics}};
}

QString visualComparisonSummary(const VisualComparisonResult& result)
{
    if (!result.valid)
        return result.error;
    if (!result.sizeMatches) {
        return QStringLiteral("Visual snapshot size %1x%2 does not match baseline %3x%4.")
            .arg(result.metrics.actualSize.width())
            .arg(result.metrics.actualSize.height())
            .arg(result.metrics.baselineSize.width())
            .arg(result.metrics.baselineSize.height());
    }
    if (result.passed)
        return QStringLiteral("Visual comparison passed with %1 differing pixel(s).")
            .arg(result.metrics.differentPixels);

    QString summary = QStringLiteral(
        "%1 of %2 pixels differ (ratio %3, max channel delta %4)")
        .arg(result.metrics.differentPixels)
        .arg(result.metrics.totalPixels)
        .arg(result.metrics.differentRatio, 0, 'g', 6)
        .arg(result.metrics.maxChannelDelta);
    if (result.metrics.differenceBounds.isValid()) {
        const QRect bounds = result.metrics.differenceBounds;
        summary += QStringLiteral(", bounds %1,%2 %3x%4")
            .arg(bounds.x())
            .arg(bounds.y())
            .arg(bounds.width())
            .arg(bounds.height());
    }
    if (result.metrics.translation.confident) {
        summary += QStringLiteral(", likely translation dx=%1 dy=%2")
            .arg(result.metrics.translation.offset.x())
            .arg(result.metrics.translation.offset.y());
    }
    return summary + QLatin1Char('.');
}

} // namespace tests::support
