#include "VisualComparison.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>

#include <optional>

namespace {

bool parseInteger(const QString& value, int minimum, int maximum,
                  int& result, QString& error)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok || parsed < minimum || parsed > maximum) {
        error = QStringLiteral("Expected an integer between %1 and %2, got %3.")
            .arg(minimum)
            .arg(maximum)
            .arg(value);
        return false;
    }
    result = parsed;
    return true;
}

bool parseNonNegativeInteger64(const QString& value, qint64& result,
                               QString& error)
{
    bool ok = false;
    const qint64 parsed = value.toLongLong(&ok);
    if (!ok || parsed < 0) {
        error = QStringLiteral("Expected a non-negative integer, got %1.").arg(value);
        return false;
    }
    result = parsed;
    return true;
}

bool parseRatio(const QString& value, double& result, QString& error)
{
    bool ok = false;
    const double parsed = value.toDouble(&ok);
    if (!ok || parsed < 0.0 || parsed > 1.0) {
        error = QStringLiteral("Expected a ratio between 0 and 1, got %1.").arg(value);
        return false;
    }
    result = parsed;
    return true;
}

bool parseRegion(const QString& value, QRect& region, QString& error)
{
    const QStringList parts = value.split(QLatin1Char(','));
    if (parts.size() != 4) {
        error = QStringLiteral("Region must be x,y,width,height.");
        return false;
    }
    int values[4]{};
    for (int index = 0; index < 4; ++index) {
        bool ok = false;
        values[index] = parts[index].trimmed().toInt(&ok);
        if (!ok) {
            error = QStringLiteral("Region must contain four integers.");
            return false;
        }
    }
    if (values[0] < 0 || values[1] < 0 || values[2] <= 0 || values[3] <= 0) {
        error = QStringLiteral("Region origin must be non-negative and size must be positive.");
        return false;
    }
    region = QRect(values[0], values[1], values[2], values[3]);
    return true;
}

bool writeJson(const QString& path, const QJsonObject& object, QString& error)
{
    const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (path == QLatin1String("-")) {
        QTextStream stream(stdout);
        stream << QString::fromUtf8(payload);
        stream.flush();
        return stream.status() == QTextStream::Ok;
    }

    const QFileInfo info(path);
    QDir directory = info.dir();
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        error = QStringLiteral("Could not create report directory %1.")
            .arg(directory.absolutePath());
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("Could not open report %1: %2")
            .arg(path, file.errorString());
        return false;
    }
    if (file.write(payload) != payload.size() || !file.commit()) {
        error = QStringLiteral("Could not write report %1: %2")
            .arg(path, file.errorString());
        return false;
    }
    return true;
}

bool writeImage(const QString& path, const QImage& image, QString& error)
{
    const QFileInfo info(path);
    QDir directory = info.dir();
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        error = QStringLiteral("Could not create image directory %1.")
            .arg(directory.absolutePath());
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        error = QStringLiteral("Could not open diff image %1: %2")
            .arg(path, file.errorString());
        return false;
    }
    if (!image.save(&file, "PNG") || !file.commit()) {
        error = QStringLiteral("Could not write diff image %1: %2")
            .arg(path, file.errorString());
        return false;
    }
    return true;
}

QString fileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

QJsonValue regionValue(const std::optional<QRect>& region)
{
    if (!region)
        return QJsonValue(QJsonValue::Null);
    return QJsonObject{{QStringLiteral("x"), region->x()},
                       {QStringLiteral("y"), region->y()},
                       {QStringLiteral("width"), region->width()},
                       {QStringLiteral("height"), region->height()}};
}

int fail(const QString& message)
{
    QTextStream(stderr) << "fluent_qt_visual_compare: " << message << Qt::endl;
    return 2;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("fluent_qt_visual_compare"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Compare deterministic FluentQt screenshots and emit structured pixel evidence."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption baselineOption(
        QStringList{QStringLiteral("b"), QStringLiteral("baseline")},
        QStringLiteral("Approved baseline PNG."), QStringLiteral("path"));
    const QCommandLineOption actualOption(
        QStringList{QStringLiteral("a"), QStringLiteral("actual")},
        QStringLiteral("Actual PNG to verify."), QStringLiteral("path"));
    const QCommandLineOption reportOption(
        QStringList{QStringLiteral("report")},
        QStringLiteral("Write JSON report; '-' writes stdout."),
        QStringLiteral("path"), QStringLiteral("-"));
    const QCommandLineOption diffOption(
        QStringList{QStringLiteral("diff")},
        QStringLiteral("Write a red heatmap when comparison fails."),
        QStringLiteral("path"));
    const QCommandLineOption channelThresholdOption(
        QStringList{QStringLiteral("channel-threshold")},
        QStringLiteral("Ignore per-pixel max channel deltas at or below N."),
        QStringLiteral("N"), QStringLiteral("0"));
    const QCommandLineOption maxDifferentPixelsOption(
        QStringList{QStringLiteral("max-different-pixels")},
        QStringLiteral("Allow at most N pixels above the channel threshold."),
        QStringLiteral("N"));
    const QCommandLineOption maxDifferentRatioOption(
        QStringList{QStringLiteral("max-different-ratio")},
        QStringLiteral("Allow at most this 0..1 ratio of differing pixels."),
        QStringLiteral("ratio"));
    const QCommandLineOption searchRadiusOption(
        QStringList{QStringLiteral("search-radius")},
        QStringLiteral("Search up to N pixels for a likely geometry translation."),
        QStringLiteral("N"), QStringLiteral("4"));
    const QCommandLineOption maxTranslationOption(
        QStringList{QStringLiteral("max-translation")},
        QStringLiteral("Allow at most N confident translated pixels per axis."),
        QStringLiteral("N"), QStringLiteral("0"));
    const QCommandLineOption edgeThresholdOption(
        QStringList{QStringLiteral("edge-threshold")},
        QStringLiteral("Luminance/alpha edge threshold used for translation detection."),
        QStringLiteral("N"), QStringLiteral("12"));
    const QCommandLineOption regionOption(
        QStringList{QStringLiteral("region")},
        QStringLiteral("Compare a native-resolution x,y,width,height crop."),
        QStringLiteral("rect"));
    const QCommandLineOption quietOption(
        QStringList{QStringLiteral("q"), QStringLiteral("quiet")},
        QStringLiteral("Do not print the one-line result summary."));
    parser.addOptions({baselineOption, actualOption, reportOption, diffOption,
                       channelThresholdOption, maxDifferentPixelsOption,
                       maxDifferentRatioOption, searchRadiusOption,
                       maxTranslationOption, edgeThresholdOption, regionOption,
                       quietOption});
    parser.process(app);

    if (!parser.isSet(baselineOption) || !parser.isSet(actualOption))
        return fail(QStringLiteral("--baseline and --actual are required."));

    tests::support::VisualComparisonPolicy policy;
    QString parseError;
    if (!parseInteger(parser.value(channelThresholdOption), 0, 255,
                      policy.channelThreshold, parseError)
        || !parseInteger(parser.value(searchRadiusOption), 0, 32,
                         policy.translationSearchRadius, parseError)
        || !parseInteger(parser.value(edgeThresholdOption), 1, 255,
                         policy.edgeThreshold, parseError)) {
        return fail(parseError);
    }

    policy.maxDifferentPixels.reset();
    if (parser.isSet(maxDifferentPixelsOption)) {
        qint64 value = 0;
        if (!parseNonNegativeInteger64(
                parser.value(maxDifferentPixelsOption), value, parseError)) {
            return fail(parseError);
        }
        policy.maxDifferentPixels = value;
    }
    policy.maxDifferentRatio.reset();
    if (parser.isSet(maxDifferentRatioOption)) {
        double value = 0.0;
        if (!parseRatio(parser.value(maxDifferentRatioOption), value, parseError))
            return fail(parseError);
        policy.maxDifferentRatio = value;
    }
    if (!policy.maxDifferentPixels && !policy.maxDifferentRatio)
        policy.maxDifferentPixels = qint64(0);

    int maxTranslation = 0;
    if (!parseInteger(parser.value(maxTranslationOption), 0, 32,
                      maxTranslation, parseError)) {
        return fail(parseError);
    }
    policy.maxTranslation = maxTranslation;

    std::optional<QRect> region;
    if (parser.isSet(regionOption)) {
        QRect parsedRegion;
        if (!parseRegion(parser.value(regionOption), parsedRegion, parseError))
            return fail(parseError);
        region = parsedRegion;
    }

    const QString baselinePath = QFileInfo(parser.value(baselineOption)).absoluteFilePath();
    const QString actualPath = QFileInfo(parser.value(actualOption)).absoluteFilePath();
    QImage baseline(baselinePath);
    QImage actual(actualPath);
    if (baseline.isNull())
        return fail(QStringLiteral("Could not load baseline PNG: %1").arg(baselinePath));
    if (actual.isNull())
        return fail(QStringLiteral("Could not load actual PNG: %1").arg(actualPath));
    if (region) {
        if (!baseline.rect().contains(*region) || !actual.rect().contains(*region)) {
            return fail(QStringLiteral(
                "--region must be fully contained by both input images."));
        }
        baseline = baseline.copy(*region);
        actual = actual.copy(*region);
    }

    const auto result = tests::support::analyzeVisualDifference(
        baseline, actual, policy);
    QJsonObject report = tests::support::visualComparisonReport(result);
    report.insert(QStringLiteral("inputs"),
                  QJsonObject{{QStringLiteral("baseline"), baselinePath},
                              {QStringLiteral("baseline_sha256"),
                               fileSha256(baselinePath)},
                              {QStringLiteral("actual"), actualPath},
                              {QStringLiteral("actual_sha256"),
                               fileSha256(actualPath)},
                              {QStringLiteral("region"), regionValue(region)}});

    QString diffPath;
    QString artifactError;
    if (parser.isSet(diffOption)) {
        diffPath = QFileInfo(parser.value(diffOption)).absoluteFilePath();
        QFile::remove(diffPath);
        if (!result.passed && result.sizeMatches) {
            const QImage difference = tests::support::renderVisualDifference(
                baseline, actual, policy.channelThreshold);
            if (difference.isNull()
                || !writeImage(diffPath, difference, artifactError)) {
                return fail(artifactError.isEmpty()
                                ? QStringLiteral("Could not render diff image.")
                                : artifactError);
            }
        } else {
            diffPath.clear();
        }
    }
    report.insert(QStringLiteral("artifacts"),
                  QJsonObject{{QStringLiteral("diff"),
                               diffPath.isEmpty()
                                   ? QJsonValue(QJsonValue::Null)
                                   : QJsonValue(diffPath)}});

    QString reportError;
    if (!writeJson(parser.value(reportOption), report, reportError))
        return fail(reportError);
    if (!parser.isSet(quietOption)) {
        QTextStream(result.passed ? stdout : stderr)
            << tests::support::visualComparisonSummary(result) << Qt::endl;
    }
    if (!result.valid)
        return 2;
    return result.passed ? 0 : 1;
}
