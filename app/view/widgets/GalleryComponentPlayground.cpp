#include "GalleryComponentPlayground.h"
#include "GalleryPlaygroundData.h"

#include "compatibility/EventConstructionCompat.h"
#include "compatibility/MetaPropertyCompat.h"
#include "components/basicinput/Button.h"
#include "components/basicinput/RangeSlider.h"
#include "components/basicinput/FileDropZone.h"
#include "components/navigation/Pivot.h"
#include "components/navigation/SelectorBar.h"
#include "components/navigation/Stepper.h"
#include "components/navigation/TabView.h"
#include "components/collections/FlipView.h"
#include "components/collections/DrawerView.h"
#include "components/scrolling/PipsPager.h"
#include "components/charts/ChartView.h"
#include "components/collections/SelectionMode.h"
#include "components/foundation/FontIcon.h"
#include "components/foundation/MotionPolicy.h"
#include "components/status_info/Avatar.h"
#include "components/status_info/InfoBadge.h"
#include "components/status_info/SplashScreen.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/TextEdit.h"
#include "components/windowing/Window.h"
#include "components/basicinput/MultiSelectComboBox.h"
#include "components/basicinput/TreeSelect.h"
#include "components/dialogs_flyouts/CoachMark.h"
#include "components/dialogs_flyouts/Dialog.h"
#include "components/dialogs_flyouts/Popup.h"
#include "components/scrolling/ScrollBar.h"
#include <QLineEdit>
#include "GalleryPlaygroundFactory.h"
#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAbstractScrollArea>
#include <QAbstractAnimation>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QColor>
#include <QDate>
#include <QTime>
#include <QLayout>
#include <QFile>
#include <QJsonDocument>
#include <QLocale>
#include <QScopeGuard>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QPainter>
#include <QScopedValueRollback>
#include <QSet>
#include <QApplication>
#include <QMenu>
#include <QDialog>
#include <QTimer>
#include <QCheckBox>
#include <QAction>
#include <QAbstractItemView>
#include <QHash>
#include <QItemSelectionModel>
#include <QTreeView>
#include <QDateTime>
#include <limits>
#include <memory>
#include "components/layout/Accordion.h"
#include "components/layout/DataGridToolBar.h"
#include "components/layout/Expander.h"
#include "components/layout/Field.h"
#include "components/layout/ListDetailView.h"
#include "components/layout/PropertyPanel.h"
#include "components/navigation/Breadcrumb.h"
#include "components/navigation/NavigationView.h"
#include "components/collections/SplitView.h"
#include "components/collections/StackView.h"
#include "components/status_info/Toast.h"
#include <cmath>
#include <functional>

namespace fluent::gallery {
namespace {
using basicinput::Button;

bool belongsTo(const QObject* object, const QObject* owner)
{
    for (auto* parent = object; parent; parent = parent->parent())
        if (parent == owner)
            return true;
    return false;
}

QString indexKey(QModelIndex index)
{
    QStringList path;
    while (index.isValid()) {
        path.prepend(QStringLiteral("%1:%2").arg(index.row()).arg(index.column()));
        index = index.parent();
    }
    return path.join(QLatin1Char('/'));
}

QModelIndex indexForKey(const QAbstractItemModel* model, const QString& label)
{
    QModelIndex index;
    for (const auto& step : label.section(QStringLiteral(" · "), 0, 0).split(QLatin1Char('/'))) {
        const auto pair = step.split(QLatin1Char(':'));
        if (!model || pair.size() != 2)
            return {};
        index = model->index(pair[0].toInt(), pair[1].toInt(), index);
        if (!index.isValid())
            return {};
    }
    return index;
}

QString indexLabel(const QModelIndex& index)
{
    return indexKey(index) + QStringLiteral(" · ") + index.data().toString();
}

QJsonObject resolvedAdapterField(QJsonObject field, QObject* target)
{
    if (playground_data::handles(field))
        return playground_data::resolve(field, target);
    const auto adapter = field.value("adapter").toString();
    if (adapter.startsWith(QStringLiteral("model-"))) {
        auto* view = qobject_cast<QAbstractItemView*>(target);
        QJsonArray options;
        QJsonObject labels;
        if (view && view->model()) {
            const bool expanded = adapter == QStringLiteral("model-expanded");
            const bool rows = view->selectionBehavior() == QAbstractItemView::SelectRows || expanded;
            const auto visit = [&](const auto& self, const QModelIndex& parent) -> void {
                for (int row = 0; row < view->model()->rowCount(parent); ++row) {
                    for (int col = 0; col < (rows ? 1 : view->model()->columnCount(parent)); ++col) {
                        const auto index = view->model()->index(row, col, parent);
                        if (expanded ? view->model()->hasChildren(index)
                                     : (index.flags() & Qt::ItemIsSelectable) && (index.flags() & Qt::ItemIsEnabled)) {
                            const QString key = indexLabel(index);
                            options.append(key);
                            QStringList path;
                            for (auto parentIndex = index.parent(); parentIndex.isValid();
                                parentIndex = parentIndex.parent())
                                path.prepend(parentIndex.data().toString());
                            QString label = index.data().toString();
                            if (label.isEmpty())
                                label = QStringLiteral("Item %1 (empty)").arg(row + 1);
                            if (!rows && view->model()->columnCount(parent) > 1) {
                                label = view->model()->headerData(col, Qt::Horizontal).toString() + QStringLiteral(": ")
                                    + label;
                                if (col > 0)
                                    path.append(view->model()->index(row, 0, parent).data().toString());
                            }
                            path.append(label);
                            labels[key] = path.join(QStringLiteral(" / "));
                        }
                    }
                    self(self, view->model()->index(row, 0, parent));
                }
            };
            visit(visit, {});
            field["singleSelection"] = !expanded && view->selectionMode() == QAbstractItemView::SingleSelection;
        }
        field["options"] = options;
        field["optionLabels"] = labels;
        field["maximumItems"] = qMax(1, options.size());
    } else if (adapter == QStringLiteral("accordion")) {
        field["singleSelection"] = qobject_cast<layout::Accordion*>(target)->expansionMode()
            == layout::Accordion::ExpansionMode::Single;
    } else if (adapter == QStringLiteral("navigation-index")) {
        auto* nav = qobject_cast<navigation::NavigationView*>(target);
        auto* view = nav ? qobject_cast<QAbstractItemView*>(nav->mainChromeWidget()) : nullptr;
        QJsonArray choices;
        if (view && view->model())
            for (int row = 0; row < view->model()->rowCount(); ++row)
                choices.append(QJsonObject{{"value", row}, {"label", view->model()->index(row, 0).data().toString()},
                    {"enabled", true}});
        field["choices"] = choices;
    }
    return field;
}

QJsonValue adapterValue(const QJsonObject& field, QObject* target)
{
    if (playground_data::handles(field))
        return playground_data::read(field, target);
    const auto adapter = field.value("adapter").toString();
    if (adapter == QStringLiteral("stack-details"))
        return qobject_cast<collections::StackView*>(target)->depth() > 1;
    if (adapter == QStringLiteral("field-editor"))
        return QJsonValue::fromVariant(qobject_cast<layout::Field*>(target)->editor()->property(
            field.value("key").toString().toLatin1().constData()));
    if (adapter == QStringLiteral("property-value"))
        return QJsonValue::fromVariant(
            qobject_cast<layout::PropertyPanel*>(target)->value(field.value("key").toString()));
    if (adapter == QStringLiteral("pane-size"))
        return qobject_cast<collections::SplitView*>(target)->panePreferredSize(field.value("index").toInt());
    if (adapter == QStringLiteral("toast-open"))
        return qobject_cast<status_info::Toast*>(target)->isOpen();
    if (adapter == QStringLiteral("splash-progress"))
        return qobject_cast<status_info::SplashScreen*>(target)->progress();
    if (adapter == QStringLiteral("navigation-index")) {
        auto* view = qobject_cast<QAbstractItemView*>(
            qobject_cast<navigation::NavigationView*>(target)->mainChromeWidget());
        return view ? view->currentIndex().row() : -1;
    }
    QJsonArray values;
    if (adapter == QStringLiteral("accordion")) {
        auto* accordion = qobject_cast<layout::Accordion*>(target);
        const auto options = field.value("options").toArray();
        for (int i = 0; i < accordion->count() && i < options.size(); ++i)
            if (accordion->itemAt(i)->isExpanded())
                values.append(options[i]);
    } else if (adapter == QStringLiteral("data-grid-filter")) {
        const auto* bar = qobject_cast<const layout::DataGridToolBar*>(target);
        const auto selected = bar ? bar->filters().value(field.value("key").toString()).toStringList() : QStringList();
        for (const auto& item : selected)
            values.append(item);
    } else if (auto* view = qobject_cast<QAbstractItemView*>(target)) {
        const auto options = resolvedAdapterField(field, target).value("options").toArray();
        for (const auto& option : options) {
            const auto index = indexForKey(view->model(), option.toString());
            const bool selected = adapter == QStringLiteral("model-expanded")
                ? qobject_cast<QTreeView*>(view)->isExpanded(index)
                : view->selectionModel()->isSelected(index);
            if (selected)
                values.append(option);
        }
    }
    return values;
}

void writeAdapterValue(const QJsonObject& field, QObject* target, const QJsonValue& value)
{
    if (playground_data::handles(field)) {
        playground_data::write(field, value, target);
        return;
    }
    const auto adapter = field.value("adapter").toString();
    if (adapter == QStringLiteral("stack-details")) {
        auto* stack = qobject_cast<collections::StackView*>(target);
        if (value.toBool() && stack->depth() == 1)
            stack->push(stack->findChild<QWidget*>("playgroundDetails"), WidgetOwnership::Reparented);
        else if (!value.toBool() && stack->depth() > 1)
            stack->pop();
    } else if (adapter == QStringLiteral("field-editor"))
        qobject_cast<layout::Field*>(target)->editor()->setProperty(
            field.value("key").toString().toLatin1().constData(), value.toVariant());
    else if (adapter == QStringLiteral("property-value"))
        qobject_cast<layout::PropertyPanel*>(target)->setValue(field.value("key").toString(), value.toVariant());
    else if (adapter == QStringLiteral("pane-size"))
        qobject_cast<collections::SplitView*>(target)->setPanePreferredSize(field.value("index").toInt(),
            value.toInt());
    else if (adapter == QStringLiteral("toast-open")) {
        auto* toast = qobject_cast<status_info::Toast*>(target);
        if (value.toBool() && !toast->isOpen())
            toast->present(toast->parentWidget());
        else if (!value.toBool())
            toast->dismiss();
    } else if (adapter == QStringLiteral("splash-progress")) {
        if (auto* splash = qobject_cast<status_info::SplashScreen*>(target))
            splash->setProgress(value.toInt(), 100);
    } else if (adapter == QStringLiteral("navigation-index")) {
        auto* view = qobject_cast<QAbstractItemView*>(
            qobject_cast<navigation::NavigationView*>(target)->mainChromeWidget());
        if (view)
            view->setCurrentIndex(view->model()->index(value.toInt(), 0));
    } else if (adapter == QStringLiteral("accordion")) {
        auto* accordion = qobject_cast<layout::Accordion*>(target);
        const auto options = field.value("options").toArray();
        for (int i = 0; i < accordion->count() && i < options.size(); ++i)
            accordion->itemAt(i)->setExpandedAnimated(value.toArray().contains(options[i]), false);
    } else if (adapter == QStringLiteral("data-grid-filter")) {
        if (auto* bar = qobject_cast<layout::DataGridToolBar*>(target)) {
            QStringList selected;
            for (const auto& item : value.toArray())
                selected.append(item.toString());
            bar->setFilterValues(field.value("key").toString(), selected);
        }
    } else if (auto* view = qobject_cast<QAbstractItemView*>(target)) {
        const bool expanded = adapter == QStringLiteral("model-expanded");
        if (!expanded)
            view->clearSelection();
        for (const auto& option : resolvedAdapterField(field, target).value("options").toArray()) {
            const auto index = indexForKey(view->model(), option.toString());
            const bool selected = value.toArray().contains(option);
            if (expanded)
                qobject_cast<QTreeView*>(view)->setExpanded(index, selected);
            else if (selected && view->selectionMode() != QAbstractItemView::NoSelection) {
                auto flags = QItemSelectionModel::Select;
                if (view->selectionMode() == QAbstractItemView::SingleSelection)
                    view->clearSelection();
                view->selectionModel()->select(index,
                    view->selectionBehavior() == QAbstractItemView::SelectRows
                        ? flags | QItemSelectionModel::Rows
                        : flags);
                if (auto* tree = qobject_cast<QTreeView*>(view))
                    for (auto parent = index.parent(); parent.isValid(); parent = parent.parent())
                        tree->setExpanded(parent, true);
                view->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                view->scrollTo(index, QAbstractItemView::EnsureVisible);
            }
        }
    }
}

QJsonObject resolvedModelField(QJsonObject field, QObject* target, const QJsonObject& pending = {})
{
    if (field.contains("adapter"))
        return resolvedAdapterField(field, target);
    if (const auto* tree = qobject_cast<basicinput::TreeSelect*>(target);
        tree && field.value("property") == QStringLiteral("selectedKeys")) {
        field["singleSelection"] = tree->selectionMode() == basicinput::TreeSelect::SelectionMode::Single;
        return field;
    }
    if (field.value("control") != QStringLiteral("model-select"))
        return field;
    QJsonArray choices;
    const auto append = [&](int row, const QJsonValue& label, bool enabled = true) {
        choices.append(QJsonObject{{"value", row}, {"label", label}, {"enabled", enabled}});
    };
    const auto pageLabel = [](int row) { return QJsonObject{{"page", row + 1}}; };
    if (const auto* combo = qobject_cast<QComboBox*>(target)) {
        append(-1, "No selection");
        if (pending.contains("items")) {
            const auto items = pending.value("items").toArray();
            for (int row = 0; row < items.size(); ++row)
                append(row, items[row]);
        } else
            for (int row = 0; row < combo->count(); ++row) {
                const auto index = combo->model()->index(row, combo->modelColumn(), combo->rootModelIndex());
                const auto flags = combo->model()->flags(index);
                append(row, combo->itemText(row),
                    bool(flags & Qt::ItemIsEnabled) && bool(flags & Qt::ItemIsSelectable));
            }
    } else if (const auto* pivot = qobject_cast<navigation::Pivot*>(target)) {
        const auto items = pivot->items();
        for (int row = 0; row < items.size(); ++row)
            append(row, items[row].header, items[row].enabled);
    } else if (const auto* selector = qobject_cast<navigation::SelectorBar*>(target)) {
        const auto items = selector->items();
        for (int row = 0; row < items.size(); ++row)
            if (items[row].visible)
                append(row, items[row].text, items[row].enabled);
    } else if (const auto* stepper = qobject_cast<navigation::Stepper*>(target)) {
        append(-1, "No selection");
        const auto items = stepper->items();
        for (int row = 0; row < items.size(); ++row)
            append(row, items[row].text);
    } else if (const auto* tabs = qobject_cast<navigation::TabView*>(target)) {
        append(-1, "No selection");
        if (pending.contains("tabs")) {
            const auto items = pending.value("tabs").toArray();
            for (int row = 0; row < items.size(); ++row)
                append(row, items[row]);
        } else {
            const auto items = tabs->tabs();
            for (int row = 0; row < items.size(); ++row)
                append(row, items[row].text, items[row].enabled);
        }
    } else if (const auto* pager = qobject_cast<scrolling::PipsPager*>(target)) {
        const int count = pending.value("numberOfPages").toInt(pager->numberOfPages());
        for (int row = 0; row < count; ++row)
            append(row, pageLabel(row));
    } else if (const auto* flip = qobject_cast<collections::FlipView*>(target)) {
        for (int row = 0; row < flip->pageCount(); ++row)
            append(row, pageLabel(row));
    } else {
        return field;
    }
    if (choices.isEmpty())
        append(-1, "No selection", false);
    field["choices"] = choices;
    field["minimum"] = choices.first().toObject().value("value");
    field["maximum"] = choices.last().toObject().value("value");
    return field;
}

QJsonObject resolvedEnumField(QJsonObject field, const QObject* target)
{
    const auto values = field.value("enumValues").toObject();
    if (values.isEmpty())
        return field;
    auto options = field.value("options").toArray();
    if (field.value("property") == QStringLiteral("tickPosition")) {
        const bool vertical = target->property("orientation").toInt() == int(Qt::Vertical);
        options = {"NoTicks", vertical ? "TicksLeft" : "TicksAbove", vertical ? "TicksRight" : "TicksBelow",
            "TicksBothSides"};
    }
    QSet<int> seen;
    QJsonArray unique;
    for (const auto& option : options) {
        const int value = values.value(option.toString()).toInt();
        if (!seen.contains(value)) {
            seen.insert(value);
            unique.append(option);
        }
    }
    field["options"] = unique;
    return field;
}

QString resolvedEnumKey(const QJsonObject& field, const QObject* target, int value)
{
    const auto resolved = resolvedEnumField(field, target);
    const auto numbers = resolved.value("enumValues").toObject();
    for (const auto& option : resolved.value("options").toArray())
        if (numbers.value(option.toString()).toInt() == value)
            return option.toString();
    return {};
}

QJsonObject resolvedFieldBounds(QJsonObject field, const QObject* target, const QJsonObject& pending = {})
{
    if (field.value("type") == QStringLiteral("text") && field.contains("maximumLengthFrom")) {
        const auto source = field.value("maximumLengthFrom").toString();
        const int limit = pending.contains(source)
            ? pending.value(source).toInt()
            : target->property(source.toLatin1().constData()).toInt();
        field["maximumLength"] = qMin(field.value("maximumLength").toInt(), qMax(0, limit));
        return field;
    }
    if (field.value("type") != QStringLiteral("number") || field.value("control") == QStringLiteral("model-select"))
        return field;
    const auto number = [&](const QString& property) {
        return pending.contains(property)
            ? pending.value(property).toDouble()
            : target->property(property.toLatin1().constData()).toDouble();
    };
    if (field.contains("minimumFrom"))
        field["minimum"] = number(field.value("minimumFrom").toString());
    if (field.contains("maximumFrom"))
        field["maximum"] = number(field.value("maximumFrom").toString());
    const auto id = field.value("id").toString();
    if (qobject_cast<const basicinput::RangeSlider*>(target)
        && !(pending.contains("lowerValue") && pending.contains("upperValue"))) {
        const double gap = qMin(number("minimumRange"), qMax(0.0, number("maximum") - number("minimum")));
        if (id == QStringLiteral("lowerValue"))
            field["maximum"] = qMax(number("minimum"), number("upperValue") - gap);
        else if (id == QStringLiteral("upperValue"))
            field["minimum"] = qMin(number("maximum"), number("lowerValue") + gap);
    }
    // A setter may adjust a linked property beyond the fixture's original editor limits.
    // zh_CN: 联动 setter 可能调整其他属性，编辑器必须能表示回读后的真实值。
    auto raw = target->property(field.value("property").toString().toLatin1().constData());
    if (field.contains("sizeAxis"))
        raw = field.value("sizeAxis") == "width" ? raw.toSize().width() : raw.toSize().height();
    if (field.contains("pointAxis"))
        raw = field.value("pointAxis") == "x" ? raw.toPoint().x() : raw.toPoint().y();
    if (raw.isValid() && std::isfinite(raw.toDouble())) {
        field["minimum"] = qMin(raw.toDouble(), field.value("minimum").toDouble());
        field["maximum"] = qMax(raw.toDouble(), field.value("maximum").toDouble());
    }
    return field;
}

class PreviewButton final : public Button {
public:
    explicit PreviewButton(QWidget* parent, bool simulatedFocus = true)
        : Button(parent), m_simulatedFocus(simulatedFocus)
    {}

protected:
    void paintEvent(QPaintEvent* event) override
    {
        // Only this isolated preview simulates focus, using the real button painter.
        // zh_CN: 仅此独立预览模拟焦点，复用真实按钮的绘制逻辑。
        if (m_simulatedFocus)
            paintButtonState(interactionState(), true);
        else
            Button::paintEvent(event);
    }

private:
    bool m_simulatedFocus;
};

bool isCollectionsSelectionMode(const QMetaProperty& prop)
{
    return fluentMetaPropertyHasTypeName(prop, QByteArrayLiteral("fluent::collections::SelectionMode"));
}

bool isBackdropEffect(const QMetaProperty& prop)
{
    return fluentMetaPropertyHasTypeName(prop, QByteArrayLiteral("fluent::windowing::BackdropEffect"));
}

struct GlyphEntry {
    const char* name;
    const QString* glyph;
};

const GlyphEntry kPlaygroundGlyphs[] = {
    {"ChevronDown", &Typography::Icons::ChevronDown},
    {"ChevronUp", &Typography::Icons::ChevronUp},
    {"ChevronRight", &Typography::Icons::ChevronRight},
    {"Add", &Typography::Icons::Add},
    {"CheckMark", &Typography::Icons::CheckMark},
    {"Save", &Typography::Icons::Save},
    {"FavoriteStar", &Typography::Icons::FavoriteStar},
    {"Settings", &Typography::Icons::Settings},
    {"Search", &Typography::Icons::Search},
    {"Mail", &Typography::Icons::Mail},
    {"Home", &Typography::Icons::Home},
    {"Document", &Typography::Icons::Document},
    {"Info", &Typography::Icons::Info},
    {"Warning", &Typography::Icons::Warning},
    {"ErrorBadge12", &Typography::Icons::ErrorBadge12},
};

QString glyphValueFor(const QString& name)
{
    for (const auto& entry : kPlaygroundGlyphs) {
        if (name == QLatin1String(entry.name))
            return *entry.glyph;
    }
    return {};
}

QString glyphNameFor(const QString& glyph)
{
    for (const auto& entry : kPlaygroundGlyphs) {
        if (glyph == *entry.glyph)
            return QString::fromLatin1(entry.name);
    }
    return QStringLiteral("None");
}

void snapFiniteAnimations(QWidget* root)
{
    if (!root)
        return;
    const auto animations = root->findChildren<QAbstractAnimation*>();
    for (QAbstractAnimation* animation : animations) {
        if (!animation || animation->state() != QAbstractAnimation::Running || animation->loopCount() != 1)
            continue;
        const int duration = animation->totalDuration();
        if (duration > 0)
            animation->setCurrentTime(duration);
        animation->stop();
    }
}

void setCoachMarkOpenImmediate(dialogs_flyouts::CoachMark* coachMark, bool open)
{
    if (!coachMark)
        return;
    // Commit CoachMark visibility immediately so closing also hides synchronously.
    // zh_CN: 即时提交 CoachMark 的开关状态，使关闭动作也能同步触发隐藏与关闭信号。
    const auto previousMode = MotionPolicy::instance().mode();
    const auto restore = qScopeGuard([previousMode] { MotionPolicy::instance().setMode(previousMode); });
    MotionPolicy::instance().setMode(MotionPolicy::Mode::Disabled);
    coachMark->setOpen(open);
    snapFiniteAnimations(coachMark);
}

QString resolvedNullableColorFallback(const QObject* target, const QJsonObject& field)
{
    if (const auto* badge = qobject_cast<const status_info::InfoBadge*>(target)) {
        const auto id = field.value(QStringLiteral("id")).toString();
        if (id == QStringLiteral("customBackgroundColor") && badge->effectiveBackgroundColor().isValid())
            return badge->effectiveBackgroundColor().name(QColor::HexArgb);
        if (id == QStringLiteral("customTextColor") && badge->effectiveForegroundColor().isValid())
            return badge->effectiveForegroundColor().name(QColor::HexArgb);
    }
    if (const auto* avatar = qobject_cast<const status_info::Avatar*>(target)) {
        const auto id = field.value(QStringLiteral("id")).toString();
        if (id == QStringLiteral("backgroundColor") && avatar->effectiveBackgroundColor().isValid())
            return avatar->effectiveBackgroundColor().name(QColor::HexArgb);
        if (id == QStringLiteral("foregroundColor") && avatar->effectiveForegroundColor().isValid())
            return avatar->effectiveForegroundColor().name(QColor::HexArgb);
    }
    if (field.contains(QStringLiteral("fallbackColor")))
        return field.value(QStringLiteral("fallbackColor")).toString();
    if (const auto* widget = qobject_cast<const QWidget*>(target))
        return widget->palette().color(QPalette::WindowText).name(QColor::HexArgb);
    return QStringLiteral("#ff000000");
}

bool methodProperty(const QObject* target, const QString& name)
{
    if (qobject_cast<const navigation::Breadcrumb*>(target) && name == QStringLiteral("items"))
        return true;
    if (qobject_cast<const QComboBox*>(target) && name == QStringLiteral("items"))
        return true;
    if (qobject_cast<const QCheckBox*>(target) && name == QStringLiteral("checkState"))
        return true;
    if (qobject_cast<const navigation::TabView*>(target) && name == QStringLiteral("tabs"))
        return true;
    if (qobject_cast<const basicinput::MultiSelectComboBox*>(target) && name == QStringLiteral("selectedRows"))
        return true;
    if (qobject_cast<const status_info::ToolTip*>(target) && name == QStringLiteral("text"))
        return true;
    return qobject_cast<const textfields::TextEdit*>(target)
        && (name == QStringLiteral("plainText") || name == QStringLiteral("placeholderText")
            || name == QStringLiteral("readOnly"));
}

QVariant readMethodProperty(const QObject* target, const QString& name)
{
    if (const auto* breadcrumb = qobject_cast<const navigation::Breadcrumb*>(target);
        breadcrumb && name == QStringLiteral("items")) {
        QStringList items;
        for (const auto& item : breadcrumb->items())
            items.append(item.text);
        return items;
    }
    if (const auto* combo = qobject_cast<const QComboBox*>(target); combo && name == QStringLiteral("items")) {
        QStringList items;
        for (int row = 0; row < combo->count(); ++row)
            items.append(combo->itemText(row));
        return items;
    }
    if (const auto* check = qobject_cast<const QCheckBox*>(target); check && name == QStringLiteral("checkState"))
        return int(check->checkState());
    if (const auto* tabs = qobject_cast<const navigation::TabView*>(target); tabs && name == QStringLiteral("tabs")) {
        QStringList items;
        for (const auto& tab : tabs->tabs())
            items.append(tab.text);
        return items;
    }
    if (const auto* combo = qobject_cast<const basicinput::MultiSelectComboBox*>(target);
        combo && name == QStringLiteral("selectedRows")) {
        QVariantList rows;
        for (int row : combo->selectedRows())
            rows.append(row);
        return rows;
    }
    if (const auto* tooltip = qobject_cast<const status_info::ToolTip*>(target);
        tooltip && name == QStringLiteral("text"))
        return tooltip->text();
    if (const auto* edit = qobject_cast<const textfields::TextEdit*>(target)) {
        if (name == QStringLiteral("plainText"))
            return edit->toPlainText();
        if (name == QStringLiteral("placeholderText"))
            return edit->placeholderText();
        if (name == QStringLiteral("readOnly"))
            return edit->isReadOnly();
    }
    return {};
}

void writeMethodProperty(QObject* target, const QString& name, const QVariant& value)
{
    if (auto* breadcrumb = qobject_cast<navigation::Breadcrumb*>(target); breadcrumb && name == QStringLiteral("items"))
        breadcrumb->setItems(value.toStringList());
    if (auto* combo = qobject_cast<QComboBox*>(target); combo && name == QStringLiteral("items")) {
        combo->clear();
        combo->addItems(value.toStringList());
    }
    if (auto* check = qobject_cast<QCheckBox*>(target); check && name == QStringLiteral("checkState"))
        check->setCheckState(static_cast<Qt::CheckState>(value.toInt()));
    if (auto* tabs = qobject_cast<navigation::TabView*>(target); tabs && name == QStringLiteral("tabs")) {
        const QVector<navigation::TabViewItem> existing = tabs->tabs();
        QHash<QString, navigation::TabViewItem> byText;
        for (const auto& item : existing) {
            if (!byText.contains(item.text))
                byText.insert(item.text, item);
        }
        const QStringList nextTexts = value.toStringList();
        const int previousIndex = tabs->selectedIndex();
        tabs->clearTabs();
        for (int i = 0; i < nextTexts.size(); ++i) {
            const QString& text = nextTexts.at(i);
            if (byText.contains(text)) {
                tabs->addTab(byText.value(text));
            } else {
                const QString icon = i < existing.size() && !existing.at(i).iconGlyph.isEmpty()
                    ? existing.at(i).iconGlyph
                    : Typography::Icons::Document;
                tabs->addTab(navigation::TabViewItem(text, icon));
            }
        }
        if (!nextTexts.isEmpty()) {
            const int targetIndex = (previousIndex >= 0 && previousIndex < nextTexts.size()) ? previousIndex : 0;
            tabs->setSelectedIndex(targetIndex);
        }
    }
    if (auto* combo = qobject_cast<basicinput::MultiSelectComboBox*>(target);
        combo && name == QStringLiteral("selectedRows")) {
        QList<int> rows;
        for (const auto& row : value.toList())
            rows.append(row.toInt());
        combo->setSelectedRows(rows);
    }
    if (auto* tooltip = qobject_cast<status_info::ToolTip*>(target); tooltip && name == QStringLiteral("text"))
        tooltip->setText(value.toString());
    if (auto* edit = qobject_cast<textfields::TextEdit*>(target)) {
        if (name == QStringLiteral("plainText"))
            edit->setPlainText(value.toString());
        if (name == QStringLiteral("placeholderText"))
            edit->setPlaceholderText(value.toString());
        if (name == QStringLiteral("readOnly"))
            edit->setReadOnly(value.toBool());
    }
}

bool fail(QString* error, const QString& message)
{
    if (error)
        *error = message;
    return false;
}

QString quoted(const QString& text, bool python)
{
    if (python) {
        const QByteArray json = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
        return QString::fromUtf8(json.mid(1, json.size() - 2));
    }
    QString result = QStringLiteral("QStringLiteral(\"");
    for (const QChar character : text) {
        switch (character.unicode()) {
        case '\\':
            result += QStringLiteral("\\\\");
            break;
        case '"':
            result += QStringLiteral("\\\"");
            break;
        case '\n':
            result += QStringLiteral("\\n");
            break;
        case '\r':
            result += QStringLiteral("\\r");
            break;
        case '\t':
            result += QStringLiteral("\\t");
            break;
        default:
            if (character.unicode() < 32 || character.unicode() == 127)
                result += QStringLiteral("\\%1").arg(int(character.unicode()), 3, 8, QLatin1Char('0'));
            else
                result += character;
        }
    }
    return result + QStringLiteral("\")");
}
} // namespace

GalleryComponentPlayground* GalleryComponentPlayground::create(const QString& route, QWidget* parent, bool interactive)
{
    if (!parent || !playgroundRoutes().contains(route))
        return nullptr;
    QFile file(QStringLiteral(":/app/playgrounds/%1.json").arg(route));
    if (!file.open(QIODevice::ReadOnly))
        return nullptr;
    const auto schema = QJsonDocument::fromJson(file.readAll()).object();
    if (schema.value("version").toInt() != 1 || schema.value("component").toString() != route)
        return nullptr;
    auto* result = new GalleryComponentPlayground(schema, parent, interactive);
    if (!result->widget()) {
        delete result;
        return nullptr;
    }
    return result;
}

GalleryComponentPlayground* GalleryComponentPlayground::createInteractive(const QString& route, QWidget* parent)
{
    auto* result = create(route, parent, true);
    if (!result)
        return nullptr;
    result->presentInteractiveSurface();
    result->m_observedValues = result->values();
    auto* observer = new QTimer(result);
    observer->setInterval(40);
    QObject::connect(observer, &QTimer::timeout, result, [result, host = QPointer<QWidget>(parent)] {
        if (!host || !host->isVisible() || result->m_applying)
            return;
        if (host->property("fluentPlaygroundFillParent").toBool() && host->parentWidget())
            host->setGeometry(host->parentWidget()->rect());
        const auto current = result->values();
        const auto viewport = result->interactiveViewport(host);
        if (current == result->m_observedValues && viewport == result->m_observedViewport)
            return;
        result->m_observedValues = current;
        result->m_observedViewport = viewport;
        ++result->m_revision;
        emit result->stateChanged();
    });
    observer->start();
    return result;
}

bool GalleryComponentPlayground::initializeInteractive(const QJsonObject& initial, QString* error)
{
    if (!m_interactive)
        return false;
    QJsonObject supported;
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const QString id = entry.toObject().value("id").toString();
        if (initial.contains(id))
            supported[id] = initial.value(id);
    }
    if (m_button && initial.value("interactionState") == QStringLiteral("Disabled"))
        supported["enabled"] = false;
    if (!supported.isEmpty() && !apply({{"operation", "set"}, {"values", supported}}, error))
        return false;
    m_initial = values();
    m_observedValues = m_initial;
    return true;
}

GalleryComponentPlayground::GalleryComponentPlayground(const QJsonObject& schema, QWidget* parent, bool interactive)
    : QObject(parent), m_schema(schema), m_interactive(interactive)
{
    QJsonArray visibleFields;
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const auto field = entry.toObject();
        if ((interactive && field.value("snapshotOnly").toBool())
            || (!interactive && field.value("interactiveOnly").toBool()))
            continue;
        visibleFields.append(field);
    }
    m_schema["properties"] = visibleFields;
    m_schema["interactive"] = interactive;
    if (schema.value("component") != QStringLiteral("button")) {
        const auto instance = createPlaygroundControl(schema.value("component").toString(), parent);
        m_surface = instance.surface;
        m_target = instance.target;
        if (!m_target || !m_surface)
            return;
        // Resolve constructor defaults from the actual control, not a second set of constants.
        // zh_CN: 从真实控件读取构造默认值，避免维护第二套常量。
        auto fields = m_schema.value("properties").toArray();
        for (int i = 0; i < fields.size(); ++i) {
            auto field = fields[i].toObject();
            const auto name = field.value("property").toString().toLatin1();
            const auto prop = m_target->metaObject()->property(m_target->metaObject()->indexOfProperty(name));
            if (field.value("id") == QStringLiteral("icon") && qobject_cast<Button*>(m_target))
                continue;
            if (!field.contains("adapter") && (!prop.isValid() || !prop.isReadable() || !prop.isWritable())
                && !methodProperty(m_target, QString::fromLatin1(name))) {
                delete m_surface;
                m_surface = nullptr;
                m_target = nullptr;
                return;
            }
            if (prop.isEnumType() || isCollectionsSelectionMode(prop)) {
                QJsonArray options;
                QJsonObject numbers;
                const auto enumeration = isCollectionsSelectionMode(prop)
                    ? QMetaEnum::fromType<collections::SelectionMode>()
                    : prop.enumerator();
                for (int key = 0; key < enumeration.keyCount(); ++key) {
                    options.append(QString::fromLatin1(enumeration.key(key)));
                    numbers[QString::fromLatin1(enumeration.key(key))] = enumeration.value(key);
                }
                field["enumValues"] = numbers;
                if (!field.contains("options"))
                    field["options"] = options;
                const auto scope = QString::fromLatin1(enumeration.scope());
                const auto enumName = QString::fromLatin1(enumeration.name());
                if (!field.contains("cppPrefix"))
                    field["cppPrefix"] = scope + QStringLiteral("::") + enumName + QStringLiteral("::");
                if (!field.contains("pythonPrefix")) {
                    const auto pythonScope = isCollectionsSelectionMode(prop) ? QStringLiteral("fluentqt")
                        : (scope == QStringLiteral("Qt") || scope.startsWith(QLatin1Char('Q')))
                        ? scope
                        : QStringLiteral("fluentqt.") + scope.section(QStringLiteral("::"), -1);
                    field["pythonPrefix"] = pythonScope + QLatin1Char('.') + enumName + QLatin1Char('.');
                }
            }
            fields[i] = field;
        }
        m_schema["properties"] = fields;
        writeGenericValues(defaults());
        const auto initial = genericValues(m_target);
        for (int i = 0; i < fields.size(); ++i) {
            auto field = fields[i].toObject();
            field["default"] = initial.value(field.value("id").toString());
            if (field.value("type") == QStringLiteral("number")) {
                const double value = field.value("default").toDouble();
                field["minimum"] = qMin(value, field.value("minimum").toDouble());
                field["maximum"] = qMax(value, field.value("maximum").toDouble());
            }
            fields[i] = field;
        }
        m_schema["properties"] = fields;
        m_initial = initial;
        connectGenericEvents();
        return;
    }
    m_button = new PreviewButton(parent, !interactive);
    m_surface = m_button;
    m_target = m_button;
    m_button->setObjectName(QStringLiteral("playgroundButton"));
    writeValues(defaults());
    m_initial = values();
    connect(m_button, &Button::clicked, this, [this](bool checked) {
        recordEvent(QStringLiteral("clicked"), checked);
    });
    connect(m_button, &Button::toggled, this, [this](bool checked) {
        recordEvent(QStringLiteral("toggled"), checked);
    });
}

QWidget* GalleryComponentPlayground::widget() const
{
    return m_surface.data();
}

QJsonObject GalleryComponentPlayground::defaults() const
{
    QJsonObject result;
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const auto field = entry.toObject();
        if (field.contains("default"))
            result.insert(field.value("id").toString(), field.value("default"));
    }
    return result;
}

QJsonObject GalleryComponentPlayground::values() const
{
    return m_button ? valuesFor(m_button) : genericValues(m_target);
}

QJsonObject GalleryComponentPlayground::valuesFor(const Button* button) const
{
    QJsonObject result;
    if (!button)
        return result;
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const auto field = entry.toObject();
        const QString id = field.value("id").toString();
        if (id == QStringLiteral("icon"))
            result[id] = m_icon;
        else if (id == QStringLiteral("cornerRadius"))
            result[id] = m_customRadius ? QJsonValue(button->cornerRadii().left()) : QJsonValue(QJsonValue::Null);
        else if (id == QStringLiteral("iconOffsetX"))
            result[id] = button->iconOffset().x();
        else if (id == QStringLiteral("iconOffsetY"))
            result[id] = button->iconOffset().y();
        else {
            const auto property = m_button->metaObject()->property(
                m_button->metaObject()->indexOfProperty(field.value("property").toString().toLatin1().constData()));
            const QVariant value = property.read(button);
            result[id] = property.isEnumType()
                ? QJsonValue(QString::fromLatin1(property.enumerator().valueToKey(value.toInt())))
                : QJsonValue::fromVariant(value);
        }
    }
    return result;
}

GalleryComponentPlayground* GalleryComponentPlayground::fromButton(const Button* source, QWidget* parent)
{
    if (!source)
        return nullptr;
    auto* result = create(QStringLiteral("button"), parent);
    if (!result)
        return nullptr;
    // Read the source button's public properties once without mutating it; edits target only the preview.
    // zh_CN: 仅从源按钮读取一次初始属性，后续修改只作用于独立预览控件。
    result->m_customRadius = source->cornerRadii() != result->m_button->cornerRadii();
    result->writeValues(result->valuesFor(source));
    result->m_initial = result->values();
    return result;
}

GalleryComponentPlayground::~GalleryComponentPlayground()
{
    if (m_overlayHost && m_overlayHost->property("fluentOverlaySurfaceRect").isValid())
        m_overlayHost->setProperty("fluentOverlaySurfaceRect", QVariant());
    if (m_interactive && m_target && (!m_surface || !belongsTo(m_target, m_surface))) {
        if (auto* targetWidget = qobject_cast<QWidget*>(m_target.data()))
            targetWidget->hide();
        m_target->deleteLater();
    }
}

void GalleryComponentPlayground::layoutSurface()
{
    m_surface->ensurePolished();
    auto viewport = m_schema.value("previewSize").toArray();
    if (m_target->property("orientation").toInt() == int(Qt::Vertical) && m_schema.contains("verticalPreviewSize"))
        viewport = m_schema.value("verticalPreviewSize").toArray();
    const QString sizeMode = m_schema.value("sizeMode").toString();
    QSize preferred = viewport.size() == 2
        ? QSize(viewport[0].toInt(), viewport[1].toInt())
        : m_surface->sizeHint().expandedTo(QSize(24, 24)).boundedTo(m_button ? QSize(236, 68) : QSize(320, 180));
    if (sizeMode == QStringLiteral("content")) {
        preferred = m_surface->sizeHint().expandedTo(QSize(16, 16)).boundedTo(QSize(480, 480));
    } else if (sizeMode == QStringLiteral("crossAxis")) {
        const QSize hint = m_surface->sizeHint().expandedTo(QSize(1, 1));
        if (m_target->property("orientation").toInt() == int(Qt::Vertical))
            preferred.setWidth(hint.width());
        else
            preferred.setHeight(hint.height());
    } else if (sizeMode == QStringLiteral("contentHeight")) {
        const int width = qMax(120, preferred.width());
        const int fallbackH = viewport.size() == 2 ? viewport[1].toInt() : 160;
        const int rawH =
            m_surface->hasHeightForWidth() ? m_surface->heightForWidth(width) : m_surface->sizeHint().height();
        const int minH = m_surface->minimumHeight() > 0 ? m_surface->minimumHeight() : 0;
        const int resolvedH = qMax(rawH > 0 ? rawH : fallbackH, minH);
        preferred = QSize(width, qBound(48, resolvedH, 320));
    }
    const QSize targetSize = m_button
        ? preferred
        : (sizeMode == QStringLiteral("content") || sizeMode == QStringLiteral("fixed")
                  ? preferred
                  : preferred.expandedTo(m_surface->minimumSizeHint()))
              .boundedTo(QSize(640, 600));
    m_surface->resize(targetSize);
    if (m_surface->layout())
        m_surface->layout()->activate();
}

void GalleryComponentPlayground::syncOverlaySurfaceRect()
{
    const bool ownsOverlaySurface = m_target
        && (qobject_cast<dialogs_flyouts::Dialog*>(m_target.data())
            || qobject_cast<dialogs_flyouts::Popup*>(m_target.data())
            || qobject_cast<dialogs_flyouts::CoachMark*>(m_target.data()));
    QWidget* top = (m_interactive && m_surface && !m_surface->isWindow()) ? m_surface->window() : nullptr;
    if (m_overlayHost && m_overlayHost != top && m_overlayHost->property("fluentOverlaySurfaceRect").isValid())
        m_overlayHost->setProperty("fluentOverlaySurfaceRect", QVariant());
    m_overlayHost = (ownsOverlaySurface && top && top != m_surface) ? top : nullptr;
    if (!m_overlayHost)
        return;
    // Constrain previewed dialogs, popups, and flyouts to the preview stage rather than the full desktop host.
    // zh_CN: 将对话框与浮层的宿主区域限制在预览舞台内，避免居中或遮罩溢出到外部画布。
    const QRect rect(m_surface->mapTo(m_overlayHost, QPoint()), m_surface->size());
    if (m_overlayHost->property("fluentOverlaySurfaceRect").toRect() != rect)
        m_overlayHost->setProperty("fluentOverlaySurfaceRect", rect);
}

void GalleryComponentPlayground::presentInteractiveSurface()
{
    if (!m_interactive || !m_surface)
        return;
    const bool wasWindow = m_surface->isWindow();
    std::function<void()> reopenOverlay;
    const auto prepareReopen = [&](auto* overlay) {
        if (!wasWindow || !overlay || !overlay->isOpen())
            return false;
        // Reattach the overlay after its temporary window becomes an embedded preview.
        // zh_CN: 临时窗口嵌入预览后重新附着浮层，避免沿用旧窗口的父子关系与坐标原点。
        const bool animated = overlay->isAnimationEnabled();
        overlay->setAnimationEnabled(false);
        overlay->setIsOpen(false);
        reopenOverlay = [overlay = QPointer(overlay), animated]() {
            if (!overlay)
                return;
            overlay->setIsOpen(true);
            overlay->setAnimationEnabled(animated);
        };
        return true;
    };
    if (!prepareReopen(qobject_cast<collections::DrawerView*>(m_target.data()))
        && !prepareReopen(qobject_cast<dialogs_flyouts::Dialog*>(m_target.data()))
        && !prepareReopen(qobject_cast<dialogs_flyouts::Popup*>(m_target.data()))) {
        if (auto* coachMark = qobject_cast<dialogs_flyouts::CoachMark*>(m_target.data());
            wasWindow && coachMark && coachMark->isOpen()) {
            setCoachMarkOpenImmediate(coachMark, false);
            reopenOverlay = [coachMark = QPointer(coachMark)]() { setCoachMarkOpenImmediate(coachMark.data(), true); };
        }
    }
    if (wasWindow)
        m_surface->setWindowFlags(Qt::Widget);
    auto widgets = m_surface->findChildren<QWidget*>();
    widgets.prepend(m_surface);
    for (auto* widget : widgets)
        widget->setAttribute(Qt::WA_DontShowOnScreen, false);
    layoutSurface();
    m_surface->show();
    syncOverlaySurfaceRect();
    if (reopenOverlay)
        reopenOverlay();
    m_presented = true;
}

QJsonObject GalleryComponentPlayground::interactiveViewport(QWidget* host, bool relayout)
{
    if (!m_interactive || !m_surface || !host)
        return {};
    if (!m_presented && !relayout)
        return {};
    if (!m_presented)
        presentInteractiveSurface();
    const auto inHost = [host](const QWidget* widget, const QPoint& point = QPoint()) {
        return host->mapFromGlobal(widget->mapToGlobal(point));
    };
    if (relayout) {
        layoutSurface();
        m_surface->move(qMax(16, (host->width() - m_surface->width()) / 2), 16);
        syncOverlaySurfaceRect();
        if (auto* drawer = qobject_cast<collections::DrawerView*>(m_target.data())) {
            // The preview stage represents the drawer's application content area.
            // zh_CN: 独立预览舞台代表抽屉所属的应用内容区域。
            auto* window = drawer->window();
            const QPoint origin = window->mapFromGlobal(m_surface->mapToGlobal(QPoint()));
            drawer->setAvailableMargins(QMargins(origin.x(), origin.y(),
                qMax(0, window->width() - origin.x() - m_surface->width()),
                qMax(0, window->height() - origin.y() - m_surface->height())));
        }
    }
    QRect bounds;
    if ((m_schema.value("surface") != QStringLiteral("host") || m_overlayHost) && m_surface->isVisible())
        bounds = QRect(inHost(m_surface), m_surface->size());
    bool overlayOpen = false;
    bool visibleOverlay = false;
    const int displayWidth = qBound(280, m_surface->width() + 32, 680);
    auto* target = qobject_cast<QWidget*>(m_target.data());
    QJsonArray controls;
    QJsonArray hitTargets;
    const auto hitTarget = [&](QWidget* widget, const QRect& rect, const QString& action, int index) {
        if (rect.isEmpty())
            return;
        const auto origin = inHost(widget, rect.topLeft());
        hitTargets.append(QJsonObject{{"action", action}, {"index", index}, {"x", origin.x()}, {"y", origin.y()},
            {"width", rect.width()}, {"height", rect.height()}});
    };
    if (auto* pivot = qobject_cast<navigation::Pivot*>(target))
        for (int i = 0; i < pivot->itemCount(); ++i)
            hitTarget(pivot, pivot->itemHeaderGeometry(i), "select", i);
    if (auto* selector = qobject_cast<navigation::SelectorBar*>(target))
        for (int i = 0; i < selector->itemCount(); ++i)
            hitTarget(selector, selector->itemGeometry(i), "select", i);
    if (auto* stepper = qobject_cast<navigation::Stepper*>(target))
        for (int i = 0; i < stepper->itemCount(); ++i)
            hitTarget(stepper, stepper->itemGeometry(i), "select", i);
    if (auto* tabs = qobject_cast<navigation::TabView*>(target)) {
        for (int i = 0; i < tabs->tabCount(); ++i) {
            hitTarget(tabs, tabs->tabGeometry(i), "select", i);
            hitTarget(tabs, tabs->closeButtonGeometry(i), "close-tab", i);
        }
        hitTarget(tabs, tabs->addButtonGeometry(), "add-tab", -1);
    }
    if (auto* pager = qobject_cast<scrolling::PipsPager*>(target))
        for (int i = 0; i < pager->numberOfPages(); ++i)
            hitTarget(pager, pager->pipHitRect(i), "select", i);
    for (auto* widget : QApplication::allWidgets()) {
        if (!widget || !widget->isVisible() || widget == host || widget == host->window()
            || !belongsTo(widget, host->window()))
            continue;
        const auto* drawer = qobject_cast<collections::DrawerView*>(widget);
        const auto* popup = qobject_cast<dialogs_flyouts::Popup*>(widget);
        const auto* dialog = qobject_cast<dialogs_flyouts::Dialog*>(widget);
        const auto* coachMark = qobject_cast<dialogs_flyouts::CoachMark*>(widget);
        const bool overlay = widget->isWindow() || drawer || popup || coachMark || qobject_cast<QMenu*>(widget)
            || qobject_cast<QDialog*>(widget);
        const QRect geometry(inHost(widget), widget->size());
        if (widget == target || overlay || qobject_cast<QAbstractButton*>(widget))
            bounds |= geometry;
        if (overlay) {
            visibleOverlay = true;
            overlayOpen = overlayOpen
                || (popup           ? popup->isOpen()
                        : dialog    ? dialog->isOpen()
                        : coachMark ? coachMark->isOpen()
                        : drawer    ? drawer->isOpen()
                                    : true);
        }
        if (widget == target || widget->focusPolicy() != Qt::NoFocus || qobject_cast<QAbstractButton*>(widget))
            controls.append(QJsonObject{{"class", widget->metaObject()->className()}, {"name", widget->objectName()},
                {"text", widget->property("text").toString()}, {"x", geometry.x()}, {"y", geometry.y()},
                {"width", geometry.width()}, {"height", geometry.height()}, {"enabled", widget->isEnabled()},
                {"focused", widget->hasFocus()}});
        if (auto* view = qobject_cast<QAbstractItemView*>(widget); view && view->model()) {
            const auto visit = [&](const auto& self, const QModelIndex& parent) -> void {
                for (int row = 0; row < view->model()->rowCount(parent); ++row) {
                    const auto index = view->model()->index(row, 0, parent);
                    const QRect rect = view->visualRect(index);
                    if (rect.intersects(view->viewport()->rect()))
                        hitTarget(view->viewport(), rect, "model-item", row);
                    if (auto* tree = qobject_cast<QTreeView*>(view); tree && tree->isExpanded(index))
                        self(self, index);
                }
            };
            visit(visit, {});
        }
    }
    return {{"live", true}, {"width", host->width()}, {"displayWidth", displayWidth},
        {"height", qBound(84, bounds.bottom() + 17, 640)}, {"hasOpenOverlay", overlayOpen},
        {"hasVisibleOverlay", visibleOverlay},
        {"surface",
            QJsonObject{{"x", inHost(m_surface).x()}, {"y", inHost(m_surface).y()}, {"width", m_surface->width()},
                {"height", m_surface->height()}}},
        {"target",
            target ? QJsonObject{{"x", inHost(target).x()}, {"y", inHost(target).y()}, {"width", target->width()},
                         {"height", target->height()}}
                   : QJsonObject{}},
        {"controls", controls}, {"hitTargets", hitTargets}};
}

QImage GalleryComponentPlayground::renderPreview(qreal devicePixelRatio)
{
    if (!m_surface)
        return {};
    const qreal ratio = std::isfinite(devicePixelRatio) ? qBound(qreal(1), devicePixelRatio, qreal(3)) : 1;
    // Snap finite transitions in this isolated preview to their final frame.
    // zh_CN: 将独立预览中的有限转场推进到末帧，属性图片不冻结在旧状态。
    for (auto* animation : m_surface->findChildren<QAbstractAnimation*>())
        if (animation->state() == QAbstractAnimation::Running && animation->totalDuration() > 0)
            animation->setCurrentTime(animation->totalDuration());
    layoutSurface();
    if (auto* chart = qobject_cast<charts::ChartView*>(m_target.data()); chart && !m_interactive) {
        // Hidden chart models defer projection work until shown; an explicit raster request must be current.
        // zh_CN: 隐藏图表延迟计算投影；显式截图需要同步到当前模型。
        chart->exportImage(1);
    }
    const auto settleLayout = [this] {
        auto widgets = m_surface->findChildren<QWidget*>();
        widgets.prepend(m_surface);
        for (auto it = widgets.crbegin(); it != widgets.crend(); ++it) {
            // Browser fields own input focus; the raster copy never participates in tab order.
            // zh_CN: 输入焦点由网页编辑器持有，栅格预览副本不参与 Tab 导航。
            if (!m_button) {
                (*it)->setFocusPolicy(Qt::NoFocus);
                (*it)->clearFocus();
            }
            QCoreApplication::sendPostedEvents(*it, QEvent::LayoutRequest);
            if ((*it)->layout())
                (*it)->layout()->activate();
            if (qobject_cast<QAbstractScrollArea*>(*it)) {
                // Hidden scroll areas also need Qt's scrollbar-container layout.
                // zh_CN: 隐藏滚动视图仍需同步 Qt 的滚动条容器布局。
                QEvent layoutRequest(QEvent::LayoutRequest);
                QCoreApplication::sendEvent(*it, &layoutRequest);
            }
        }
    };
    QImage image;
    // Hidden auto-sizing controls can settle their height and flush pending Qt
    // resize events during the first render. Re-apply hover after pass 0 so
    // resize-sensitive hover overlays (e.g. chart readouts) open on the settled surface.
    // zh_CN: 隐藏控件首次 render 时会冲刷挂起的 resize 事件并稳定高度；首轮后重放悬停状态，
    // 确保依赖尺寸稳定后的悬停浮层（如图表读数卡）能留在最终截图中。
    for (int pass = 0; pass < 3; ++pass) {
        if (pass == 0 || m_previewHovered) {
            applyHoverState();
            snapFiniteAnimations(m_surface);
            if (m_surface->layout())
                m_surface->layout()->activate();
            settleLayout();
        }
        const QSize logical = m_surface->size() + QSize(4, 4);
        image = QImage(QSize(int(std::ceil(logical.width() * ratio)), int(std::ceil(logical.height() * ratio))),
            QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(ratio);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        m_surface->render(&painter, QPoint(2, 2), QRegion(), QWidget::DrawChildren);
        painter.end();
        settleLayout();
        if ((m_button || pass > 0) && m_surface->size() + QSize(4, 4) == logical)
            break;
    }
    if (m_schema.value("sizeMode") == QStringLiteral("popupContent")) {
        auto* target = qobject_cast<QWidget*>(m_target);
        int bottom = target ? target->mapTo(m_surface, QPoint(0, target->height())).y() : 0;
        for (auto* popup : m_surface->findChildren<dialogs_flyouts::Popup*>()) {
            if (popup->isOpen() && !popup->isHidden())
                bottom = qMax(bottom, popup->mapTo(m_surface, QPoint(0, popup->height())).y());
        }
        const int height = qBound(1, int(std::ceil((bottom + 12) * ratio)), image.height());
        image = image.copy(0, 0, image.width(), height);
        image.setDevicePixelRatio(ratio);
    }
    return image;
}

bool GalleryComponentPlayground::normalize(const QJsonObject& input, QJsonObject* output, QString* error) const
{
    const auto fields = m_schema.value("properties").toArray();
    for (auto it = input.begin(); it != input.end(); ++it) {
        QJsonObject field;
        for (const auto& entry : fields) {
            if (entry.toObject().value("id").toString() == it.key()) {
                field = entry.toObject();
                break;
            }
        }
        if (field.isEmpty())
            return fail(error, QStringLiteral("Unknown property: %1").arg(it.key()));
        field = resolvedFieldBounds(resolvedModelField(field, m_target, input), m_target, input);
        QJsonValue value = it.value();
        const QString type = field.value("type").toString();
        if (playground_data::handles(field) && !playground_data::validate(field, value, m_target, error))
            return false;
        if (value.isNull() && field.value("nullable").toBool()) {
            output->insert(it.key(), value);
            continue;
        }
        if (type == QStringLiteral("boolean") && !value.isBool())
            return fail(error, QStringLiteral("Expected a boolean: %1").arg(it.key()));
        if (type == QStringLiteral("text")
            && (!value.isString() || value.toString().size() > field.value("maximumLength").toInt()
                || (field.value("control") == QStringLiteral("select")
                    && (field.value("options").toArray().isEmpty()
                        || !field.value("options").toArray().contains(value)))))
            return fail(error, QStringLiteral("Invalid text: %1").arg(it.key()));
        if (type == QStringLiteral("enum") && (!value.isString() || !field.value("options").toArray().contains(value)))
            return fail(error, QStringLiteral("Unknown option: %1").arg(it.key()));
        if (type == QStringLiteral("string-list") || type == QStringLiteral("multi-enum")) {
            if (!value.isArray() || value.toArray().size() > field.value("maximumItems").toInt())
                return fail(error, QStringLiteral("Invalid item list: %1").arg(it.key()));
            for (const auto& item : value.toArray()) {
                if (!item.isString() || item.toString().isEmpty()
                    || (type == QStringLiteral("multi-enum") && !field.value("options").toArray().contains(item))
                    || (type == QStringLiteral("string-list")
                        && item.toString().size() > field.value("maximumLength").toInt()))
                    return fail(error, QStringLiteral("Invalid list item: %1").arg(it.key()));
            }
        }
        if (type == QStringLiteral("color") && (!value.isString() || !QColor(value.toString()).isValid()))
            return fail(error, QStringLiteral("Invalid color: %1").arg(it.key()));
        if ((type == QStringLiteral("datetime") && !QDateTime::fromString(value.toString(), Qt::ISODate).isValid())
            || (type == QStringLiteral("date") && !QDate::fromString(value.toString(), Qt::ISODate).isValid())
            || (type == QStringLiteral("time") && !QTime::fromString(value.toString(), Qt::ISODate).isValid()))
            return fail(error, QStringLiteral("Invalid date or time: %1").arg(it.key()));
        if (type == QStringLiteral("number")) {
            if (!value.isDouble() || !std::isfinite(value.toDouble()))
                return fail(error, QStringLiteral("Expected a finite number: %1").arg(it.key()));
            double number = qBound(field.value("minimum").toDouble(), value.toDouble(),
                field.value("maximum").toDouble());
            if (field.value("integer").toBool())
                number = std::round(number);
            for (const auto& entry : field.value("choices").toArray()) {
                const auto choice = entry.toObject();
                if (choice.value("value").toInt() == int(number) && !choice.value("enabled").toBool(true))
                    return fail(error, QStringLiteral("The model item is disabled: %1").arg(it.key()));
            }
            value = number;
        }
        output->insert(it.key(), value);
    }
    return true;
}

void GalleryComponentPlayground::writeValues(const QJsonObject& input)
{
    QScopedValueRollback<bool> applying(m_applying, true);
    if (!m_button) {
        writeGenericValues(input);
        return;
    }
    // Schema order applies checkable before checked, and icon before layout.
    // zh_CN: 描述顺序保证先设置可选中再设置选中，先设置图标再设置布局。
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const auto field = entry.toObject();
        const QString id = field.value("id").toString();
        if (!input.contains(id))
            continue;
        const auto value = input.value(id);
        if (id == QStringLiteral("icon")) {
            const bool hadIcon = m_icon != QStringLiteral("None");
            m_icon = value.toString();
            const QString glyph = glyphValueFor(m_icon);
            m_button->setIconGlyph(glyph);
            if (!input.contains(QStringLiteral("fluentLayout"))) {
                if (glyph.isEmpty())
                    m_button->setFluentLayout(Button::TextOnly);
                else if (!hadIcon && m_button->fluentLayout() == Button::TextOnly)
                    m_button->setFluentLayout(Button::IconBefore);
            }
        } else if (id == QStringLiteral("cornerRadius")) {
            m_customRadius = !value.isNull();
            if (m_customRadius) {
                const int radius = value.toInt();
                m_button->setCornerRadii(QMargins(radius, radius, radius, radius));
            } else
                m_button->resetCornerRadii();
        } else if (id == QStringLiteral("iconOffsetX") || id == QStringLiteral("iconOffsetY")) {
            QPoint offset = m_button->iconOffset();
            if (id == QStringLiteral("iconOffsetX"))
                offset.setX(value.toInt());
            else
                offset.setY(value.toInt());
            m_button->setIconOffset(offset);
        } else {
            const auto property = m_button->metaObject()->property(
                m_button->metaObject()->indexOfProperty(field.value("property").toString().toLatin1().constData()));
            property.write(m_button,
                property.isEnumType()
                    ? QVariant(property.enumerator().keyToValue(value.toString().toLatin1().constData()))
                    : value.toVariant());
        }
    }
}

bool GalleryComponentPlayground::apply(const QJsonObject& command, QString* error)
{
    if (!m_target)
        return fail(error, QStringLiteral("The preview is no longer available."));
    const QString operation = command.value("operation").toString();
    if (operation == QStringLiteral("read")) {
        ++m_revision;
        emit stateChanged();
        return true;
    }
    if (operation == QStringLiteral("hover")) {
        if (!command.contains(QStringLiteral("hovered")) || !command.value(QStringLiteral("hovered")).isBool())
            return fail(error, QStringLiteral("Expected a hovered boolean."));
        m_previewHovered = command.value(QStringLiteral("hovered")).toBool();
        if (command.contains(QStringLiteral("x")) && command.value(QStringLiteral("x")).isDouble()
            && command.contains(QStringLiteral("y")) && command.value(QStringLiteral("y")).isDouble()) {
            const double x = command.value(QStringLiteral("x")).toDouble();
            const double y = command.value(QStringLiteral("y")).toDouble();
            if (std::isfinite(x) && std::isfinite(y)) {
                m_customHoverPos = true;
                m_hoverX = qBound(0.0, x, 1.0);
                m_hoverY = qBound(0.0, y, 1.0);
            }
        } else {
            m_customHoverPos = false;
            m_hoverX = 0.6;
            m_hoverY = 0.45;
        }
        applyHoverState();
        snapFiniteAnimations(m_surface);
        ++m_revision;
        emit stateChanged();
        return true;
    }
    if (operation == QStringLiteral("activate")) {
        if (auto* button = qobject_cast<QAbstractButton*>(m_target)) {
            button->click();
            return true;
        }
        return fail(error, QStringLiteral("This component is edited through its properties."));
    }
    QScopedValueRollback<bool> applying(m_applying, true);
    QJsonObject input;
    if (operation == QStringLiteral("set")) {
        if (!command.value("values").isObject())
            return fail(error, QStringLiteral("Expected property values."));
        input = command.value("values").toObject();
    } else if (operation == QStringLiteral("reset")) {
        input = m_initial;
        if (command.contains("property")) {
            const QString id = command.value("property").toString();
            if (!input.contains(id))
                return fail(error, QStringLiteral("Unknown property: %1").arg(id));
            input = QJsonObject{{id, input.value(id)}};
        }
    } else if (operation == QStringLiteral("preset")) {
        bool found = false;
        for (const auto& entry : m_schema.value("presets").toArray()) {
            const auto preset = entry.toObject();
            if (preset.value("id") != command.value("preset"))
                continue;
            input = preset.value("id") == QStringLiteral("default") ? m_initial : defaults();
            const auto overrides = preset.value("values").toObject();
            for (auto it = overrides.begin(); it != overrides.end(); ++it)
                input[it.key()] = it.value();
            found = true;
            break;
        }
        if (!found)
            return fail(error, QStringLiteral("Unknown preset."));
    } else
        return fail(error, QStringLiteral("Unknown playground operation."));
    QJsonObject normalized;
    const bool resetFixture = !m_button && operation == QStringLiteral("reset") && !command.contains("property");
    if (!resetFixture && !normalize(input, &normalized, error))
        return false;
    if (resetFixture) {
        // Full reset also restores internal cursor, scroll and disclosure state owned by the fixture.
        // zh_CN: 完整重置也恢复示例内部的光标、滚动位置和折叠状态。
        const auto replacement = createPlaygroundControl(m_schema.value("component").toString(),
            m_surface->parentWidget());
        if (!replacement.surface || !replacement.target)
            return fail(error, QStringLiteral("Could not reset the preview."));
        const QPointer<QWidget> previousSurface = m_surface;
        const QPointer<QObject> previousTarget = m_target;
        const bool targetOwnedBySurface = belongsTo(previousTarget, previousSurface);
        m_surface = replacement.surface;
        m_target = replacement.target;
        if (!normalize(input, &normalized, error)) {
            const bool replacementOwned = belongsTo(replacement.target, replacement.surface);
            delete replacement.surface;
            if (replacement.target && !replacementOwned)
                delete replacement.target;
            m_surface = previousSurface;
            m_target = previousTarget;
            return false;
        }
        if (m_interactive) {
            // Qt's browser window callbacks can still reference the old surface in this event turn.
            // zh_CN: 当前事件轮次中的浏览器窗口回调可能仍引用旧表面，先退出交互再延迟析构。
            previousSurface->hide();
            QObject::disconnect(previousTarget, nullptr, this, nullptr);
            for (auto* action : previousSurface->findChildren<QAction*>())
                QObject::disconnect(action, nullptr, this, nullptr);
            previousSurface->deleteLater();
            if (previousTarget && !targetOwnedBySurface) {
                if (auto* targetWidget = qobject_cast<QWidget*>(previousTarget.data()))
                    targetWidget->hide();
                previousTarget->deleteLater();
            }
        } else {
            delete previousSurface;
            if (previousTarget && !targetOwnedBySurface)
                delete previousTarget;
        }
        m_presented = false;
        connectGenericEvents();
    }
    writeValues(normalized);
    if (operation == QStringLiteral("reset") && !command.contains("property")) {
        m_events = {};
        m_previewHovered = false;
        m_customHoverPos = false;
        m_hoverX = 0.6;
        m_hoverY = 0.45;
    }
    applyHoverState();
    if (!m_interactive)
        snapFiniteAnimations(m_surface);
    if (!m_button) {
        layoutSurface();
        if (m_presented) {
            if (QWidget* host = m_surface->parentWidget())
                m_surface->move(qMax(16, (host->width() - m_surface->width()) / 2), m_surface->y());
            syncOverlaySurfaceRect();
        }
    }
    ++m_revision;
    emit stateChanged();
    return true;
}

QString GalleryComponentPlayground::code(const QJsonObject& current, bool python) const
{
    if (!m_button)
        return genericCode(current, python);
    QString result = python
        ? QStringLiteral("import fluentqt\n")
        : QStringLiteral("#include <FluentQt/BasicInput.h>\nusing fluent::basicinput::Button;\n");
    const bool radius = !current.value("cornerRadius").isNull();
    const bool offset = current.value("iconOffsetX").toInt() || current.value("iconOffsetY").toInt();
    if (python && (radius || offset))
        result += QStringLiteral("from PySide6.QtCore import ") + (radius ? QStringLiteral("QMargins") : QString())
            + (radius && offset ? QStringLiteral(", ") : QString()) + (offset ? QStringLiteral("QPoint") : QString())
            + QLatin1Char('\n');
    if (!python && current.value("icon").toString() != QStringLiteral("None"))
        result += QStringLiteral("#include <FluentQt/Design.h>\n");
    result += python
        ? QStringLiteral("\nbutton = fluentqt.Button(%1, self)\n").arg(quoted(current.value("text").toString(), true))
        : QStringLiteral("\nauto* button = new Button(%1, this);\n")
              .arg(quoted(current.value("text").toString(), false));
    const QString access = python ? QStringLiteral("button.") : QStringLiteral("button->");
    const QString end = python ? QStringLiteral(")\n") : QStringLiteral(");\n");
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const auto field = entry.toObject();
        const QString id = field.value("id").toString();
        const auto value = current.value(id);
        if (id == QStringLiteral("text") || id.startsWith(QStringLiteral("iconOffset")))
            continue;
        if (value == field.value("default")) {
            if (!(id == QStringLiteral("fluentLayout")
                    && current.value(QStringLiteral("icon")).toString() != QStringLiteral("None")))
                continue;
        }
        QString setter = field.value("setter").toString();
        QString argument;
        if (id == QStringLiteral("icon")) {
            setter = QStringLiteral("setIconGlyph");
            argument = (python ? QStringLiteral("fluentqt.Typography.Icons.") : QStringLiteral("Typography::Icons::"))
                + value.toString();
        } else if (id == QStringLiteral("cornerRadius")) {
            setter = QStringLiteral("setCornerRadii");
            argument = QStringLiteral("QMargins(%1, %1, %1, %1)").arg(value.toInt());
        } else if (value.isBool())
            argument = python
                ? (value.toBool() ? QStringLiteral("True") : QStringLiteral("False"))
                : (value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
        else if (value.isDouble())
            argument = QLocale::c().toString(value.toDouble(), 'g', 12);
        else
            argument = field.value(python ? "pythonPrefix" : "cppPrefix").toString() + value.toString();
        result += access + setter + QLatin1Char('(') + argument + end;
    }
    if (offset)
        result += access
            + QStringLiteral("setIconOffset(QPoint(%1, %2)")
                  .arg(current.value("iconOffsetX").toInt())
                  .arg(current.value("iconOffsetY").toInt())
            + end;
    return result;
}

QJsonObject GalleryComponentPlayground::snapshot() const
{
    if (!m_target)
        return {};
    const auto current = values();
    const auto initial = m_initial;
    QJsonArray modified;
    for (auto it = current.begin(); it != current.end(); ++it)
        if (it.value() != initial.value(it.key()))
            modified.append(it.key());
    QString matchingPreset = QStringLiteral("custom");
    for (const auto& entry : m_schema.value("presets").toArray()) {
        const auto preset = entry.toObject();
        auto expected = preset.value("id") == QStringLiteral("default") ? initial : defaults();
        const auto overrides = preset.value("values").toObject();
        for (auto it = overrides.begin(); it != overrides.end(); ++it)
            expected[it.key()] = it.value();
        if (expected == current) {
            matchingPreset = preset.value("id").toString();
            break;
        }
    }
    QJsonObject effective;
    if (m_button) {
        effective.insert(QStringLiteral("cornerRadius"), m_button->cornerRadii().left());
    } else {
        for (const auto& entry : m_schema.value("properties").toArray()) {
            const auto field = entry.toObject();
            const auto id = field.value("id").toString();
            if (field.value("nullable").toBool() && field.value("type") == QStringLiteral("color")
                && current.value(id).isNull())
                effective.insert(id, resolvedNullableColorFallback(m_target, field));
        }
    }
    auto schema = m_schema;
    auto fields = schema.value("properties").toArray();
    for (int i = 0; i < fields.size(); ++i)
        fields[i] = resolvedFieldBounds(resolvedEnumField(resolvedModelField(fields[i].toObject(), m_target), m_target),
            m_target);
    schema["properties"] = fields;
    return {{"schema", schema}, {"values", current}, {"modified", modified}, {"preset", matchingPreset},
        {"revision", m_revision}, {"events", m_events}, {"hovered", m_previewHovered},
        {"canActivate", bool(qobject_cast<QAbstractButton*>(m_target))}, {"effective", effective},
        {"cpp", code(current, false)}, {"python", code(current, true)}};
}

bool GalleryComponentPlayground::accepts(const QString& route, const QWidget* source)
{
    if (!source)
        return false;
    QFile file(QStringLiteral(":/app/playgrounds/%1.json").arg(route));
    if (!playgroundRoutes().contains(route) || !file.open(QIODevice::ReadOnly))
        return false;
    const auto schema = QJsonDocument::fromJson(file.readAll()).object();
    const auto type = route == QStringLiteral("button")
        ? QByteArray("fluent::basicinput::Button")
        : schema.value("className").toString().toLatin1();
    return source->inherits(type.constData());
}

GalleryComponentPlayground*
GalleryComponentPlayground::fromWidget(const QString& route, const QWidget* source, QWidget* parent)
{
    if (!accepts(route, source))
        return nullptr;
    if (route == QStringLiteral("button"))
        return fromButton(qobject_cast<const Button*>(source), parent);
    auto* result = create(route, parent);
    if (!result)
        return nullptr;
    // Non-button editors use the published independent fixture, including its model/content.
    // zh_CN: 其他编辑器使用公开的独立示例，连同模型和内容一起创建。
    result->m_initial = result->values();
    return result;
}

QJsonObject GalleryComponentPlayground::genericValues(const QObject* target) const
{
    QJsonObject result;
    if (!target)
        return result;
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const auto field = entry.toObject();
        if (field.value("id") == QStringLiteral("icon")) {
            result["icon"] = m_icon;
            continue;
        }
        if (field.contains("adapter")) {
            result[field.value("id").toString()] = adapterValue(field, const_cast<QObject*>(target));
            continue;
        }
        const auto name = field.value("property").toString().toLatin1();
        const auto prop = target->metaObject()->property(target->metaObject()->indexOfProperty(name));
        if (!prop.isValid() && !methodProperty(target, QString::fromLatin1(name)))
            continue;
        const auto value = prop.isValid() ? prop.read(target) : readMethodProperty(target, QString::fromLatin1(name));
        const auto type = field.value("type").toString();
        const auto id = field.value("id").toString();
        if (field.contains("sizeAxis")) {
            result[id] =
                field.value("sizeAxis") == QStringLiteral("width") ? value.toSize().width() : value.toSize().height();
        } else if (field.contains("pointAxis")) {
            result[id] = field.value("pointAxis") == QStringLiteral("x") ? value.toPoint().x() : value.toPoint().y();
        } else if (field.value("itemList").toBool()) {
            QJsonArray items;
            for (const auto& item : value.toList())
                items.append(item.toMap().value(QStringLiteral("text")).toString());
            result[id] = items;
        } else if (id == QStringLiteral("selectedRows")) {
            QJsonArray items;
            const auto options = field.value("options").toArray();
            for (const auto& row : value.toList())
                if (row.toInt() >= 0 && row.toInt() < options.size())
                    items.append(options[row.toInt()]);
            result[id] = items;
        } else if (field.value("glyphEnum").toBool()) {
            result[id] = glyphNameFor(value.toString());
        } else if (isCollectionsSelectionMode(prop)) {
            result[id] = QString::fromLatin1(QMetaEnum::fromType<collections::SelectionMode>().valueToKey(
                int(value.value<collections::SelectionMode>())));
        } else if (isBackdropEffect(prop)) {
            switch (value.value<windowing::BackdropEffect>()) {
            case windowing::BackdropEffect::Solid:
                result[id] = QStringLiteral("Solid");
                break;
            case windowing::BackdropEffect::Mica:
                result[id] = QStringLiteral("Mica");
                break;
            case windowing::BackdropEffect::Acrylic:
                result[id] = QStringLiteral("Acrylic");
                break;
            }
        } else if (prop.isEnumType() || field.contains("enumValues"))
            result[id] = resolvedEnumKey(field, target, value.toInt());
        else if (type == QStringLiteral("color")) {
            const QColor color = value.value<QColor>();
            result[id] = (field.value("nullable").toBool() && !color.isValid())
                ? QJsonValue(QJsonValue::Null)
                : QJsonValue(color.name(QColor::HexArgb));
        } else if (type == QStringLiteral("date")) {
            const QDate date = value.toDate();
            result[id] = (!date.isValid() && field.value("nullable").toBool())
                ? QJsonValue(QJsonValue::Null)
                : QJsonValue(date.toString(Qt::ISODate));
        } else if (type == QStringLiteral("time"))
            result[id] = !value.toTime().isValid() && field.value("nullable").toBool()
                ? QJsonValue(QJsonValue::Null)
                : QJsonValue(value.toTime().toString(Qt::ISODate));
        else if (type == QStringLiteral("datetime"))
            result[id] = value.toDateTime().isValid()
                ? QJsonValue(value.toDateTime().toString(Qt::ISODate))
                : QJsonValue(QJsonValue::Null);
        else
            result[id] = QJsonValue::fromVariant(value);
    }
    return result;
}

void GalleryComponentPlayground::writeGenericValues(const QJsonObject& values)
{
    auto* range = qobject_cast<basicinput::RangeSlider*>(m_target);
    const bool writeRangePair = range && values.contains("lowerValue") && values.contains("upperValue");
    auto* deferredPopup =
        m_schema.value("deferOpen").toBool() ? qobject_cast<dialogs_flyouts::Popup*>(m_target) : nullptr;
    const bool openAfter = deferredPopup && values.value("isOpen").toBool(deferredPopup->isOpen());
    QJsonObject afterOpenValues;
    if (deferredPopup) {
        const auto current = genericValues(m_target);
        for (const auto& entry : m_schema.value("properties").toArray()) {
            const auto field = entry.toObject();
            const auto id = field.value("id").toString();
            if (field.value("afterOpen").toBool())
                afterOpenValues[id] = values.contains(id) ? values.value(id) : current.value(id);
        }
    }
    const auto setOpenImmediate = [](auto* overlay, bool open) {
        // Property edits commit state immediately; on-stage triggers retain native motion.
        // zh_CN: 属性编辑即时提交开关状态；舞台上的触发按钮仍保留组件原生动效。
        const bool animated = overlay->isAnimationEnabled();
        overlay->setAnimationEnabled(false);
        overlay->setIsOpen(open);
        overlay->setAnimationEnabled(animated);
    };
    if (deferredPopup && deferredPopup->isOpen()) {
        for (const auto& field : m_schema.value("reopenOnChange").toArray()) {
            if (values.contains(field.toString())) {
                setOpenImmediate(deferredPopup, false);
                break;
            }
        }
    }
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const auto field = entry.toObject();
        const auto id = field.value("id").toString();
        if (!values.contains(id))
            continue;
        if (id == QStringLiteral("isOpen")) {
            if (auto* drawer = qobject_cast<collections::DrawerView*>(m_target.data())) {
                setOpenImmediate(drawer, values.value(id).toBool());
                continue;
            }
            if (auto* dialog = qobject_cast<dialogs_flyouts::Dialog*>(m_target.data())) {
                setOpenImmediate(dialog, values.value(id).toBool());
                continue;
            }
            if (auto* popup = qobject_cast<dialogs_flyouts::Popup*>(m_target.data()); popup && !deferredPopup) {
                setOpenImmediate(popup, values.value(id).toBool());
                continue;
            }
        }
        if (id == QStringLiteral("open")) {
            if (auto* coachMark = qobject_cast<dialogs_flyouts::CoachMark*>(m_target.data())) {
                setCoachMarkOpenImmediate(coachMark, values.value(id).toBool());
                continue;
            }
        }
        if (id == QStringLiteral("expanded")) {
            if (auto* expander = qobject_cast<layout::Expander*>(m_target.data())) {
                expander->setExpandedAnimated(values.value(id).toBool(), false);
                continue;
            }
        }
        if (m_schema.value("component") == QStringLiteral("image-viewer") && id == QStringLiteral("zoomFactor")
            && values.contains("fitMode") && values.value("fitMode") != QStringLiteral("Manual"))
            continue;
        if (field.contains("adapter")) {
            writeAdapterValue(field, m_target, values.value(id));
            continue;
        }
        if (writeRangePair && (id == QStringLiteral("lowerValue") || id == QStringLiteral("upperValue")))
            continue;
        if (deferredPopup && id == QStringLiteral("isOpen"))
            continue;
        if (deferredPopup && field.value("afterOpen").toBool())
            continue;
        if (id == QStringLiteral("icon")) {
            auto* button = qobject_cast<Button*>(m_target);
            const bool hadIcon = m_icon != QStringLiteral("None");
            m_icon = values.value(id).toString();
            const auto glyph = glyphValueFor(m_icon);
            button->setIconGlyph(glyph);
            if (!values.contains(QStringLiteral("fluentLayout"))) {
                if (glyph.isEmpty())
                    button->setFluentLayout(Button::TextOnly);
                else if (!hadIcon && button->fluentLayout() == Button::TextOnly)
                    button->setFluentLayout(Button::IconBefore);
            }
            continue;
        }
        const auto name = field.value("property").toString().toLatin1();
        const auto prop = m_target->metaObject()->property(m_target->metaObject()->indexOfProperty(name));
        const auto value = values.value(id);
        const auto type = field.value("type").toString();
        QVariant argument = value.toVariant();
        if (field.contains("sizeAxis")) {
            QSize size = prop.read(m_target).toSize();
            if (field.value("sizeAxis") == QStringLiteral("width"))
                size.setWidth(value.toInt());
            else
                size.setHeight(value.toInt());
            argument = size;
        } else if (field.contains("pointAxis")) {
            QPoint point = prop.read(m_target).toPoint();
            if (field.value("pointAxis") == QStringLiteral("x"))
                point.setX(value.toInt());
            else
                point.setY(value.toInt());
            argument = point;
        } else if (field.value("itemList").toBool()) {
            QVariantList items;
            for (const auto& item : value.toArray())
                items.append(
                    QVariantMap{{QStringLiteral("key"), item.toString()}, {QStringLiteral("text"), item.toString()}});
            argument = items;
        } else if (id == QStringLiteral("selectedRows")) {
            QVariantList rows;
            const auto options = field.value("options").toArray();
            for (const auto& item : value.toArray()) {
                for (int row = 0; row < options.size(); ++row)
                    if (options[row] == item)
                        rows.append(row);
            }
            argument = rows;
        } else if (type == QStringLiteral("string-list") || type == QStringLiteral("multi-enum")) {
            QStringList items;
            for (const auto& item : value.toArray())
                items.append(item.toString());
            argument = items;
        } else if (field.value("glyphEnum").toBool()) {
            argument = glyphValueFor(value.toString());
        } else if (isCollectionsSelectionMode(prop)) {
            argument = fluentEnumPropertyValue(prop,
                static_cast<collections::SelectionMode>(QMetaEnum::fromType<collections::SelectionMode>().keyToValue(
                    value.toString().toLatin1().constData())));
        } else if (isBackdropEffect(prop)) {
            const auto key = value.toString();
            const auto effect = key == QStringLiteral("Solid") ? windowing::BackdropEffect::Solid
                : key == QStringLiteral("Acrylic")
                ? windowing::BackdropEffect::Acrylic
                : windowing::BackdropEffect::Mica;
            argument = fluentEnumPropertyValue(prop, effect);
        } else if (field.contains("enumValues"))
            argument = field.value("enumValues").toObject().value(value.toString()).toInt();
        else if (prop.isEnumType())
            argument = prop.enumerator().keyToValue(value.toString().toLatin1().constData());
        else if (type == QStringLiteral("color"))
            argument = value.isNull() ? QColor() : QColor(value.toString());
        else if (type == QStringLiteral("date"))
            argument = value.isNull() ? QDate() : QDate::fromString(value.toString(), Qt::ISODate);
        else if (type == QStringLiteral("time"))
            argument = value.isNull() ? QTime() : QTime::fromString(value.toString(), Qt::ISODate);
        else if (type == QStringLiteral("datetime"))
            argument = value.isNull() ? QDateTime() : QDateTime::fromString(value.toString(), Qt::ISODate);
        else if (type == QStringLiteral("number") && value.isNull())
            argument = std::numeric_limits<double>::quiet_NaN();
        if (prop.isValid())
            prop.write(m_target, argument);
        else
            writeMethodProperty(m_target, QString::fromLatin1(name), argument);
    }
    if (writeRangePair)
        range->setValues(values.value("lowerValue").toInt(), values.value("upperValue").toInt());
    // QComboBox keeps its field placeholder separate from the editable QLineEdit.
    // zh_CN: QComboBox 的闭合框和可编辑输入框分别保存占位文字；示例同步两者。
    if (auto* combo = qobject_cast<QComboBox*>(m_target); combo && combo->lineEdit())
        combo->lineEdit()->setPlaceholderText(combo->placeholderText());
    if (auto* splash = qobject_cast<status_info::SplashScreen*>(m_target);
        splash && values.contains(QStringLiteral("indeterminate"))) {
        splash->setIndeterminate(values.value(QStringLiteral("indeterminate")).toBool());
    }
    if (deferredPopup) {
        setOpenImmediate(deferredPopup, openAfter);
        for (const auto& entry : m_schema.value("properties").toArray()) {
            const auto field = entry.toObject();
            const auto id = field.value("id").toString();
            if (afterOpenValues.contains(id))
                m_target->setProperty(field.value("property").toString().toLatin1().constData(),
                    afterOpenValues.value(id).toVariant());
        }
    }
    if (auto* coachMark = qobject_cast<dialogs_flyouts::CoachMark*>(m_target.data()))
        snapFiniteAnimations(coachMark);
}

QString GalleryComponentPlayground::genericCode(const QJsonObject& current, bool python) const
{
    const auto language = python ? "python" : "cpp";
    const auto recipe = m_schema.value("recipe").toObject().value(language).toObject();
    QString result = recipe.value("imports").toString() + QLatin1Char('\n')
        + recipe.value("setup").toString().replace(QStringLiteral("%PARENT%"),
            python ? QStringLiteral("self") : QStringLiteral("this"));
    const QString access = python ? QStringLiteral("control.") : QStringLiteral("control->");
    const QString end = python ? QStringLiteral(")\n") : QStringLiteral(");\n");
    QSet<QString> emittedPoints;
    QString afterOpenCode;
    const bool rangePair = m_schema.value("component") == QStringLiteral("range-slider");
    // Emit all editable properties. Constructor/fixture defaults may differ between subclasses.
    // zh_CN: 输出全部可编辑属性，避免派生类型或示例初始值差异导致导出不一致。
    for (const auto& entry : m_schema.value("properties").toArray()) {
        const auto field = entry.toObject();
        if (field.contains("adapter")) {
            const auto adapter = field.value("adapter").toString();
            const auto value = current.value(field.value("id").toString());
            if (playground_data::handles(field)) {
                if ((adapter == "table-data" || adapter == "chart-data" || adapter == "chart-name")
                    && value == m_initial.value(field.value("id").toString()))
                    continue;
                result += playground_data::code(field, value, m_target, python, quoted);
            } else if (adapter == QStringLiteral("stack-details")) {
                if (value.toBool())
                    result += python
                        ? QStringLiteral("control.pushReparentedItem(details)\n")
                        : QStringLiteral("control->push(details, fluent::WidgetOwnership::Reparented);\n");
            } else if (adapter == QStringLiteral("field-editor")) {
                const auto argument = value.isBool()
                    ? (python ? (value.toBool() ? "True" : "False") : (value.toBool() ? "true" : "false"))
                    : quoted(value.toString(), python);
                result += (python ? QStringLiteral("editor.") : QStringLiteral("editor->"))
                    + field.value("setter").toString() + QLatin1Char('(') + argument + end;
            } else if (adapter == QStringLiteral("property-value")) {
                const auto argument = value.isBool()
                    ? (python ? (value.toBool() ? "True" : "False") : (value.toBool() ? "true" : "false"))
                    : quoted(value.toString(), python);
                result += access + QStringLiteral("setValue(") + quoted(field.value("key").toString(), python)
                    + QStringLiteral(", ") + argument + end;
            } else if (adapter == QStringLiteral("pane-size")) {
                result += access
                    + QStringLiteral("setPanePreferredSize(%1, %2").arg(field.value("index").toInt()).arg(value.toInt())
                    + end;
            } else if (adapter == QStringLiteral("toast-open")) {
                result += access
                    + (value.toBool() ? (python ? QStringLiteral("present(self") : QStringLiteral("present(this"))
                                      : QStringLiteral("dismiss("))
                    + end;
            } else if (adapter == QStringLiteral("splash-progress")) {
                result += access + QStringLiteral("setProgress(%1, 100").arg(value.toInt()) + end;
            } else if (adapter == QStringLiteral("navigation-index")) {
                result += python
                    ? QStringLiteral("nav_list.setCurrentIndex(nav_model.index(%1, 0))\n").arg(value.toInt())
                    : QStringLiteral("navList->setCurrentIndex(navModel->index(%1, 0));\n").arg(value.toInt());
            } else if (adapter == QStringLiteral("accordion")) {
                const auto options = field.value("options").toArray();
                for (int i = 0; i < options.size(); ++i) {
                    const bool expanded = value.toArray().contains(options[i]);
                    result += access + QStringLiteral("itemAt(%1)").arg(i) + (python ? "." : "->")
                        + QStringLiteral("setExpanded(")
                        + (python ? (expanded ? "True" : "False") : (expanded ? "true" : "false")) + end;
                }
            } else if (adapter == QStringLiteral("data-grid-filter")) {
                QStringList items;
                for (const auto& item : value.toArray())
                    items.append(quoted(item.toString(), python));
                const QString list = (python ? QStringLiteral("[") : QStringLiteral("QStringList{"))
                    + items.join(QStringLiteral(", ")) + (python ? QStringLiteral("]") : QStringLiteral("}"));
                result += access + QStringLiteral("setFilterValues(") + quoted(field.value("key").toString(), python)
                    + QStringLiteral(", ") + list + end;
            } else if (auto* view = qobject_cast<QAbstractItemView*>(m_target)) {
                const bool expanded = adapter == QStringLiteral("model-expanded");
                result += access + (expanded ? QStringLiteral("collapseAll(") : QStringLiteral("clearSelection("))
                    + end;
                for (const auto& option : value.toArray()) {
                    QString index;
                    for (const auto& step :
                        option.toString().section(QStringLiteral(" · "), 0, 0).split(QLatin1Char('/'))) {
                        const auto pair = step.split(QLatin1Char(':'));
                        const auto parent = index.isEmpty() ? QString() : QStringLiteral(", ") + index;
                        index = access + (python ? QStringLiteral("model().index(") : QStringLiteral("model()->index("))
                            + pair.value(0) + QStringLiteral(", ") + pair.value(1) + parent + QLatin1Char(')');
                    }
                    if (expanded)
                        result += access + QStringLiteral("setExpanded(") + index + (python ? ", True" : ", true")
                            + end;
                    else {
                        const QString prefix = python
                            ? QStringLiteral("QItemSelectionModel.SelectionFlag.")
                            : QStringLiteral("QItemSelectionModel::");
                        const QString flags = prefix + QStringLiteral("Select")
                            + (view->selectionBehavior() == QAbstractItemView::SelectRows
                                    ? QStringLiteral(" | ") + prefix + QStringLiteral("Rows")
                                    : QString());
                        result += access
                            + (python ? QStringLiteral("selectionModel().select(")
                                      : QStringLiteral("selectionModel()->select("))
                            + index + QStringLiteral(", ") + flags + end;
                        result += access
                            + (python ? QStringLiteral("selectionModel().setCurrentIndex(")
                                      : QStringLiteral("selectionModel()->setCurrentIndex("))
                            + index + QStringLiteral(", ") + prefix + QStringLiteral("NoUpdate") + end;
                        result += access + QStringLiteral("scrollTo(") + index
                            + (python ? QStringLiteral(", QAbstractItemView.ScrollHint.EnsureVisible")
                                      : QStringLiteral(", QAbstractItemView::EnsureVisible"))
                            + end;
                    }
                }
            }
            continue;
        }
        if (field.contains("itemOperations")) {
            const auto operations = field.value("itemOperations").toObject();
            result += access + operations.value("clear").toString()
                + (python ? QStringLiteral("()\n") : QStringLiteral("();\n"));
            QStringList items;
            for (const auto& item : current.value(field.value("id").toString()).toArray()) {
                if (operations.contains("append"))
                    result += access + operations.value("append").toString() + QLatin1Char('(')
                        + quoted(item.toString(), python) + end;
                else
                    items.append(quoted(item.toString(), python));
            }
            if (operations.contains("appendList"))
                result += access + operations.value("appendList").toString() + QLatin1Char('(')
                    + (python ? QStringLiteral("[") : QStringLiteral("QStringList{")) + items.join(QStringLiteral(", "))
                    + (python ? QStringLiteral("]") : QStringLiteral("}")) + end;
            continue;
        }
        if (field.value("id") == QStringLiteral("currentText") && !current.value("editable").toBool())
            continue;
        if (m_schema.value("component") == QStringLiteral("image-viewer")
            && field.value("id") == QStringLiteral("zoomFactor")
            && current.value("fitMode") != QStringLiteral("Manual"))
            continue;
        if (rangePair
            && (field.value("id") == QStringLiteral("lowerValue") || field.value("id") == QStringLiteral("upperValue")))
            continue;
        if (m_schema.value("deferOpen").toBool() && field.value("id") == QStringLiteral("isOpen"))
            continue;
        const auto value = current.value(field.value("id").toString());
        const auto type = field.value("type").toString();
        QString argument;
        if (field.contains("sizeAxis")) {
            const QString property = field.value("property").toString();
            if (emittedPoints.contains(property))
                continue;
            emittedPoints.insert(property);
            QSize size;
            for (const auto& coordinateEntry : m_schema.value("properties").toArray()) {
                const auto coordinate = coordinateEntry.toObject();
                if (coordinate.value("property") != property)
                    continue;
                const int number = current.value(coordinate.value("id").toString()).toInt();
                if (coordinate.value("sizeAxis") == "width")
                    size.setWidth(number);
                else
                    size.setHeight(number);
            }
            argument = QStringLiteral("QSize(%1, %2)").arg(size.width()).arg(size.height());
        } else if (field.contains("pointAxis")) {
            const QString property = field.value("property").toString();
            if (emittedPoints.contains(property))
                continue;
            emittedPoints.insert(property);
            QPoint point;
            for (const auto& coordinateEntry : m_schema.value("properties").toArray()) {
                const auto coordinate = coordinateEntry.toObject();
                if (coordinate.value("property") != property)
                    continue;
                const int number = current.value(coordinate.value("id").toString()).toInt();
                if (coordinate.value("pointAxis") == QStringLiteral("x"))
                    point.setX(number);
                else
                    point.setY(number);
            }
            argument = QStringLiteral("QPoint(%1, %2)").arg(point.x()).arg(point.y());
        } else if (type == QStringLiteral("string-list") || type == QStringLiteral("multi-enum")) {
            QStringList items;
            for (const auto& item : value.toArray()) {
                if (field.value("itemList").toBool()) {
                    const auto text = quoted(item.toString(), python);
                    items.append(python
                            ? QStringLiteral("{\"key\": %1, \"text\": %1}").arg(text)
                            : QStringLiteral(
                                  "QVariantMap{{QStringLiteral(\"key\"), %1}, {QStringLiteral(\"text\"), %1}}")
                                  .arg(text));
                } else if (field.value("id") == QStringLiteral("selectedRows")) {
                    const auto options = field.value("options").toArray();
                    for (int row = 0; row < options.size(); ++row)
                        if (options[row] == item)
                            items.append(QString::number(row));
                } else
                    items.append(quoted(item.toString(), python));
            }
            const auto prefix = field.value("itemList").toBool() ? QStringLiteral("QVariantList{")
                : field.value("id") == QStringLiteral("selectedRows")
                ? QStringLiteral("QList<int>{")
                : QStringLiteral("QStringList{");
            argument = (python ? QStringLiteral("[") : prefix) + items.join(QStringLiteral(", "))
                + (python ? QStringLiteral("]") : QStringLiteral("}"));
        } else if (field.value("id") == QStringLiteral("icon") || field.value("glyphEnum").toBool()) {
            const QString prefix = field.contains("cppPrefix")
                ? field.value(python ? "pythonPrefix" : "cppPrefix").toString()
                : (python ? QStringLiteral("fluentqt.Typography.Icons.") : QStringLiteral("Typography::Icons::"));
            argument =
                value.toString() == QStringLiteral("None") ? quoted(QString(), python) : prefix + value.toString();
        } else if (type == QStringLiteral("enum")) {
            QString key = value.toString();
            if (python && key == QStringLiteral("None"))
                key = QStringLiteral("None_");
            argument = field.value(python ? "pythonPrefix" : "cppPrefix").toString() + key;
        } else if (type == QStringLiteral("boolean"))
            argument = python
                ? (value.toBool() ? QStringLiteral("True") : QStringLiteral("False"))
                : (value.toBool() ? QStringLiteral("true") : QStringLiteral("false"));
        else if (type == QStringLiteral("number"))
            argument = value.isNull()
                ? (python ? QStringLiteral("float(\"nan\")")
                          : QStringLiteral("std::numeric_limits<double>::quiet_NaN()"))
                : QLocale::c().toString(value.toDouble(), 'g', 12);
        else if (type == QStringLiteral("color")) {
            if (value.isNull())
                continue;
            argument = QStringLiteral("QColor(") + quoted(value.toString(), python) + QLatin1Char(')');
        } else if (type == QStringLiteral("date") || type == QStringLiteral("time")
            || type == QStringLiteral("datetime")) {
            const QString name = type == QStringLiteral("datetime") ? QStringLiteral("QDateTime")
                : type == QStringLiteral("date")
                ? QStringLiteral("QDate")
                : QStringLiteral("QTime");
            if (value.isNull())
                argument = name + QStringLiteral("()");
            else
                argument = name + (python ? QStringLiteral(".fromString(") : QStringLiteral("::fromString("))
                    + quoted(value.toString(), python)
                    + (python ? QStringLiteral(", Qt.DateFormat.ISODate)") : QStringLiteral(", Qt::ISODate)"));
        } else
            argument = quoted(value.toString(), python);
        const auto statement = access + field.value("setter").toString() + QLatin1Char('(') + argument + end;
        if ((m_schema.value("deferOpen").toBool() && field.value("afterOpen").toBool())
            || (m_schema.value("component") == QStringLiteral("splash-screen")
                && field.value("id") == QStringLiteral("indeterminate")))
            afterOpenCode += statement;
        else
            result += statement;
    }
    if (m_schema.value("component") == QStringLiteral("combobox") && current.value("editable").toBool())
        result += python
            ? QStringLiteral("control.lineEdit().setPlaceholderText(control.placeholderText())\n")
            : QStringLiteral("control->lineEdit()->setPlaceholderText(control->placeholderText());\n");
    if (m_schema.value("deferOpen").toBool())
        result += access + QStringLiteral("setIsOpen(")
            + (python ? (current.value("isOpen").toBool() ? QStringLiteral("True") : QStringLiteral("False"))
                      : (current.value("isOpen").toBool() ? QStringLiteral("true") : QStringLiteral("false")))
            + end;
    if (rangePair)
        result += access
            + QStringLiteral("setValues(%1, %2")
                  .arg(current.value("lowerValue").toInt())
                  .arg(current.value("upperValue").toInt())
            + end;
    return result + afterOpenCode;
}

void GalleryComponentPlayground::connectGenericEvents()
{
    if (auto* dropZone = qobject_cast<basicinput::FileDropZone*>(m_target)) {
        connect(dropZone, &basicinput::FileDropZone::browseRequested, this, [this] {
            recordEvent(QStringLiteral("browseRequested"), false);
        });
        connect(dropZone, &basicinput::FileDropZone::filesDropped, this, [this] {
            recordEvent(QStringLiteral("filesDropped"), false);
        });
    }
    for (auto* action : m_surface->findChildren<QAction*>())
        connect(action, &QAction::triggered, this, [this, action](bool checked) {
            recordEvent(action->text(), checked);
        });
    if (auto* button = qobject_cast<QAbstractButton*>(m_target))
        connect(button, &QAbstractButton::clicked, this, [this](bool checked) {
            recordEvent(QStringLiteral("clicked"), checked);
        });
    const auto publishInteractiveChange = [this] {
        if (!m_applying) {
            ++m_revision;
            emit stateChanged();
        }
    };
    const auto refreshLayoutAndPublish = [this, publishInteractiveChange] {
        if (!m_target || !m_surface)
            return;
        if (m_schema.value("sizeMode") == QStringLiteral("contentHeight"))
            layoutSurface();
        publishInteractiveChange();
    };
    if (auto* expander = qobject_cast<layout::Expander*>(m_target)) {
        connect(expander, &layout::Expander::expandedChanged, this, [refreshLayoutAndPublish](bool) {
            refreshLayoutAndPublish();
        });
        connect(expander, &layout::Expander::layoutHeightChanged, this, [refreshLayoutAndPublish](int) {
            refreshLayoutAndPublish();
        });
    }
    if (auto* accordion = qobject_cast<layout::Accordion*>(m_target)) {
        connect(accordion, &layout::Accordion::itemExpansionChanged, this, [this, refreshLayoutAndPublish](int, bool) {
            QTimer::singleShot(0, this, refreshLayoutAndPublish);
        });
        for (int i = 0; i < accordion->count(); ++i) {
            if (auto* item = accordion->itemAt(i)) {
                connect(item, &layout::Expander::layoutHeightChanged, this, [refreshLayoutAndPublish](int) {
                    refreshLayoutAndPublish();
                });
            }
        }
    }
    if (auto* bar = qobject_cast<layout::DataGridToolBar*>(m_target)) {
        connect(bar, &layout::DataGridToolBar::searchTextChanged, this, [publishInteractiveChange](const QString&) {
            publishInteractiveChange();
        });
        connect(bar, &layout::DataGridToolBar::filtersChanged, this, [refreshLayoutAndPublish] {
            refreshLayoutAndPublish();
        });
    }
    if (auto* view = qobject_cast<layout::ListDetailView*>(m_target)) {
        connect(view, &layout::ListDetailView::currentPaneChanged, this,
            [publishInteractiveChange](layout::ListDetailView::Pane) { publishInteractiveChange(); });
        connect(view, &layout::ListDetailView::compactChanged, this, [publishInteractiveChange](bool) {
            publishInteractiveChange();
        });
    }
    if (auto* panel = qobject_cast<layout::PropertyPanel*>(m_target)) {
        connect(panel, &layout::PropertyPanel::valueEdited, this,
            [publishInteractiveChange](const QString&, const QVariant&) { publishInteractiveChange(); });
        connect(panel, &layout::PropertyPanel::searchTextChanged, this, [publishInteractiveChange](const QString&) {
            publishInteractiveChange();
        });
    }
    if (auto* breadcrumb = qobject_cast<navigation::Breadcrumb*>(m_target)) {
        connect(breadcrumb, &navigation::Breadcrumb::itemsChanged, this, refreshLayoutAndPublish);
        connect(breadcrumb, &navigation::Breadcrumb::itemActivated, this,
            [publishInteractiveChange](int, const navigation::BreadcrumbItem&) { publishInteractiveChange(); });
    }
    if (auto* nav = qobject_cast<navigation::NavigationView*>(m_target)) {
        connect(nav, &navigation::NavigationView::displayModeChanged, this,
            [refreshLayoutAndPublish](navigation::NavigationView::DisplayMode) { refreshLayoutAndPublish(); });
        connect(nav, &navigation::NavigationView::effectiveDisplayModeChanged, this,
            [refreshLayoutAndPublish](navigation::NavigationView::DisplayMode) { refreshLayoutAndPublish(); });
        connect(nav, &navigation::NavigationView::paneOpenChanged, this, [publishInteractiveChange](bool) {
            publishInteractiveChange();
        });
        auto navSelectionConnection = std::make_shared<QMetaObject::Connection>();
        const auto connectNavSelection = [this, nav, publishInteractiveChange, navSelectionConnection] {
            QObject::disconnect(*navSelectionConnection);
            if (auto* view = qobject_cast<QAbstractItemView*>(nav->mainChromeWidget());
                view && view->selectionModel()) {
                *navSelectionConnection = connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
                    [publishInteractiveChange](const QModelIndex&, const QModelIndex&) { publishInteractiveChange(); });
            }
        };
        connectNavSelection();
        connect(nav, &navigation::NavigationView::mainChromeWidgetChanged, this,
            [connectNavSelection, refreshLayoutAndPublish](QWidget*) {
                connectNavSelection();
                refreshLayoutAndPublish();
            });
    }
    if (auto* pivot = qobject_cast<navigation::Pivot*>(m_target)) {
        connect(pivot, &navigation::Pivot::selectedIndexChanged, this, [publishInteractiveChange](int) {
            publishInteractiveChange();
        });
        connect(pivot, &navigation::Pivot::itemsChanged, this, refreshLayoutAndPublish);
    }
    if (auto* selector = qobject_cast<navigation::SelectorBar*>(m_target)) {
        connect(selector, &navigation::SelectorBar::selectedIndexChanged, this, [publishInteractiveChange](int) {
            publishInteractiveChange();
        });
        connect(selector, &navigation::SelectorBar::itemsChanged, this, refreshLayoutAndPublish);
    }
    if (auto* stepper = qobject_cast<navigation::Stepper*>(m_target)) {
        connect(stepper, &navigation::Stepper::currentIndexChanged, this, [refreshLayoutAndPublish](int) {
            refreshLayoutAndPublish();
        });
        connect(stepper, &navigation::Stepper::itemCountChanged, this, [refreshLayoutAndPublish](int) {
            refreshLayoutAndPublish();
        });
        connect(stepper, &navigation::Stepper::itemsChanged, this, refreshLayoutAndPublish);
    }
    if (auto* tabs = qobject_cast<navigation::TabView*>(m_target)) {
        connect(tabs, &navigation::TabView::selectedIndexChanged, this, [publishInteractiveChange](int) {
            publishInteractiveChange();
        });
        connect(tabs, &navigation::TabView::tabsChanged, this, refreshLayoutAndPublish);
        connect(tabs, &navigation::TabView::tabMoved, this, [publishInteractiveChange](int, int) {
            publishInteractiveChange();
        });
        connect(tabs, &navigation::TabView::tabsClosableChanged, this, [refreshLayoutAndPublish](bool) {
            refreshLayoutAndPublish();
        });
        connect(tabs, &navigation::TabView::addTabButtonVisibleChanged, this, [refreshLayoutAndPublish](bool) {
            refreshLayoutAndPublish();
        });
    }
}

void GalleryComponentPlayground::applyHoverState()
{
    if (m_interactive)
        return;
    if (!m_surface)
        return;
    auto* targetWidget = qobject_cast<QWidget*>(m_target.data());
    QList<QWidget*> targets;
    targets.append(m_surface.data());
    if (targetWidget && targetWidget != m_surface.data())
        targets.append(targetWidget);
    if (auto* view = qobject_cast<QAbstractScrollArea*>(targetWidget))
        targets.append(view->viewport());
    if (m_schema.value("hoverTarget") == QStringLiteral("scrollbar")) {
        // Exercise the bar's real enter/leave behavior in the read-only preview.
        // zh_CN: 只读预览通过滚动条自身的进入/离开事件展示其悬停状态。
        for (auto* bar : m_surface->findChildren<scrolling::ScrollBar*>())
            if (!bar->isHidden())
                targets.append(bar);
    }
    for (QWidget* w : targets) {
        if (!w || w->size().isEmpty())
            continue;
        w->setAttribute(Qt::WA_UnderMouse, m_previewHovered);
        const int px = qBound(1, qRound(w->width() * m_hoverX), qMax(1, w->width() - 2));
        const int py = qBound(1, qRound(w->height() * m_hoverY), qMax(1, w->height() - 2));
        QPoint pos(px, py);
        if (m_previewHovered) {
            if (auto* chart = qobject_cast<charts::ChartView*>(w)) {
                FLUENT_MAKE_MOUSE_EVENT(primeMove, QEvent::MouseMove, w, pos, Qt::NoButton, Qt::NoButton,
                    Qt::NoModifier);
                QCoreApplication::sendEvent(w, &primeMove);
                if (!m_customHoverPos && !chart->plotRect().isEmpty()) {
                    const QRectF pr = chart->plotRect();
                    pos = QPoint(qBound(1, qRound(pr.left() + pr.width() * 0.5), qMax(1, w->width() - 2)),
                        qBound(1, qRound(pr.top() + pr.height() * 0.56), qMax(1, w->height() - 2)));
                }
            }
            FLUENT_MAKE_ENTER_EVENT(enterEvent, pos.x(), pos.y());
            QCoreApplication::sendEvent(w, &enterEvent);
            FLUENT_MAKE_HOVER_EVENT(hoverMove, QEvent::HoverMove, w, QPointF(pos), QPointF(pos));
            QCoreApplication::sendEvent(w, &hoverMove);
            FLUENT_MAKE_MOUSE_EVENT(mouseMove, QEvent::MouseMove, w, pos, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(w, &mouseMove);
        } else {
            QEvent leaveEvent(QEvent::Leave);
            QCoreApplication::sendEvent(w, &leaveEvent);
            FLUENT_MAKE_HOVER_EVENT(hoverLeave, QEvent::HoverLeave, w, QPointF(-1, -1), QPointF(pos));
            QCoreApplication::sendEvent(w, &hoverLeave);
        }
        w->update();
    }
}

void GalleryComponentPlayground::recordEvent(const QString& name, bool checked)
{
    if (m_applying)
        return;
    m_events.append(QJsonObject{{"sequence", ++m_eventSequence}, {"name", name}, {"checked", checked}});
    while (m_events.size() > 8)
        m_events.removeAt(0);
    ++m_revision;
    emit stateChanged();
}
} // namespace fluent::gallery
