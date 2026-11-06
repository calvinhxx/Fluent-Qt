#pragma once

#include "OverlayGeometry.h"
#include "OverlayShadow.h"
#include "components/foundation/FluentElement.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "design/Spacing.h"

namespace fluent::overlay {

// Input-owned dropdowns share a compact surface and a soft, low-spread shadow.
// zh_CN: 输入框下拉层共用紧凑表面和小范围柔和阴影。
inline void paintFieldPopupSurface(QPainter& painter, const QRect& bounds, const FluentElement& theme)
{
    const QRect card = visibleCardRect(bounds, ::Spacing::Standard);
    const qreal radius = theme.themeRadius().overlay;
    paintLayeredShadow(painter, card, radius, theme.themeShadow(Elevation::Medium), 0.4, 4, 2);
    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = theme.themeColorsRef().bgLayer;
    surface.radius = radius;
    surface.border = theme.themeColorsRef().strokeDefault;
    fluent::painting::paintRoundedSurface(painter, QRectF(card), surface);
}

} // namespace fluent::overlay
