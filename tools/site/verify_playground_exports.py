#!/usr/bin/env python3
"""Compile and run the exact C++/Python snippets exported by the browser audit.

Run with a Qt 6 static build, its PySide6 interpreter and matching PYTHONPATH.
The input must be a trusted local property-audit.json, since snippets are executed.
"""

import argparse
import json
import math
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

CPP = r'''
#include <FluentQt/FluentQt.h>
#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaProperty>
#include <QAbstractAnimation>
#include <QColor>
#include <QDate>
#include <QMetaEnum>
#include <QTime>
#include <QDateTime>
#include <QTreeView>
#include <QCheckBox>
#include <QHeaderView>
#include <QTableView>
#include <FluentQt/Charts.h>
#include <QComboBox>
#include <FluentQt/Collections.h>
#include <FluentQt/Design.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/MenusToolbars.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/BasicInput.h>
#include <FluentQt/Windowing.h>
#include <QPainter>
#include <memory>
#include <cmath>
using fluent::basicinput::Button;
static QString playgroundGlyphName(const QString& glyph) {
    struct Entry { const char* name; const QString* glyph; };
    static const Entry kGlyphs[] = {
        {"ChevronDown", &Typography::Icons::ChevronDown}, {"ChevronUp", &Typography::Icons::ChevronUp},
        {"ChevronRight", &Typography::Icons::ChevronRight}, {"Add", &Typography::Icons::Add}, {"CheckMark", &Typography::Icons::CheckMark},
        {"Save", &Typography::Icons::Save}, {"FavoriteStar", &Typography::Icons::FavoriteStar},
        {"Settings", &Typography::Icons::Settings}, {"Search", &Typography::Icons::Search},
        {"Mail", &Typography::Icons::Mail}, {"Home", &Typography::Icons::Home},
        {"Document", &Typography::Icons::Document}, {"Info", &Typography::Icons::Info},
        {"Warning", &Typography::Icons::Warning}, {"ErrorBadge12", &Typography::Icons::ErrorBadge12},
    };
    for (const auto& e : kGlyphs)
        if (glyph == *e.glyph) return QString::fromLatin1(e.name);
    return QStringLiteral("None");
}
static QList<QModelIndex> modelRows(const QAbstractItemModel* model, const QModelIndex& parent = {}) {
    QList<QModelIndex> rows;
    for (int row = 0; row < model->rowCount(parent); ++row) {
        const auto index = model->index(row, 0, parent);
        rows.append(index); rows.append(modelRows(model, index));
    }
    return rows;
}
class Host : public QWidget {
public:
    using QWidget::QWidget;
    QWidget* create(int index) {
        switch (index) {
@CASES@
        default: return nullptr;
        }
    }
};
int main(int argc, char** argv) {
    fluent::prepareHighDpiApplication();
    QApplication app(argc, argv);
    fluent::initializeResources();
    fluent::FluentElement::setTheme(fluent::FluentElement::Light);
    app.setFont(Typography::Styles::Body.toQFont());
    QFile input(argv[1]);
    if (!input.open(QIODevice::ReadOnly)) return 2;
    const auto cases = QJsonDocument::fromJson(input.readAll()).object().value("cases").toArray();
    const QDir output(QString::fromLocal8Bit(argv[2]));
    QJsonArray results;
    Host host;
    host.resize(320, 160);
    host.show();
    host.activateWindow();
    app.processEvents();
    QCursor::setPos(QPoint(0, 0));
    for (int index = 0; index < cases.size(); ++index) {
        const auto theme = cases[index].toObject().value("theme").toString();
        fluent::FluentElement::setTheme(theme == "dark" ? fluent::FluentElement::Dark
            : theme == "high-contrast" ? fluent::FluentElement::HighContrast : fluent::FluentElement::Light);
        const auto state = cases[index].toObject().value("state").toObject();
        const auto schema = state.value("schema").toObject();
        const bool isHostSurface = schema.value("surface").toString() == "host";
        std::unique_ptr<QWidget> outerHost;
        QWidget* button = nullptr;
        if (isHostSurface) {
            outerHost = std::make_unique<QWidget>(nullptr, Qt::Window | Qt::FramelessWindowHint);
            outerHost->setAttribute(Qt::WA_DontShowOnScreen, true);
            const auto sz = schema.value("previewSize").toArray();
            const QSize hostSize(sz.size() == 2 ? sz[0].toInt() : 360, sz.size() == 2 ? sz[1].toInt() : 220);
            outerHost->setFixedSize(hostSize);
            auto* stage = new Host(outerHost.get());
            stage->setFixedSize(hostSize);
            button = stage->create(index);
        } else {
            button = host.create(index);
        }
        if (!button) return 3;
        auto* fluentButton = qobject_cast<Button*>(button);
        if (schema.value("component") == QStringLiteral("image-viewer")) {
            const auto size = schema.value("previewSize").toArray();
            button->resize(size[0].toInt(), size[1].toInt());
            button->show();
            QCoreApplication::processEvents();
        }
        const auto expected = state.value("values").toObject();
        QJsonObject actual;
        for (const auto& entry : state.value("schema").toObject().value("properties").toArray()) {
            const auto field = entry.toObject();
            const auto id = field.value("id").toString();
            if (field.contains("adapter")) {
                const auto adapter = field.value("adapter").toString();
                if (adapter == "table-data") {
                    auto* view = qobject_cast<QAbstractItemView*>(button);
                    QJsonArray rows;
                    for (const auto& row : modelRows(view->model())) {
                        QJsonArray cells;
                        const auto columns = field.value("columns").toArray();
                        for (int col = 0; col < columns.size(); ++col) {
                            const auto column = columns[col].toObject();
                            cells.append(row.sibling(row.row(), column.value("modelColumn").toInt(col))
                                .data(column.value("role").toInt(Qt::DisplayRole)).toString());
                        }
                        rows.append(cells);
                    }
                    actual[id] = rows;
                } else if (adapter.startsWith("chart-")) {
                    auto* chart = qobject_cast<fluent::charts::ChartView*>(button);
                    auto* model = chart->model();
                    if (adapter == "chart-name") actual[id] = model->name();
                    else if (adapter == "chart-current") actual[id] = chart->currentRow();
                    else {
                        QJsonArray rows;
                        for (int row = 0; row < model->rowCount(); ++row) {
                            const auto point = model->pointAt(row);
                            rows.append(QJsonArray{point.x(), std::isfinite(point.y()) ? QJsonValue(point.y()) : QJsonValue(), model->labelAt(row)});
                        }
                        actual[id] = rows;
                    }
                } else if (adapter == "view-editable") actual[id] = qobject_cast<QAbstractItemView*>(button)->editTriggers() != QAbstractItemView::NoEditTriggers;
                else if (adapter == "grid-column-width") actual[id] = qobject_cast<QTableView*>(button)->columnWidth(field.value("index").toInt());
                else if (adapter == "grid-column-pinned") actual[id] = qobject_cast<fluent::collections::DataGrid*>(button)->isColumnPinned(field.value("index").toInt());
                else if (adapter == "grid-sections-movable") actual[id] = qobject_cast<QTableView*>(button)->horizontalHeader()->sectionsMovable();
                else if (adapter == "tree-selection-indicator") actual[id] = qobject_cast<fluent::collections::TreeView*>(button)->selectionIndicatorVisible();
                else if (adapter == "grid-sort-column") actual[id] = qobject_cast<QTableView*>(button)->horizontalHeader()->sortIndicatorSection();
                else if (adapter == "grid-sort-order") actual[id] = qobject_cast<QTableView*>(button)->horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder ? "AscendingOrder" : "DescendingOrder";
                else if (adapter == "field-editor") actual[id] = QJsonValue::fromVariant(qobject_cast<fluent::layout::Field*>(button)->editor()->property(field.value("key").toString().toLatin1().constData()));
                else if (adapter == "property-value") actual[id] = QJsonValue::fromVariant(qobject_cast<fluent::layout::PropertyPanel*>(button)->value(field.value("key").toString()));
                else if (adapter == "pane-size") actual[id] = qobject_cast<fluent::collections::SplitView*>(button)->panePreferredSize(field.value("index").toInt());
                else if (adapter == "toast-open") actual[id] = qobject_cast<fluent::status_info::Toast*>(button)->isOpen();
                else if (adapter == "splash-progress") actual[id] = qobject_cast<fluent::status_info::SplashScreen*>(button)->progress();
                else if (adapter == "stack-details") actual[id] = qobject_cast<fluent::collections::StackView*>(button)->depth() > 1;
                else if (adapter == "navigation-index") actual[id] = qobject_cast<QAbstractItemView*>(qobject_cast<fluent::navigation::NavigationView*>(button)->mainChromeWidget())->currentIndex().row();
                else {
                    QJsonArray selected;
                    const auto options = field.value("options").toArray();
                    for (int i = 0; i < options.size(); ++i) {
                        if (adapter == "accordion") {
                            if (qobject_cast<fluent::layout::Accordion*>(button)->itemAt(i)->isExpanded()) selected.append(options[i]);
                            continue;
                        }
                        if (adapter == "data-grid-filter") {
                            if (qobject_cast<fluent::layout::DataGridToolBar*>(button)->filters().value(field.value("key").toString()).toStringList().contains(options[i].toString())) selected.append(options[i]);
                            continue;
                        }
                        auto* view = qobject_cast<QAbstractItemView*>(button);
                        QModelIndex index;
                        for (const auto& step : options[i].toString().section(QStringLiteral(" · "), 0, 0).split('/')) {
                            const auto pair = step.split(':');
                            index = view->model()->index(pair[0].toInt(), pair[1].toInt(), index);
                        }
                        if (adapter == "model-expanded" ? qobject_cast<QTreeView*>(view)->isExpanded(index) : view->selectionModel()->isSelected(index)) selected.append(options[i]);
                    }
                    actual[id] = selected;
                }
                continue;
            }
            if (id == "icon") continue; // Python checks the protected glyph accessor; pixels must match both runtimes.
            if (id == "cornerRadius") {
                Button reference;
                actual[id] = expected.value(id).isNull() && fluentButton->cornerRadii() == reference.cornerRadii()
                    ? QJsonValue(QJsonValue::Null) : QJsonValue(fluentButton->cornerRadii().left());
            } else if (id == "iconOffsetX") actual[id] = fluentButton->iconOffset().x();
            else if (id == "iconOffsetY") actual[id] = fluentButton->iconOffset().y();
            else {
                const auto property = button->metaObject()->property(button->metaObject()->indexOfProperty(
                    field.value("property").toString().toLatin1().constData()));
                QVariant value = property.read(button);
                if (!property.isValid()) {
                    if (auto* check = qobject_cast<QCheckBox*>(button); check && id == "checkState") value = int(check->checkState());
                    if (id == "items") {
                        QStringList items;
                        if (auto* combo = qobject_cast<QComboBox*>(button)) for (int row = 0; row < combo->count(); ++row) items.append(combo->itemText(row));
                        if (auto* crumb = qobject_cast<fluent::navigation::Breadcrumb*>(button)) for (const auto& item : crumb->items()) items.append(item.text);
                        value = items;
                    }
                    if (auto* tabs = qobject_cast<fluent::navigation::TabView*>(button); tabs && id == "tabs") {
                        QStringList items; for (const auto& item : tabs->tabs()) items.append(item.text); value = items;
                    }
                    if (auto* combo = qobject_cast<fluent::basicinput::MultiSelectComboBox*>(button); combo && id == "selectedRows") {
                        QVariantList rows;
                        for (int row : combo->selectedRows()) rows.append(row);
                        value = rows;
                    }
                    if (auto* edit = qobject_cast<fluent::textfields::TextEdit*>(button)) {
                        if (id == "plainText") value = edit->toPlainText();
                        if (id == "placeholderText") value = edit->placeholderText();
                        if (id == "readOnly") value = edit->isReadOnly();
                    }
                    if (auto* tip = qobject_cast<fluent::status_info::ToolTip*>(button); tip && id == "text")
                        value = tip->text();
                }
                if (id == "selectedRows") {
                    QVariantList items;
                    const auto options = field.value("options").toArray();
                    for (const auto& row : value.toList()) items.append(options[row.toInt()].toString());
                    value = items;
                } else if (field.value("itemList").toBool()) {
                    QVariantList items;
                    for (const auto& item : value.toList()) items.append(item.toMap().value("text"));
                    value = items;
                }
                const auto type = field.value("type").toString();
                if (field.contains("sizeAxis")) {
                    actual[id] = field.value("sizeAxis") == "width" ? value.toSize().width() : value.toSize().height();
                } else if (field.contains("pointAxis")) {
                    actual[id] = field.value("pointAxis") == "x" ? value.toPoint().x() : value.toPoint().y();
                } else if (field.value("glyphEnum").toBool()) {
                    actual[id] = playgroundGlyphName(value.toString());
                } else if (QByteArray(property.typeName()) == "fluent::collections::SelectionMode") {
                    actual[id] = QString::fromLatin1(QMetaEnum::fromType<fluent::collections::SelectionMode>().valueToKey(
                        int(value.value<fluent::collections::SelectionMode>())));
                } else if (QByteArray(property.typeName()) == "fluent::windowing::BackdropEffect") {
                    const auto eff = value.value<fluent::windowing::BackdropEffect>();
                    actual[id] = eff == fluent::windowing::BackdropEffect::Solid ? "Solid"
                        : eff == fluent::windowing::BackdropEffect::Acrylic ? "Acrylic" : "Mica";
                } else if (type == "color") {
                    const QColor color = value.value<QColor>();
                    actual[id] = (field.value("nullable").toBool() && !color.isValid())
                        ? QJsonValue(QJsonValue::Null) : QJsonValue(color.name(QColor::HexArgb));
                } else if (type == "date") {
                    const QDate date = value.toDate();
                    actual[id] = (field.value("nullable").toBool() && !date.isValid())
                        ? QJsonValue(QJsonValue::Null) : QJsonValue(date.toString(Qt::ISODate));
                } else if (type == "time") {
                    actual[id] = value.toTime().isValid() ? QJsonValue(value.toTime().toString(Qt::ISODate)) : QJsonValue(QJsonValue::Null);
                } else if (type == "datetime") {
                    actual[id] = value.toDateTime().isValid() ? QJsonValue(value.toDateTime().toString(Qt::ISODate)) : QJsonValue(QJsonValue::Null);
                } else {
                    actual[id] = property.isEnumType() ? QJsonValue(QString::fromLatin1(property.enumerator().valueToKey(value.toInt())))
                        : QJsonValue::fromVariant(value);
                    if (field.contains("enumValues")) {
                        const auto numbers = field.value("enumValues").toObject();
                        for (const auto& option : field.value("options").toArray())
                            if (numbers.value(option.toString()).toInt() == value.toInt()) {
                                actual[id] = option;
                                break;
                            }
                    }
                }
            }
        }
        QWidget* surfaceWidget = isHostSurface ? outerHost.get() : button;
        if (!isHostSurface) {
            const auto comp = schema.value("component").toString();
            if (comp == "menu" || comp == "window") {
                button->setWindowFlag(Qt::Window, false);
                button->setWindowFlag(Qt::Popup, false);
                button->setAttribute(Qt::WA_DontShowOnScreen, true);
                if (comp == "window")
                    button->setMinimumSize(320, 200);
            }
            button->move(30, 30);
            auto size = schema.value("previewSize").toArray();
            if (button->property("orientation").toInt() == int(Qt::Vertical) && schema.contains("verticalPreviewSize"))
                size = schema.value("verticalPreviewSize").toArray();
            const auto sizeMode = schema.value("sizeMode").toString();
            if (sizeMode == "content")
                button->resize(button->sizeHint().expandedTo(QSize(16, 16)).boundedTo(QSize(480, 480)));
            else if (sizeMode == "contentHeight") {
                const int w = size.size() == 2 ? qMax(120, size[0].toInt()) : 320;
                const int fallbackH = size.size() == 2 ? size[1].toInt() : 160;
                const int rawH = button->hasHeightForWidth() ? button->heightForWidth(w) : button->sizeHint().height();
                button->resize(w, qBound(48, qMax(rawH > 0 ? rawH : fallbackH, button->minimumHeight() > 0 ? button->minimumHeight() : 0), 320));
            } else {
                button->resize(size.size() == 2 ? QSize(size[0].toInt(), size[1].toInt())
                    : button->sizeHint().expandedTo(QSize(24, 24)).boundedTo(comp == "button" ? QSize(236, 68) : QSize(320, 180)));
            }
            if (comp != "button")
                button->resize(button->size().expandedTo(button->minimumSizeHint()).boundedTo(QSize(480, 600)));
            if (comp != "menu" && comp != "window")
                button->show();
            else
                button->ensurePolished();
        } else {
            outerHost->ensurePolished();
        }
        app.processEvents();
        if (!isHostSurface) {
            if (expected.value("focusVisual").toBool()) button->setFocus(Qt::TabFocusReason);
            else button->clearFocus();
            if (expected.value("focusVisual").toBool() && button->isEnabled() && !button->hasFocus()) return 4;
        }
        for (auto* animation : surfaceWidget->findChildren<QAbstractAnimation*>())
            if (animation->state() == QAbstractAnimation::Running && animation->totalDuration() > 0)
                animation->setCurrentTime(animation->totalDuration());
        const QSize logical = surfaceWidget->size() + QSize(4, 4);
        QImage image(logical * 2, QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(2);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        surfaceWidget->render(&painter, QPoint(2, 2), QRegion(), QWidget::DrawChildren);
        painter.end();
        if (!image.save(output.filePath(QString("cpp-%1.png").arg(index)))) return 5;
        results.append(actual);
        if (!isHostSurface)
            delete button;
        else
            outerHost.reset();
    }
    QFile result(output.filePath("cpp-values.json"));
    if (!result.open(QIODevice::WriteOnly)) return 6;
    result.write(QJsonDocument(results).toJson());
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--audit', type=Path, required=True, action='append')
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--qt-prefix', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--report-pixel-differences', action='store_true')
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    cases = []
    for path in args.audit:
        audit = json.loads(path.read_text(encoding='utf-8'))
        cases.extend(dict(case, theme=audit.get('theme', 'light')) for case in audit['cases'])
    merged = output / 'input.json'
    merged.write_text(json.dumps({'cases': cases}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    bodies = []
    includes = set()
    for index, case in enumerate(cases):
        lines = case['state']['cpp'].splitlines()
        includes.update(line for line in lines if line.startswith('#include'))
        body = '\n'.join(line for line in lines if not line.startswith('#include'))
        variable = 'button' if case['state']['schema']['component'] == 'button' else 'control'
        bodies.append(f'case {index}: {{\n{body}\nreturn {variable};\n}}')
    (output / 'probe.cpp').write_text('\n'.join(sorted(includes)) + '\n' + CPP.replace('@CASES@', '\n'.join(bodies)), encoding='utf-8')
    cmake = '''cmake_minimum_required(VERSION 3.21)
project(PlaygroundExports LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(playground_exports probe.cpp)
target_include_directories(playground_exports PRIVATE "@ROOT@/include" "@ROOT@/src")
target_link_libraries(playground_exports PRIVATE "@BUILD@/libFluentQt.a" Qt6::Widgets)
if(APPLE)
    include("@ROOT@/cmake/FluentQtApplePlatformDependencies.cmake")
    target_link_libraries(playground_exports PRIVATE FluentQt::ApplePlatformDependencies
        "-framework AppKit" "-framework ImageIO" "-framework Metal" "-framework IOKit"
        "-framework DiskArbitration" "-framework UniformTypeIdentifiers")
endif()
'''.replace('@ROOT@', str(ROOT)).replace('@BUILD@', str(args.build_dir.resolve()))
    (output / 'CMakeLists.txt').write_text(cmake, encoding='utf-8')
    subprocess.run(['cmake', '-S', str(output), '-B', str(output / 'build'), '-DCMAKE_PREFIX_PATH=' + str(args.qt_prefix.resolve())], check=True)
    subprocess.run(['cmake', '--build', str(output / 'build'), '--parallel', '2'], check=True)
    subprocess.run([str(output / 'build/playground_exports'), str(merged), str(output)], check=True)
    cpp_values = json.loads((output / 'cpp-values.json').read_text(encoding='utf-8'))

    sys.path.insert(0, str((args.build_dir / 'python').resolve()))
    import fluentqt
    fluentqt.prepare_high_dpi_application()
    from PySide6.QtCore import Qt, QPoint, QSize, QAbstractAnimation, QModelIndex
    from PySide6.QtGui import QImage, QPainter, QCursor, QRegion, QColor
    from PySide6.QtWidgets import QApplication, QWidget, QAbstractItemView
    def model_rows(model, parent=QModelIndex()):
        rows=[]
        for row in range(model.rowCount(parent)):
            index=model.index(row,0,parent);rows.append(index);rows.extend(model_rows(model,index))
        return rows
    from fluentqt._qt_compat import delete_qobject
    app = QApplication([])
    fluentqt.initialize_resources()
    fluentqt.set_theme(fluentqt.Theme.Light)
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))
    host = QWidget()
    host.resize(320, 160)
    host.show()
    host.activateWindow()
    app.processEvents()
    QCursor.setPos(QPoint(0, 0))
    results = []
    failures = []
    for index, case in enumerate(cases):
        property_failures = []
        fluentqt.set_theme({'light': fluentqt.Theme.Light, 'dark': fluentqt.Theme.Dark,
                            'high-contrast': fluentqt.Theme.HighContrast}[case['theme']])
        schema = case['state']['schema']
        is_host_surface = schema.get('surface') == 'host'
        outer_host = None
        if is_host_surface:
            outer_host = QWidget(None, Qt.Window | Qt.FramelessWindowHint)
            outer_host.setAttribute(Qt.WA_DontShowOnScreen, True)
            sz = schema.get('previewSize') or [360, 220]
            outer_host.setFixedSize(sz[0], sz[1])
            stage = QWidget(outer_host)
            stage.setFixedSize(sz[0], sz[1])
            namespace = {'self': stage}
        else:
            namespace = {'self': host}
        exec(compile(case['state']['python'], case['case'], 'exec'), namespace)
        button = namespace.get('button', namespace.get('control'))
        if schema['component'] == 'image-viewer':
            button.resize(QSize(*schema['previewSize']))
            button.show()
            app.processEvents()
        expected = case['state']['values']
        for field in case['state']['schema']['properties']:
            name = field['id']
            if name == 'icon':
                glyph = '' if expected[name] == 'None' else getattr(fluentqt.Typography.Icons, expected[name])
                assert button.iconGlyph() == glyph, (case['case'], name)
                continue
            if name == 'cornerRadius':
                reference = fluentqt.Button()
                actual = None if expected[name] is None and button.cornerRadii() == reference.cornerRadii() else button.cornerRadii().left()
                delete_qobject(reference)
            elif name == 'iconOffsetX':
                actual = button.iconOffset().x()
            elif name == 'iconOffsetY':
                actual = button.iconOffset().y()
            else:
                if field.get('adapter'):
                    adapter = field['adapter']
                    if adapter == 'table-data':
                        actual=[[row.sibling(row.row(),col.get('modelColumn',c)).data(col.get('role',int(Qt.DisplayRole)))
                                 for c,col in enumerate(field['columns'])] for row in model_rows(button.model())]
                        actual=[[str(cell) if cell is not None else '' for cell in row] for row in actual]
                    elif adapter == 'chart-data':
                        model=button.model();actual=[[model.pointAt(i).x(),model.pointAt(i).y() if math.isfinite(model.pointAt(i).y()) else None,model.labelAt(i)] for i in range(model.rowCount())]
                    elif adapter == 'chart-name': actual=button.model().name()
                    elif adapter == 'chart-current': actual=button.currentRow()
                    elif adapter == 'view-editable': actual=button.editTriggers()!=QAbstractItemView.EditTrigger.NoEditTriggers
                    elif adapter == 'grid-column-width': actual=button.columnWidth(field['index'])
                    elif adapter == 'grid-column-pinned': actual=button.isColumnPinned(field['index'])
                    elif adapter == 'grid-sections-movable': actual=button.horizontalHeader().sectionsMovable()
                    elif adapter == 'tree-selection-indicator': actual=button.selectionIndicatorVisible()
                    elif adapter == 'grid-sort-column': actual=button.horizontalHeader().sortIndicatorSection()
                    elif adapter == 'grid-sort-order': actual=button.horizontalHeader().sortIndicatorOrder().name
                    elif adapter == 'field-editor': actual = button.editor().property(field['key'])
                    elif adapter == 'property-value': actual = button.value(field['key'])
                    elif adapter == 'pane-size': actual = button.panePreferredSize(field['index'])
                    elif adapter == 'toast-open': actual = button.isOpen()
                    elif adapter == 'splash-progress': actual = button.progress()
                    elif adapter == 'stack-details': actual = button.depth() > 1
                    elif adapter == 'navigation-index': actual = button.mainChromeWidget().currentIndex().row()
                    elif adapter == 'accordion': actual = [option for i, option in enumerate(field['options']) if button.itemAt(i).isExpanded()]
                    elif adapter == 'data-grid-filter': actual = list(button.filters().get(field['key'], []))
                    else:
                        actual = []
                        for option in field['options']:
                            model_index = QModelIndex()
                            for part in option.split(' · ')[0].split('/'):
                                row, col = map(int,part.split(':')); model_index = button.model().index(row,col,model_index)
                            if button.isExpanded(model_index) if adapter == 'model-expanded' else button.selectionModel().isSelected(model_index): actual.append(option)
                elif name == 'items' and hasattr(button, 'itemText'): actual = [button.itemText(i) for i in range(button.count())]
                elif name == 'items' and isinstance(button, fluentqt.Breadcrumb): actual = [item.text for item in button.items()]
                elif name == 'tabs': actual = [item.text for item in button.tabs()]
                elif name == 'checkState': actual = button.checkState()
                elif name == 'selectedRows':
                    actual = [field['options'][row] for row in button.selectedRows()]
                elif isinstance(button, fluentqt.TextEdit) and name in ('plainText', 'placeholderText', 'readOnly'):
                    actual = {'plainText': button.toPlainText, 'placeholderText': button.placeholderText, 'readOnly': button.isReadOnly}[name]()
                elif isinstance(button, fluentqt.ToolTip) and name == 'text':
                    actual = button.text()
                elif field['property'] == 'selectionMode' and hasattr(button, 'selectionMode'):
                    actual = button.selectionMode()
                else:
                    actual = button.property(field['property'])
                if field.get('itemList'):
                    actual = [item['text'] for item in actual]
                if field.get('sizeAxis'):
                    actual=actual.width() if field['sizeAxis']=='width' else actual.height()
                elif field.get('pointAxis'):
                    actual = actual.x() if field['pointAxis'] == 'x' else actual.y()
                elif field.get('glyphEnum'):
                    glyph_map = {getattr(fluentqt.Typography.Icons, k): k for k in (
                        'ChevronDown', 'ChevronUp', 'ChevronRight', 'Add', 'CheckMark', 'Save', 'FavoriteStar', 'Settings', 'Search', 'Mail', 'Home', 'Document', 'Info', 'Warning', 'ErrorBadge12'
                    )}
                    actual = glyph_map.get(actual, 'None')
                elif field['type'] == 'enum' and not isinstance(actual,str):
                    actual = next((option for option in field.get('options', [])
                                   if field.get('enumValues', {}).get(option) == actual.value),
                                  'None' if actual.name == 'None_' else actual.name)
                elif field['type'] == 'color':
                    actual = None if (field.get('nullable') and not actual.isValid()) else actual.name(QColor.NameFormat.HexArgb)
                elif field['type'] in ('date', 'time', 'datetime'):
                    actual = None if (field.get('nullable') and not actual.isValid()) else actual.toString(Qt.DateFormat.ISODate)
            if isinstance(actual,float) and math.isnan(actual): actual=None
            # An unlaid native constructor has no viewport size. Automatic zoom is
            # verified against live canvas cases after applying their fixture size.
            if schema['component'] == 'image-viewer' and name == 'zoomFactor' and expected['fitMode'] != 'Manual' and not case['state'].get('preview',{}).get('live'):
                continue
            for language, observed in [('Python', actual), ('C++', cpp_values[index][name])]:
                equal = observed == expected[name]
                if field['type'] == 'number' and not field.get('integer') and isinstance(observed,(int,float)) and isinstance(expected[name],(int,float)):
                    equal = math.isclose(observed,expected[name],rel_tol=1e-11,abs_tol=1e-12)
                if not equal:
                    property_failures.append({'case': case['case'], 'property': name,
                                              'actual': observed, 'expected': expected[name], 'language': language})
        surface_widget = outer_host if is_host_surface else button
        if not is_host_surface:
            if schema['component'] in ('menu', 'window'):
                button.setWindowFlag(Qt.Window, False)
                button.setWindowFlag(Qt.Popup, False)
                button.setAttribute(Qt.WA_DontShowOnScreen, True)
                if schema['component'] == 'window':
                    button.setMinimumSize(320, 200)
            button.move(30, 30)
            size = schema.get('previewSize')
            if button.property('orientation') == Qt.Orientation.Vertical:
                size = schema.get('verticalPreviewSize', size)
            size_mode = schema.get('sizeMode')
            if size_mode == 'content':
                button.resize(button.sizeHint().expandedTo(QSize(16, 16)).boundedTo(QSize(480, 480)))
            elif size_mode == 'contentHeight':
                w = max(120, size[0]) if size else 320
                fallback_h = size[1] if size else 160
                raw_h = button.heightForWidth(w) if button.hasHeightForWidth() else button.sizeHint().height()
                min_h = button.minimumHeight() if button.minimumHeight() > 0 else 0
                button.resize(w, max(48, min(320, max(raw_h if raw_h > 0 else fallback_h, min_h))))
            else:
                button.resize(QSize(*size) if size else button.sizeHint().expandedTo(QSize(24, 24)).boundedTo(
                    QSize(236, 68) if schema['component'] == 'button' else QSize(320, 180)))
            if schema['component'] != 'button':
                button.resize(button.size().expandedTo(button.minimumSizeHint()).boundedTo(QSize(480, 600)))
            if schema['component'] not in ('menu', 'window'):
                button.show()
            else:
                button.ensurePolished()
        else:
            outer_host.ensurePolished()
        app.processEvents()
        if not is_host_surface:
            if expected.get('focusVisual'):
                button.setFocus(Qt.TabFocusReason)
            else:
                button.clearFocus()
            if expected.get('focusVisual') and button.isEnabled():
                assert button.hasFocus()
        for animation in surface_widget.findChildren(QAbstractAnimation):
            if animation.state() == QAbstractAnimation.State.Running and animation.totalDuration() > 0:
                animation.setCurrentTime(animation.totalDuration())
        image = QImage((surface_widget.size() + QSize(4, 4)) * 2, QImage.Format_ARGB32_Premultiplied)
        image.setDevicePixelRatio(2)
        image.fill(Qt.transparent)
        painter = QPainter(image)
        surface_widget.render(painter, QPoint(2, 2), QRegion(), QWidget.DrawChildren)
        painter.end()
        image.save(str(output / f'python-{index}.png'))
        cpp_image = QImage(str(output / f'cpp-{index}.png')).convertToFormat(image.format())
        cpp_image.setDevicePixelRatio(2)
        pixel_equal = image == cpp_image
        failures.extend(property_failures)
        if not pixel_equal and (not args.report_pixel_differences or schema['component'] == 'button'):
            failures.append({'case': case['case'], 'error': 'C++ and Python rendering differs'})
        results.append({'case': case['case'], 'properties': 'fail' if property_failures else 'pass',
                        'cpp_python_pixels': 'pass' if pixel_equal else 'review-required'})
        delete_qobject(outer_host if is_host_surface else button)
    status = 'fail' if failures else ('pass' if all(r['cpp_python_pixels'] == 'pass' for r in results) else 'review-required')
    (output / 'result.json').write_text(json.dumps({'status': status, 'cases': results, 'failures': failures}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Executed {len(results)} exported C++/Python configurations; {sum(r["cpp_python_pixels"] != "pass" for r in results)} pixel comparisons need review.')
    if failures:
        raise AssertionError(f'{len(failures)} export checks failed; see {output / "result.json"}')


if __name__ == '__main__':
    os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
    main()
