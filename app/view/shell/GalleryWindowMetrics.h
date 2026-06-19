#ifndef GALLERYWINDOWMETRICS_H
#define GALLERYWINDOWMETRICS_H

#include <QMargins>
#include <QtGlobal>

#include "design/Breakpoints.h"

namespace fluent::gallery::metrics {

struct AppWindow {
    static constexpr int MinWidth = 460;
    static constexpr int MinHeight = Breakpoints::MinWindowHeight;
    static constexpr int InitialWidth = 1180;
    static constexpr int InitialHeight = 760;
    static constexpr int LeftPanelReserve = 160;
};

struct Navigation {
    static constexpr int CompactThresholdWidth = Breakpoints::Small;
    static constexpr int ExpandedThresholdWidth = Breakpoints::Medium + 1;
    static constexpr int CompactPaneWidth = Breakpoints::NavigationPaneCompactWidth;
    static constexpr int ExpandedPaneWidth = 320;
};

struct TitleBar {
    static constexpr int Height = 42;
    static constexpr int HorizontalMargin = 8;
    static constexpr int ItemGap = 8;
    static constexpr int ButtonSize = 24;
    static constexpr int ButtonIconSize = 12;
    static constexpr int AppIconSize = 18;
    static constexpr int TitleWidth = 144;
    static constexpr int TitleHeight = 24;
    static constexpr int SearchMaxWidth = 360;
    static constexpr int SearchMinWidth = 180;
    static constexpr int SearchHeight = 28;
    static constexpr int SearchEdgeGap = 12;
    static constexpr int ToolTipGap = 4;

    static int leadingOffset(int systemReservedLeadingWidth)
    {
        return systemReservedLeadingWidth > 0
            ? systemReservedLeadingWidth + HorizontalMargin
            : HorizontalMargin;
    }

    static int leadingChromeRight(int systemReservedLeadingWidth,
                                  bool showAppIcon,
                                  bool showTitle,
                                  qreal backReveal)
    {
        int right = leadingOffset(systemReservedLeadingWidth)
                    + qRound(backReveal * (ButtonSize + ItemGap))
                    + ButtonSize;
        if (showAppIcon)
            right += ItemGap + AppIconSize;
        if (showTitle)
            right += ItemGap + TitleWidth;
        return right;
    }

    static int searchLeftBound(int systemReservedLeadingWidth,
                               bool showAppIcon,
                               bool showTitle,
                               qreal backReveal)
    {
        return leadingChromeRight(systemReservedLeadingWidth, showAppIcon, showTitle, backReveal) + ItemGap;
    }

    static int searchRightBound(int barWidth, int systemReservedTrailingWidth)
    {
        return barWidth - systemReservedTrailingWidth - HorizontalMargin;
    }

    static int searchAvailableWidth(int leftBound, int rightBound)
    {
        return rightBound - leftBound;
    }

    static bool canShowSearch(int availableWidth)
    {
        return availableWidth >= SearchMinWidth;
    }

    static int searchWidth(int availableWidth)
    {
        return qBound(SearchMinWidth,
                      availableWidth - SearchEdgeGap,
                      SearchMaxWidth);
    }

    static int searchX(int barWidth, int width, int leftBound, int rightBound)
    {
        const int centeredX = (barWidth - width) / 2;
        return centeredX >= leftBound && centeredX <= rightBound - width
            ? centeredX
            : leftBound;
    }
};

struct Drawer {
    static QMargins titleBarAvoidanceMargins()
    {
#ifdef Q_OS_MAC
        return QMargins(0, TitleBar::Height, 0, 0);
#else
        return QMargins();
#endif
    }
};

} // namespace fluent::gallery::metrics

#endif // GALLERYWINDOWMETRICS_H
