#ifndef UPDATECHECKER_H
#define UPDATECHECKER_H

#include <QJsonArray>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace fluent::gallery {

class UpdateChecker final : public QObject {
    Q_OBJECT

public:
    struct Result {
        enum class Status { UpToDate, UpdateAvailable, Error };

        Status status = Status::Error;
        QString currentVersion;
        QString latestVersion;
        QString releaseName;
        QUrl releaseUrl;
        QUrl assetUrl;
        QString assetName;
        QString message;
    };

    explicit UpdateChecker(QObject* parent = nullptr);
    ~UpdateChecker() override;

    [[nodiscard]] bool isChecking() const;
    [[nodiscard]] QString currentVersion() const;
    [[nodiscard]] QString platformLabel() const;

public slots:
    void checkForUpdates();

signals:
    void checkStarted();
    void checkFinished(const fluent::gallery::UpdateChecker::Result& result);

private:
    [[nodiscard]] Result parseReleasePayload(const QByteArray& payload) const;
    [[nodiscard]] QUrl selectPlatformAsset(const QJsonArray& assets,
                                           QString* assetName) const;
    void handleReplyFinished();

    static int compareVersions(const QString& left, const QString& right);
    static QString normalizedVersion(const QString& version);
    static QString platformKey();
    static QString platformDisplayName();

    QNetworkAccessManager* m_network = nullptr;
    QNetworkReply* m_reply = nullptr;
};

}  // namespace fluent::gallery

Q_DECLARE_METATYPE(fluent::gallery::UpdateChecker::Result)

#endif  // UPDATECHECKER_H
