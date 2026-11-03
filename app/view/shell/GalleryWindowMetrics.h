#ifndef GALLERYWINDOWMETRICS_H
#define GALLERYWINDOWMETRICS_H

#include <QMargins>
#include <QSize>
#include <QtGlobal>

#include "design/Breakpoints.h"
#include "design/Typography.h"

namespace fluent::gallery::metrics {

struct AppWindow {
    static constexpr int MinWidth = 460;
    static constexpr int MinHeight = Breakpoints::MinWindowHeight;
    static constexpr int InitialWidth = 1180;
    static constexpr int InitialHeight = 760;
    static constexpr int InitialWidthPercent = 72;
    static constexpr int InitialHeightPercent = 78;
    static constexpr int InitialMinWidth = 900;
    static constexpr int InitialMinHeight = 600;
    static constexpr int InitialMaxWidth = 1440;
    static constexpr int InitialMaxHeight = 900;
    static constexpr int LeftPanelReserve = 160;

    // The minimum is fixed in native pixels, so a larger interface zoom leaves
    // it fewer logical pixels, as in a browser window; it never exceeds the screen.
    // zh_CN: 最小尺寸以原生像素固定，界面缩放越大对应的逻辑像素越少，与浏览器窗口一致；不超过屏幕可用区域。
    static QSize minimumSize(const QSize& availableSize, int zoomPercent)
    {
        const int percent = qMax(1, zoomPercent);
        const QSize minimum((MinWidth * 100 + percent - 1) / percent, (MinHeight * 100 + percent - 1) / percent);
        if (!availableSize.isValid() || availableSize.isEmpty())
            return minimum;
        return minimum.boundedTo(availableSize);
    }
};

struct Navigation {
    static constexpr int CompactThresholdWidth = Breakpoints::Small;
    static constexpr int ExpandedThresholdWidth = Breakpoints::Medium + 1;
    static constexpr int CompactPaneWidth = Breakpoints::NavigationPaneCompactWidth;
    static constexpr int ExpandedPaneWidth = 260;
};

struct TitleBar {
    static constexpr int Height = 42;
    static constexpr int HorizontalMargin = 8;
    static constexpr int ItemGap = 8;
    static constexpr int ButtonSize = 24;
    static constexpr int ButtonIconSize = Typography::IconSize::Standard;
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
        return systemReservedLeadingWidth > 0 ? systemReservedLeadingWidth + HorizontalMargin : HorizontalMargin;
    }

    static int leadingChromeRight(int systemReservedLeadingWidth,
        bool showAppIcon,
        bool showTitle,
        qreal backReveal,
        bool showMenu = true)
    {
        int right = leadingOffset(systemReservedLeadingWidth) + qRound(backReveal * (ButtonSize + ItemGap));
        if (showMenu)
            right += ButtonSize;
        if (showAppIcon)
            right += (showMenu ? ItemGap : 0) + AppIconSize;
        if (showTitle)
            right += ItemGap + TitleWidth;
        return right;
    }

    static int searchLeftBound(int systemReservedLeadingWidth,
        bool showAppIcon,
        bool showTitle,
        qreal backReveal,
        bool showMenu = true)
    {
        return leadingChromeRight(systemReservedLeadingWidth, showAppIcon, showTitle, backReveal, showMenu) + ItemGap;
    }

    static int searchRightBound(int barWidth, int systemReservedTrailingWidth)
    {
        return barWidth - systemReservedTrailingWidth - HorizontalMargin;
    }

    static int searchAvailableWidth(int leftBound, int rightBound) { return rightBound - leftBound; }

    static bool canShowSearch(int availableWidth) { return availableWidth >= SearchMinWidth; }

    static int searchWidth(int availableWidth)
    {
        return qBound(SearchMinWidth, availableWidth - SearchEdgeGap, SearchMaxWidth);
    }

    static int searchX(int barWidth, int width, int leftBound, int rightBound)
    {
        const int centeredX = (barWidth - width) / 2;
        return centeredX >= leftBound && centeredX <= rightBound - width ? centeredX : leftBound;
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
