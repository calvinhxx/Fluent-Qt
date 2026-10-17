#pragma once

#include "components/foundation/FluentElement.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "design/MenuTokens_p.h"

namespace fluent::menus_toolbars::detail {

inline void paintMenuSurface(QPainter& painter, const QRect& rect, const FluentElement& theme, bool shadow = true)
{
    const auto& colors = theme.themeColorsRef();
    const int radius = theme.themeRadius().overlay;
    if (shadow) {
        overlay::paintLayeredShadow(painter, rect, radius, theme.themeShadow(menu_tokens::shadowLevel),
            menu_tokens::shadowIntensity);
    }
    painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.border = colors.strokeCard;
    surface.radius = radius;
    painting::paintRoundedSurface(painter, QRectF(rect), surface);
}

} // namespace fluent::menus_toolbars::detail
