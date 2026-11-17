#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Dedicated horizontal bar chart widget with a fixed HorizontalBar presentation.
 * References external ChartModel data without taking ownership.
 * zh_CN: 专用横向条形图控件，图表类型固定为横向条形图，关联外部 ChartModel 而不接管所有权。
 */
class HorizontalBarChart : public ChartView {
    Q_OBJECT
public:
    explicit HorizontalBarChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
