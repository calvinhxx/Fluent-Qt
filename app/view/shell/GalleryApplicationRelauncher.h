#ifndef GALLERYAPPLICATIONRELAUNCHER_H
#define GALLERYAPPLICATIONRELAUNCHER_H

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QtGlobal>

namespace fluent::gallery::relaunch {

inline constexpr char RestartArgument[] = "--fluent-qt-restarted";

struct LaunchResult {
    bool started = false;
    qint64 processId = -1;
    QString launcher;
    QString warningString;
    QString errorString;
};

bool isRestartedLaunch(const QStringList& arguments);
QStringList stripRestartArguments(QStringList arguments);

namespace detail {

QString macApplicationBundlePath(const QString& executablePath);
QStringList macOpenArguments(const QString& bundlePath, const QStringList& applicationArguments,
                             const QProcessEnvironment& environment, bool forwardScaleEnvironment);

} // namespace detail

LaunchResult launchApplication(const QString& executablePath,
                               const QStringList& applicationArguments);

} // namespace fluent::gallery::relaunch

#endif // GALLERYAPPLICATIONRELAUNCHER_H
