#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Compact sparkline chart widget with a fixed Sparkline presentation.
 * References external ChartModel data without taking ownership.
 * zh_CN: 紧凑型微型趋势图控件，图表类型固定，关联外部 ChartModel 而不接管所有权。
 */
class Sparkline : public ChartView {
    Q_OBJECT
public:
    explicit Sparkline(QWidget* parent = nullptr);
};
} // namespace fluent::charts
