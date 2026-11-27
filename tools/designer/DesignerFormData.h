#pragma once

#include <FluentQt/Charts.h>
#include <FluentQt/Collections.h>
#include <FluentQt/Navigation.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardItemModel>
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

// Form-owned sample data. Applications can replace it with their own borrowed model.
inline bool setFormTable(fluent::collections::DataGrid* grid, const QString& data)
{
    QStringList headers;
    QList<QStringList> rows;
    if (!data.isEmpty()) {
        const auto table = QJsonDocument::fromJson(data.toUtf8()).object();
        if (!table.value("headers").isArray() || !table.value("rows").isArray())
            return false;
        for (const auto& label : table.value("headers").toArray()) {
            if (!label.isString())
                return false;
            headers.append(label.toString());
        }
        for (const auto& row : table.value("rows").toArray()) {
            if (!row.isArray())
                return false;
            QStringList cells;
            for (const auto& cell : row.toArray()) {
                if (!cell.isString())
                    return false;
                cells.append(cell.toString());
            }
            rows.append(cells);
        }
    }

    auto* model = grid->findChild<QStandardItemModel*>(QStringLiteral("designerTableData"), Qt::FindDirectChildrenOnly);
    if (!model) {
        model = new QStandardItemModel(grid);
        model->setObjectName(QStringLiteral("designerTableData"));
    }
    model->clear();
    model->setHorizontalHeaderLabels(headers);
    for (const auto& row : rows) {
        QList<QStandardItem*> items;
        for (const auto& cell : row)
            items.append(new QStandardItem(cell));
        model->appendRow(items);
    }
    grid->setModel(model);
    return true;
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
