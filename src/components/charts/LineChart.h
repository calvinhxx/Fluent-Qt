#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Line chart widget backed by a ChartModel.
 * zh_CN: 折线图控件，通过 ChartModel 提供数据。
 */
class LineChart : public ChartView {
    Q_OBJECT
public:
    explicit LineChart(QWidget* parent = nullptr);
};
} // namespace fluent::charts
