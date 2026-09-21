#include "GallerySpatialController.h"

#include <QWidget>
#include "view/support/GalleryDepth.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {
void GallerySpatialController::prepareApplicationStyle() {}
struct GallerySpatialController::Private {};

GallerySpatialController::GallerySpatialController(QWidget* window, navigation::NavigationView*)
    : QObject(window), d(new Private)
{
    setObjectName(QStringLiteral("gallerySpatialController"));
    depth::setEnabled(window, false);
    disableSpatial(tr("This build provides the 2D Gallery."));
}

GallerySpatialController::~GallerySpatialController() = default;
bool GallerySpatialController::transitionRunning() const
{
    return false;
}
QVariantMap GallerySpatialController::renderingStatistics() const
{
    return {};
}
void GallerySpatialController::cancelTransition() {}
void GallerySpatialController::startPresentation() {}
void GallerySpatialController::applyMode(bool) {}
void GallerySpatialController::checkRenderer() {}
void GallerySpatialController::releaseOversizedPresentation() {}
bool GallerySpatialController::eventFilter(QObject*, QEvent*)
{
    return false;
}
void GallerySpatialController::disableSpatial(const QString& reason)
{
    GallerySettings::instance().setSpatialAvailability(false, reason);
}
QPoint GallerySpatialController::projectedPosition(const QWidget* widget,
                                                   const QPoint& position) const
{
    return widget->mapTo(qobject_cast<QWidget*>(parent()), position);
}
} // namespace fluent::gallery
