#include "GallerySpatialSupportBadge.h"

#include "components/status_info/ToolTip.h"
#include "viewmodel/GallerySettings.h"

namespace fluent::gallery {

GallerySpatialSupportBadge::GallerySpatialSupportBadge(QWidget* parent) : InfoBadge(parent)
{
    setAccessibleName(tr("3D support"));
    setDisplayMode(InfoBadgeDisplayMode::Icon);
    setIconGlyph(Typography::Icons::Info);
    setIconGlyphSize(Typography::IconSize::Standard);
    setBadgeHeight(24);
    setFixedSize(24, 24);
    setCustomBackgroundColor(Qt::transparent);
    setAttribute(Qt::WA_NoMousePropagation);
    connect(&GallerySettings::instance(), &GallerySettings::spatialAvailabilityChanged, this,
            &GallerySpatialSupportBadge::refresh);
    refresh();
}

void GallerySpatialSupportBadge::onThemeUpdated()
{
    InfoBadge::onThemeUpdated();
    refresh();
}

void GallerySpatialSupportBadge::refresh()
{
    const auto& settings = GallerySettings::instance();
    const bool fallback = !settings.spatialAvailabilityPending() && !settings.spatialAvailable();
    setStatus(fallback ? InfoBadgeStatus::Critical : InfoBadgeStatus::Informational);
    const auto& colors = themeColorsRef();
    setCustomTextColor(fallback ? colors.systemCritical : colors.textSecondary);
    const QString text = fallback ? tr("3D is unavailable on this device.\n%1")
                                        .arg(settings.spatialUnavailableReason().isEmpty()
                                                 ? tr("The Gallery is using 2D.")
                                                 : settings.spatialUnavailableReason())
                                  : tr("3D requires a GPU with hardware acceleration.");
    setAccessibleDescription(text);
    status_info::ToolTip::attach(this, text);
}

} // namespace fluent::gallery
