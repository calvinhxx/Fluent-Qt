#pragma once
#include "ChartView.h"

namespace fluent::charts {
/**
 * @brief Compact sparkline chart widget backed by a ChartModel.
 * zh_CN: 紧凑型趋势图控件，通过 ChartModel 提供数据。
 */
class Sparkline : public ChartView {
    Q_OBJECT
public:
    explicit Sparkline(QWidget* parent = nullptr);
};
} // namespace fluent::charts
