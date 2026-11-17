#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Area chart widget backed by a ChartModel.
 * zh_CN: 面积图控件，通过 ChartModel 提供数据。
 */
class AreaChart : public ChartView {
    Q_OBJECT
public:
    explicit AreaChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
