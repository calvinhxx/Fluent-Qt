#include "utils/private/Inspector_p.h"

#include <QAbstractScrollArea>
#include <QAccessible>
#include <QAccessibleInterface>
#include <QAction>
#include <QFontMetrics>
#include <QHash>
#include <QJsonArray>
#include <QLabel>
#include <QLayout>
#include <QMargins>
#include <QScrollBar>
#include <QSet>
#include <QTextDocument>
#include <QToolButton>
#include <QVariant>

#include <algorithm>

namespace fluent::diagnostics {
namespace {

QString severityName(InspectorSeverity severity)
{
    switch (severity) {
    case InspectorSeverity::Info:
        return QStringLiteral("info");
    case InspectorSeverity::Warning:
        return QStringLiteral("warning");
    case InspectorSeverity::Error:
        return QStringLiteral("error");
    }
    return QStringLiteral("warning");
}

QString categoryName(InspectorCategory category)
{
    switch (category) {
    case InspectorCategory::Text:
        return QStringLiteral("text");
    case InspectorCategory::Accessibility:
        return QStringLiteral("accessibility");
    case InspectorCategory::Input:
        return QStringLiteral("input");
    case InspectorCategory::Focus:
        return QStringLiteral("focus");
    case InspectorCategory::Layout:
        return QStringLiteral("layout");
    case InspectorCategory::Actions:
        return QStringLiteral("actions");
    case InspectorCategory::Scrolling:
        return QStringLiteral("scrolling");
    }
    return QStringLiteral("layout");
}

QString normalizedText(const QString& text)
{
    return text.simplified().toCaseFolded();
}

bool isVisibleWithin(QWidget* widget, QWidget* root)
{
    if (!widget || !root || (widget != root && !widget->isVisibleTo(root)))
        return false;
    if (widget == root)
        return true;
    for (QWidget* ancestor = widget->parentWidget(); ancestor;
         ancestor = ancestor->parentWidget()) {
        const QRect mappedRect(widget->mapTo(ancestor, QPoint(0, 0)), widget->size());
        if (!ancestor->rect().intersects(mappedRect))
            return false;
        if (ancestor == root)
            break;
    }
    return true;
}

bool isQtImplementationChild(QWidget* widget)
{
    return !widget || widget->objectName().startsWith(QStringLiteral("qt_"));
}

QString accessibleText(QWidget* widget, QAccessible::Text kind)
{
    if (!widget)
        return {};
    if (QAccessibleInterface* interface = QAccessible::queryAccessibleInterface(widget))
        return interface->text(kind);
    return {};
}

bool hasAccessibleName(QWidget* widget)
{
    return !accessibleText(widget, QAccessible::Name).trimmed().isEmpty();
}

bool hasFullValue(QWidget* widget, const QString& fullText)
{
    const QString expected = normalizedText(fullText);
    if (expected.isEmpty())
        return true;
    const QStringList values{
        widget->accessibleName(),
        widget->accessibleDescription(),
        widget->toolTip(),
        accessibleText(widget, QAccessible::Name),
        accessibleText(widget, QAccessible::Description),
        accessibleText(widget, QAccessible::Value),
        accessibleText(widget, QAccessible::Help),
    };
    return std::any_of(values.cbegin(), values.cend(), [&](const QString& value) {
        return normalizedText(value).contains(expected);
    });
}

QAccessible::Role accessibleRole(QWidget* widget)
{
    if (QAccessibleInterface* interface = QAccessible::queryAccessibleInterface(widget))
        return interface->role();
    return QAccessible::NoRole;
}

bool roleRequiresAccessibleName(QAccessible::Role role)
{
    switch (role) {
    case QAccessible::PushButton:
    case QAccessible::CheckBox:
    case QAccessible::RadioButton:
    case QAccessible::ComboBox:
    case QAccessible::EditableText:
    case QAccessible::Link:
    case QAccessible::MenuItem:
    case QAccessible::PageTab:
    case QAccessible::List:
    case QAccessible::Tree:
    case QAccessible::Table:
    case QAccessible::Slider:
    case QAccessible::SpinBox:
    case QAccessible::Dial:
    case QAccessible::HotkeyField:
    case QAccessible::ButtonDropDown:
    case QAccessible::ButtonMenu:
    case QAccessible::ButtonDropGrid:
        return true;
    default:
        return false;
    }
}

bool roleHasMinimumHitArea(QAccessible::Role role)
{
    switch (role) {
    case QAccessible::PushButton:
    case QAccessible::CheckBox:
    case QAccessible::RadioButton:
    case QAccessible::ComboBox:
    case QAccessible::EditableText:
    case QAccessible::Link:
    case QAccessible::MenuItem:
    case QAccessible::PageTab:
    case QAccessible::Slider:
    case QAccessible::SpinBox:
    case QAccessible::Dial:
    case QAccessible::HotkeyField:
    case QAccessible::ButtonDropDown:
    case QAccessible::ButtonMenu:
    case QAccessible::ButtonDropGrid:
        return true;
    default:
        return false;
    }
}

bool hasEffectiveAccessibleName(QWidget* widget)
{
    if (hasAccessibleName(widget))
        return true;
    for (QWidget* ancestor = widget ? widget->parentWidget() : nullptr; ancestor;
         ancestor = ancestor->parentWidget()) {
        if (ancestor->focusProxy() == widget)
            return hasAccessibleName(ancestor);
        if (qobject_cast<QAbstractScrollArea*>(ancestor))
            break;
    }
    return false;
}

bool hasScrollBoundaryContract(QAbstractScrollArea* area)
{
    for (QWidget* current = area; current; current = current->parentWidget()) {
        if (current->property("scrollChainingEnabled").isValid())
            return true;
        if (current != area && qobject_cast<QAbstractScrollArea*>(current))
            break;
    }
    return false;
}

bool isInternalScrollBar(QWidget* widget)
{
    auto* scrollBar = qobject_cast<QScrollBar*>(widget);
    if (!scrollBar)
        return false;
    for (QWidget* parent = scrollBar->parentWidget(); parent; parent = parent->parentWidget()) {
        if (qobject_cast<QAbstractScrollArea*>(parent))
            return true;
    }
    return false;
}

QString widgetSegment(QWidget* widget)
{
    if (!widget->objectName().isEmpty())
        return widget->objectName();
    const QString className = QString::fromLatin1(widget->metaObject()->className());
    int index = 0;
    if (QWidget* parent = widget->parentWidget()) {
        const auto siblings = parent->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget* sibling : siblings) {
            if (sibling == widget)
                break;
            if (!sibling->objectName().startsWith(QStringLiteral("qt_")) &&
                QString::fromLatin1(sibling->metaObject()->className()) == className)
                ++index;
        }
    }
    return QStringLiteral("%1[%2]").arg(className).arg(index);
}

QString widgetPath(QWidget* widget, QWidget* root)
{
    QStringList segments;
    for (QWidget* current = widget; current; current = current->parentWidget()) {
        if (!current->objectName().startsWith(QStringLiteral("qt_")))
            segments.prepend(widgetSegment(current));
        if (current == root)
            break;
    }
    return segments.join(QLatin1Char('/'));
}

QRect relativeRect(QWidget* widget, QWidget* root)
{
    return QRect(widget->mapTo(root, QPoint(0, 0)), widget->size());
}

InspectorFinding finding(QWidget* widget, QWidget* root, const QString& code,
                         InspectorCategory category, const QString& message,
                         const QJsonObject& details = {})
{
    InspectorFinding result;
    result.code = code;
    result.category = category;
    result.path = widgetPath(widget, root);
    result.rect = relativeRect(widget, root);
    result.message = message;
    result.details = details;
    result.widget = widget;
    return result;
}

QVector<QWidget*> widgetsUnder(QWidget* root)
{
    QVector<QWidget*> widgets;
    if (!root)
        return widgets;
    widgets.append(root);
    const auto descendants = root->findChildren<QWidget*>();
    widgets.reserve(descendants.size() + 1);
    for (QWidget* widget : descendants)
        widgets.append(widget);
    return widgets;
}

QStringList activeScrollAxes(QAbstractScrollArea* area)
{
    QStringList axes;
    if (area->horizontalScrollBar() &&
        area->horizontalScrollBar()->maximum() > area->horizontalScrollBar()->minimum()) {
        axes.append(QStringLiteral("horizontal"));
    }
    if (area->verticalScrollBar() &&
        area->verticalScrollBar()->maximum() > area->verticalScrollBar()->minimum()) {
        axes.append(QStringLiteral("vertical"));
    }
    return axes;
}

bool isOffGrid(int value, int grid)
{
    return value >= 0 && grid > 0 && value % grid != 0;
}

} // namespace

QJsonObject InspectorFinding::toJson() const
{
    return {
        {QStringLiteral("code"), code},
        {QStringLiteral("category"), categoryName(category)},
        {QStringLiteral("severity"), severityName(severity)},
        {QStringLiteral("path"), path},
        {QStringLiteral("rect"), QJsonObject{{QStringLiteral("x"), rect.x()},
                                             {QStringLiteral("y"), rect.y()},
                                             {QStringLiteral("width"), rect.width()},
                                             {QStringLiteral("height"), rect.height()}}},
        {QStringLiteral("message"), message},
        {QStringLiteral("details"), details},
    };
}

QVector<InspectorFinding> inspectFindings(QWidget* root, const InspectorOptions& options)
{
    QVector<InspectorFinding> findings;
    if (!root)
        return findings;

    const QVector<QWidget*> widgets = widgetsUnder(root);
    QSet<QWidget*> focusChain;
    QWidget* focusCursor = root;
    for (int step = 0; focusCursor && step <= widgets.size(); ++step) {
        focusChain.insert(focusCursor);
        focusCursor = focusCursor->nextInFocusChain();
        if (focusCursor == root)
            break;
    }

    QHash<QString, QVector<QWidget*>> semanticActions;
    QHash<QAction*, QVector<QWidget*>> nativeActions;

    for (QWidget* widget : widgets) {
        if (!isVisibleWithin(widget, root) || isQtImplementationChild(widget))
            continue;

        if (options.checkClippedText) {
            if (auto* label = qobject_cast<QLabel*>(widget)) {
                const bool plainText =
                    label->textFormat() == Qt::PlainText ||
                    (label->textFormat() == Qt::AutoText && !Qt::mightBeRichText(label->text()));
                const QRect content = label->contentsRect();
                const QFontMetrics metrics(label->font());
                const bool clipped = plainText && !label->wordWrap() &&
                                     (metrics.horizontalAdvance(label->text()) > content.width() ||
                                      metrics.height() > content.height());
                if (clipped && !hasFullValue(label, label->text())) {
                    findings.append(finding(label, root,
                                            QStringLiteral("text.clipped-without-full-value"),
                                            InspectorCategory::Text,
                                            QStringLiteral("Single-line text is clipped without an "
                                                           "accessible full value."),
                                            {{QStringLiteral("text"), label->text()},
                                             {QStringLiteral("available_width"), content.width()},
                                             {QStringLiteral("required_width"),
                                              metrics.horizontalAdvance(label->text())}}));
                }
            }
        }

        const QAccessible::Role role = accessibleRole(widget);
        if (!isInternalScrollBar(widget) && widget->isEnabled()) {
            if (options.checkAccessibilityNames && roleRequiresAccessibleName(role) &&
                !hasEffectiveAccessibleName(widget)) {
                findings.append(
                    finding(widget, root, QStringLiteral("accessibility.missing-name"),
                            InspectorCategory::Accessibility,
                            QStringLiteral("Interactive widget has no accessible name.")));
            }
            if (options.checkHitAreas && roleHasMinimumHitArea(role) &&
                (widget->width() < options.minimumHitArea.width() ||
                 widget->height() < options.minimumHitArea.height())) {
                findings.append(finding(
                    widget, root, QStringLiteral("input.small-hit-area"), InspectorCategory::Input,
                    QStringLiteral("Interactive widget is smaller than the configured hit area."),
                    {{QStringLiteral("minimum_width"), options.minimumHitArea.width()},
                     {QStringLiteral("minimum_height"), options.minimumHitArea.height()}}));
            }
        }

        if (options.checkFocusOrder && !isInternalScrollBar(widget) && widget->isEnabled() &&
            widget->focusPolicy() != Qt::NoFocus && !focusChain.contains(widget)) {
            findings.append(
                finding(widget, root, QStringLiteral("focus.unreachable"), InspectorCategory::Focus,
                        QStringLiteral("Focusable widget is absent from the root focus chain.")));
        }

        if (options.checkDuplicateActions) {
            const QString semanticAction =
                widget->property("fluentSemanticAction").toString().trimmed();
            if (!semanticAction.isEmpty())
                semanticActions[semanticAction].append(widget);
            if (auto* toolButton = qobject_cast<QToolButton*>(widget)) {
                if (toolButton->defaultAction())
                    nativeActions[toolButton->defaultAction()].append(widget);
            }
        }

        if (options.checkNestedScrolling) {
            if (auto* area = qobject_cast<QAbstractScrollArea*>(widget)) {
                if (hasScrollBoundaryContract(area))
                    continue;
                const QStringList axes = activeScrollAxes(area);
                for (QWidget* parent = area->parentWidget(); parent;
                     parent = parent->parentWidget()) {
                    auto* ancestor = qobject_cast<QAbstractScrollArea*>(parent);
                    if (!ancestor)
                        continue;
                    QStringList sharedAxes;
                    const QStringList ancestorAxes = activeScrollAxes(ancestor);
                    for (const QString& axis : axes) {
                        if (ancestorAxes.contains(axis))
                            sharedAxes.append(axis);
                    }
                    if (!sharedAxes.isEmpty()) {
                        findings.append(finding(
                            area, root, QStringLiteral("scroll.nested-boundary"),
                            InspectorCategory::Scrolling,
                            QStringLiteral("Nested scroll areas can both scroll on the same axis."),
                            {{QStringLiteral("axes"), QJsonArray::fromStringList(sharedAxes)},
                             {QStringLiteral("ancestor"), widgetPath(ancestor, root)}}));
                        break;
                    }
                }
            }
        }

        if (options.checkLayoutGrid && options.spacingGrid > 0 && widget->layout()) {
            QLayout* layout = widget->layout();
            const QMargins margins = layout->contentsMargins();
            const int spacing = layout->spacing();
            const bool offGrid = isOffGrid(margins.left(), options.spacingGrid) ||
                                 isOffGrid(margins.top(), options.spacingGrid) ||
                                 isOffGrid(margins.right(), options.spacingGrid) ||
                                 isOffGrid(margins.bottom(), options.spacingGrid) ||
                                 isOffGrid(spacing, options.spacingGrid);
            if (offGrid) {
                findings.append(finding(widget, root, QStringLiteral("layout.off-grid"),
                                        InspectorCategory::Layout,
                                        QStringLiteral("Explicit layout margins or spacing do not "
                                                       "follow the configured grid."),
                                        {{QStringLiteral("grid"), options.spacingGrid},
                                         {QStringLiteral("left"), margins.left()},
                                         {QStringLiteral("top"), margins.top()},
                                         {QStringLiteral("right"), margins.right()},
                                         {QStringLiteral("bottom"), margins.bottom()},
                                         {QStringLiteral("spacing"), spacing}}));
            }
        }
    }

    const auto appendDuplicate = [&](const QString& action,
                                     const QVector<QWidget*>& actionWidgets) {
        if (actionWidgets.size() < 2)
            return;
        QJsonArray paths;
        for (QWidget* widget : actionWidgets)
            paths.append(widgetPath(widget, root));
        findings.append(
            finding(actionWidgets.first(), root, QStringLiteral("action.duplicate-entry"),
                    InspectorCategory::Actions,
                    QStringLiteral("Multiple visible entries resolve to the same semantic action."),
                    {{QStringLiteral("action"), action},
                     {QStringLiteral("entry_count"), actionWidgets.size()},
                     {QStringLiteral("entries"), paths}}));
    };

    QStringList semanticKeys = semanticActions.keys();
    std::sort(semanticKeys.begin(), semanticKeys.end());
    for (const QString& action : semanticKeys)
        appendDuplicate(action, semanticActions.value(action));

    for (auto it = nativeActions.cbegin(); it != nativeActions.cend(); ++it) {
        QAction* action = it.key();
        const bool hasExplicitSemanticAction =
            std::any_of(it.value().cbegin(), it.value().cend(), [](QWidget* widget) {
                return !widget->property("fluentSemanticAction").toString().trimmed().isEmpty();
            });
        if (hasExplicitSemanticAction)
            continue;
        const QString actionName = !action->objectName().isEmpty()
                                       ? action->objectName()
                                       : QString(action->text()).remove(QLatin1Char('&'));
        appendDuplicate(actionName, it.value());
    }

    std::sort(findings.begin(), findings.end(),
              [](const InspectorFinding& left, const InspectorFinding& right) {
                  if (left.path != right.path)
                      return left.path < right.path;
                  return left.code < right.code;
              });
    return findings;
}

QJsonObject Inspector::report(QWidget* root, const InspectorOptions& options)
{
    const QVector<InspectorFinding> findings = inspectFindings(root, options);
    QJsonArray findingArray;
    QJsonObject severityCounts{
        {QStringLiteral("info"), 0}, {QStringLiteral("warning"), 0}, {QStringLiteral("error"), 0}};
    QJsonObject categoryCounts;
    for (const InspectorFinding& item : findings) {
        findingArray.append(item.toJson());
        const QString severity = severityName(item.severity);
        severityCounts[severity] = severityCounts.value(severity).toInt() + 1;
        const QString category = categoryName(item.category);
        categoryCounts[category] = categoryCounts.value(category).toInt() + 1;
    }

    QJsonObject rootObject;
    if (root) {
        rootObject = {
            {QStringLiteral("class"), QString::fromLatin1(root->metaObject()->className())},
            {QStringLiteral("object_name"), root->objectName()},
            {QStringLiteral("width"), root->width()},
            {QStringLiteral("height"), root->height()}};
    }
    return {
        {QStringLiteral("schema_version"), ReportSchemaVersion},
        {QStringLiteral("tool"), QStringLiteral("FluentQt Inspector")},
        {QStringLiteral("root"), rootObject},
        {QStringLiteral("summary"), QJsonObject{{QStringLiteral("findings"), findings.size()},
                                                {QStringLiteral("by_severity"), severityCounts},
                                                {QStringLiteral("by_category"), categoryCounts}}},
        {QStringLiteral("findings"), findingArray}};
}

} // namespace fluent::diagnostics
