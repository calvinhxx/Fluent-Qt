#ifndef APPICON_H
#define APPICON_H

#include <QIcon>
#include <QPixmap>

namespace fluent::gallery::appicon {

QIcon icon();
QPixmap pixmap(int logicalSize, qreal devicePixelRatio = 1.0);

} // namespace fluent::gallery::appicon

#endif // APPICON_H
