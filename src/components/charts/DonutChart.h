#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Dedicated donut chart widget with a fixed Donut presentation.
 * References external ChartModel data without taking ownership and draws only the first model (prefer setModel()).
 * zh_CN: 专用环形图控件，图表类型固定为环形图，关联外部 ChartModel 而不接管所有权；仅绘制第一个模型（建议使用 setModel()）。
 */
class DonutChart : public ChartView {
    Q_OBJECT
public:
    explicit DonutChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
