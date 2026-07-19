#ifndef GALLERYAPPLICATIONLIFECYCLE_H
#define GALLERYAPPLICATIONLIFECYCLE_H

#include <QCoreApplication>

namespace fluent::gallery {

inline constexpr int RestartExitCode = 773;

inline void requestApplicationRestart()
{
    QCoreApplication::exit(RestartExitCode);
}

} // namespace fluent::gallery

#endif // GALLERYAPPLICATIONLIFECYCLE_H
