#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Dedicated line chart widget with a fixed Line presentation.
 * References external ChartModel data without taking ownership.
 * zh_CN: 专用折线图控件，图表类型固定为折线图，关联外部 ChartModel 而不接管所有权。
 */
class LineChart : public ChartView {
    Q_OBJECT
public:
    explicit LineChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
