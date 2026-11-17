#ifndef OVERLAYSCROLLCHROME_H
#define OVERLAYSCROLLCHROME_H

#include <QRect>
#include <QScrollBar>
#include <QString>
#include <QWidget>

#include "ScrollBar.h"

namespace fluent::scrolling {

// Shared helpers for item views (ListView, TreeView, GridView, FlowView) that hide native
// Qt scroll bars and float Fluent ScrollBars above the viewport.

/**
 * @brief Creates a hidden overlay ScrollBar two-way value-synced with a native bar; range/pageStep are synced via mirrorNativeScrollBar().
 * zh_CN: 创建隐藏的覆盖式滚动条并与原生滚动条建立双向值同步（range/pageStep 由 mirrorNativeScrollBar() 同步）。
 */
inline ScrollBar*
createOverlayScrollBar(Qt::Orientation orientation, QWidget* host, QScrollBar* native, const QString& objectName)
{
    auto* bar = new ScrollBar(orientation, host);
    bar->setObjectName(objectName);
    bar->hide();
    QObject::connect(native, &QScrollBar::valueChanged, bar, &QScrollBar::setValue);
    QObject::connect(bar, &QScrollBar::valueChanged, native, &QScrollBar::setValue);
    return bar;
}

/**
 * @brief Keeps native scroll bars allocated as the scroll model while hiding their on-screen rendering.
 * zh_CN: 保留原生滚动条以维持滚动范围和数值状态，但禁止其在屏幕上显示。
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
 * @brief Mirrors range/pageStep onto the overlay bar and returns true when scrollable overflow exists.
 * zh_CN: 同步滚动范围与页步长到覆盖条，并在内容溢出需要显示时返回 true。
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
 * @brief Anchors a vertical overlay bar to the host's right edge; `top` allows leaving space for headers.
 * zh_CN: 将垂直覆盖条定位到宿主右缘（top 可为表头预留偏移）。
 */
inline void placeVerticalScrollBar(ScrollBar* bar, const QRect& hostRect, int top, int rightInset, int bottomInset)
{
    const int x = hostRect.right() - rightInset - bar->thickness() + 1;
    const int height = qMax(0, hostRect.bottom() - top - bottomInset);
    bar->setGeometry(x, top, bar->thickness(), height);
    bar->raise();
}

/**
 * @brief Anchors a horizontal overlay bar to the host's bottom edge.
 * zh_CN: 将水平覆盖条定位到宿主底缘。
 */
inline void
placeHorizontalScrollBar(ScrollBar* bar, const QRect& hostRect, int leftInset, int rightInset, int bottomInset)
{
    const int left = hostRect.left() + leftInset;
    const int width = qMax(0, hostRect.width() - leftInset - rightInset);
    const int y = hostRect.bottom() - bar->thickness() - bottomInset + 1;
    bar->setGeometry(left, y, width, bar->thickness());
    bar->raise();
}

} // namespace fluent::scrolling

#endif // OVERLAYSCROLLCHROME_H
