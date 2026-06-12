#ifndef OVERLAYSCROLLCHROME_H
#define OVERLAYSCROLLCHROME_H

#include <QRect>
#include <QScrollBar>
#include <QString>
#include <QWidget>

#include "ScrollBar.h"

namespace fluent::scrolling {

/**
 * @brief Shared plumbing for hosting Fluent overlay scroll bars inside views.
 * zh_CN: 供视图内嵌 Fluent 覆盖式滚动条的共享管线。
 *
 * Item views (ListView, TreeView, GridView, FlowView) suppress the native Qt
 * scroll bars and float a fluent ScrollBar above the viewport. The create,
 * suppress, mirror, and place steps are identical across hosts and live here
 * once; hosts keep only their view-specific anchor math (e.g. header offsets).
 * zh_CN: 各视图（ListView、TreeView、GridView、FlowView）压制 Qt 原生滚动条，
 * 在视口上悬浮 Fluent ScrollBar。创建/压制/镜像/摆放四步各处一致，统一收敛于此；
 * 视图只保留自身的锚点计算（如 header 偏移）。
 */

/**
 * @brief Creates a hidden overlay ScrollBar two-way value-synced with a native bar.
 * zh_CN: 创建隐藏的覆盖式 ScrollBar，并与原生滚动条建立双向值同步。
 *
 * Range/page-step mirroring is left to mirrorNativeScrollBar() so hosts can
 * re-run it from their own rangeChanged slots.
 * zh_CN: range/pageStep 镜像交由 mirrorNativeScrollBar()，便于宿主在自身
 * rangeChanged 槽中重复执行。
 */
inline ScrollBar* createOverlayScrollBar(Qt::Orientation orientation,
                                         QWidget* host,
                                         QScrollBar* native,
                                         const QString& objectName)
{
    auto* bar = new ScrollBar(orientation, host);
    bar->setObjectName(objectName);
    bar->hide();
    QObject::connect(native, &QScrollBar::valueChanged, bar, &QScrollBar::setValue);
    QObject::connect(bar, &QScrollBar::valueChanged, native, &QScrollBar::setValue);
    return bar;
}

/**
 * @brief Keeps native scroll bars allocated but permanently invisible.
 * zh_CN: 保留原生滚动条对象但使其永久不可见。
 *
 * The native bars still own the scroll model (range/value); only their
 * on-screen chrome is suppressed in favor of the fluent overlay bars.
 * zh_CN: 原生滚动条仍持有滚动模型（范围/值），仅压制其屏幕外观，由
 * Fluent 覆盖条代替显示。
 */
inline void suppressNativeScrollBars(QScrollBar* vertical, QScrollBar* horizontal)
{
    for (QScrollBar* bar : {vertical, horizontal}) {
        if (bar) {
            bar->setAttribute(Qt::WA_DontShowOnScreen, true);
            bar->hide();
        }
    }
}

/**
 * @brief Mirrors range/page-step onto the overlay bar and toggles its visibility.
 * zh_CN: 将范围/页步镜像到覆盖条并按需显隐。
 *
 * Returns true when the content overflows and the bar should also be placed.
 * zh_CN: 内容溢出需要继续摆放滚动条时返回 true。
 */
inline bool mirrorNativeScrollBar(ScrollBar* overlay, const QScrollBar* native)
{
    overlay->setRange(native->minimum(), native->maximum());
    overlay->setPageStep(native->pageStep());
    const bool needScroll = native->maximum() > native->minimum();
    overlay->setVisible(needScroll);
    return needScroll;
}

/**
 * @brief Anchors a vertical overlay bar to the host's right edge.
 * zh_CN: 将垂直覆盖条锚定到宿主右缘。
 *
 * @param top Top edge in host coordinates; hosts add their header offset here.
 *            zh_CN: 宿主坐标系中的上缘；header 偏移由宿主算入。
 */
inline void placeVerticalScrollBar(ScrollBar* bar, const QRect& hostRect,
                                   int top, int rightInset, int bottomInset)
{
    const int x = hostRect.right() - rightInset - bar->thickness() + 1;
    const int height = qMax(0, hostRect.bottom() - top - bottomInset);
    bar->setGeometry(x, top, bar->thickness(), height);
    bar->raise();
}

/**
 * @brief Anchors a horizontal overlay bar to the host's bottom edge.
 * zh_CN: 将水平覆盖条锚定到宿主底缘。
 */
inline void placeHorizontalScrollBar(ScrollBar* bar, const QRect& hostRect,
                                     int leftInset, int rightInset, int bottomInset)
{
    const int left = hostRect.left() + leftInset;
    const int width = qMax(0, hostRect.width() - leftInset - rightInset);
    const int y = hostRect.bottom() - bar->thickness() - bottomInset + 1;
    bar->setGeometry(left, y, width, bar->thickness());
    bar->raise();
}

} // namespace fluent::scrolling

#endif // OVERLAYSCROLLCHROME_H
