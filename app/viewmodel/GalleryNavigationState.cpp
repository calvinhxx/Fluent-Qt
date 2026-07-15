#include "GalleryNavigationState.h"

#include "support/logging/Log.h"

namespace fluent::gallery {

GalleryNavigationState::GalleryNavigationState(QObject* parent)
    : QObject(parent)
{
}

void GalleryNavigationState::setSelectedRouteId(const QString& routeId)
{
    if (m_selectedRouteId == routeId)
        return;
    LOG_DEBUG(QStringLiteral("GalleryNavigationState selectedRouteChanged old=%1 new=%2")
                  .arg(m_selectedRouteId, routeId));
    m_selectedRouteId = routeId;
    emit selectedRouteIdChanged(m_selectedRouteId);
}

} // namespace fluent::gallery
