#include "WindowGeometryCompat.h"
#include "private/PopupWindowCompat_p.h"
#include "private/WindowPlatformCompat_p.h"

void fluentPrepareWindowNormalGeometry(QWidget* window, const QRect& geometry)
{
    compatibility::detail::prepareWindowNormalGeometry(window, geometry);
}

QPoint fluentClampPopupPosition(const QWidget* popup, const QPoint& position, const QScreen* screen)
{
    return compatibility::detail::clampPopupPosition(popup, position, screen);
}
