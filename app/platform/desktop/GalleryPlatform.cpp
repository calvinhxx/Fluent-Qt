#include "platform/GalleryPlatform.h"
#include "support/clipboard/ClipboardSupport.h"
#include "compatibility/WindowGeometryCompat.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QPointer>
#include <QRect>
#include <QSettings>
#include <QStandardPaths>
#include <QWidget>
#include <QWindow>
#include <QSaveFile>
#include <QDir>
#include "utils/DiagnosticSession.h"
#include <QThread>
#include <memory>
#include <QClipboard>
#include <QMimeData>
#include <QDateTime>
#include <QUuid>
#include <QJsonDocument>
#include <QRegularExpression>

#include "support/interfacezoom/InterfaceZoom.h"

#ifndef FLUENT_QT_GALLERY_DISPLAY_NAME
#define FLUENT_QT_GALLERY_DISPLAY_NAME "Fluent-Qt Gallery"
#endif

namespace fluent::gallery::platform {

QString diagnosticStorageDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/diagnostics");
}

QVariantMap diagnosticProcessIdentity()
{
    return {{"pid", QCoreApplication::applicationPid()},
        {"executable", QFileInfo(QCoreApplication::applicationFilePath()).canonicalFilePath()},
        {"started_ms", QDateTime::currentMSecsSinceEpoch()}};
}

QVariantMap existingCrashAttachments(const QVariantList& identities, const QString& directory)
{
    QVariantMap files;
    qint64 remaining = 16 * 1024 * 1024;
    const auto candidates = QDir(directory).entryInfoList({"*.ips", "*.crash", "*.dmp"}, QDir::Files | QDir::NoSymLinks,
        QDir::Time);
    for (const auto& info : candidates) {
        if (files.size() >= 2)
            break;
        if (info.size() <= 0 || info.size() > remaining)
            continue;
        QByteArray bytes;
        for (const auto& value : identities) {
            const auto identity = value.toMap();
            const auto pid = identity.value("pid").toLongLong();
            const auto executable = identity.value("executable").toString();
            const auto started = identity.value("started_ms").toLongLong();
            const auto checkpoint = identity.value("checkpoint_ms").toLongLong();
            const auto modified = info.lastModified().toMSecsSinceEpoch();
            if (pid <= 0 || executable.isEmpty() || started <= 0 || checkpoint < started || modified < started - 2000
                || modified > checkpoint + 120000)
                continue;
            const auto suffix = info.suffix().toLower();
            // WER filenames include both executable and process ID. Generic Python names alone are unsafe.
            // zh_CN: WER 文件名同时包含可执行文件和进程号，不能仅凭 Python 名称认领崩溃报告。
            if (suffix == "dmp"
                && info.fileName().compare(QFileInfo(executable).fileName() + "." + QString::number(pid) + ".dmp",
                       Qt::CaseInsensitive)
                    != 0)
                continue;
            if (bytes.isEmpty()) {
                QFile file(info.absoluteFilePath());
                if (!file.open(QIODevice::ReadOnly))
                    break;
                bytes = file.read(remaining + 1);
                if (bytes.isEmpty() || bytes.size() > remaining)
                    break;
            }
            bool matches = suffix == "dmp";
            if (suffix == "ips") {
                auto crash = QJsonDocument::fromJson(bytes).toVariant().toMap();
                if (crash.isEmpty())
                    crash = QJsonDocument::fromJson(bytes.mid(bytes.indexOf('\n') + 1)).toVariant().toMap();
                matches = crash.value("pid").toLongLong() == pid && crash.value("procPath").toString() == executable;
            } else if (suffix == "crash") {
                const QString header = QString::fromUtf8(bytes.left(64 * 1024));
                const auto path =
                    QRegularExpression(QStringLiteral("^(?:Path|ExecutablePath):[ \t]*(.+)$"),
                        QRegularExpression::MultilineOption)
                        .match(header)
                        .captured(1)
                        .trimmed();
                const bool process =
                    QRegularExpression(QStringLiteral("^Process:.*\\[%1\\][ \t]*$").arg(pid),
                        QRegularExpression::MultilineOption)
                        .match(header)
                        .hasMatch()
                    || QRegularExpression(QStringLiteral("^[ \t]*Pid:[ \t]*%1[ \t]*$").arg(pid),
                        QRegularExpression::MultilineOption)
                           .match(header)
                           .hasMatch();
                matches = path == executable && process;
            }
            if (matches) {
                files.insert(QStringLiteral("crash-%1.%2").arg(files.size() + 1).arg(suffix), bytes);
                remaining -= bytes.size();
                break;
            }
        }
    }
    return files;
}

void prepareDiagnosticFeedback(QWidget* context,
    const QVariantMap& report,
    const QVariantMap& attachments,
    const QVariantList& identities,
    const QString& directory,
    std::function<void(bool, const QString&)> finished)
{
    auto result = std::make_shared<QString>();
    auto ok = std::make_shared<bool>(false);
    auto* worker = QThread::create([=] {
        QString crashDirectory;
#if defined(Q_OS_MACOS)
        crashDirectory = QDir::homePath() + QStringLiteral("/Library/Logs/DiagnosticReports");
#elif defined(Q_OS_WIN)
        const auto localData = qEnvironmentVariable("LOCALAPPDATA");
        if (!localData.isEmpty())
            crashDirectory = localData + QStringLiteral("/CrashDumps");
#else
        crashDirectory = QStringLiteral("/var/crash");
#endif
        auto files = attachments;
        const auto crashes =
            crashDirectory.isEmpty() ? QVariantMap{} : existingCrashAttachments(identities, crashDirectory);
        for (auto it = crashes.cbegin(); it != crashes.cend(); ++it)
            files.insert(it.key(), it.value());
        auto snapshot = report;
        snapshot.insert("crash_files_included", crashes.size());
        const auto bytes = diagnostics::DiagnosticSession::createBundle(snapshot, files);
        const auto folderPath = directory + QStringLiteral("/reports");
        if (directory.isEmpty() || bytes.isEmpty() || !QDir().mkpath(folderPath)) {
            *result = QObject::tr("Could not prepare the report. Try again.");
            return;
        }
        QFile::setPermissions(folderPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const auto name =
            QStringLiteral("fluentqt-report-%1.zip").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        const auto path = QDir(folderPath).filePath(name);
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            *result = QObject::tr("Could not save the report.");
            return;
        }
        file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
        if (file.write(bytes) != bytes.size() || !file.commit()) {
            *result = QObject::tr("Could not save the report.");
            return;
        }
        const auto old =
            QDir(folderPath).entryInfoList({"fluentqt-report-*.zip"}, QDir::Files | QDir::NoSymLinks, QDir::Time);
        for (int i = 5; i < old.size(); ++i)
            QFile::remove(old.at(i).absoluteFilePath());
        *result = path;
        *ok = true;
    });
    QObject::connect(worker, &QThread::finished, context, [=] {
        if (*ok) {
            const QPointer<QWidget> guard(context);
            auto* mime = new QMimeData;
            const auto url = QUrl::fromLocalFile(*result);
            mime->setUrls({url});
            mime->setData("x-special/gnome-copied-files", "copy\n" + url.toEncoded());
            QGuiApplication::clipboard()->setMimeData(mime);
            if (!guard)
                return;
            if (!support::clipboard::writeSucceeded()) {
                finished(false, QObject::tr("Could not copy the report to the clipboard. Try again."));
                return;
            }
        }
        finished(*ok, *result);
    });
    QObject::connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

QString graphicsRendererOverride()
{
    return {};
}

void chooseFiles(QWidget* context, const QString& filter, std::function<void(const QString&, qint64)> selected)
{
    if (!context || !selected)
        return;
    auto* dialog = new QFileDialog(context, QObject::tr("Choose files"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setNameFilter(filter);
    const QPointer<QWidget> guard(context);
    QObject::connect(dialog, &QFileDialog::filesSelected, context, [guard, selected](const QStringList& paths) {
        for (const QString& path : paths) {
            if (!guard)
                return;
            const QFileInfo file(path);
            selected(file.fileName(), file.size());
        }
    });
    dialog->open();
}

const Capabilities& capabilities()
{
    static const Capabilities value = [] {
        Capabilities result;
        result.applicationName = QStringLiteral(FLUENT_QT_GALLERY_DISPLAY_NAME);
        result.windowTitle = QStringLiteral("Fluent-Qt Gallery");
        result.distributionSectionTitle = QStringLiteral("Updates");
        result.distributionTitle = QStringLiteral("Gallery updates");
        result.distributionDescription = QStringLiteral(
            "Check GitHub Releases and open the latest package for this platform");
        return result;
    }();
    return value;
}

bool persistenceAvailable()
{
    return QCoreApplication::organizationName() == QStringLiteral("Fluent-Qt")
        && QCoreApplication::applicationName() == capabilities().applicationName;
}

QSettings createSettings()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/config.ini");
    return QSettings(path, QSettings::IniFormat);
}

HostTheme hostTheme()
{
    return HostTheme::System;
}

void setHostThemeChangedHandler(QObject* context, HostThemeChangedHandler handler)
{
    Q_UNUSED(context);
    Q_UNUSED(handler);
}

void showTopLevelWindow(QWidget* window, const QRect& normalGeometry, bool maximized)
{
    if (!window)
        return;
    window->setGeometry(normalGeometry);
    if (!maximized) {
        window->show();
        return;
    }
    fluentPrepareWindowNormalGeometry(window, normalGeometry);
    window->setWindowState(window->windowState() | Qt::WindowMaximized);
    window->show();
}

bool supportsInterfaceZoom()
{
    return true;
}

bool supportsLiveInterfaceZoom()
{
    return support::interfacezoom::isSupported();
}

qreal appliedInterfaceZoom()
{
    return support::interfacezoom::appliedZoom();
}

bool prepareInterfaceZoom(qreal zoom)
{
    return support::interfacezoom::prepareStartup(zoom);
}

bool applyInterfaceZoom(qreal zoom)
{
    return support::interfacezoom::apply(zoom);
}

} // namespace fluent::gallery::platform
