#include "AutoSuggestBoxAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QFontMetrics>
#include <QListView>

#include <limits>

#include "components/textfields/AutoSuggestBox.h"

namespace fluent::textfields::detail {

#if QT_CONFIG(accessibility)

class AutoSuggestBoxAccessible final
    : public QAccessibleWidget,
      public QAccessibleTextInterface,
      public QAccessibleEditableTextInterface {
public:
    explicit AutoSuggestBoxAccessible(AutoSuggestBox* box)
        : QAccessibleWidget(box, QAccessible::EditableText)
    {
    }

    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::TextInterface)
            return static_cast<QAccessibleTextInterface*>(this);
        if (type == QAccessible::EditableTextInterface)
            return static_cast<QAccessibleEditableTextInterface*>(this);
        if (type == QAccessible::ActionInterface)
            return static_cast<QAccessibleActionInterface*>(this);
        return QAccessibleWidget::interface_cast(type);
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        AutoSuggestBox* box = view();
        if (!box)
            return result;
        const bool hasSuggestions = !box->suggestions().isEmpty();
        result.focusable = true;
        result.selectableText = true;
        result.readOnly = box->isReadOnly();
        result.editable = box->isEnabled() && !box->isReadOnly();
        result.supportsAutoCompletion = hasSuggestions;
        result.hasPopup = hasSuggestions;
        result.expandable = hasSuggestions;
        result.expanded = hasSuggestions && box->isSuggestionListOpen();
        result.collapsed = hasSuggestions && !box->isSuggestionListOpen();
        return result;
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = QAccessibleWidget::text(type);
        if (type == QAccessible::Name && inherited.isEmpty() && view())
            return view()->header();
        if (type == QAccessible::Value && view())
            return view()->text();
        return inherited;
    }

    void setText(QAccessible::Text type, const QString& value) override
    {
        if (type == QAccessible::Value && canEditText()) {
            view()->setText(value);
            return;
        }
        QAccessibleWidget::setText(type, value);
    }

    QList<std::pair<QAccessibleInterface*, QAccessible::Relation>>
    relations(QAccessible::Relation match) const override
    {
        auto result = QAccessibleWidget::relations(match);
        if (!(match & QAccessible::Controller) || !view())
            return result;
        QWidget* list = view()->findChild<QWidget*>(
            QStringLiteral("AutoSuggestBoxSuggestionList"));
        QAccessibleInterface* target = list
            ? QAccessible::queryAccessibleInterface(list) : nullptr;
        if (target)
            result.append({target, QAccessible::Controller});
        return result;
    }

    QStringList actionNames() const override
    {
        QStringList result = QAccessibleWidget::actionNames();
        if (view() && view()->isEnabled()
            && !view()->suggestions().isEmpty()) {
            const QString action =
                QAccessibleActionInterface::showMenuAction();
            if (!result.contains(action))
                result.append(action);
        }
        return result;
    }

    void doAction(const QString& actionName) override
    {
        if (view() && view()->isEnabled()
            && !view()->suggestions().isEmpty()
            && actionName == QAccessibleActionInterface::showMenuAction()) {
            view()->openSuggestionList();
            return;
        }
        QAccessibleWidget::doAction(actionName);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::showMenuAction())
            return {QStringLiteral("Alt+Down"), QStringLiteral("F4")};
        return QAccessibleWidget::keyBindingsForAction(actionName);
    }

    void selection(int selectionIndex, int* startOffset,
                   int* endOffset) const override
    {
        if (startOffset)
            *startOffset = -1;
        if (endOffset)
            *endOffset = -1;
        if (!view() || selectionIndex != 0 || !view()->hasSelectedText())
            return;
        const int start = view()->selectionStart();
        if (startOffset)
            *startOffset = start;
        if (endOffset)
            *endOffset = start + view()->selectedText().size();
    }

    int selectionCount() const override
    {
        return view() && view()->hasSelectedText() ? 1 : 0;
    }

    void addSelection(int startOffset, int endOffset) override
    {
        applySelection(startOffset, endOffset);
    }

    void removeSelection(int selectionIndex) override
    {
        if (view() && selectionIndex == 0)
            view()->deselect();
    }

    void setSelection(int selectionIndex, int startOffset,
                      int endOffset) override
    {
        if (selectionIndex == 0)
            applySelection(startOffset, endOffset);
    }

    int cursorPosition() const override
    {
        return view() ? view()->cursorPosition() : 0;
    }

    void setCursorPosition(int position) override
    {
        if (view())
            view()->setCursorPosition(qBound(0, position, characterCount()));
    }

    QString text(int startOffset, int endOffset) const override
    {
        if (!view())
            return {};
        const QString content = view()->text();
        const int start = qBound(0, startOffset, content.size());
        const int end = endOffset < 0
            ? content.size() : qBound(start, endOffset, content.size());
        return content.mid(start, end - start);
    }

    int characterCount() const override
    {
        return view() ? view()->text().size() : 0;
    }

    QRect characterRect(int offset) const override
    {
        AutoSuggestBox* box = view();
        if (!box || offset < 0 || offset > characterCount())
            return {};
        const QFontMetrics metrics(box->font());
        const int y = qMax(0, box->height() - 16);
        int first = -1;
        int last = -1;
        int nearestX = 0;
        int nearestDistance = std::numeric_limits<int>::max();
        for (int x = 0; x < box->width(); ++x) {
            const int candidate = box->cursorPositionAt(QPoint(x, y));
            const int distance = qAbs(candidate - offset);
            if (distance < nearestDistance) {
                nearestDistance = distance;
                nearestX = x;
            }
            if (candidate == offset) {
                if (first < 0)
                    first = x;
                last = x;
            }
        }
        if (first < 0)
            first = last = nearestX;
        const int textHeight = qMax(1, metrics.height());
        const QRect local(first, qMax(0, y - textHeight / 2),
                          qMax(1, last - first + 1), textHeight);
        return QRect(box->mapToGlobal(local.topLeft()), local.size());
    }

    int offsetAtPoint(const QPoint& point) const override
    {
        return view()
            ? view()->cursorPositionAt(view()->mapFromGlobal(point)) : -1;
    }

    void scrollToSubstring(int, int endIndex) override
    {
        setCursorPosition(endIndex);
    }

    QString attributes(int, int* startOffset,
                       int* endOffset) const override
    {
        if (startOffset)
            *startOffset = 0;
        if (endOffset)
            *endOffset = characterCount();
        return {};
    }

    void deleteText(int startOffset, int endOffset) override
    {
        replaceText(startOffset, endOffset, {});
    }

    void insertText(int offset, const QString& value) override
    {
        replaceText(offset, offset, value);
    }

    void replaceText(int startOffset, int endOffset,
                     const QString& replacement) override
    {
        if (!canEditText())
            return;
        QString content = view()->text();
        const int start = qBound(0, startOffset, content.size());
        const int end = qBound(start, endOffset, content.size());
        content.replace(start, end - start, replacement);
        view()->setText(content);
        view()->setCursorPosition(start + replacement.size());
    }

private:
    AutoSuggestBox* view() const
    {
        return static_cast<AutoSuggestBox*>(widget());
    }

    bool canEditText() const
    {
        return view() && view()->isEnabled() && !view()->isReadOnly();
    }

    void applySelection(int startOffset, int endOffset)
    {
        if (!view())
            return;
        const int start = qBound(0, startOffset, characterCount());
        const int end = qBound(start, endOffset, characterCount());
        view()->setSelection(start, end - start);
    }
};

namespace {

QAccessibleInterface* autoSuggestBoxAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* box = dynamic_cast<AutoSuggestBox*>(object);
    return box ? new AutoSuggestBoxAccessible(box) : nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureAutoSuggestBoxAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(autoSuggestBoxAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyAutoSuggestSuggestionsChanged(AutoSuggestBox* box)
{
#if QT_CONFIG(accessibility)
    if (!box)
        return;
    QAccessible::State changed;
    changed.supportsAutoCompletion = true;
    changed.hasPopup = true;
    changed.expandable = true;
    QAccessibleStateChangeEvent stateEvent(box, changed);
    QAccessible::updateAccessibility(&stateEvent);
    QAccessibleEvent actionEvent(box, QAccessible::ActionChanged);
    QAccessible::updateAccessibility(&actionEvent);
    if (QWidget* list = box->findChild<QWidget*>(
            QStringLiteral("AutoSuggestBoxSuggestionList"))) {
        QAccessibleEvent structureEvent(list, QAccessible::ObjectReorder);
        QAccessible::updateAccessibility(&structureEvent);
    }
#else
    Q_UNUSED(box)
#endif
}

void notifyAutoSuggestPopupChanged(AutoSuggestBox* box)
{
#if QT_CONFIG(accessibility)
    if (!box)
        return;
    QAccessible::State changed;
    changed.expanded = true;
    changed.collapsed = true;
    QAccessibleStateChangeEvent event(box, changed);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(box)
#endif
}

void notifyAutoSuggestActiveDescendantChanged(AutoSuggestBox* box)
{
#if QT_CONFIG(accessibility)
    if (!box)
        return;
    QAccessibleEvent event(box, QAccessible::ActiveDescendantChanged);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(box)
#endif
}

void notifyAutoSuggestNameChanged(AutoSuggestBox* box)
{
#if QT_CONFIG(accessibility)
    if (!box || !box->accessibleName().isEmpty())
        return;
    QAccessibleEvent event(box, QAccessible::NameChanged);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(box)
#endif
}

} // namespace fluent::textfields::detail
