#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Dedicated scatter chart widget with a fixed Scatter presentation.
 * References external ChartModel data without taking ownership.
 * zh_CN: 专用散点图控件，图表类型固定为散点图，关联外部 ChartModel 而不接管所有权。
 */
class ScatterChart : public ChartView {
    Q_OBJECT
public:
    explicit ScatterChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
