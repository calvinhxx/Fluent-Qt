#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Vertical bar chart widget backed by a ChartModel.
 * zh_CN: 纵向柱状图控件，通过 ChartModel 提供数据。
 */
class BarChart : public ChartView {
    Q_OBJECT
public:
    explicit BarChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
