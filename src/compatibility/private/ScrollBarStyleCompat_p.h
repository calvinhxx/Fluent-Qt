#pragma once

#include <QProxyStyle>
#include <QScrollBar>
#include <QStyleOptionSlider>
#include <functional>
#include <utility>

namespace compatibility::detail {

// QScrollBar leaves horizontal RTL mirroring to the style geometry.
// zh_CN: QScrollBar 将水平 RTL 镜像留给样式几何，绘制与分页须使用同一数值方向。
inline bool scrollBarValueReversed(const QStyleOptionSlider& option)
{
    return option.upsideDown != (option.orientation == Qt::Horizontal && option.direction == Qt::RightToLeft);
}

// Feed component-owned geometry into Qt's scrollbar interaction without replacing
// its drag, tracking, repeat-action or accessibility state machines.
// zh_CN: 将组件几何接入 Qt 滚动条交互，保留拖动、tracking、重复动作及可访问性状态机。
class ScrollBarStyle final : public QProxyStyle {
public:
    using GeometryProvider = std::function<QRect(const QStyleOptionSlider&, SubControl)>;

    explicit ScrollBarStyle(GeometryProvider geometry) : m_geometry(std::move(geometry)) {}

    using QProxyStyle::polish;

    void polish(QWidget* widget) override
    {
        QProxyStyle::polish(widget);
        // Native styles may suppress the background for fully painted scrollbars.
        // Preserve parent propagation for this component's transparent surface.
        // zh_CN: 原生样式可能关闭滚动条的背景透传；此透明自绘表面仍需要父背景。
        if (qobject_cast<QScrollBar*>(widget))
            widget->setAttribute(Qt::WA_NoSystemBackground, false);
    }

    QRect subControlRect(ComplexControl control,
        const QStyleOptionComplex* option,
        SubControl subControl,
        const QWidget* widget = nullptr) const override
    {
        const auto* slider = qstyleoption_cast<const QStyleOptionSlider*>(option);
        if (control != CC_ScrollBar || !slider)
            return QProxyStyle::subControlRect(control, option, subControl, widget);

        if (subControl == SC_ScrollBarGroove || subControl == SC_ScrollBarSlider)
            return m_geometry(*slider, subControl);
        if (subControl != SC_ScrollBarSubPage && subControl != SC_ScrollBarAddPage)
            return {};

        const QRect thumb = m_geometry(*slider, SC_ScrollBarSlider);
        QRect page = slider->rect;
        const bool horizontal = slider->orientation == Qt::Horizontal;
        // Page actions change values directly, so map them to the same effective
        // value direction as the painted thumb, including inversion and RTL.
        // zh_CN: 分页动作直接改变数值，应与滑块绘制的反向及 RTL 数值方向保持一致。
        const bool before = (subControl == SC_ScrollBarSubPage) != scrollBarValueReversed(*slider);
        if (horizontal) {
            if (before)
                page.setRight(thumb.left() - 1);
            else
                page.setLeft(thumb.right() + 1);
        } else {
            if (before)
                page.setBottom(thumb.top() - 1);
            else
                page.setTop(thumb.bottom() + 1);
        }
        return page;
    }

    SubControl hitTestComplexControl(ComplexControl control,
        const QStyleOptionComplex* option,
        const QPoint& position,
        const QWidget* widget = nullptr) const override
    {
        if (control != CC_ScrollBar || !qstyleoption_cast<const QStyleOptionSlider*>(option))
            return QProxyStyle::hitTestComplexControl(control, option, position, widget);
        if (!option->rect.contains(position))
            return SC_None;
        for (const auto subControl : {SC_ScrollBarSlider, SC_ScrollBarSubPage, SC_ScrollBarAddPage}) {
            if (subControlRect(control, option, subControl, widget).contains(position))
                return subControl;
        }
        return SC_None;
    }

private:
    GeometryProvider m_geometry;
};

} // namespace compatibility::detail
