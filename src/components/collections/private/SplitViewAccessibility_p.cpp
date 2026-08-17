#include "SplitViewAccessibility_p.h"

#include <algorithm>

#include <QAccessible>
#include <QAccessibleWidget>
#include <QApplication>
#include <QCoreApplication>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QVariant>

#include "components/collections/SplitView.h"

namespace fluent::collections::detail {

namespace {

constexpr int kKeyboardResizeStep = 8;

QString splitViewText(const char* source)
{
    return QCoreApplication::translate("SplitViewAccessibility", source);
}

QList<SplitViewHandle*> handlesFor(SplitView* splitView)
{
    QList<SplitViewHandle*> result;
    if (!splitView)
        return result;
    for (QObject* child : splitView->children()) {
        if (auto* handle = dynamic_cast<SplitViewHandle*>(child))
            result.append(handle);
    }
    std::sort(result.begin(), result.end(),
              [](const SplitViewHandle* left,
                 const SplitViewHandle* right) {
                  return left->handleIndex() < right->handleIndex();
              });
    return result;
}

SplitViewHandle* handleAt(SplitView* splitView, int handleIndex)
{
    const QList<SplitViewHandle*> handles = handlesFor(splitView);
    for (SplitViewHandle* handle : handles) {
        if (handle && handle->handleIndex() == handleIndex)
            return handle;
    }
    return nullptr;
}

QString paneName(SplitView* splitView, int paneIndex)
{
    QWidget* pane = splitView ? splitView->paneAt(paneIndex) : nullptr;
    QString result = pane ? pane->accessibleName() : QString();
#if QT_CONFIG(accessibility)
    if (result.isEmpty() && pane) {
        if (QAccessibleInterface* interface =
                QAccessible::queryAccessibleInterface(pane)) {
            result = interface->text(QAccessible::Name);
        }
    }
#endif
    return result.isEmpty()
        ? splitViewText("Pane %1").arg(paneIndex + 1)
        : result;
}

#if QT_CONFIG(accessibility)

class SplitViewAccessible final : public QAccessibleWidget {
public:
    explicit SplitViewAccessible(SplitView* splitView)
        : QAccessibleWidget(splitView, QAccessible::Splitter)
    {
    }

    QAccessibleInterface* childAt(int x, int y) const override
    {
        SplitView* owner = splitView();
        if (!owner)
            return nullptr;
        const QList<SplitViewHandle*> handles = handlesFor(owner);
        for (auto iterator = handles.crbegin();
             iterator != handles.crend(); ++iterator) {
            if (QAccessibleInterface* interface =
                    QAccessible::queryAccessibleInterface(*iterator)) {
                if (!interface->state().invisible
                    && interface->rect().contains(x, y)) {
                    return interface;
                }
            }
        }
        for (int index = owner->paneCount() - 1; index >= 0; --index) {
            if (QAccessibleInterface* interface = child(index)) {
                if (!interface->state().invisible
                    && interface->rect().contains(x, y)) {
                    return interface;
                }
            }
        }
        return nullptr;
    }

    QAccessibleInterface* focusChild() const override
    {
        SplitView* owner = splitView();
        QWidget* focused = QApplication::focusWidget();
        if (!owner || !focused)
            return nullptr;
        if (auto* handle = dynamic_cast<SplitViewHandle*>(focused)) {
            if (handle->splitView() == owner)
                return QAccessible::queryAccessibleInterface(handle);
        }
        for (int index = 0; index < owner->paneCount(); ++index) {
            QWidget* pane = owner->paneAt(index);
            if (pane && (pane == focused || pane->isAncestorOf(focused)))
                return QAccessible::queryAccessibleInterface(pane);
        }
        return nullptr;
    }

    int childCount() const override
    {
        SplitView* owner = splitView();
        return owner
            ? owner->paneCount() + handlesFor(owner).size()
            : 0;
    }

    int indexOfChild(const QAccessibleInterface* childInterface) const override
    {
        SplitView* owner = splitView();
        if (!owner || !childInterface)
            return -1;
        QObject* object = childInterface->object();
        for (int index = 0; index < owner->paneCount(); ++index) {
            if (owner->paneAt(index) == object)
                return index;
        }
        if (auto* handle = dynamic_cast<SplitViewHandle*>(object)) {
            return handle->splitView() == owner
                ? owner->paneCount() + handle->handleIndex()
                : -1;
        }
        return -1;
    }

    QAccessibleInterface* child(int index) const override
    {
        SplitView* owner = splitView();
        if (!owner || index < 0)
            return nullptr;
        if (index < owner->paneCount()) {
            QWidget* pane = owner->paneAt(index);
            return pane
                ? QAccessible::queryAccessibleInterface(pane)
                : nullptr;
        }
        SplitViewHandle* handle =
            handleAt(owner, index - owner->paneCount());
        return handle
            ? QAccessible::queryAccessibleInterface(handle)
            : nullptr;
    }

private:
    SplitView* splitView() const
    {
        return static_cast<SplitView*>(widget());
    }
};

class SplitViewHandleAccessible final : public QAccessibleWidget,
                                        public QAccessibleValueInterface {
public:
    explicit SplitViewHandleAccessible(SplitViewHandle* handle)
        : QAccessibleWidget(handle, QAccessible::Grip)
    {
    }

    QString text(QAccessible::Text type) const override
    {
        const QString authored = QAccessibleWidget::text(type);
        SplitViewHandle* grip = handle();
        SplitView* owner = grip ? grip->splitView() : nullptr;
        if (!authored.isEmpty() || !owner)
            return authored;
        if (type == QAccessible::Name) {
            const int leading = grip->leadingPaneIndex();
            const int trailing = grip->trailingPaneIndex();
            return splitViewText("Resize %1 and %2")
                .arg(paneName(owner, leading))
                .arg(paneName(owner, trailing));
        }
        if (type == QAccessible::Value)
            return splitViewText("%1 pixels").arg(grip->currentValue());
        return {};
    }

    QAccessible::State state() const override
    {
        QAccessible::State result = QAccessibleWidget::state();
        SplitViewHandle* grip = handle();
        result.sizeable = grip && grip->isEnabled();
        result.focusable = grip && grip->isEnabled()
            && grip->focusPolicy() != Qt::NoFocus;
        result.focused = grip && grip->hasFocus();
        return result;
    }

    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ValueInterface)
            return static_cast<QAccessibleValueInterface*>(this);
        return QAccessibleWidget::interface_cast(type);
    }

    QStringList actionNames() const override
    {
        SplitViewHandle* grip = handle();
        QStringList result;
        if (!grip || !grip->isEnabled())
            return result;
        if (grip->currentValue() > grip->minimumValue())
            result.append(QAccessibleActionInterface::decreaseAction());
        if (grip->currentValue() < grip->maximumValue())
            result.append(QAccessibleActionInterface::increaseAction());
        return result;
    }

    void doAction(const QString& actionName) override
    {
        SplitViewHandle* grip = handle();
        if (!grip || !grip->isEnabled())
            return;
        if (actionName == QAccessibleActionInterface::decreaseAction())
            grip->stepBy(-kKeyboardResizeStep);
        else if (actionName == QAccessibleActionInterface::increaseAction())
            grip->stepBy(kKeyboardResizeStep);
    }

    QStringList keyBindingsForAction(
        const QString& actionName) const override
    {
        SplitViewHandle* grip = handle();
        SplitView* owner = grip ? grip->splitView() : nullptr;
        if (!owner)
            return {};
        const bool increase =
            actionName == QAccessibleActionInterface::increaseAction();
        const bool decrease =
            actionName == QAccessibleActionInterface::decreaseAction();
        if (!increase && !decrease)
            return {};
        const QString key = owner->orientation() == Qt::Horizontal
            ? (increase ? QStringLiteral("Right")
                        : QStringLiteral("Left"))
            : (increase ? QStringLiteral("Down")
                        : QStringLiteral("Up"));
        return {key, QStringLiteral("Shift+") + key};
    }

    QString localizedActionName(const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return splitViewText("Move splitter backward");
        if (actionName == QAccessibleActionInterface::increaseAction())
            return splitViewText("Move splitter forward");
        return QAccessibleWidget::localizedActionName(actionName);
    }

    QString localizedActionDescription(
        const QString& actionName) const override
    {
        if (actionName == QAccessibleActionInterface::decreaseAction())
            return splitViewText("Makes the leading pane smaller");
        if (actionName == QAccessibleActionInterface::increaseAction())
            return splitViewText("Makes the leading pane larger");
        return QAccessibleWidget::localizedActionDescription(actionName);
    }

    QVariant currentValue() const override
    {
        return handle() ? QVariant(handle()->currentValue()) : QVariant();
    }

    void setCurrentValue(const QVariant& value) override
    {
        if (SplitViewHandle* grip = handle())
            grip->setValue(value.toInt());
    }

    QVariant maximumValue() const override
    {
        return handle() ? QVariant(handle()->maximumValue()) : QVariant();
    }

    QVariant minimumValue() const override
    {
        return handle() ? QVariant(handle()->minimumValue()) : QVariant();
    }

    QVariant minimumStepSize() const override
    {
        return handle() ? QVariant(1) : QVariant();
    }

private:
    SplitViewHandle* handle() const
    {
        return static_cast<SplitViewHandle*>(widget());
    }
};

QAccessibleInterface* splitViewAccessibilityFactory(
    const QString&, QObject* object)
{
    if (auto* splitView = dynamic_cast<SplitView*>(object))
        return new SplitViewAccessible(splitView);
    if (auto* handle = dynamic_cast<SplitViewHandle*>(object))
        return new SplitViewHandleAccessible(handle);
    return nullptr;
}

void sendEvent(QObject* object, QAccessible::Event type)
{
    if (!object)
        return;
    QAccessibleEvent event(object, type);
    QAccessible::updateAccessibility(&event);
}

#endif // QT_CONFIG(accessibility)

} // namespace

SplitViewHandle::SplitViewHandle(
    SplitView* splitView, int handleIndex)
    : QWidget(splitView)
    , m_splitView(splitView)
    , m_handleIndex(handleIndex)
{
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    resetSemanticSnapshot();
}

int SplitViewHandle::currentValue() const
{
    return m_splitView
        ? m_splitView->handleAccessibleValue(m_handleIndex) : 0;
}

int SplitViewHandle::minimumValue() const
{
    return m_splitView
        ? m_splitView->handleAccessibleMinimum(m_handleIndex) : 0;
}

int SplitViewHandle::maximumValue() const
{
    return m_splitView
        ? m_splitView->handleAccessibleMaximum(m_handleIndex) : 0;
}

int SplitViewHandle::leadingPaneIndex() const
{
    return m_splitView
        ? m_splitView->handleLeadingPaneIndex(m_handleIndex) : -1;
}

int SplitViewHandle::trailingPaneIndex() const
{
    return m_splitView
        ? m_splitView->handleTrailingPaneIndex(m_handleIndex) : -1;
}

bool SplitViewHandle::setValue(int value)
{
    return m_splitView
        && m_splitView->setHandleAccessibleValue(m_handleIndex, value);
}

bool SplitViewHandle::stepBy(int delta)
{
    return setValue(currentValue() + delta);
}

bool SplitViewHandle::consumeSemanticChange(bool* actionsChanged)
{
    const int current = currentValue();
    const int minimum = minimumValue();
    const int maximum = maximumValue();
    const bool oldDecrease = m_lastCurrentValue > m_lastMinimumValue;
    const bool oldIncrease = m_lastCurrentValue < m_lastMaximumValue;
    const bool newDecrease = current > minimum;
    const bool newIncrease = current < maximum;
    if (actionsChanged) {
        *actionsChanged = oldDecrease != newDecrease
            || oldIncrease != newIncrease;
    }
    const bool valueChanged = current != m_lastCurrentValue
        || minimum != m_lastMinimumValue
        || maximum != m_lastMaximumValue;
    m_lastCurrentValue = current;
    m_lastMinimumValue = minimum;
    m_lastMaximumValue = maximum;
    return valueChanged;
}

void SplitViewHandle::resetSemanticSnapshot()
{
    m_lastCurrentValue = currentValue();
    m_lastMinimumValue = minimumValue();
    m_lastMaximumValue = maximumValue();
}

void SplitViewHandle::focusInEvent(QFocusEvent* event)
{
    QWidget::focusInEvent(event);
    update();
#if QT_CONFIG(accessibility)
    QAccessibleEvent accessibilityEvent(this, QAccessible::Focus);
    QAccessible::updateAccessibility(&accessibilityEvent);
#endif
}

void SplitViewHandle::focusOutEvent(QFocusEvent* event)
{
    QWidget::focusOutEvent(event);
    update();
}

void SplitViewHandle::keyPressEvent(QKeyEvent* event)
{
    if (!event || !m_splitView || !isEnabled()) {
        QWidget::keyPressEvent(event);
        return;
    }

    const int step = event->modifiers().testFlag(Qt::ShiftModifier)
        ? 1 : kKeyboardResizeStep;
    int delta = 0;
    if (m_splitView->orientation() == Qt::Horizontal) {
        if (event->key() == Qt::Key_Left)
            delta = -step;
        else if (event->key() == Qt::Key_Right)
            delta = step;
    } else {
        if (event->key() == Qt::Key_Up)
            delta = -step;
        else if (event->key() == Qt::Key_Down)
            delta = step;
    }

    if (delta != 0) {
        stepBy(delta);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Home) {
        setValue(minimumValue());
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_End) {
        setValue(maximumValue());
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SplitViewHandle::paintEvent(QPaintEvent*)
{
    if (!hasFocus() || !m_splitView)
        return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_splitView->themeColorsRef().accentDefault);
    const QRect focusLine = centeredSplitHandleVisualRect(
        rect(), m_splitView->orientation(), 2);
    painter.drawRoundedRect(focusLine, 1.0, 1.0);
}

void ensureSplitViewAccessibilityFactory()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory(splitViewAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
#endif
}

void notifySplitViewAccessibilityStructureChanged(SplitView* splitView)
{
#if QT_CONFIG(accessibility)
    sendEvent(splitView, QAccessible::ObjectReorder);
    for (SplitViewHandle* handle : handlesFor(splitView))
        handle->resetSemanticSnapshot();
#else
    Q_UNUSED(splitView)
#endif
}

void notifySplitViewAccessibilityOrientationChanged(SplitView* splitView)
{
#if QT_CONFIG(accessibility)
    for (SplitViewHandle* handle : handlesFor(splitView))
        sendEvent(handle, QAccessible::ActionChanged);
#else
    Q_UNUSED(splitView)
#endif
}

void notifySplitViewAccessibilityHandleValueChanged(
    SplitView* splitView, int handleIndex)
{
#if QT_CONFIG(accessibility)
    SplitViewHandle* handle = handleAt(splitView, handleIndex);
    if (!handle)
        return;
    bool actionsChanged = false;
    if (handle->consumeSemanticChange(&actionsChanged)) {
        QAccessibleValueChangeEvent event(handle, handle->currentValue());
        QAccessible::updateAccessibility(&event);
    }
    if (actionsChanged)
        sendEvent(handle, QAccessible::ActionChanged);
#else
    Q_UNUSED(splitView)
    Q_UNUSED(handleIndex)
#endif
}

void notifySplitViewAccessibilityAllHandleValuesChanged(
    SplitView* splitView)
{
#if QT_CONFIG(accessibility)
    for (SplitViewHandle* handle : handlesFor(splitView)) {
        notifySplitViewAccessibilityHandleValueChanged(
            splitView, handle->handleIndex());
    }
#else
    Q_UNUSED(splitView)
#endif
}

} // namespace fluent::collections::detail
