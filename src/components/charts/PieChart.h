#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Dedicated pie chart widget with a fixed Pie presentation.
 * References external ChartModel data without taking ownership and draws only the first model (prefer setModel()).
 * zh_CN: 专用饼图控件，图表类型固定为饼图，关联外部 ChartModel 而不接管所有权；仅绘制第一个模型（建议使用 setModel()）。
 */
class PieChart : public ChartView {
    Q_OBJECT
public:
    explicit PieChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
