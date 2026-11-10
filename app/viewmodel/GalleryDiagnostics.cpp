#include "GalleryDiagnostics.h"
#include "view/widgets/GalleryFeedbackTip.h"
#include <FluentQt/Diagnostics.h>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QKeySequence>
#include <QSaveFile>
#include <QTimer>
#include <QUuid>
#include <QUrlQuery>
#include <QWidget>
#include "platform/GalleryPlatform.h"
#include "viewmodel/GallerySettings.h"
#include "support/logging/Log.h"

namespace fluent::gallery {
namespace {
void writeSnapshot(const QString& path, const QByteArray& bytes)
{
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        if (file.write(bytes) == bytes.size())
            file.commit();
    }
}

QString pasteShortcutText()
{
    const QString native = QKeySequence(QKeySequence::Paste).toString(QKeySequence::NativeText);
    return native.isEmpty() ? QStringLiteral("Ctrl+V") : native;
}
} // namespace

GalleryDiagnostics::GalleryDiagnostics(QWidget* root, const QString& directory)
    : QObject(root), m_root(root), m_session(new diagnostics::DiagnosticSession(this)),
      m_sessionId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    setObjectName(QStringLiteral("galleryDiagnostics"));
    m_directory = directory;
    m_identity = platform::diagnosticProcessIdentity();
    if (!directory.isEmpty() && QDir().mkpath(directory)) {
        QFile::setPermissions(directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QDir folder(directory);
        const auto existing = folder.entryInfoList({QStringLiteral("session-*.json")}, QDir::Files | QDir::NoSymLinks,
            QDir::Time);
        if (!existing.isEmpty() && existing.first().size() <= 1024 * 1024) {
            QFile prior(existing.first().absoluteFilePath());
            if (prior.open(QIODevice::ReadOnly)) {
                const auto bytes = prior.read(1024 * 1024 + 1);
                auto priorReport = QJsonDocument::fromJson(bytes).toVariant().toMap();
                if (priorReport.value("tool") == "FluentQt Diagnostics") {
                    m_previousIdentity = priorReport.take("_local_process").toMap();
                    m_previous = QJsonDocument::fromVariant(priorReport).toJson(QJsonDocument::Compact);
                }
            }
        }
        // Keep this run and two previous bounded reports; never prune unrelated files.
        // zh_CN: 保留本次及前两次报告，不清理其他文件。
        for (int i = 2; i < existing.size(); ++i)
            QFile::remove(existing.at(i).absoluteFilePath());
        m_path = folder.filePath(QStringLiteral("session-%1.json").arg(m_sessionId));
        m_writer = new QObject;
        m_writer->moveToThread(&m_writerThread);
        connect(&m_writerThread, &QThread::finished, m_writer, &QObject::deleteLater);
        m_writerThread.start();
    }
    m_session->start(root, true, 0);
    support::logging::setDiagnosticSink(m_session->messageSink());
    auto* timer = new QTimer(this);
    timer->setInterval(15000);
    connect(timer, &QTimer::timeout, this, &GalleryDiagnostics::persist);
    if (m_writer)
        timer->start();
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &GalleryDiagnostics::persist);
    persist();
}

GalleryDiagnostics::~GalleryDiagnostics()
{
    m_session->stop();
    if (m_writer) {
        const auto path = m_path;
        auto report = storedReport();
        const auto bytes = QJsonDocument::fromVariant(report).toJson();
        QMetaObject::invokeMethod(
            m_writer, [path, bytes] { writeSnapshot(path, bytes); }, Qt::BlockingQueuedConnection);
        m_writerThread.quit();
        m_writerThread.wait();
    }
}

GalleryDiagnostics* GalleryDiagnostics::ensure(QWidget* root)
{
    if (!root)
        return nullptr;
    for (auto* ancestor = root; ancestor; ancestor = ancestor->parentWidget()) {
        const auto children = ancestor->children();
        for (QObject* child : children)
            if (auto* service = dynamic_cast<GalleryDiagnostics*>(child))
                return service;
    }
    return new GalleryDiagnostics(root, platform::diagnosticStorageDirectory());
}

QVariantMap GalleryDiagnostics::captureReport()
{
    auto report = m_session->captureReport();
    report.insert(QStringLiteral("session_id"), m_sessionId);
    report.insert(QStringLiteral("captured_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    report.insert(QStringLiteral("crash_capture"), QStringLiteral("existing-files-only"));
    const auto& settings = GallerySettings::instance();
    report.insert(QStringLiteral("gallery"),
        QVariantMap{{"last_content_route", m_root ? m_root->property("galleryLastContentRoute") : QVariant()},
            {"backdrop", static_cast<int>(settings.windowEffect())}, {"spatial_enabled", settings.spatialModeEnabled()},
            {"interface_zoom_percent", settings.interfaceZoomPercent()}});
    return report;
}

QVariantMap GalleryDiagnostics::storedReport() const
{
    auto report = m_session->report();
    report.insert("session_id", m_sessionId);
    auto identity = m_identity;
    identity.insert("checkpoint_ms", QDateTime::currentMSecsSinceEpoch());
    report.insert("_local_process", identity);
    return report;
}

GalleryFeedbackTip* GalleryDiagnostics::showFeedback(const QString& title, const QString& message, bool error)
{
    if (!m_feedbackVisible || !m_feedbackAnchor || !m_feedbackAnchor->isVisible())
        return nullptr;
    if (!m_feedbackTip) {
        m_feedbackTip = new GalleryFeedbackTip(m_root);
        connect(m_feedbackTip, &dialogs_flyouts::Popup::aboutToHide, this, [this] { m_feedbackVisible = false; });
    }
    m_feedbackTip->setFeedback(title, message, m_preparing, error);
    m_feedbackTip->showFor(m_feedbackAnchor);
    return m_feedbackTip;
}

void GalleryDiagnostics::showPreparedReport(const QUrl& url, bool downloaded, bool openFailed)
{
    const QString pasteHint = tr("Press %1 on GitHub to attach it.").arg(pasteShortcutText());
    auto* tip = showFeedback(openFailed ? tr("Couldn’t open GitHub") : tr("Diagnostic ZIP ready"),
        openFailed
            ? (downloaded ? tr("ZIP downloaded. Try opening GitHub again.") : tr("ZIP is in clipboard. Try again."))
            : downloaded
            ? tr("Attach fluentqt-report.zip on GitHub.")
            : pasteHint,
        openFailed);
    if (!tip)
        return;
    tip->setArtifact(QStringLiteral("fluentqt-report.zip"), m_lastReportBytes, downloaded);
    tip->setAction(tr("Open GitHub issue"), QStringLiteral("ic_fluent_open_20_regular"), this, [this, url, downloaded] {
        if (!QDesktopServices::openUrl(url)) {
            QTimer::singleShot(0, this, [this, url, downloaded] {
                m_feedbackVisible = true;
                showPreparedReport(url, downloaded, true);
            });
        }
    });
}

void GalleryDiagnostics::reportProblem(QWidget* source)
{
    if (!m_root)
        return;
    if (source) {
        m_feedbackAnchor = source;
        m_feedbackVisible = true;
    }
    if (!m_feedbackAnchor)
        return;
    const bool alreadyPreparing = m_preparing;
    m_preparing = true;
    showFeedback(tr("Preparing report ZIP…"),
        m_directory.isEmpty() ? tr("Packaging diagnostic archive…") : tr("Packaging logs for clipboard…"));
    if (alreadyPreparing)
        return;
    const auto report = captureReport();
    const auto url = issueUrl(report);
    auto identity = m_identity;
    identity.insert("checkpoint_ms", QDateTime::currentMSecsSinceEpoch());
    platform::prepareDiagnosticFeedback(m_root, report, savedAttachments(), {identity, m_previousIdentity}, m_directory,
        [this, guard = QPointer<GalleryDiagnostics>(this), url](bool ok, const QString& detail) {
            if (!guard)
                return;
            m_preparing = false;
            if (ok) {
                m_lastReportBytes = detail.isEmpty() ? 0 : QFileInfo(detail).size();
                showPreparedReport(url, detail.isEmpty());
                return;
            }
            auto* tip = showFeedback(tr("Couldn’t save report"), tr("Logs are still available."), true);
            if (tip)
                tip->setAction(tr("Try again"), QStringLiteral("ic_fluent_arrow_clockwise_20_regular"), this, [this] {
                    QTimer::singleShot(0, this, [this] { reportProblem(m_feedbackAnchor); });
                });
        });
}

void GalleryDiagnostics::persist()
{
    if (!m_writer)
        return;
    auto report = storedReport();
    const auto bytes = QJsonDocument::fromVariant(report).toJson(QJsonDocument::Compact);
    const auto path = m_path;
    QMetaObject::invokeMethod(m_writer, [path, bytes] { writeSnapshot(path, bytes); }, Qt::QueuedConnection);
}

QVariantMap GalleryDiagnostics::savedAttachments() const
{
    QVariantMap files;
    if (!m_previous.isEmpty())
        files.insert(QStringLiteral("previous-session.json"), m_previous);
    const auto logPath = support::logging::defaultLogFilePath();
    QFile log(logPath);
    if (!logPath.isEmpty() && log.open(QIODevice::ReadOnly)) {
        log.seek(qMax<qint64>(0, log.size() - 256 * 1024));
        files.insert(QStringLiteral("gallery.log"), log.read(256 * 1024));
    }
    return files;
}

QUrl GalleryDiagnostics::issueUrl(const QVariantMap& report) const
{
    const auto env = report.value(QStringLiteral("environment")).toMap();
    QUrl url(QStringLiteral("https://github.com/calvinhxx/Fluent-Qt/issues/new"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("template"), QStringLiteral("bug_report.yml"));
    query.addQueryItem(QStringLiteral("version"), env.value(QStringLiteral("fluentqt_version")).toString());
    query.addQueryItem(QStringLiteral("environment"),
        QStringLiteral("%1, %2, Qt %3, %4")
            .arg(env.value("os").toString(), env.value("architecture").toString(), env.value("qt_version").toString(),
                env.value("platform").toString()));
    query.addQueryItem(QStringLiteral("install"), QStringLiteral("C++ Gallery"));
    query.addQueryItem(QStringLiteral("evidence"),
        QStringLiteral(
            "Report ID: %1\nPaste or attach the diagnostic ZIP here before submitting.\nReview the attachment before sharing.")
            .arg(m_sessionId));
    url.setQuery(query);
    return url;
}
} // namespace fluent::gallery
