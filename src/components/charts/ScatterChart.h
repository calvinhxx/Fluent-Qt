#pragma once
#include "ChartView.h"

namespace fluent::charts {
/** @brief A dedicated scatter chart with its own bounded renderer.
 * zh_CN: 独立的散点图组件，拥有专用且受预算约束的绘制器。
 * Borrows ChartModel; presentation is fixed for the lifetime of this widget.
 * zh_CN: 借用 ChartModel；此组件的图表类型在生命周期内保持不变。
 */
class ScatterChart : public ChartView {
    Q_OBJECT
public:
    explicit ScatterChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
