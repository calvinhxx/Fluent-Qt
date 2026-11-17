#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Scatter chart widget backed by a ChartModel.
 * zh_CN: 散点图控件，通过 ChartModel 提供数据。
 */
class ScatterChart : public ChartView {
    Q_OBJECT
public:
    explicit ScatterChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
