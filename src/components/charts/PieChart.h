#pragma once
#include "ChartView.h"

namespace fluent::charts {
/** @brief A dedicated pie chart with its own bounded renderer.
 * zh_CN: 独立的饼图组件，拥有专用且受预算约束的绘制器。
 * Borrows ChartModel; presentation is fixed for the lifetime of this widget.
 * zh_CN: 借用 ChartModel；此组件的图表类型在生命周期内保持不变。
 * Renders only the first model. Use setModel(); additional series are not drawn.
 * zh_CN: 仅绘制第一个模型；请使用 setModel()，额外添加的序列不参与绘制。
 */
class PieChart : public ChartView {
    Q_OBJECT
public:
    explicit PieChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
