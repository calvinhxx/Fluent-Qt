#include "FlipViewAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QCoreApplication>
#include <QVariant>

#include "components/collections/FlipView.h"

namespace fluent::collections::detail {

#if QT_CONFIG(accessibility)

namespace {

QString flipViewText(const char* source)
{
    return QCoreApplication::translate("FlipViewAccessibility", source);
}

bool canPrevious(int index)
{
    return index > 0;
}

bool canNext(int index, int count)
{
    return index >= 0 && index < count - 1;
}

QVariant accessibleCurrentValue(const FlipView* view)
{
    return view && view->currentIndex() >= 0
        ? QVariant(view->currentIndex() + 1)
        : QVariant();
}

void sendEvent(QObject* object, QAccessible::Event type)
{
    if (!object)
        return;
    QAccessibleEvent event(object, type);
    QAccessible::updateAccessibility(&event);
}

void sendValueChanged(FlipView* view)
{
    if (!view)
        return;
    QAccessibleValueChangeEvent event(view, accessibleCurrentValue(view));
    QAccessible::updateAccessibility(&event);
}

class FlipViewAccessible final : public QAccessibleWidget,
                                 public QAccessibleValueInterface {
public:
    explicit FlipViewAccessible(FlipView* view)
        : QAccessibleWidget(view, QAccessible::LayeredPane)
    {
    }

    QAccessibleInterface* childAt(int x, int y) const override
    {
        FlipView* owner = view();
        if (!owner)
            return nullptr;
        for (int index = owner->pageCount() - 1; index >= 0; --index) {
            QAccessibleInterface* page = child(index);
            if (page && !page->state().invisible
                && page->rect().contains(x, y)) {
                return page;
            }
        }
        return nullptr;
    }

    QAccessibleInterface* focusChild() const override
    {
        FlipView* owner = view();
        QWidget* focused = QApplication::focusWidget();
        if (!owner || !focused)
            return nullptr;
        for (int index = 0; index < owner->pageCount(); ++index) {
            QWidget* page = owner->pageAt(index);
            if (page && (page == focused || page->isAncestorOf(focused)))
                return QAccessible::queryAccessibleInterface(page);
        }
        return nullptr;
    }

    int childCount() const override
    {
        return view() ? view()->pageCount() : 0;
    }

    int indexOfChild(const QAccessibleInterface* childInterface) const override
    {
        FlipView* owner = view();
        if (!owner || !childInterface)
            return -1;
        QObject* object = childInterface->object();
        for (int index = 0; index < owner->pageCount(); ++index) {
            if (owner->pageAt(index) == object)
                return index;
        }
        return -1;
    }

    QAccessibleInterface* child(int index) const override
    {
        FlipView* owner = view();
        QWidget* page = owner ? owner->pageAt(index) : nullptr;
        return page ? QAccessible::queryAccessibleInterface(page) : nullptr;
    }

    QString text(QAccessible::Text type) const override
    {
        const QString authored = QAccessibleWidget::text(type);
        FlipView* owner = view();
        if (!authored.isEmpty() || type != QAccessible::Value
            || !owner || owner->currentIndex() < 0) {
            return authored;
        }

        QString pageName;
        if (QWidget* page = owner->pageAt(owner->currentIndex())) {
            if (QAccessibleInterface* pageInterface =
                    QAccessible::queryAccessibleInterface(page)) {
                pageName = pageInterface->text(QAccessible::Name);
            }
        }
        return pageName.isEmpty()
            ? flipViewText("Page %1 of %2")
                  .arg(owner->currentIndex() + 1)
                  .arg(owner->pageCount())
            : flipViewText("Page %1 of %2: %3")
                  .arg(owner->currentIndex() + 1)
                  .arg(owner->pageCount())
                  .arg(pageName);
    }

    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ValueInterface)
            return static_cast<QAccessibleValueInterface*>(this);
        return QAccessibleWidget::interface_cast(type);
    }

    QStringList actionNames() const override
    {
        FlipView* owner = view();
        QStringList result;
        if (!owner || !owner->isEnabled())
            return result;
        if (canPrevious(owner->currentIndex()))
            result.append(QAccessibleActionInterface::decreaseAction());
        if (canNext(owner->currentIndex(), owner->pageCount()))
            result.append(QAccessibleActionInterface::increaseAction());
        return result;
    }

    void doAction(const QString& actionName) override
    {
        FlipView* owner = view();
        if (!owner || !owner->isEnabled())
            return;
        if (actionName == QAccessibleActionInterface::decreaseAction())
            owner->goPrevious();
        else if (actionName == QAccessibleActionInterface::increaseAction())
            owner->goNext();
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        FlipView* owner = view();
        if (!owner)
            return {};
        if (actionName == QAccessibleActionInterface::decreaseAction()) {
            return {owner->orientation() == Qt::Horizontal
                        ? QStringLiteral("Left")
                        : QStringLiteral("Up")};
        }
        if (actionName == QAccessibleActionInterface::increaseAction()) {
            return {owner->orientation() == Qt::Horizontal
                        ? QStringLiteral("Right")
                        : QStringLiteral("Down")};
        }
        return {};
    }

    QString localizedActionName(const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return flipViewText("Previous page");
        if (actionName == QAccessibleActionInterface::increaseAction())
            return flipViewText("Next page");
        return QAccessibleWidget::localizedActionName(actionName);
    }

    QString localizedActionDescription(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return flipViewText("Shows the previous page");
        if (actionName == QAccessibleActionInterface::increaseAction())
            return flipViewText("Shows the next page");
        return QAccessibleWidget::localizedActionDescription(actionName);
    }

    QVariant currentValue() const override
    {
        return accessibleCurrentValue(view());
    }

    void setCurrentValue(const QVariant& value) override
    {
        FlipView* owner = view();
        if (owner && owner->isEnabled() && owner->pageCount() > 0)
            owner->setCurrentIndex(value.toInt() - 1);
    }

    QVariant maximumValue() const override
    {
        return view() && view()->pageCount() > 0
            ? QVariant(view()->pageCount()) : QVariant();
    }

    QVariant minimumValue() const override
    {
        return view() && view()->pageCount() > 0
            ? QVariant(1) : QVariant();
    }

    QVariant minimumStepSize() const override
    {
        return view() && view()->pageCount() > 0
            ? QVariant(1) : QVariant();
    }

private:
    FlipView* view() const
    {
        return static_cast<FlipView*>(widget());
    }
};

QAccessibleInterface* flipViewAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* view = dynamic_cast<FlipView*>(object);
    return view ? new FlipViewAccessible(view) : nullptr;
}

} // namespace

#endif // QT_CONFIG(accessibility)

void ensureFlipViewAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(flipViewAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifyFlipViewAccessibilityStructureChanged(
    FlipView* view, int oldCount, int oldIndex)
{
#if QT_CONFIG(accessibility)
    if (!view)
        return;
    sendEvent(view, QAccessible::ObjectReorder);
    if (oldCount != view->pageCount()
        || oldIndex != view->currentIndex()) {
        sendValueChanged(view);
    }
    if (view->isEnabled()
        && (canPrevious(oldIndex) != canPrevious(view->currentIndex())
            || canNext(oldIndex, oldCount)
                != canNext(view->currentIndex(), view->pageCount()))) {
        sendEvent(view, QAccessible::ActionChanged);
    }
#else
    Q_UNUSED(view)
    Q_UNUSED(oldCount)
    Q_UNUSED(oldIndex)
#endif
}

void notifyFlipViewAccessibilityCurrentChanged(
    FlipView* view, int oldIndex)
{
#if QT_CONFIG(accessibility)
    if (!view || oldIndex == view->currentIndex())
        return;
    sendValueChanged(view);
    if (view->isEnabled()
        && (canPrevious(oldIndex) != canPrevious(view->currentIndex())
            || canNext(oldIndex, view->pageCount())
                != canNext(view->currentIndex(), view->pageCount()))) {
        sendEvent(view, QAccessible::ActionChanged);
    }
#else
    Q_UNUSED(view)
    Q_UNUSED(oldIndex)
#endif
}

void notifyFlipViewAccessibilityOrientationChanged(FlipView* view)
{
#if QT_CONFIG(accessibility)
    if (view && view->isEnabled()
        && (canPrevious(view->currentIndex())
            || canNext(view->currentIndex(), view->pageCount()))) {
        sendEvent(view, QAccessible::ActionChanged);
    }
#else
    Q_UNUSED(view)
#endif
}

} // namespace fluent::collections::detail
