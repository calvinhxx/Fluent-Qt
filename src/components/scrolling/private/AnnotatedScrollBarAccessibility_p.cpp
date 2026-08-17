#include "AnnotatedScrollBarAccessibility_p.h"

#include <QAccessible>
#include <QCoreApplication>

#include "components/foundation/private/LogicalItemAccessibility_p.h"
#include "components/scrolling/ScrollView.h"
#include "components/scrolling/AnnotatedScrollBar.h"

namespace fluent::scrolling::detail {

#if QT_CONFIG(accessibility)

namespace {

using accessibility::detail::LogicalItemAccessibleAdapter;
using accessibility::detail::LogicalItemAccessibleState;

QString annotatedText(const char* source)
{
    return QCoreApplication::translate(
        "AnnotatedScrollBarAccessibility", source);
}

} // namespace

class AnnotatedScrollBarAccessible final
    : public LogicalItemAccessibleAdapter,
      public QAccessibleValueInterface {
public:
    explicit AnnotatedScrollBarAccessible(AnnotatedScrollBar* bar)
        : LogicalItemAccessibleAdapter(bar, QAccessible::ScrollBar)
    {
    }

    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ValueInterface)
            return static_cast<QAccessibleValueInterface*>(this);
        if (type == QAccessible::ActionInterface)
            return static_cast<QAccessibleActionInterface*>(this);
        return LogicalItemAccessibleAdapter::interface_cast(type);
    }

    QString text(QAccessible::Text type) const override
    {
        const QString inherited = LogicalItemAccessibleAdapter::text(type);
        if (!inherited.isEmpty())
            return inherited;
        if (type == QAccessible::Name)
            return annotatedText("Annotated scroll bar");
        if (type == QAccessible::Value && view())
            return QString::number(view()->value());
        return inherited;
    }

    QAccessible::State state() const override
    {
        QAccessible::State result =
            LogicalItemAccessibleAdapter::state();
        result.readOnly = false;
        result.editable = view() && view()->isEnabled();
        return result;
    }

    QVariant currentValue() const override
    {
        return view() ? QVariant(view()->value()) : QVariant();
    }

    void setCurrentValue(const QVariant& value) override
    {
        if (!view() || !view()->isEnabled())
            return;
        bool ok = false;
        const int requested = value.toInt(&ok);
        if (ok)
            view()->setValue(requested);
    }

    QVariant maximumValue() const override
    {
        return view() ? QVariant(view()->maximum()) : QVariant();
    }

    QVariant minimumValue() const override
    {
        return view() ? QVariant(view()->minimum()) : QVariant();
    }

    QVariant minimumStepSize() const override
    {
        return view() ? QVariant(view()->lineStep()) : QVariant();
    }

    QStringList actionNames() const override
    {
        QStringList result = LogicalItemAccessibleAdapter::actionNames();
        if (!view() || !view()->isEnabled())
            return result;
        const QString increase = QAccessibleActionInterface::increaseAction();
        const QString decrease = QAccessibleActionInterface::decreaseAction();
        if (view()->value() < view()->maximum()
            && !result.contains(increase)) {
            result.append(increase);
        }
        if (view()->value() > view()->minimum()
            && !result.contains(decrease)) {
            result.append(decrease);
        }
        return result;
    }

    void doAction(const QString& actionName) override
    {
        if (view() && view()->isEnabled()
            && actionName == QAccessibleActionInterface::increaseAction()) {
            view()->requestScrollTo(view()->value() + view()->lineStep());
            return;
        }
        if (view() && view()->isEnabled()
            && actionName == QAccessibleActionInterface::decreaseAction()) {
            view()->requestScrollTo(view()->value() - view()->lineStep());
            return;
        }
        LogicalItemAccessibleAdapter::doAction(actionName);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::increaseAction())
            return {QStringLiteral("Down")};
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return {QStringLiteral("Up")};
        return LogicalItemAccessibleAdapter::keyBindingsForAction(actionName);
    }

    int logicalChildCount() const override
    {
        return view() ? view()->m_labels.size() : 0;
    }

    QAccessible::Role logicalChildRole(int) const override
    {
        return QAccessible::Link;
    }

    QString logicalChildText(
        int logicalIndex, QAccessible::Text type) const override
    {
        if (!validIndex(logicalIndex))
            return {};
        const auto& label = view()->m_labels.at(logicalIndex);
        if (type == QAccessible::Name)
            return label.text;
        if (type == QAccessible::Description)
            return label.detailText;
        if (type == QAccessible::Value)
            return QString::number(label.offset);
        return {};
    }

    QRect logicalChildRect(int logicalIndex) const override
    {
        if (!validIndex(logicalIndex))
            return {};
        view()->ensureLabelLayout();
        for (const auto& visible : view()->m_visibleLabels) {
            if (visible.originalIndex == logicalIndex)
                return toGlobalRect(visible.rect);
        }
        return {};
    }

    LogicalItemAccessibleState logicalChildState(
        int logicalIndex) const override
    {
        LogicalItemAccessibleState result;
        result.valid = validIndex(logicalIndex);
        result.enabled = result.valid && view()->isEnabled();
        result.focusable = result.valid;
        result.selectable = false;
        result.linked = result.valid;
        result.offscreen = result.valid
            && logicalChildRect(logicalIndex).isEmpty();
        return result;
    }

    QStringList logicalChildActions(int logicalIndex) const override
    {
        return validIndex(logicalIndex) && view()->isEnabled()
            ? QStringList{QAccessibleActionInterface::pressAction()}
            : QStringList();
    }

    void performLogicalChildAction(
        int logicalIndex, const QString& actionName) override
    {
        if (!validIndex(logicalIndex) || !view()->isEnabled()
            || actionName != QAccessibleActionInterface::pressAction()) {
            return;
        }
        const auto label = view()->m_labels.at(logicalIndex);
        QPointer<AnnotatedScrollBar> guard(view());
        emit guard->labelActivated(label.offset, label.text);
        if (guard)
            guard->requestScrollTo(label.offset);
    }

    bool logicalSelectionSupported() const override { return false; }

private:
    AnnotatedScrollBar* view() const
    {
        return static_cast<AnnotatedScrollBar*>(widget());
    }

    bool validIndex(int index) const
    {
        return view() && index >= 0 && index < view()->m_labels.size();
    }
};

namespace {

QAccessibleInterface* annotatedScrollBarAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* bar = dynamic_cast<AnnotatedScrollBar*>(object))
        return new AnnotatedScrollBarAccessible(bar);
    return nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureAnnotatedScrollBarAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(
            annotatedScrollBarAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyAnnotatedScrollBarValueChanged(AnnotatedScrollBar* bar)
{
#if QT_CONFIG(accessibility)
    if (!bar)
        return;
    QAccessibleValueChangeEvent event(bar, bar->value());
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(bar)
#endif
}

void notifyAnnotatedScrollBarRangeChanged(AnnotatedScrollBar* bar)
{
#if QT_CONFIG(accessibility)
    if (!bar)
        return;
    QAccessibleEvent event(bar, QAccessible::ActionChanged);
    QAccessible::updateAccessibility(&event);
#else
    Q_UNUSED(bar)
#endif
}

void notifyAnnotatedScrollBarStructureChanged(AnnotatedScrollBar* bar)
{
#if QT_CONFIG(accessibility)
    accessibility::detail::notifyLogicalItemAccessibilityStructure(bar);
#else
    Q_UNUSED(bar)
#endif
}

} // namespace fluent::scrolling::detail
