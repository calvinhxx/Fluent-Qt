#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Dedicated vertical bar chart widget with a fixed Bar presentation.
 * References external ChartModel data without taking ownership.
 * zh_CN: 专用纵向柱状图控件，图表类型固定为柱状图，关联外部 ChartModel 而不接管所有权。
 */
class BarChart : public ChartView {
    Q_OBJECT
public:
    explicit BarChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
