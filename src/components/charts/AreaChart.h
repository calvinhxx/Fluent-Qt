#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Dedicated area chart widget with a fixed Area presentation.
 * References external ChartModel data without taking ownership.
 * zh_CN: 专用面积图控件，图表类型固定为面积图，关联外部 ChartModel 而不接管所有权。
 */
class AreaChart : public ChartView {
    Q_OBJECT
public:
    explicit AreaChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
