#include "StackView.h"

#include <algorithm>

#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QSet>
#include <QSizePolicy>

namespace fluent::collections {

namespace {
constexpr qreal kOutgoingTravelRatio = 0.25;

bool isBackKey(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Back)
        return true;
    return event->key() == Qt::Key_Left && event->modifiers().testFlag(Qt::AltModifier);
}
} // namespace

StackView::OperationGuard::OperationGuard(StackView* owner)
    : m_owner(owner),
      m_ownsOperation(owner && !owner->m_operationInProgress)
{
    if (m_ownsOperation)
        owner->m_operationInProgress = true;
}

StackView::OperationGuard::~OperationGuard()
{
    release();
}

void StackView::OperationGuard::release()
{
    if (m_owner && m_ownsOperation)
        m_owner->m_operationInProgress = false;
    m_owner = nullptr;
    m_ownsOperation = false;
}

StackView::StackView(QWidget* parent)
    : QStackedWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_transitionDuration = themeAnimation().normal;
    connect(this, &QStackedWidget::currentChanged, this, &StackView::onCurrentChanged);
    connect(this, &QStackedWidget::widgetRemoved, this, [this](int) {
        if (!m_internalStackChange && !m_destroying)
            pruneRemovedWidgets();
    });
}

StackView::~StackView()
{
    m_destroying = true;
    if (m_transitionGroup) {
        m_transitionGroup->stop();
        m_transitionGroup->deleteLater();
        m_transitionGroup = nullptr;
    }
    for (const PendingRemoval& removal : std::as_const(m_transitionRemovals))
        cleanupPendingRemoval(removal, true);
    m_transitionRemovals.clear();
    cleanupAll(true);
}

QWidget* StackView::currentItem() const
{
    return m_stack.isEmpty() ? nullptr : m_stack.last().item.data();
}

QWidget* StackView::initialItem() const
{
    return m_stack.isEmpty() ? nullptr : m_stack.first().item.data();
}

void StackView::setOrientation(Qt::Orientation orientation)
{
    if (m_orientation == orientation)
        return;
    m_orientation = orientation;
    emit orientationChanged(m_orientation);
}

void StackView::setTransitionAnimationEnabled(bool enabled)
{
    if (m_transitionAnimationEnabled == enabled)
        return;
    m_transitionAnimationEnabled = enabled;
    emit transitionAnimationEnabledChanged(m_transitionAnimationEnabled);
}

void StackView::setTransitionDuration(int durationMs)
{
    const int normalized = std::max(0, durationMs);
    if (m_transitionDuration == normalized)
        return;
    m_transitionDuration = normalized;
    emit transitionDurationChanged(m_transitionDuration);
}

void StackView::setTransitionType(StackViewTransitionType type)
{
    if (m_transitionType == type)
        return;
    m_transitionType = type;
    emit transitionTypeChanged(m_transitionType);
}

void StackView::setDefaultItemOwnership(WidgetOwnership ownership)
{
    if (m_defaultOwnership == ownership)
        return;
    m_defaultOwnership = ownership;
    emit defaultItemOwnershipChanged(m_defaultOwnership);
}

void StackView::setInitialItem(QWidget* item)
{
    setInitialItem(item, m_defaultOwnership);
}

bool StackView::setInitialItem(QWidget* item, WidgetOwnership ownership)
{
    if (!canStartOperation())
        return false;
    if (!item)
        return clear();

    QPointer<StackView> guard(this);
    QPointer<QWidget> itemGuard(item);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);

    const int oldDepth = depth();
    cleanupAll(false);
    if (!guard || !itemGuard)
        return true;
    m_stack.clear();

    QPointer<QWidget> originalParent = itemGuard->parentWidget();
    prepareItem(itemGuard.data());
    if (!guard || !itemGuard)
        return true;
    m_stack.append(makeEntry(itemGuard.data(), originalParent.data(), ownership,
                             StackViewItemStatus::Active));
    m_internalStackChange = true;
    QStackedWidget::setCurrentWidget(itemGuard.data());
    m_internalStackChange = false;
    if (!guard || !itemGuard)
        return true;
    itemGuard->setGeometry(itemRect());
    if (!guard || !itemGuard)
        return true;
    itemGuard->show();
    if (!guard || !itemGuard)
        return true;
    itemGuard->raise();
    if (!guard || !itemGuard)
        return true;

    emit itemPushed(itemGuard.data());
    if (!guard || !itemGuard) return true;
    emitDepthIfChanged(oldDepth);
    if (!guard || !itemGuard) return true;
    emitInitialIfChanged(oldInitial.data());
    if (!guard || !itemGuard) return true;
    emitCurrentIfChanged(oldCurrent.data());
    if (!guard || !itemGuard) return true;
    emit itemStatusChanged(itemGuard.data(), StackViewItemStatus::Active);
    return true;
}

bool StackView::push(QWidget* item)
{
    return push(item, m_defaultOwnership);
}

bool StackView::push(QWidget* item, WidgetOwnership ownership)
{
    if (!canStartOperation() || !item || contains(item))
        return false;

    QPointer<StackView> guard(this);
    QPointer<QWidget> itemGuard(item);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);

    const int oldDepth = depth();
    QPointer<QWidget> originalParent = itemGuard->parentWidget();
    prepareItem(itemGuard.data());
    if (!guard || !itemGuard)
        return true;
    m_stack.append(makeEntry(itemGuard.data(), originalParent.data(), ownership,
                             StackViewItemStatus::Inactive));

    emit itemPushed(itemGuard.data());
    if (!guard || !itemGuard) return true;
    emitDepthIfChanged(oldDepth);
    if (!guard || !itemGuard) return true;
    emitInitialIfChanged(oldInitial.data());
    if (!guard || !itemGuard) return true;
    emitCurrentIfChanged(oldCurrent.data());
    if (!guard || !itemGuard) return true;

    if (oldCurrent)
        setItemStatus(oldCurrent.data(), StackViewItemStatus::Deactivating);
    if (!guard || !itemGuard) return true;
    setItemStatus(itemGuard.data(), StackViewItemStatus::Activating);
    if (!guard || !itemGuard) return true;

    operationGuard.release();
    startTransition(StackViewTransitionOperation::Push, oldCurrent.data(), itemGuard.data(), {});
    return true;
}

bool StackView::push(const QVector<QWidget*>& items, WidgetOwnership ownership)
{
    if (!canStartOperation() || items.isEmpty())
        return false;
    QSet<QWidget*> uniqueItems;
    QVector<QPointer<QWidget>> itemGuards;
    itemGuards.reserve(items.size());
    for (QWidget* item : items) {
        if (!item || contains(item) || uniqueItems.contains(item))
            return false;
        uniqueItems.insert(item);
        itemGuards.append(item);
    }

    if (items.size() == 1)
        return push(items.first(), ownership);

    QPointer<StackView> guard(this);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);

    const int oldDepth = depth();
    for (const QPointer<QWidget>& itemGuard : std::as_const(itemGuards)) {
        if (!itemGuard)
            continue;
        QPointer<QWidget> originalParent = itemGuard->parentWidget();
        prepareItem(itemGuard.data());
        if (!guard)
            return true;
        if (!itemGuard)
            continue;
        m_stack.append(makeEntry(itemGuard.data(), originalParent.data(), ownership,
                                 StackViewItemStatus::Inactive));
        itemGuard->hide();
        if (!guard)
            return true;
        if (!itemGuard)
            continue;
        emit itemPushed(itemGuard.data());
        if (!guard) return true;
    }

    emitDepthIfChanged(oldDepth);
    if (!guard) return true;
    emitInitialIfChanged(oldInitial.data());
    if (!guard) return true;
    emitCurrentIfChanged(oldCurrent.data());
    if (!guard) return true;

    if (oldCurrent)
        setItemStatus(oldCurrent.data(), StackViewItemStatus::Deactivating);
    if (!guard) return true;
    QPointer<QWidget> topItem = currentItem();
    const bool topWasRequested = std::any_of(
        itemGuards.cbegin(), itemGuards.cend(),
        [topItem](const QPointer<QWidget>& requestedItem) {
            return requestedItem && requestedItem == topItem;
        });
    if (!topItem || !topWasRequested)
        return true;
    setItemStatus(topItem.data(), StackViewItemStatus::Activating);
    if (!guard || !topItem) return true;

    operationGuard.release();
    startTransition(StackViewTransitionOperation::Push, oldCurrent.data(), topItem.data(), {});
    return true;
}

bool StackView::replace(QWidget* item)
{
    return replace(item, m_defaultOwnership);
}

bool StackView::replace(QWidget* item, WidgetOwnership ownership)
{
    return replace(depth() - 1, item, ownership);
}

bool StackView::replace(int index, QWidget* item, WidgetOwnership ownership)
{
    if (!canStartOperation() || !item || contains(item))
        return false;
    if (m_stack.isEmpty())
        return push(item, ownership);
    if (index < 0 || index >= m_stack.size())
        return false;

    QPointer<StackView> guard(this);
    QPointer<QWidget> itemGuard(item);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);

    StackEntry oldEntry = m_stack.at(index);
    QPointer<QWidget> oldItem = oldEntry.item;
    QPointer<QWidget> originalParent = itemGuard->parentWidget();
    prepareItem(itemGuard.data());
    if (!guard || !itemGuard)
        return true;

    PendingRemoval removal{oldEntry.item, oldEntry.rawItem,
                           oldEntry.originalParent, oldEntry.ownership,
                           oldEntry.destroyedConnection};
    auto finishDeletedReplacement = [&] {
        cleanupPendingRemoval(removal);
        if (guard)
            layoutStackItems();
    };
    m_stack[index] = makeEntry(itemGuard.data(), originalParent.data(), ownership,
                               StackViewItemStatus::Inactive);

    emit itemReplaced(oldItem.data(), itemGuard.data());
    if (!guard)
        return true;
    if (!itemGuard) {
        finishDeletedReplacement();
        return true;
    }
    emitInitialIfChanged(oldInitial.data());
    if (!guard)
        return true;
    if (!itemGuard) {
        finishDeletedReplacement();
        return true;
    }
    const bool replacesCurrent = currentItem() == itemGuard;
    if (replacesCurrent)
        emitCurrentIfChanged(oldCurrent.data());
    if (!guard)
        return true;
    if (!itemGuard) {
        finishDeletedReplacement();
        return true;
    }

    if (oldItem)
        emitItemStatus(oldItem.data(), StackViewItemStatus::Deactivating);
    if (!guard)
        return true;
    if (!itemGuard) {
        finishDeletedReplacement();
        return true;
    }
    setItemStatus(itemGuard.data(), replacesCurrent ? StackViewItemStatus::Activating
                                                    : StackViewItemStatus::Inactive);
    if (!guard)
        return true;
    if (!itemGuard) {
        finishDeletedReplacement();
        return true;
    }

    if (replacesCurrent) {
        operationGuard.release();
        startTransition(StackViewTransitionOperation::Replace,
                        oldItem.data(), itemGuard.data(), {removal});
    } else {
        cleanupPendingRemoval(removal);
        if (guard)
            layoutStackItems();
    }
    return true;
}

bool StackView::pop()
{
    if (!canStartOperation() || m_stack.size() <= 1)
        return false;

    QPointer<StackView> guard(this);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);

    const int oldDepth = depth();
    StackEntry oldEntry = m_stack.takeLast();
    PendingRemoval removal{oldEntry.item, oldEntry.rawItem,
                           oldEntry.originalParent, oldEntry.ownership,
                           oldEntry.destroyedConnection};
    QPointer<QWidget> target = currentItem();
    auto finishInterruptedPop = [&] {
        cleanupPendingRemoval(removal);
        if (guard)
            layoutStackItems();
    };

    if (oldCurrent)
        emitItemStatus(oldCurrent.data(), StackViewItemStatus::Deactivating);
    if (!guard) return true;
    if (target)
        setItemStatus(target.data(), StackViewItemStatus::Activating);
    if (!guard)
        return true;
    if (!target) {
        finishInterruptedPop();
        return true;
    }

    emit itemPopped(oldCurrent.data());
    if (!guard)
        return true;
    if (!target) {
        finishInterruptedPop();
        return true;
    }
    emitDepthIfChanged(oldDepth);
    if (!guard)
        return true;
    if (!target) {
        finishInterruptedPop();
        return true;
    }
    emitInitialIfChanged(oldInitial.data());
    if (!guard)
        return true;
    if (!target) {
        finishInterruptedPop();
        return true;
    }
    emitCurrentIfChanged(oldCurrent.data());
    if (!guard)
        return true;
    if (!target) {
        finishInterruptedPop();
        return true;
    }

    operationGuard.release();
    startTransition(StackViewTransitionOperation::Pop,
                    oldCurrent.data(), target.data(), {removal});
    return true;
}

bool StackView::pop(QWidget* item)
{
    if (!item)
        return pop();
    return popToItem(item);
}

bool StackView::popToRoot()
{
    return popToItem(initialItem());
}

bool StackView::popToItem(QWidget* item)
{
    if (!canStartOperation() || !item)
        return false;
    const int targetIndex = stackIndexOf(item);
    if (targetIndex < 0)
        return false;
    if (targetIndex == m_stack.size() - 1)
        return true;

    QPointer<StackView> guard(this);
    QPointer<QWidget> targetGuard(item);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);

    const int oldDepth = depth();
    QVector<PendingRemoval> removals;
    auto cleanupRemovals = [&] {
        for (const PendingRemoval& removal : std::as_const(removals)) {
            cleanupPendingRemoval(removal);
            if (!guard)
                return;
        }
        removals.clear();
        layoutStackItems();
    };

    while (targetGuard
           && stackIndexOf(targetGuard.data()) >= 0
           && stackIndexOf(targetGuard.data()) < m_stack.size() - 1) {
        StackEntry entry = m_stack.takeLast();
        QPointer<QWidget> removed = entry.item;
        removals.append({entry.item, entry.rawItem, entry.originalParent,
                         entry.ownership, entry.destroyedConnection});
        if (removed)
            emitItemStatus(removed.data(), StackViewItemStatus::Deactivating);
        if (!guard)
            return true;
        emit itemPopped(removed.data());
        if (!guard)
            return true;
        if (!targetGuard) {
            cleanupRemovals();
            return true;
        }
    }

    if (!targetGuard || stackIndexOf(targetGuard.data()) < 0) {
        cleanupRemovals();
        return true;
    }

    QPointer<QWidget> target = currentItem();
    if (target != targetGuard) {
        cleanupRemovals();
        return true;
    }
    if (target)
        setItemStatus(target.data(), StackViewItemStatus::Activating);
    if (!guard)
        return true;
    if (!target) {
        cleanupRemovals();
        return true;
    }
    emitDepthIfChanged(oldDepth);
    if (!guard)
        return true;
    if (!target) {
        cleanupRemovals();
        return true;
    }
    emitInitialIfChanged(oldInitial.data());
    if (!guard)
        return true;
    if (!target) {
        cleanupRemovals();
        return true;
    }
    emitCurrentIfChanged(oldCurrent.data());
    if (!guard)
        return true;
    if (!target) {
        cleanupRemovals();
        return true;
    }

    if (removals.size() > 1) {
        for (int index = 1; index < removals.size(); ++index) {
            if (removals.at(index).item)
                emitItemStatus(removals.at(index).item, StackViewItemStatus::Inactive);
            if (!guard)
                return true;
            cleanupPendingRemoval(removals.at(index));
            if (!guard)
                return true;
        }
        removals = {removals.first()};
    }
    if (!target) {
        cleanupRemovals();
        return true;
    }

    operationGuard.release();
    startTransition(StackViewTransitionOperation::Pop,
                    oldCurrent.data(), target.data(), removals);
    return true;
}

bool StackView::goBack()
{
    return pop();
}

bool StackView::clear()
{
    if (!canStartOperation())
        return false;
    if (m_stack.isEmpty())
        return true;

    QPointer<StackView> guard(this);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);

    const int oldDepth = depth();
    QVector<StackEntry> entries = m_stack;
    m_stack.clear();
    for (const StackEntry& entry : entries) {
        QPointer<QWidget> item = entry.item;
        if (item) {
            emitItemStatus(item.data(), StackViewItemStatus::Deactivating);
            if (!guard) return true;
            if (item)
                emitItemStatus(item.data(), StackViewItemStatus::Inactive);
            if (!guard) return true;
            if (item)
                emit itemPopped(item.data());
            if (!guard) return true;
        }
        cleanupPendingRemoval({entry.item, entry.rawItem, entry.originalParent,
                               entry.ownership, entry.destroyedConnection});
        if (!guard) return true;
    }

    emitDepthIfChanged(oldDepth);
    if (!guard) return true;
    emitInitialIfChanged(oldInitial.data());
    if (!guard) return true;
    emitCurrentIfChanged(oldCurrent.data());
    if (!guard) return true;
    setBusy(true);
    if (!guard) return true;

    operationGuard.release();
    emit transitionStarted(StackViewTransitionOperation::Clear, oldCurrent.data(), nullptr);
    if (!guard) return true;
    m_finishingTransition = true;
    setBusy(false);
    if (!guard) return true;
    m_finishingTransition = false;
    emit transitionFinished(StackViewTransitionOperation::Clear, oldCurrent.data(), nullptr);
    return true;
}

bool StackView::adoptWidget(QWidget* item, WidgetOwnership ownership)
{
    if (!canStartOperation() || !item || contains(item))
        return false;

    QPointer<StackView> guard(this);
    QPointer<QWidget> itemGuard(item);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);

    const int oldDepth = depth();
    QPointer<QWidget> originalParent = itemGuard->parentWidget();
    prepareItem(itemGuard.data());
    if (!guard || !itemGuard)
        return true;
    const bool becomesCurrent = !oldCurrent || QStackedWidget::currentWidget() == itemGuard;
    m_stack.append(makeEntry(itemGuard.data(), originalParent.data(), ownership,
                             becomesCurrent ? StackViewItemStatus::Active
                                            : StackViewItemStatus::Inactive));
    itemGuard->setVisible(becomesCurrent);
    if (!guard || !itemGuard)
        return true;
    if (becomesCurrent) {
        m_internalStackChange = true;
        QStackedWidget::setCurrentWidget(itemGuard.data());
        m_internalStackChange = false;
        if (!guard || !itemGuard)
            return true;
    }

    emit itemPushed(itemGuard.data());
    if (!guard || !itemGuard) return true;
    emitDepthIfChanged(oldDepth);
    if (!guard || !itemGuard) return true;
    emitInitialIfChanged(oldInitial.data());
    if (!guard || !itemGuard) return true;
    emitCurrentIfChanged(oldCurrent.data());
    if (!guard || !itemGuard) return true;
    const StackViewItemStatus finalStatus = currentItem() == itemGuard
        ? StackViewItemStatus::Active
        : StackViewItemStatus::Inactive;
    emit itemStatusChanged(itemGuard.data(),
                           finalStatus);
    return true;
}

QWidget* StackView::itemAt(int index) const
{
    if (index < 0 || index >= m_stack.size())
        return nullptr;
    return m_stack.at(index).item.data();
}

int StackView::indexOf(QWidget* item) const
{
    return stackIndexOf(item);
}

StackView::StackViewItemStatus StackView::itemStatus(QWidget* item) const
{
    const int index = stackIndexOf(item);
    if (index < 0)
        return StackViewItemStatus::Inactive;
    return m_stack.at(index).status;
}

QSize StackView::sizeHint() const
{
    QSize hint = QStackedWidget::sizeHint();
    if (QWidget* current = currentItem())
        hint = hint.expandedTo(current->sizeHint());
    if (!hint.isValid() || hint.isEmpty())
        hint = QSize(360, 240);
    return hint;
}

QSize StackView::minimumSizeHint() const
{
    QSize hint = QStackedWidget::minimumSizeHint();
    if (QWidget* current = currentItem())
        hint = hint.expandedTo(current->minimumSizeHint());
    if (!hint.isValid() || hint.isEmpty())
        hint = QSize(120, 80);
    return hint;
}

void StackView::onThemeUpdated()
{
    // Duration tokens are theme-independent; only the painted surface refreshes.
    // zh_CN: 时长 token 不随主题变化，这里只需刷新绘制表面。
    update();
}

void StackView::setCurrentIndex(int index)
{
    if (!canStartOperation() || index < 0 || index >= QStackedWidget::count())
        return;

    QPointer<QWidget> item = QStackedWidget::widget(index);
    if (!item)
        return;

    if (!contains(item.data())) {
        adoptWidget(item.data(), WidgetOwnership::Borrowed);
        return;
    }

    QPointer<StackView> guard(this);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);
    const int oldDepth = depth();

    const int stackIndex = stackIndexOf(item.data());
    if (stackIndex >= 0 && stackIndex != m_stack.size() - 1) {
        StackEntry entry = m_stack.takeAt(stackIndex);
        m_stack.append(entry);
    }

    QVector<QPointer<QWidget>> stackItems;
    stackItems.reserve(m_stack.size());
    for (const StackEntry& entry : std::as_const(m_stack))
        stackItems.append(entry.item);
    for (const QPointer<QWidget>& stackItem : std::as_const(stackItems)) {
        if (!stackItem)
            continue;
        setItemStatus(stackItem.data(),
                      stackItem == item ? StackViewItemStatus::Active
                                        : StackViewItemStatus::Inactive);
        if (!guard || !item)
            return;
    }

    m_internalStackChange = true;
    QStackedWidget::setCurrentWidget(item.data());
    m_internalStackChange = false;
    if (!guard || !item)
        return;
    layoutStackItems();
    if (!guard || !item)
        return;
    emitDepthIfChanged(oldDepth);
    if (!guard || !item)
        return;
    emitInitialIfChanged(oldInitial.data());
    if (!guard || !item)
        return;
    emitCurrentIfChanged(oldCurrent.data());
}

void StackView::setCurrentWidget(QWidget* widget)
{
    const int index = QStackedWidget::indexOf(widget);
    if (index >= 0)
        setCurrentIndex(index);
}

void StackView::keyPressEvent(QKeyEvent* event)
{
    if (isBackKey(event)) {
        if (!m_busy && canPop()) {
            goBack();
            event->accept();
            return;
        }
        if (m_busy) {
            event->accept();
            return;
        }
    }
    QStackedWidget::keyPressEvent(event);
}

void StackView::resizeEvent(QResizeEvent* event)
{
    QStackedWidget::resizeEvent(event);
    layoutStackItems();
}

bool StackView::canStartOperation() const
{
    return !m_busy && !m_operationInProgress && !m_finishingTransition;
}

int StackView::stackIndexOf(QWidget* item) const
{
    if (!item)
        return -1;
    for (int index = 0; index < m_stack.size(); ++index) {
        if (m_stack.at(index).item == item)
            return index;
    }
    return -1;
}

StackView::StackEntry StackView::makeEntry(QWidget* item,
                                           QWidget* originalParent,
                                           WidgetOwnership ownership,
                                           StackViewItemStatus status)
{
    StackEntry entry;
    entry.item = item;
    entry.rawItem = item;
    entry.originalParent = originalParent;
    entry.ownership = ownership;
    entry.status = status;
    entry.destroyedConnection = connect(item, &QObject::destroyed, this, [this](QObject* object) {
        onItemDestroyed(object);
    });
    return entry;
}

void StackView::prepareItem(QWidget* item)
{
    if (!item)
        return;

    QPointer<StackView> guard(this);
    QPointer<QWidget> itemGuard(item);
    if (QStackedWidget::indexOf(itemGuard.data()) < 0) {
        m_internalStackChange = true;
        QStackedWidget::addWidget(itemGuard.data());
        m_internalStackChange = false;
    }
    if (!guard || !itemGuard)
        return;
    itemGuard->setGeometry(itemRect());
    if (!guard || !itemGuard)
        return;
    itemGuard->hide();
}

void StackView::setItemStatus(QWidget* item, StackViewItemStatus status)
{
    const int index = stackIndexOf(item);
    if (index < 0) {
        if (item)
            emitItemStatus(item, status);
        return;
    }

    if (m_stack[index].status == status)
        return;
    m_stack[index].status = status;
    emit itemStatusChanged(item, status);
}

void StackView::emitItemStatus(QWidget* item, StackViewItemStatus status)
{
    if (item)
        emit itemStatusChanged(item, status);
}

void StackView::emitDepthIfChanged(int oldDepth)
{
    if (oldDepth != depth())
        emit depthChanged(depth());
}

void StackView::emitCurrentIfChanged(QWidget* oldCurrent)
{
    if (oldCurrent != currentItem())
        emit currentItemChanged(currentItem());
}

void StackView::emitInitialIfChanged(QWidget* oldInitial)
{
    if (oldInitial != initialItem())
        emit initialItemChanged(initialItem());
}

void StackView::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged(m_busy);
}

void StackView::startTransition(StackViewTransitionOperation operation,
                                QWidget* fromItem,
                                QWidget* toItem,
                                const QVector<PendingRemoval>& removals)
{
    m_transitionOperation = operation;
    m_transitionFrom = fromItem;
    m_transitionTo = toItem;
    m_transitionRemovals = removals;
    QPointer<StackView> guard(this);
    setBusy(true);
    if (!guard)
        return;
    emit transitionStarted(operation, m_transitionFrom.data(), m_transitionTo.data());
    if (!guard)
        return;

    QPointer<QWidget> fromGuard = m_transitionFrom;
    QPointer<QWidget> toGuard = m_transitionTo;
    if (!shouldAnimate(fromGuard.data(), toGuard.data())) {
        completeTransition();
        return;
    }

    const QRect endRect = itemRect();
    const bool isPop = operation == StackViewTransitionOperation::Pop;
    qreal toOpacityStart = isPop ? 0.85 : 0.0;

    auto prepareStackedItems = [fromGuard, toGuard]() {
        if (fromGuard) {
            fromGuard->show();
            if (fromGuard)
                fromGuard->raise();
        }
        if (toGuard) {
            toGuard->show();
            if (toGuard)
                toGuard->raise();
        }
    };

    m_transitionGroup = new QParallelAnimationGroup(this);
    auto addPosAnimation = [this](QWidget* item, const QPoint& start, const QPoint& end) {
        auto* animation = new QPropertyAnimation(item, "pos", m_transitionGroup);
        animation->setStartValue(start);
        animation->setEndValue(end);
        animation->setDuration(m_transitionDuration);
        animation->setEasingCurve(themeAnimation().decelerate);
        m_transitionGroup->addAnimation(animation);
    };
    auto addGeometryAnimation = [this](QWidget* item, const QRect& start, const QRect& end) {
        auto* animation = new QPropertyAnimation(item, "geometry", m_transitionGroup);
        animation->setStartValue(start);
        animation->setEndValue(end);
        animation->setDuration(m_transitionDuration);
        animation->setEasingCurve(themeAnimation().decelerate);
        m_transitionGroup->addAnimation(animation);
    };
    auto addOpacityAnimation = [this](QGraphicsOpacityEffect* effect, qreal start, qreal end) {
        if (!effect)
            return;
        auto* animation = new QPropertyAnimation(effect, "opacity", m_transitionGroup);
        animation->setStartValue(start);
        animation->setEndValue(end);
        animation->setDuration(m_transitionDuration);
        animation->setEasingCurve(themeAnimation().decelerate);
        m_transitionGroup->addAnimation(animation);
    };

    if (m_transitionType == StackViewTransitionType::ScaleFade) {
        if (toGuard) {
            const QRect startRect = scaledTransitionRect(endRect, isPop ? 1.045 : 0.94);
            toGuard->setGeometry(startRect);
            setGraphicsOpacity(toGuard.data(), 0.0, m_toOpacityEffect);
            toOpacityStart = 0.0;
        }
        if (fromGuard) {
            fromGuard->setGeometry(endRect);
            setGraphicsOpacity(fromGuard.data(), 1.0, m_fromOpacityEffect);
        }

        prepareStackedItems();

        if (toGuard)
            addGeometryAnimation(toGuard.data(), toGuard->geometry(), endRect);
        if (fromGuard) {
            const QRect exitRect = scaledTransitionRect(endRect, isPop ? 0.94 : 1.045);
            addGeometryAnimation(fromGuard.data(), fromGuard->geometry(), exitRect);
        }
    } else {
        const QPoint fullOffset(m_orientation == Qt::Horizontal ? endRect.width() : 0,
                                m_orientation == Qt::Vertical ? endRect.height() : 0);
        const QPoint smallOffset(qRound(fullOffset.x() * kOutgoingTravelRatio),
                                 qRound(fullOffset.y() * kOutgoingTravelRatio));

        if (toGuard) {
            toGuard->setGeometry(QRect(endRect.topLeft() + (isPop ? -smallOffset : fullOffset), endRect.size()));
            setGraphicsOpacity(toGuard.data(), isPop ? 0.85 : 0.0, m_toOpacityEffect);
        }
        if (fromGuard) {
            fromGuard->setGeometry(endRect);
            setGraphicsOpacity(fromGuard.data(), 1.0, m_fromOpacityEffect);
        }

        prepareStackedItems();

        if (toGuard)
            addPosAnimation(toGuard.data(), toGuard->pos(), endRect.topLeft());
        if (fromGuard) {
            const QPoint exitPos = endRect.topLeft() + (isPop ? fullOffset : -smallOffset);
            addPosAnimation(fromGuard.data(), fromGuard->pos(), exitPos);
        }
    }
    addOpacityAnimation(m_toOpacityEffect, toOpacityStart, 1.0);
    addOpacityAnimation(m_fromOpacityEffect, 1.0, 0.0);

    connect(m_transitionGroup, &QParallelAnimationGroup::finished, this, &StackView::completeTransition);
    m_transitionGroup->start();
}

void StackView::completeTransition()
{
    QPointer<QWidget> fromItem = m_transitionFrom;
    QPointer<QWidget> toItem = m_transitionTo;
    const StackViewTransitionOperation operation = m_transitionOperation;
    QPointer<StackView> guard(this);

    if (m_transitionGroup) {
        m_transitionGroup->deleteLater();
        m_transitionGroup = nullptr;
    }
    clearGraphicsOpacity(fromItem.data(), m_fromOpacityEffect);
    clearGraphicsOpacity(toItem.data(), m_toOpacityEffect);

    if (toItem && QStackedWidget::indexOf(toItem.data()) >= 0) {
        m_internalStackChange = true;
        QStackedWidget::setCurrentWidget(toItem.data());
        m_internalStackChange = false;
        if (!guard)
            return;
        if (toItem)
            toItem->setGeometry(itemRect());
        if (!guard)
            return;
        if (toItem)
            toItem->show();
        if (!guard)
            return;
        if (toItem)
            toItem->raise();
        if (!guard)
            return;
        if (toItem)
            setItemStatus(toItem.data(), StackViewItemStatus::Active);
        if (!guard)
            return;
    }

    QPointer<QWidget> activeItem = currentItem();
    if (activeItem && activeItem != toItem) {
        m_internalStackChange = true;
        QStackedWidget::setCurrentWidget(activeItem.data());
        m_internalStackChange = false;
        if (!guard)
            return;
        if (activeItem)
            setItemStatus(activeItem.data(), StackViewItemStatus::Active);
        if (!guard)
            return;
    }

    QVector<QPointer<QWidget>> stackItems;
    stackItems.reserve(m_stack.size());
    for (const StackEntry& entry : std::as_const(m_stack))
        stackItems.append(entry.item);
    for (const QPointer<QWidget>& stackItem : std::as_const(stackItems)) {
        if (!stackItem)
            continue;
        const StackViewItemStatus status = stackItem == currentItem()
            ? StackViewItemStatus::Active
            : StackViewItemStatus::Inactive;
        setItemStatus(stackItem.data(), status);
        if (!guard)
            return;
    }
    activeItem = currentItem();

    const bool fromIsPendingRemoval = std::any_of(m_transitionRemovals.cbegin(), m_transitionRemovals.cend(), [fromItem](const PendingRemoval& removal) {
        return removal.item == fromItem || removal.rawItem == fromItem.data();
    });
    if (fromItem && fromItem != activeItem) {
        if (fromIsPendingRemoval)
            emitItemStatus(fromItem.data(), StackViewItemStatus::Inactive);
        if (!guard)
            return;
    }

    for (const PendingRemoval& removal : std::as_const(m_transitionRemovals)) {
        cleanupPendingRemoval(removal);
        if (!guard)
            return;
    }
    m_transitionRemovals.clear();

    m_transitionOperation = StackViewTransitionOperation::None;
    m_transitionFrom = nullptr;
    m_transitionTo = nullptr;
    m_finishingTransition = true;
    setBusy(false);
    if (!guard)
        return;
    layoutStackItems();
    if (!guard)
        return;
    m_finishingTransition = false;
    emit transitionFinished(operation, fromItem.data(), toItem.data());
}

void StackView::cleanupPendingRemoval(const PendingRemoval& removal, bool immediateDelete)
{
    QPointer<StackView> guard(this);
    QObject::disconnect(removal.destroyedConnection);
    QPointer<QWidget> item = removal.item;
    if (!item)
        return;

    if (QStackedWidget::indexOf(item.data()) >= 0) {
        m_internalStackChange = true;
        QStackedWidget::removeWidget(item.data());
        m_internalStackChange = false;
    }
    if (!guard || !item)
        return;
    item->hide();
    if (!guard || !item)
        return;
    if (removal.ownership == WidgetOwnership::Owned) {
        if (immediateDelete)
            delete item.data();
        else
            item->deleteLater();
    } else if (removal.ownership == WidgetOwnership::Reparented) {
        item->setParent(removal.originalParent.data());
    } else {
        item->setParent(nullptr);
    }
}

void StackView::cleanupAll(bool immediateDelete)
{
    QVector<StackEntry> entries = m_stack;
    m_stack.clear();
    QPointer<StackView> guard(this);
    for (const StackEntry& entry : entries) {
        cleanupPendingRemoval({entry.item, entry.rawItem, entry.originalParent,
                               entry.ownership, entry.destroyedConnection},
                              immediateDelete);
        if (!guard)
            return;
    }
}

void StackView::layoutStackItems()
{
    const QRect rect = itemRect();
    QPointer<StackView> guard(this);
    QPointer<QWidget> active = currentItem();
    QVector<QPointer<QWidget>> items;
    items.reserve(m_stack.size());
    for (const StackEntry& entry : std::as_const(m_stack))
        items.append(entry.item);
    for (const QPointer<QWidget>& item : std::as_const(items)) {
        if (!item)
            continue;
        if (!m_busy || item == active) {
            item->setGeometry(rect);
            if (!guard)
                return;
            if (!item)
                continue;
            item->setVisible(item == active);
            if (!guard)
                return;
        }
    }
}

QRect StackView::itemRect() const
{
    return rect();
}

QPoint StackView::transitionOffset(StackViewTransitionOperation operation, bool entering) const
{
    Q_UNUSED(operation)
    Q_UNUSED(entering)
    const QRect r = itemRect();
    return QPoint(m_orientation == Qt::Horizontal ? r.width() : 0,
                  m_orientation == Qt::Vertical ? r.height() : 0);
}

bool StackView::shouldAnimate(QWidget* fromItem, QWidget* toItem) const
{
    return m_transitionAnimationEnabled
        && m_transitionType != StackViewTransitionType::Immediate
        && m_transitionDuration > 0
        && fromItem
        && toItem
        && fromItem != toItem;
}

QRect StackView::scaledTransitionRect(const QRect& rect, qreal scale) const
{
    const QSize scaledSize(qRound(rect.width() * scale), qRound(rect.height() * scale));
    QRect scaled(QPoint(0, 0), scaledSize);
    scaled.moveCenter(rect.center());
    return scaled;
}

void StackView::setGraphicsOpacity(QWidget* item, qreal opacity, QPointer<QGraphicsOpacityEffect>& effectStore)
{
    if (!item)
        return;
    auto* effect = new QGraphicsOpacityEffect(item);
    effect->setOpacity(opacity);
    item->setGraphicsEffect(effect);
    effectStore = effect;
}

void StackView::clearGraphicsOpacity(QWidget* item, QPointer<QGraphicsOpacityEffect>& effectStore)
{
    if (item && effectStore && item->graphicsEffect() == effectStore)
        item->setGraphicsEffect(nullptr);
    effectStore = nullptr;
}

void StackView::onCurrentChanged(int index)
{
    if (m_internalStackChange || !canStartOperation() || m_destroying || index < 0)
        return;

    QPointer<QWidget> item = QStackedWidget::widget(index);
    if (!item)
        return;
    QPointer<StackView> guard(this);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);
    const int oldDepth = depth();
    if (!contains(item.data())) {
        operationGuard.release();
        adoptWidget(item.data(), WidgetOwnership::Borrowed);
        return;
    }

    const int stackIndex = stackIndexOf(item.data());
    if (stackIndex >= 0 && stackIndex != m_stack.size() - 1) {
        StackEntry entry = m_stack.takeAt(stackIndex);
        m_stack.append(entry);
    }
    QVector<QPointer<QWidget>> stackItems;
    stackItems.reserve(m_stack.size());
    for (const StackEntry& entry : std::as_const(m_stack))
        stackItems.append(entry.item);
    for (const QPointer<QWidget>& stackItem : std::as_const(stackItems)) {
        if (!stackItem)
            continue;
        setItemStatus(stackItem.data(),
                      stackItem == item ? StackViewItemStatus::Active
                                        : StackViewItemStatus::Inactive);
        if (!guard || !item)
            return;
    }
    layoutStackItems();
    if (!guard || !item)
        return;
    emitDepthIfChanged(oldDepth);
    if (!guard || !item) return;
    emitInitialIfChanged(oldInitial.data());
    if (!guard || !item) return;
    emitCurrentIfChanged(oldCurrent.data());
}

void StackView::onItemDestroyed(QObject* object)
{
    if (m_destroying || !object)
        return;

    QPointer<StackView> guard(this);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);
    const int oldDepth = depth();

    for (int index = m_stack.size() - 1; index >= 0; --index) {
        if (m_stack.at(index).rawItem == object)
            removeEntryAt(index);
    }
    if (QPointer<QWidget> current = currentItem()) {
        m_internalStackChange = true;
        QStackedWidget::setCurrentWidget(current.data());
        m_internalStackChange = false;
        if (!guard)
            return;
        if (current)
            setItemStatus(current.data(), StackViewItemStatus::Active);
        if (!guard) return;
    }
    layoutStackItems();
    if (!guard) return;
    emitDepthIfChanged(oldDepth);
    if (!guard) return;
    emitInitialIfChanged(oldInitial.data());
    if (!guard) return;
    emitCurrentIfChanged(oldCurrent.data());
}

void StackView::pruneRemovedWidgets()
{
    QPointer<StackView> guard(this);
    QPointer<QWidget> oldCurrent = currentItem();
    QPointer<QWidget> oldInitial = initialItem();
    OperationGuard operationGuard(this);
    const int oldDepth = depth();
    bool removedAny = false;

    for (int index = m_stack.size() - 1; index >= 0; --index) {
        QPointer<QWidget> item = m_stack.at(index).item;
        if (!item || QStackedWidget::indexOf(item.data()) >= 0)
            continue;
        QObject::disconnect(m_stack.at(index).destroyedConnection);
        item->hide();
        if (!guard)
            return;
        if (item)
            emitItemStatus(item.data(), StackViewItemStatus::Inactive);
        if (!guard)
            return;
        const int currentIndex = stackIndexOf(item.data());
        if (currentIndex >= 0)
            m_stack.removeAt(currentIndex);
        removedAny = true;
    }

    if (!removedAny)
        return;

    if (QPointer<QWidget> current = currentItem()) {
        if (QStackedWidget::indexOf(current.data()) >= 0) {
            m_internalStackChange = true;
            QStackedWidget::setCurrentWidget(current.data());
            m_internalStackChange = false;
            if (!guard)
                return;
            if (current)
                setItemStatus(current.data(), StackViewItemStatus::Active);
            if (!guard) return;
        }
    }
    layoutStackItems();
    if (!guard) return;
    emitDepthIfChanged(oldDepth);
    if (!guard) return;
    emitInitialIfChanged(oldInitial.data());
    if (!guard) return;
    emitCurrentIfChanged(oldCurrent.data());
}

void StackView::removeEntryAt(int index)
{
    if (index < 0 || index >= m_stack.size())
        return;
    m_stack.removeAt(index);
}

} // namespace fluent::collections
