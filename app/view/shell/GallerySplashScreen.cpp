#include "GallerySplashScreen.h"

#include "view/shell/AppIcon.h"

namespace fluent::gallery {

GallerySplashScreen::GallerySplashScreen(QWidget* parent) : SplashScreen(parent)
{
    setObjectName(QStringLiteral("gallerySplashScreen"));
    setIcon(appicon::icon());
    setIconSize(QSize(112, 112));
    setTitle(QStringLiteral("FluentQt"));
    setSubtitle(QStringLiteral("Small details. Fluent experiences."));
    setText(QStringLiteral("Preparing your workspace"));
    connect(this, &SplashScreen::dismissed, this, &QObject::deleteLater);
    connect(this, &SplashScreen::replaced, this, &QObject::deleteLater);
}

} // namespace fluent::gallery
