#include "GallerySingleInstance.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QThread>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace fluent::gallery {

namespace {

constexpr int ExistingInstanceTimeoutMs = 2000;
constexpr int ConnectAttemptMs = 100;
constexpr int ConnectRetryDelayMs = 20;
constexpr int MaximumCommandBytes = 256;
const QByteArray ActivateCommand = QByteArrayLiteral("activate/1\n");

QString runtimeDirectory()
{
    QString path =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (path.isEmpty() && !QDir::homePath().isEmpty())
        path = QDir(QDir::homePath()).filePath(QStringLiteral(".fluent-qt"));
    if (path.isEmpty())
        path = QDir::tempPath();
    // The lock deliberately uses a stable per-user directory. If two launch
    // environments resolve the local socket differently, the second launch
    // fails closed behind this one lock instead of becoming another primary.
    // zh_CN: 锁固定在稳定的用户目录。即使两个启动环境对本地 socket 的解析不同，
    // 第二次启动也会在同一把锁后安全退出，而不会成为另一个主实例。
    return QDir(path).filePath(QStringLiteral("single-instance"));
}

QString scopedInstanceName(const QString& applicationId)
{
    QString userScope =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (userScope.isEmpty())
        userScope = QDir::homePath();
    const QByteArray scope = applicationId.toUtf8()
        + QByteArrayLiteral("\n")
        + userScope.toUtf8();
    const QByteArray digest = QCryptographicHash::hash(scope, QCryptographicHash::Sha256)
                                  .toHex()
                                  .left(24);
    return QStringLiteral("fluent-qt-gallery-%1").arg(QString::fromLatin1(digest));
}

void allowOwnerForegroundActivation(QLockFile* lockFile)
{
#ifdef Q_OS_WIN
    if (!lockFile)
        return;

    qint64 ownerProcessId = 0;
    QString ownerHostName;
    QString ownerApplicationName;
    if (!lockFile->getLockInfo(&ownerProcessId, &ownerHostName, &ownerApplicationName)
        || ownerProcessId <= 0
        || static_cast<quint64>(ownerProcessId) > MAXDWORD) {
        return;
    }

    // The user launched this secondary process, so Windows allows it to hand
    // foreground permission to the existing Gallery before the IPC request.
    // zh_CN: 用户启动了当前次进程，因此可在发送 IPC 请求前把前台权限转交给已有 Gallery。
    AllowSetForegroundWindow(static_cast<DWORD>(ownerProcessId));
#else
    Q_UNUSED(lockFile);
#endif
}

} // namespace

GallerySingleInstance::GallerySingleInstance(const QString& applicationId, QObject* parent)
    : QObject(parent)
    , m_applicationId(applicationId.trimmed())
{
    setObjectName(QStringLiteral("gallerySingleInstance"));
}

GallerySingleInstance::~GallerySingleInstance()
{
    releasePrimaryOwnership();
}

GallerySingleInstance::StartResult GallerySingleInstance::start()
{
    if (m_started)
        return m_startResult;
    m_started = true;

    if (m_applicationId.isEmpty()) {
        m_errorString = QStringLiteral("The Gallery application ID is empty.");
        return m_startResult;
    }

    const QString runtimePath = runtimeDirectory();
    if (runtimePath.isEmpty() || !QDir().mkpath(runtimePath)) {
        m_errorString = QStringLiteral("Cannot create the Gallery runtime directory: %1")
                            .arg(runtimePath);
        return m_startResult;
    }

    m_serverName = scopedInstanceName(m_applicationId);
    m_lockFilePath = QDir(runtimePath).filePath(m_serverName + QStringLiteral(".lock"));
    m_lockFile = std::make_unique<QLockFile>(m_lockFilePath);
    // A dead process is still detected by PID. Disabling age-only expiry avoids
    // stealing ownership from a healthy Gallery that has simply run for a while.
    // zh_CN: 进程退出仍可通过 PID 检测；禁用仅按时间过期，避免从长期运行的 Gallery 手中抢锁。
    m_lockFile->setStaleLockTime(0);

    if (!m_lockFile->tryLock(0)) {
        allowOwnerForegroundActivation(m_lockFile.get());
        if (notifyExistingInstance(ExistingInstanceTimeoutMs)) {
            m_startResult = StartResult::ExistingInstanceNotified;
            return m_startResult;
        }

        // The owner may have exited during the connection retry window. Make
        // one final lock attempt; never remove an endpoint without owning this lock.
        // zh_CN: 原实例可能在连接重试期间退出；最后再尝试一次取锁，未持锁时绝不删除 IPC 端点。
        if (!m_lockFile->tryLock(0)) {
            m_errorString = QStringLiteral(
                "Another Gallery instance owns the lock, but its activation endpoint is unavailable.");
            return m_startResult;
        }
    }

    // Only the lock owner may remove a stale Unix socket. This prevents two
    // simultaneous launches from unlinking each other's live endpoint.
    // zh_CN: 只有持锁者可以删除陈旧 Unix socket，避免并发启动时误删对方的活动端点。
    QLocalServer::removeServer(m_serverName);
    m_server = std::make_unique<QLocalServer>();
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server.get(), &QLocalServer::newConnection,
            this, &GallerySingleInstance::acceptPendingConnections);
    if (!m_server->listen(m_serverName)) {
        m_errorString = QStringLiteral("Cannot listen on the Gallery activation endpoint: %1")
                            .arg(m_server->errorString());
        releasePrimaryOwnership();
        return m_startResult;
    }

    m_startResult = StartResult::Primary;
    return m_startResult;
}

bool GallerySingleInstance::isPrimary() const
{
    return m_startResult == StartResult::Primary;
}

QString GallerySingleInstance::serverName() const
{
    return m_serverName;
}

QString GallerySingleInstance::errorString() const
{
    return m_errorString;
}

bool GallerySingleInstance::notifyExistingInstance(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    const int deadline = qMax(1, timeoutMs);

    while (timer.elapsed() < deadline) {
        QLocalSocket socket;
        socket.connectToServer(m_serverName, QIODevice::WriteOnly);
        const int remaining = deadline - static_cast<int>(timer.elapsed());
        if (socket.waitForConnected(qMin(ConnectAttemptMs, qMax(1, remaining)))) {
            const qint64 written = socket.write(ActivateCommand);
            socket.flush();
            const int writeRemaining = deadline - static_cast<int>(timer.elapsed());
            const bool delivered = written == ActivateCommand.size()
                && (socket.bytesToWrite() == 0
                    || socket.waitForBytesWritten(qMax(1, writeRemaining)));
            socket.disconnectFromServer();
            if (delivered)
                return true;
        }
        socket.abort();
        QThread::msleep(ConnectRetryDelayMs);
    }
    return false;
}

void GallerySingleInstance::acceptPendingConnections()
{
    if (!m_server)
        return;

    while (m_server->hasPendingConnections()) {
        QLocalSocket* socket = m_server->nextPendingConnection();
        if (!socket)
            continue;
        socket->setParent(this);
        connect(socket, &QLocalSocket::readyRead,
                this, [this, socket]() { processSocket(socket); });
        connect(socket, &QLocalSocket::disconnected,
                this, [this, socket]() {
                    processSocket(socket);
                    socket->deleteLater();
                });
        if (socket->bytesAvailable() > 0)
            processSocket(socket);
    }
}

void GallerySingleInstance::processSocket(QLocalSocket* socket)
{
    if (!socket || socket->property("galleryInstanceHandled").toBool())
        return;

    QByteArray command = socket->property("galleryInstanceCommand").toByteArray();
    command += socket->readAll();
    if (command.size() > MaximumCommandBytes) {
        socket->setProperty("galleryInstanceHandled", true);
        socket->disconnectFromServer();
        return;
    }

    const int newline = command.indexOf('\n');
    if (newline < 0) {
        socket->setProperty("galleryInstanceCommand", command);
        return;
    }

    socket->setProperty("galleryInstanceHandled", true);
    if (command.left(newline).trimmed() == QByteArrayLiteral("activate/1"))
        emit activationRequested();
    socket->disconnectFromServer();
}

void GallerySingleInstance::releasePrimaryOwnership()
{
    if (m_startResult != StartResult::Primary && (!m_lockFile || !m_lockFile->isLocked()))
        return;

    if (m_server)
        m_server->close();
    if (!m_serverName.isEmpty())
        QLocalServer::removeServer(m_serverName);
    if (m_lockFile && m_lockFile->isLocked())
        m_lockFile->unlock();
    m_startResult = StartResult::Error;
}

} // namespace fluent::gallery
