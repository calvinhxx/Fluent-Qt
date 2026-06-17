#ifndef GALLERYNAVIGATIONMETRICS_H
#define GALLERYNAVIGATIONMETRICS_H

#include <QMargins>
#include <QtCore/qnamespace.h>

namespace fluent::gallery {

/**
 * @brief Item-model roles shared by the navigation pane and its row delegate.
 * zh_CN: 导航面板与其行委托共享的 item-model 角色。
 */
enum GalleryNavigationRole {
    RouteIdRole = Qt::UserRole + 1,
    KindRole,
    ParentRouteIdRole,
    IconGlyphRole,
    IndicatorInsetRole
};

// Shared layout metrics for navigation rows, the row delegate, and the compact flyout.
// zh_CN: 导航行、行委托与紧凑 flyout 共享的布局度量。
constexpr int kSectionHeight = 32;
constexpr int kRouteHeight = 36;
constexpr int kCompactPaneWidth = 48;
constexpr int kCompactFlyoutHorizontalOffset = 8;
constexpr int kCompactFlyoutVerticalOffset = -4;
constexpr int kCompactFlyoutEntranceOffset = 8;
constexpr int kCompactFlyoutWindowMargin = 12;
constexpr QMargins kCompactFlyoutContentMargins(3, 4, 3, 4);
constexpr qreal kRowLeftInset = 4.0;
constexpr qreal kRowRightInset = 12.0;
constexpr qreal kRowVerticalInset = 2.0;
constexpr qreal kContentStart = 12.0;
constexpr qreal kIconAreaWidth = 20.0;
constexpr qreal kIconTextGap = 11.0;
constexpr qreal kTextStart = kContentStart + kIconAreaWidth + kIconTextGap;
constexpr qreal kChevronAreaWidth = 28.0;
constexpr qreal kChevronRightInset = 6.0;  // Gap from the chevron to the row's right edge; tightened slightly. zh_CN: 箭头到行右缘的间距，略收紧。
constexpr qreal kTextRightGap = 8.0;
constexpr qreal kSelectionIndicatorWidth = 3.0;
constexpr qreal kSelectionIndicatorHeight = 14.0;
constexpr qreal kSelectionIndicatorTextGap = 8.0;
constexpr int kRouteTextPixelSize = 13;
constexpr qreal kSettingsIconRotationDegrees = 360.0;

} // namespace fluent::gallery

#endif // GALLERYNAVIGATIONMETRICS_H
