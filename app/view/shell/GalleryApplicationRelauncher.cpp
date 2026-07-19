#include "GalleryApplicationRelauncher.h"

#include <array>
#include <utility>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace fluent::gallery::relaunch {
namespace {

#ifdef Q_OS_MACOS
constexpr int kLauncherStartTimeoutMs = 3000;
constexpr int kLauncherFinishTimeoutMs = 5000;
#endif

const std::array<const char*, 8> kScaleEnvironmentNames{{
    "QT_SCALE_FACTOR",
    "QT_SCREEN_SCALE_FACTORS",
    "QT_AUTO_SCREEN_SCALE_FACTOR",
    "QT_ENABLE_HIGHDPI_SCALING",
    "QT_SCALE_FACTOR_ROUNDING_POLICY",
    "QT_FONT_DPI",
    "FLUENT_QT_GALLERY_SCALE_MANAGED",
    "FLUENT_QT_GALLERY_ORIGINAL_QT_SCALE_FACTOR",
}};

QStringList restartedArguments(QStringList arguments)
{
    arguments = stripRestartArguments(std::move(arguments));
    arguments.append(QString::fromLatin1(RestartArgument));
    return arguments;
}

LaunchResult launchDirectly(const QString& executablePath,
                            const QStringList& applicationArguments)
{
    LaunchResult result;
    result.launcher = executablePath;
    if (executablePath.trimmed().isEmpty()) {
        result.errorString = QStringLiteral("The restart executable path is empty.");
        return result;
    }

    QProcess process;
    process.setProgram(executablePath);
    process.setArguments(restartedArguments(applicationArguments));
    process.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    result.started = process.startDetached(&result.processId);
    if (!result.started)
        result.errorString = process.errorString();
    return result;
}

#ifdef Q_OS_MACOS
QString processFailureDetail(QProcess& process)
{
    const QString output = QString::fromLocal8Bit(process.readAll()).trimmed();
    if (!output.isEmpty())
        return output;
    return process.errorString();
}

LaunchResult launchMacBundle(const QString& bundlePath, const QStringList& applicationArguments,
                             const QProcessEnvironment& environment, bool forwardScaleEnvironment)
{
    LaunchResult result;
    result.launcher = QStringLiteral("/usr/bin/open");

    QProcess process;
    process.setProgram(result.launcher);
    process.setArguments(detail::macOpenArguments(bundlePath, applicationArguments, environment,
                                                  forwardScaleEnvironment));
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QIODevice::ReadOnly);
    if (!process.waitForStarted(kLauncherStartTimeoutMs)) {
        result.errorString = processFailureDetail(process);
        return result;
    }
    if (!process.waitForFinished(kLauncherFinishTimeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        result.errorString = QStringLiteral("The macOS application launcher timed out.");
        return result;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        result.errorString = processFailureDetail(process);
        return result;
    }

    result.started = true;
    return result;
}
#endif

} // namespace

bool isRestartedLaunch(const QStringList& arguments)
{
    return arguments.contains(QString::fromLatin1(RestartArgument));
}

QStringList stripRestartArguments(QStringList arguments)
{
    arguments.removeAll(QString::fromLatin1(RestartArgument));
    return arguments;
}

namespace detail {

QString macApplicationBundlePath(const QString& executablePath)
{
    if (executablePath.trimmed().isEmpty())
        return {};

    QDir directory(QFileInfo(executablePath).absolutePath());
    if (directory.dirName().compare(QStringLiteral("MacOS"), Qt::CaseInsensitive) != 0 ||
        !directory.cdUp() ||
        directory.dirName().compare(QStringLiteral("Contents"), Qt::CaseInsensitive) != 0 ||
        !directory.cdUp()) {
        return {};
    }

    const QFileInfo bundle(directory.absolutePath());
    return bundle.suffix().compare(QStringLiteral("app"), Qt::CaseInsensitive) == 0
               ? QDir::cleanPath(bundle.absoluteFilePath())
               : QString();
}

QStringList macOpenArguments(const QString& bundlePath, const QStringList& applicationArguments,
                             const QProcessEnvironment& environment, bool forwardScaleEnvironment)
{
    QStringList arguments{QStringLiteral("-n")};
    if (forwardScaleEnvironment) {
        for (const char* name : kScaleEnvironmentNames) {
            const QString key = QString::fromLatin1(name);
            if (environment.contains(key)) {
                arguments.append(QStringLiteral("--env"));
                arguments.append(key + QLatin1Char('=') + environment.value(key));
            }
        }
    }

    arguments.append(bundlePath);
    arguments.append(QStringLiteral("--args"));
    arguments.append(restartedArguments(applicationArguments));
    return arguments;
}

} // namespace detail

LaunchResult launchApplication(const QString& executablePath,
                               const QStringList& applicationArguments)
{
#ifdef Q_OS_MACOS
    const QString bundlePath = detail::macApplicationBundlePath(executablePath);
    if (!bundlePath.isEmpty() && QDir(bundlePath).exists()) {
        const QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        LaunchResult result = launchMacBundle(bundlePath, applicationArguments, environment, true);
        if (result.started)
            return result;

        const QString environmentLaunchError = result.errorString;
        result = launchMacBundle(bundlePath, applicationArguments, environment, false);
        if (result.started) {
            result.warningString =
                QStringLiteral("The macOS launcher rejected forwarded scale environment values; "
                               "the bundle was relaunched without them: %1")
                    .arg(environmentLaunchError);
            return result;
        }

        LaunchResult fallback = launchDirectly(executablePath, applicationArguments);
        if (fallback.started) {
            fallback.warningString = QStringLiteral("LaunchServices relaunch failed; the bundle "
                                                    "executable was started directly: %1")
                                         .arg(result.errorString);
        }
        return fallback;
    }
#endif
    return launchDirectly(executablePath, applicationArguments);
}

} // namespace fluent::gallery::relaunch
