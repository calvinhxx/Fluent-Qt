#include "viewmodel/UpdateChecker.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QtGlobal>

namespace fluent::gallery {
namespace {
constexpr const char* kLatestReleaseApi =
    "https://api.github.com/repos/calvinhxx/Fluent-Qt/releases/latest";

bool assetNameMatchesPlatform(const QString& assetName,
                              const QString& platformKey) {
    const QString name = assetName.toLower();

    if (name.contains(QStringLiteral(".sha256"))) {
        return false;
    }

    if (platformKey == QStringLiteral("windows-arm64")) {
        return name.contains(QStringLiteral("windows-arm64")) &&
               name.endsWith(QStringLiteral(".exe"));
    }

    if (platformKey == QStringLiteral("windows-x64")) {
        return name.contains(QStringLiteral("windows-x64")) &&
               name.endsWith(QStringLiteral(".exe")) &&
               !name.contains(QStringLiteral("qt5.15"));
    }

    if (platformKey == QStringLiteral("macos-arm64")) {
        return name.contains(QStringLiteral("darwin-arm64")) &&
               name.endsWith(QStringLiteral(".dmg"));
    }

    if (platformKey == QStringLiteral("macos-x64")) {
        return name.contains(QStringLiteral("darwin-x86_64")) &&
               name.endsWith(QStringLiteral(".dmg")) &&
               !name.contains(QStringLiteral("qt5.15"));
    }

    if (platformKey == QStringLiteral("linux-arm64")) {
        return name.contains(QStringLiteral("linux-arm64")) &&
               name.endsWith(QStringLiteral(".deb"));
    }

    if (platformKey == QStringLiteral("linux-x64")) {
        return name.contains(QStringLiteral("linux-x86_64")) &&
               name.endsWith(QStringLiteral(".deb"));
    }

    return false;
}
}  // namespace

UpdateChecker::UpdateChecker(QObject* parent) : QObject(parent) {
    qRegisterMetaType<fluent::gallery::UpdateChecker::Result>(
        "fluent::gallery::UpdateChecker::Result");
    m_network = new QNetworkAccessManager(this);
}

UpdateChecker::~UpdateChecker() {
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

bool UpdateChecker::isChecking() const { return m_reply != nullptr; }

QString UpdateChecker::currentVersion() const {
    const QString version = QCoreApplication::applicationVersion().trimmed();
    return version.isEmpty() ? QStringLiteral("0.0.0") : version;
}

QString UpdateChecker::platformLabel() const { return platformDisplayName(); }

void UpdateChecker::checkForUpdates() {
    if (m_reply) {
        return;
    }

    QNetworkRequest request{QUrl(QString::fromLatin1(kLatestReleaseApi))};
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Fluent-Qt-Gallery/%1")
                          .arg(currentVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::finished, this,
            &UpdateChecker::handleReplyFinished);
    emit checkStarted();
}

UpdateChecker::Result UpdateChecker::parseReleasePayload(
    const QByteArray& payload) const {
    Result result;
    result.currentVersion = currentVersion();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        result.message = tr("GitHub release response could not be parsed.");
        return result;
    }

    const QJsonObject release = document.object();
    const QString tagName = release.value(QStringLiteral("tag_name")).toString();
    const QString htmlUrl = release.value(QStringLiteral("html_url")).toString();
    result.releaseName = release.value(QStringLiteral("name")).toString();
    result.latestVersion = normalizedVersion(tagName);
    result.releaseUrl = QUrl(htmlUrl);

    if (result.latestVersion.isEmpty() || !result.releaseUrl.isValid()) {
        result.message = tr("Latest release metadata is incomplete.");
        return result;
    }

    const int comparison =
        compareVersions(result.latestVersion, result.currentVersion);
    if (comparison <= 0) {
        result.status = Result::Status::UpToDate;
        result.message = tr("You are on the latest version (%1).")
                             .arg(result.currentVersion);
        return result;
    }

    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    result.assetUrl = selectPlatformAsset(assets, &result.assetName);
    result.status = Result::Status::UpdateAvailable;

    if (result.assetUrl.isValid()) {
        result.message =
            tr("Version %1 is available for %2.")
                .arg(result.latestVersion, platformDisplayName());
    } else {
        result.message =
            tr("Version %1 is available. Open the release page to download it.")
                .arg(result.latestVersion);
    }

    return result;
}

QUrl UpdateChecker::selectPlatformAsset(const QJsonArray& assets,
                                        QString* assetName) const {
    const QString key = platformKey();
    for (const QJsonValue& value : assets) {
        if (!value.isObject()) {
            continue;
        }

        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (!assetNameMatchesPlatform(name, key)) {
            continue;
        }

        const QUrl url{
            asset.value(QStringLiteral("browser_download_url")).toString()};
        if (!url.isValid()) {
            continue;
        }

        if (assetName) {
            *assetName = name;
        }
        return url;
    }

    if (assetName) {
        assetName->clear();
    }
    return {};
}

void UpdateChecker::handleReplyFinished() {
    QNetworkReply* reply = m_reply;
    m_reply = nullptr;

    Result result;
    result.currentVersion = currentVersion();

    const QVariant statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    const int httpStatus = statusCode.isValid() ? statusCode.toInt() : 0;
    if (reply->error() != QNetworkReply::NoError) {
        result.message = reply->errorString();
    } else if (httpStatus >= 400) {
        result.message =
            tr("GitHub release request failed with HTTP %1.").arg(httpStatus);
    } else {
        result = parseReleasePayload(reply->readAll());
    }

    reply->deleteLater();
    emit checkFinished(result);
}

int UpdateChecker::compareVersions(const QString& left, const QString& right) {
    const QStringList leftParts = normalizedVersion(left).split('.');
    const QStringList rightParts = normalizedVersion(right).split('.');

    for (int index = 0; index < 3; ++index) {
        const int leftValue =
            index < leftParts.size() ? leftParts.at(index).toInt() : 0;
        const int rightValue =
            index < rightParts.size() ? rightParts.at(index).toInt() : 0;

        if (leftValue < rightValue) {
            return -1;
        }
        if (leftValue > rightValue) {
            return 1;
        }
    }

    return 0;
}

QString UpdateChecker::normalizedVersion(const QString& version) {
    QString normalized = version.trimmed();
    if (normalized.startsWith(QLatin1Char('v')) ||
        normalized.startsWith(QLatin1Char('V'))) {
        normalized.remove(0, 1);
    }

    const int suffixIndex = normalized.indexOf(QLatin1Char('-'));
    if (suffixIndex >= 0) {
        normalized.truncate(suffixIndex);
    }

    static const QRegularExpression validVersionPattern{
        QStringLiteral(R"(^\d+(?:\.\d+){0,2})")};
    const QRegularExpressionMatch match =
        validVersionPattern.match(normalized);
    return match.hasMatch() ? match.captured(0) : QString();
}

QString UpdateChecker::platformKey() {
#if defined(Q_OS_WIN)
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("windows-arm64");
#else
    return QStringLiteral("windows-x64");
#endif
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("macos-arm64");
#else
    return QStringLiteral("macos-x64");
#endif
#elif defined(Q_OS_LINUX)
#if defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("linux-arm64");
#else
    return QStringLiteral("linux-x64");
#endif
#else
    return QStringLiteral("release-page");
#endif
}

QString UpdateChecker::platformDisplayName() {
    const QString key = platformKey();
    if (key == QStringLiteral("windows-arm64")) {
        return QStringLiteral("Windows ARM64");
    }
    if (key == QStringLiteral("windows-x64")) {
        return QStringLiteral("Windows x64");
    }
    if (key == QStringLiteral("macos-arm64")) {
        return QStringLiteral("macOS Apple Silicon");
    }
    if (key == QStringLiteral("macos-x64")) {
        return QStringLiteral("macOS Intel");
    }
    if (key == QStringLiteral("linux-arm64")) {
        return QStringLiteral("Linux ARM64");
    }
    if (key == QStringLiteral("linux-x64")) {
        return QStringLiteral("Linux x64");
    }
    return QStringLiteral("this platform");
}

}  // namespace fluent::gallery
