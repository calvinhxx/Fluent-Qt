#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Donut chart widget backed by a ChartModel (renders only the first series; prefer setModel()).
 * zh_CN: 环形图控件，通过 ChartModel 提供数据（仅显示首条序列，建议用 setModel()）。
 */
class DonutChart : public ChartView {
    Q_OBJECT
public:
    explicit DonutChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
