#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Horizontal bar chart widget backed by a ChartModel.
 * zh_CN: 横向条形图控件，通过 ChartModel 提供数据。
 */
class HorizontalBarChart : public ChartView {
    Q_OBJECT
public:
    explicit HorizontalBarChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
