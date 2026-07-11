#ifndef GALLERYSINGLEINSTANCE_H
#define GALLERYSINGLEINSTANCE_H

#include <QObject>
#include <QString>

#include <memory>

class QLocalServer;
class QLocalSocket;
class QLockFile;

namespace fluent::gallery {

/**
 * @brief Coordinates one Gallery process per user and forwards activation requests.
 * zh_CN: 协调每个用户仅运行一个 Gallery 进程，并转发窗口激活请求。
 */
class GallerySingleInstance final : public QObject {
    Q_OBJECT

public:
    enum class StartResult {
        Primary,
        ExistingInstanceNotified,
        Error,
    };

    explicit GallerySingleInstance(const QString& applicationId,
                                   QObject* parent = nullptr);
    ~GallerySingleInstance() override;

    /**
     * @brief Acquires primary ownership or notifies the process that already owns it.
     * zh_CN: 获取主实例所有权，或通知已经持有所有权的进程。
     */
    StartResult start();

    bool isPrimary() const;
    QString serverName() const;
    QString errorString() const;

signals:
    void activationRequested();

private:
    bool notifyExistingInstance(int timeoutMs);
    void acceptPendingConnections();
    void processSocket(QLocalSocket* socket);
    void releasePrimaryOwnership();

    QString m_applicationId;
    QString m_serverName;
    QString m_lockFilePath;
    QString m_errorString;
    std::unique_ptr<QLockFile> m_lockFile;
    std::unique_ptr<QLocalServer> m_server;
    StartResult m_startResult = StartResult::Error;
    bool m_started = false;
};

} // namespace fluent::gallery

#endif // GALLERYSINGLEINSTANCE_H
