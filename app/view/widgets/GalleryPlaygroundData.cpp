#include "GalleryPlaygroundData.h"

#include "components/charts/ChartView.h"
#include "components/collections/DataGrid.h"
#include "components/collections/TreeView.h"
#include <QAbstractItemView>
#include <QHeaderView>
#include <QJsonArray>
#include <QStandardItemModel>
#include <QTableView>
#include <cmath>
#include <limits>

namespace fluent::gallery::playground_data {
namespace {
QList<QModelIndex> rowsOf(const QAbstractItemModel* model)
{
    QList<QModelIndex> rows;
    const auto visit = [&](const auto& self, const QModelIndex& parent) -> void {
        if (!model)
            return;
        for (int row = 0; row < model->rowCount(parent); ++row) {
            const auto index = model->index(row, 0, parent);
            rows.append(index);
            self(self, index);
        }
    };
    visit(visit, {});
    return rows;
}

QString modelIndexCode(QModelIndex index, bool python)
{
    QStringList path;
    while (index.isValid()) {
        path.prepend(QStringLiteral("%1, %2").arg(index.row()).arg(index.column()));
        index = index.parent();
    }
    QString result;
    for (const auto& step : path)
        result = (python ? QStringLiteral("model.index(") : QStringLiteral("model->index(")) + step
            + (result.isEmpty() ? QString() : QStringLiteral(", ") + result) + QLatin1Char(')');
    return result;
}

QString rowLabel(QModelIndex index)
{
    QStringList labels;
    while (index.isValid()) {
        labels.prepend(index.data().toString());
        index = index.parent();
    }
    return labels.join(QStringLiteral(" / "));
}

charts::ChartModel* chartModel(QObject* target)
{
    auto* chart = qobject_cast<charts::ChartView*>(target);
    return chart ? chart->model() : nullptr;
}

QString number(double value, bool python)
{
    if (std::isnan(value))
        return python ? QStringLiteral("float(\"nan\")") : QStringLiteral("std::numeric_limits<double>::quiet_NaN()");
    return QString::number(value, 'g', 17);
}
} // namespace

bool handles(const QJsonObject& field)
{
    const auto adapter = field.value("adapter").toString();
    return adapter == "table-data" || adapter.startsWith("chart-") || adapter.startsWith("grid-sort-")
        || adapter == "grid-column-width" || adapter == "grid-column-pinned" || adapter == "grid-sections-movable"
        || adapter == "tree-selection-indicator" || adapter == "view-editable";
}

QJsonObject resolve(QJsonObject field, QObject* target)
{
    const auto adapter = field.value("adapter").toString();
    if (adapter == "table-data") {
        auto* view = qobject_cast<QAbstractItemView*>(target);
        QJsonArray labels;
        if (view && field.value("fixedRows").toBool())
            for (const auto& row : rowsOf(view->model()))
                labels.append(rowLabel(row));
        field["rowLabels"] = labels;
    } else if (adapter == "chart-current" || adapter == "grid-sort-column") {
        QJsonArray choices;
        if (adapter == "chart-current") {
            choices.append(QJsonObject{{"value", -1}, {"label", "No selection"}, {"enabled", true}});
            if (auto* model = chartModel(target))
                for (int row = 0; row < model->rowCount(); ++row)
                    choices.append(QJsonObject{{"value", row},
                        {"label",
                            QStringLiteral("%1 · %2").arg(model->labelAt(row).isEmpty()
                                    ? QString::number(model->pointAt(row).x())
                                    : model->labelAt(row),
                                QString::number(model->pointAt(row).y()))},
                        {"enabled", true}});
        } else if (auto* view = qobject_cast<QTableView*>(target)) {
            for (int col = 0; col < view->model()->columnCount(); ++col)
                choices.append(QJsonObject{{"value", col},
                    {"label", view->model()->headerData(col, Qt::Horizontal).toString()}, {"enabled", true}});
        }
        field["choices"] = choices;
        field["minimum"] = adapter == "chart-current" ? -1 : 0;
        field["maximum"] = choices.isEmpty() ? -1 : choices.last().toObject().value("value").toInt();
    }
    return field;
}

QJsonValue read(const QJsonObject& field, QObject* target)
{
    const auto adapter = field.value("adapter").toString();
    if (adapter == "chart-current")
        return qobject_cast<charts::ChartView*>(target)->currentRow();
    if (adapter == "chart-name")
        return chartModel(target) ? chartModel(target)->name() : QString();
    if (adapter == "chart-data") {
        QJsonArray rows;
        if (auto* model = chartModel(target))
            for (int row = 0; row < model->rowCount(); ++row) {
                const auto point = model->pointAt(row);
                rows.append(QJsonArray{point.x(), std::isfinite(point.y()) ? QJsonValue(point.y()) : QJsonValue(),
                    model->labelAt(row)});
            }
        return rows;
    }
    auto* view = qobject_cast<QAbstractItemView*>(target);
    if (adapter == "view-editable")
        return view->editTriggers() != QAbstractItemView::NoEditTriggers;
    if (adapter == "grid-column-width") {
        auto* grid = qobject_cast<QTableView*>(view);
        return grid ? grid->columnWidth(field.value("index").toInt()) : 0;
    }
    if (adapter == "grid-column-pinned") {
        auto* grid = qobject_cast<collections::DataGrid*>(view);
        return grid ? grid->isColumnPinned(field.value("index").toInt()) : false;
    }
    if (adapter == "grid-sections-movable") {
        auto* grid = qobject_cast<QTableView*>(view);
        return grid && grid->horizontalHeader() ? grid->horizontalHeader()->sectionsMovable() : false;
    }
    if (adapter == "tree-selection-indicator") {
        auto* tree = qobject_cast<collections::TreeView*>(view);
        return tree ? tree->selectionIndicatorVisible() : false;
    }
    if (adapter == "grid-sort-column")
        return qobject_cast<QTableView*>(view)->horizontalHeader()->sortIndicatorSection();
    if (adapter == "grid-sort-order")
        return qobject_cast<QTableView*>(view)->horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder
            ? "AscendingOrder"
            : "DescendingOrder";
    QJsonArray rows;
    if (view && view->model())
        for (const auto& index : rowsOf(view->model())) {
            QJsonArray cells;
            const auto columns = field.value("columns").toArray();
            for (int col = 0; col < columns.size(); ++col) {
                const auto column = columns[col].toObject();
                cells.append(index.sibling(index.row(), column.value("modelColumn").toInt(col))
                        .data(column.value("role").toInt(Qt::DisplayRole))
                        .toString());
            }
            rows.append(cells);
        }
    return rows;
}

bool validate(const QJsonObject& field, const QJsonValue& value, QObject* target, QString* error)
{
    if (field.value("type") != "table")
        return true;
    const auto fail = [&](const QString& message) {
        if (error)
            *error = message;
        return false;
    };
    if (!value.isArray() || value.toArray().size() > field.value("maximumRows").toInt(32))
        return fail(QStringLiteral("Too many data rows."));
    const auto rows = value.toArray();
    if (field.value("fixedRows").toBool() && rows.size() != read(field, target).toArray().size())
        return fail(QStringLiteral("Keep the tree structure when editing node text."));
    const auto columns = field.value("columns").toArray();
    double previousX = -std::numeric_limits<double>::infinity();
    for (const auto& row : rows) {
        if (!row.isArray() || row.toArray().size() != columns.size())
            return fail(QStringLiteral("Each data row must contain all columns."));
        const auto cells = row.toArray();
        for (int col = 0; col < cells.size(); ++col) {
            const auto column = columns[col].toObject();
            if (column.value("type") == "number") {
                if (cells[col].isNull() && column.value("nullable").toBool())
                    continue;
                if (!cells[col].isDouble() || !std::isfinite(cells[col].toDouble()))
                    return fail(QStringLiteral("Enter a finite number."));
            } else if (!cells[col].isString() || cells[col].toString().size() > 160)
                return fail(QStringLiteral("Cell text must not exceed 160 characters."));
        }
        if (field.value("adapter") == "chart-data") {
            const double x = cells[0].toDouble();
            if (x < previousX)
                return fail(QStringLiteral("X values must be in ascending order."));
            previousX = x;
        }
    }
    return true;
}

void write(const QJsonObject& field, const QJsonValue& value, QObject* target)
{
    const auto adapter = field.value("adapter").toString();
    if (adapter == "chart-current") {
        qobject_cast<charts::ChartView*>(target)->setCurrentPoint(0, value.toInt(-1));
    } else if (adapter == "chart-name") {
        if (auto* model = chartModel(target))
            model->setName(value.toString());
    } else if (adapter == "chart-data") {
        auto* chart = qobject_cast<charts::ChartView*>(target);
        auto* model = chartModel(target);
        if (!model)
            return;
        QVector<QPointF> points;
        QStringList labels;
        for (const auto& row : value.toArray()) {
            const auto cells = row.toArray();
            points.append(QPointF(cells[0].toDouble(),
                cells[1].isNull() ? std::numeric_limits<double>::quiet_NaN() : cells[1].toDouble()));
            labels.append(cells[2].toString());
        }
        const int selected = chart->currentRow();
        if (model->setPoints(points, labels))
            chart->setCurrentPoint(0, selected < points.size() ? selected : -1);
    } else if (adapter == "view-editable") {
        qobject_cast<QAbstractItemView*>(target)->setEditTriggers(value.toBool()
                ? QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                : QAbstractItemView::NoEditTriggers);
    } else if (adapter == "grid-column-width") {
        if (auto* grid = qobject_cast<QTableView*>(target))
            grid->setColumnWidth(field.value("index").toInt(), value.toInt());
    } else if (adapter == "grid-column-pinned") {
        if (auto* grid = qobject_cast<collections::DataGrid*>(target))
            grid->setColumnPinned(field.value("index").toInt(), value.toBool());
    } else if (adapter == "grid-sections-movable") {
        if (auto* grid = qobject_cast<QTableView*>(target); grid && grid->horizontalHeader())
            grid->horizontalHeader()->setSectionsMovable(value.toBool());
    } else if (adapter == "tree-selection-indicator") {
        if (auto* tree = qobject_cast<collections::TreeView*>(target))
            tree->setSelectionIndicatorVisible(value.toBool());
    } else if (adapter.startsWith("grid-sort-")) {
        auto* view = qobject_cast<QTableView*>(target);
        if (!view->isSortingEnabled())
            return;
        view->sortByColumn(
            adapter == "grid-sort-column" ? value.toInt() : view->horizontalHeader()->sortIndicatorSection(),
            adapter == "grid-sort-order"
                ? (value == "AscendingOrder" ? Qt::AscendingOrder : Qt::DescendingOrder)
                : view->horizontalHeader()->sortIndicatorOrder());
    } else if (adapter == "table-data") {
        auto* view = qobject_cast<QAbstractItemView*>(target);
        auto* model = qobject_cast<QStandardItemModel*>(view->model());
        if (!model)
            return;
        const auto data = value.toArray();
        if (!field.value("fixedRows").toBool())
            model->setRowCount(data.size());
        const auto indexes = rowsOf(model);
        for (int row = 0; row < data.size() && row < indexes.size(); ++row) {
            const auto cells = data[row].toArray();
            for (int col = 0; col < cells.size(); ++col) {
                const auto column = field.value("columns").toArray()[col].toObject();
                const auto index = indexes[row].sibling(indexes[row].row(), column.value("modelColumn").toInt(col));
                const int role = column.value("role").toInt(Qt::EditRole);
                if (index.data(role).toString() != cells[col].toString())
                    model->setData(index, cells[col].toString(), role);
            }
        }
    }
}

QString code(const QJsonObject& field, const QJsonValue& value, QObject* target, bool python, const Quote& quote)
{
    const auto adapter = field.value("adapter").toString();
    const QString arrow = python ? "." : "->", end = python ? ")\n" : ");\n";
    if (adapter == "chart-current")
        return "control" + arrow + "setCurrentPoint(0, " + QString::number(value.toInt()) + end;
    if (adapter == "chart-name")
        return "model" + arrow + "setName(" + quote(value.toString(), python) + end;
    if (adapter == "chart-data") {
        QStringList points, labels;
        for (const auto& row : value.toArray()) {
            const auto cells = row.toArray();
            points.append("QPointF(" + number(cells[0].toDouble(), python) + ", "
                + number(cells[1].isNull() ? std::numeric_limits<double>::quiet_NaN() : cells[1].toDouble(), python)
                + ")");
            labels.append(quote(cells[2].toString(), python));
        }
        return "model" + arrow + "setPoints(" + (python ? "[" : "{") + points.join(", ") + (python ? "], [" : "}, {")
            + labels.join(", ") + (python ? "]" : "}") + end;
    }
    if (adapter == "view-editable") {
        const QString prefix = python ? "QAbstractItemView.EditTrigger." : "QAbstractItemView::";
        return "control" + arrow + "setEditTriggers("
            + (value.toBool() ? prefix + "DoubleClicked | " + prefix + "EditKeyPressed" : prefix + "NoEditTriggers")
            + end;
    }
    if (adapter == "grid-column-width")
        return "control" + arrow + "setColumnWidth(" + QString::number(field.value("index").toInt()) + ", "
            + QString::number(value.toInt()) + end;
    if (adapter == "grid-column-pinned")
        return "control" + arrow + "setColumnPinned(" + QString::number(field.value("index").toInt()) + ", "
            + (value.toBool() ? (python ? "True" : "true") : (python ? "False" : "false")) + end;
    if (adapter == "grid-sections-movable")
        return "control" + arrow + "horizontalHeader()" + arrow + "setSectionsMovable("
            + (value.toBool() ? (python ? "True" : "true") : (python ? "False" : "false")) + end;
    if (adapter == "tree-selection-indicator")
        return "control" + arrow + "setSelectionIndicatorVisible("
            + (value.toBool() ? (python ? "True" : "true") : (python ? "False" : "false")) + end;
    if (adapter == "grid-sort-column" || adapter == "grid-sort-order") {
        // Both fields emit one actual header state, independent of property order.
        if (adapter == "grid-sort-order")
            return {};
        auto* view = qobject_cast<QTableView*>(target);
        if (!view->isSortingEnabled())
            return {};
        const QString order =
            view->horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder ? "AscendingOrder" : "DescendingOrder";
        return "control" + arrow + "sortByColumn(" + QString::number(view->horizontalHeader()->sortIndicatorSection())
            + ", " + (python ? "Qt.SortOrder." : "Qt::") + order + end;
    }
    QString result;
    const auto data = value.toArray();
    const auto* view = qobject_cast<QAbstractItemView*>(target);
    if (!field.value("fixedRows").toBool())
        result += "model" + arrow + "setRowCount(" + QString::number(data.size()) + end;
    const auto indexes = rowsOf(view->model());
    for (int row = 0; row < data.size() && row < indexes.size(); ++row)
        for (int col = 0; col < data[row].toArray().size(); ++col) {
            const auto column = field.value("columns").toArray()[col].toObject();
            const auto index = indexes[row].sibling(indexes[row].row(), column.value("modelColumn").toInt(col));
            const QString role = column.value(python ? "pythonRole" : "cppRole").toString();
            result += "model" + arrow + "setData(" + modelIndexCode(index, python) + ", "
                + quote(data[row].toArray()[col].toString(), python)
                + (role.isEmpty() ? QString() : QStringLiteral(", ") + role) + end;
        }
    return result;
}
} // namespace fluent::gallery::playground_data
