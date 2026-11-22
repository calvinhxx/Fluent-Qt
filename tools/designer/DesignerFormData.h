#pragma once

#include <FluentQt/Charts.h>
#include <FluentQt/Collections.h>
#include <FluentQt/Navigation.h>
#include <QStringListModel>

namespace fluentqt_designer {

inline void setFormItems(fluent::navigation::Breadcrumb* control, const QStringList& items)
{
    control->setItems(items);
}

inline void setFormItems(fluent::collections::ListView* control, const QStringList& items)
{
    auto* model = control->findChild<QStringListModel*>(QStringLiteral("designerListData"), Qt::FindDirectChildrenOnly);
    if (!model) {
        model = new QStringListModel(control);
        model->setObjectName(QStringLiteral("designerListData"));
    }
    model->setStringList(items);
    control->setModel(model);
}

template <typename T> void setNavigationItems(T* control, const QStringList& items)
{
    control->clearItems();
    for (const auto& label : items)
        control->addItem(label);
}

inline void setFormSeries(fluent::charts::ChartView* chart, const QStringList& series)
{
    using namespace fluent::charts;
    auto* model = chart->findChild<ChartModel*>(QStringLiteral("designerChartData"), Qt::FindDirectChildrenOnly);
    if (!model) {
        model = new ChartModel(chart);
        model->setObjectName(QStringLiteral("designerChartData"));
    }
    QVector<QPointF> points;
    for (int i = 1; i < series.size(); ++i)
        points.append(QPointF(i, series.at(i).toDouble()));
    model->setName(series.value(0));
    model->setDataSnapshot(ChartData::fromPoints(points));
    chart->setModel(model);
}

} // namespace fluentqt_designer
