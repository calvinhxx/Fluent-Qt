#include "StackView.h"

#include <algorithm>

#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QResizeEvent>
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

void StackView::setDefaultItemOwnership(StackViewItemOwnership ownership)
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

bool StackView::setInitialItem(QWidget* item, StackViewItemOwnership ownership)
{
    if (m_busy)
        return false;
    if (!item)
        return clear();

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    cleanupAll(false);
    m_stack.clear();

    prepareItem(item);
    m_stack.append(makeEntry(item, ownership, StackViewItemStatus::Active));
    m_internalStackChange = true;
    QStackedWidget::setCurrentWidget(item);
    m_internalStackChange = false;
    item->setGeometry(itemRect());
    item->show();
    item->raise();

    emit itemPushed(item);
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);
    emit itemStatusChanged(item, StackViewItemStatus::Active);
    return true;
}

bool StackView::push(QWidget* item)
{
    return push(item, m_defaultOwnership);
}

bool StackView::push(QWidget* item, StackViewItemOwnership ownership)
{
    if (!canStartOperation() || !item || contains(item))
        return false;

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    prepareItem(item);
    m_stack.append(makeEntry(item, ownership, StackViewItemStatus::Inactive));

    emit itemPushed(item);
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);

    if (oldCurrent)
        setItemStatus(oldCurrent, StackViewItemStatus::Deactivating);
    setItemStatus(item, StackViewItemStatus::Activating);
    startTransition(StackViewTransitionOperation::Push, oldCurrent, item, {});
    return true;
}

bool StackView::push(const QVector<QWidget*>& items, StackViewItemOwnership ownership)
{
    if (!canStartOperation() || items.isEmpty())
        return false;
    for (QWidget* item : items) {
        if (!item || contains(item))
            return false;
    }

    if (items.size() == 1)
        return push(items.first(), ownership);

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    for (QWidget* item : items) {
        prepareItem(item);
        m_stack.append(makeEntry(item, ownership, StackViewItemStatus::Inactive));
        item->hide();
        emit itemPushed(item);
    }

    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);

    if (oldCurrent)
        setItemStatus(oldCurrent, StackViewItemStatus::Deactivating);
    QWidget* topItem = items.last();
    setItemStatus(topItem, StackViewItemStatus::Activating);
    startTransition(StackViewTransitionOperation::Push, oldCurrent, topItem, {});
    return true;
}

bool StackView::replace(QWidget* item)
{
    return replace(item, m_defaultOwnership);
}

bool StackView::replace(QWidget* item, StackViewItemOwnership ownership)
{
    return replace(depth() - 1, item, ownership);
}

bool StackView::replace(int index, QWidget* item, StackViewItemOwnership ownership)
{
    if (!canStartOperation() || !item || contains(item))
        return false;
    if (m_stack.isEmpty())
        return push(item, ownership);
    if (index < 0 || index >= m_stack.size())
        return false;

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    StackEntry oldEntry = m_stack.at(index);
    QWidget* oldItem = oldEntry.item.data();
    prepareItem(item);

    PendingRemoval removal{oldEntry.item, oldEntry.rawItem, oldEntry.ownership, oldEntry.destroyedConnection};
    m_stack[index] = makeEntry(item, ownership, StackViewItemStatus::Inactive);

    emit itemReplaced(oldItem, item);
    emitInitialIfChanged(oldInitial);
    if (index == m_stack.size() - 1)
        emitCurrentIfChanged(oldCurrent);

    if (oldItem)
        emitItemStatus(oldItem, StackViewItemStatus::Deactivating);
    setItemStatus(item, index == m_stack.size() - 1 ? StackViewItemStatus::Activating : StackViewItemStatus::Inactive);

    if (index == m_stack.size() - 1) {
        startTransition(StackViewTransitionOperation::Replace, oldItem, item, {removal});
    } else {
        cleanupPendingRemoval(removal);
        layoutStackItems();
    }
    return true;
}

bool StackView::pop()
{
    if (!canStartOperation() || m_stack.size() <= 1)
        return false;

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    StackEntry oldEntry = m_stack.takeLast();
    PendingRemoval removal{oldEntry.item, oldEntry.rawItem, oldEntry.ownership, oldEntry.destroyedConnection};
    QWidget* target = currentItem();

    if (oldCurrent)
        emitItemStatus(oldCurrent, StackViewItemStatus::Deactivating);
    if (target)
        setItemStatus(target, StackViewItemStatus::Activating);

    emit itemPopped(oldCurrent);
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);
    startTransition(StackViewTransitionOperation::Pop, oldCurrent, target, {removal});
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

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    QVector<PendingRemoval> removals;
    while (m_stack.size() - 1 > targetIndex) {
        StackEntry entry = m_stack.takeLast();
        QWidget* removed = entry.item.data();
        if (removed)
            emitItemStatus(removed, StackViewItemStatus::Deactivating);
        emit itemPopped(removed);
        removals.append({entry.item, entry.rawItem, entry.ownership, entry.destroyedConnection});
    }

    QWidget* target = currentItem();
    if (target)
        setItemStatus(target, StackViewItemStatus::Activating);
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);

    if (removals.size() > 1) {
        for (int index = 1; index < removals.size(); ++index) {
            if (removals.at(index).item)
                emitItemStatus(removals.at(index).item, StackViewItemStatus::Inactive);
            cleanupPendingRemoval(removals.at(index));
        }
        removals = {removals.first()};
    }
    startTransition(StackViewTransitionOperation::Pop, oldCurrent, target, removals);
    return true;
}

bool StackView::goBack()
{
    return pop();
}

bool StackView::clear()
{
    if (m_busy)
        return false;
    if (m_stack.isEmpty())
        return true;

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    QVector<StackEntry> entries = m_stack;
    m_stack.clear();
    for (const StackEntry& entry : entries) {
        QWidget* item = entry.item.data();
        if (item) {
            emitItemStatus(item, StackViewItemStatus::Deactivating);
            emitItemStatus(item, StackViewItemStatus::Inactive);
            emit itemPopped(item);
        }
        cleanupPendingRemoval({entry.item, entry.rawItem, entry.ownership, entry.destroyedConnection});
    }

    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);
    emit transitionStarted(StackViewTransitionOperation::Clear, oldCurrent, nullptr);
    emit transitionFinished(StackViewTransitionOperation::Clear, oldCurrent, nullptr);
    return true;
}

bool StackView::adoptWidget(QWidget* item, StackViewItemOwnership ownership)
{
    if (m_busy || !item || contains(item))
        return false;

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    prepareItem(item);
    const bool becomesCurrent = !oldCurrent || QStackedWidget::currentWidget() == item;
    m_stack.append(makeEntry(item, ownership, becomesCurrent ? StackViewItemStatus::Active : StackViewItemStatus::Inactive));
    item->setVisible(becomesCurrent);
    if (becomesCurrent) {
        m_internalStackChange = true;
        QStackedWidget::setCurrentWidget(item);
        m_internalStackChange = false;
    }

    emit itemPushed(item);
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);
    emit itemStatusChanged(item, becomesCurrent ? StackViewItemStatus::Active : StackViewItemStatus::Inactive);
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
    if (m_busy || index < 0 || index >= QStackedWidget::count())
        return;

    QWidget* item = QStackedWidget::widget(index);
    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();

    if (!contains(item)) {
        adoptWidget(item, StackViewItemOwnership::DoesNotOwnItem);
    } else {
        const int stackIndex = stackIndexOf(item);
        if (stackIndex >= 0 && stackIndex != m_stack.size() - 1) {
            StackEntry entry = m_stack.takeAt(stackIndex);
            m_stack.append(entry);
        }
    }

    for (int i = 0; i < m_stack.size(); ++i)
        setItemStatus(m_stack.at(i).item, i == m_stack.size() - 1 ? StackViewItemStatus::Active : StackViewItemStatus::Inactive);

    m_internalStackChange = true;
    QStackedWidget::setCurrentWidget(item);
    m_internalStackChange = false;
    layoutStackItems();
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);
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
    return !m_busy;
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
                                           StackViewItemOwnership ownership,
                                           StackViewItemStatus status)
{
    StackEntry entry;
    entry.item = item;
    entry.rawItem = item;
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

    if (QStackedWidget::indexOf(item) < 0) {
        m_internalStackChange = true;
        QStackedWidget::addWidget(item);
        m_internalStackChange = false;
    }
    item->setGeometry(itemRect());
    item->hide();
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
    emit transitionStarted(operation, fromItem, toItem);

    if (!shouldAnimate(fromItem, toItem)) {
        completeTransition();
        return;
    }

    setBusy(true);
    const QRect endRect = itemRect();
    const bool isPop = operation == StackViewTransitionOperation::Pop;
    qreal toOpacityStart = isPop ? 0.85 : 0.0;

    auto prepareStackedItems = [this, fromItem, toItem]() {
        if (fromItem) {
            fromItem->show();
            fromItem->raise();
        }
        if (toItem) {
            toItem->show();
            toItem->raise();
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
        if (toItem) {
            const QRect startRect = scaledTransitionRect(endRect, isPop ? 1.045 : 0.94);
            toItem->setGeometry(startRect);
            setGraphicsOpacity(toItem, 0.0, m_toOpacityEffect);
            toOpacityStart = 0.0;
        }
        if (fromItem) {
            fromItem->setGeometry(endRect);
            setGraphicsOpacity(fromItem, 1.0, m_fromOpacityEffect);
        }

        prepareStackedItems();

        if (toItem)
            addGeometryAnimation(toItem, toItem->geometry(), endRect);
        if (fromItem) {
            const QRect exitRect = scaledTransitionRect(endRect, isPop ? 0.94 : 1.045);
            addGeometryAnimation(fromItem, fromItem->geometry(), exitRect);
        }
    } else {
        const QPoint fullOffset(m_orientation == Qt::Horizontal ? endRect.width() : 0,
                                m_orientation == Qt::Vertical ? endRect.height() : 0);
        const QPoint smallOffset(qRound(fullOffset.x() * kOutgoingTravelRatio),
                                 qRound(fullOffset.y() * kOutgoingTravelRatio));

        if (toItem) {
            toItem->setGeometry(QRect(endRect.topLeft() + (isPop ? -smallOffset : fullOffset), endRect.size()));
            setGraphicsOpacity(toItem, isPop ? 0.85 : 0.0, m_toOpacityEffect);
        }
        if (fromItem) {
            fromItem->setGeometry(endRect);
            setGraphicsOpacity(fromItem, 1.0, m_fromOpacityEffect);
        }

        prepareStackedItems();

        if (toItem)
            addPosAnimation(toItem, toItem->pos(), endRect.topLeft());
        if (fromItem) {
            const QPoint exitPos = endRect.topLeft() + (isPop ? fullOffset : -smallOffset);
            addPosAnimation(fromItem, fromItem->pos(), exitPos);
        }
    }
    addOpacityAnimation(m_toOpacityEffect, toOpacityStart, 1.0);
    addOpacityAnimation(m_fromOpacityEffect, 1.0, 0.0);

    connect(m_transitionGroup, &QParallelAnimationGroup::finished, this, &StackView::completeTransition);
    m_transitionGroup->start();
}

void StackView::completeTransition()
{
    QWidget* fromItem = m_transitionFrom.data();
    QWidget* toItem = m_transitionTo.data();
    const StackViewTransitionOperation operation = m_transitionOperation;

    if (m_transitionGroup) {
        m_transitionGroup->deleteLater();
        m_transitionGroup = nullptr;
    }
    clearGraphicsOpacity(fromItem, m_fromOpacityEffect);
    clearGraphicsOpacity(toItem, m_toOpacityEffect);

    if (toItem && QStackedWidget::indexOf(toItem) >= 0) {
        m_internalStackChange = true;
        QStackedWidget::setCurrentWidget(toItem);
        m_internalStackChange = false;
        toItem->setGeometry(itemRect());
        toItem->show();
        toItem->raise();
        setItemStatus(toItem, StackViewItemStatus::Active);
    }

    const bool fromIsPendingRemoval = std::any_of(m_transitionRemovals.cbegin(), m_transitionRemovals.cend(), [fromItem](const PendingRemoval& removal) {
        return removal.item == fromItem || removal.rawItem == fromItem;
    });
    if (fromItem) {
        if (fromIsPendingRemoval)
            emitItemStatus(fromItem, StackViewItemStatus::Inactive);
        else
            setItemStatus(fromItem, StackViewItemStatus::Inactive);
    }

    for (const PendingRemoval& removal : std::as_const(m_transitionRemovals))
        cleanupPendingRemoval(removal);
    m_transitionRemovals.clear();

    layoutStackItems();
    setBusy(false);
    emit transitionFinished(operation, fromItem, toItem);
    m_transitionOperation = StackViewTransitionOperation::None;
    m_transitionFrom = nullptr;
    m_transitionTo = nullptr;
}

void StackView::cleanupPendingRemoval(const PendingRemoval& removal, bool immediateDelete)
{
    QObject::disconnect(removal.destroyedConnection);
    QWidget* item = removal.item.data();
    if (!item)
        return;

    if (QStackedWidget::indexOf(item) >= 0) {
        m_internalStackChange = true;
        QStackedWidget::removeWidget(item);
        m_internalStackChange = false;
    }
    item->hide();
    if (removal.ownership == StackViewItemOwnership::OwnsItem) {
        if (immediateDelete)
            delete item;
        else
            item->deleteLater();
    } else {
        item->setParent(nullptr);
    }
}

void StackView::cleanupAll(bool immediateDelete)
{
    QVector<StackEntry> entries = m_stack;
    m_stack.clear();
    for (const StackEntry& entry : entries)
        cleanupPendingRemoval({entry.item, entry.rawItem, entry.ownership, entry.destroyedConnection}, immediateDelete);
}

void StackView::layoutStackItems()
{
    const QRect rect = itemRect();
    QWidget* active = currentItem();
    for (const StackEntry& entry : std::as_const(m_stack)) {
        QWidget* item = entry.item.data();
        if (!item)
            continue;
        if (!m_busy || item == active) {
            item->setGeometry(rect);
            item->setVisible(item == active);
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
    if (m_internalStackChange || m_busy || m_destroying || index < 0)
        return;

    QWidget* item = QStackedWidget::widget(index);
    if (!item)
        return;
    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    if (!contains(item)) {
        adoptWidget(item, StackViewItemOwnership::DoesNotOwnItem);
        return;
    }

    const int stackIndex = stackIndexOf(item);
    if (stackIndex >= 0 && stackIndex != m_stack.size() - 1) {
        StackEntry entry = m_stack.takeAt(stackIndex);
        m_stack.append(entry);
    }
    for (int i = 0; i < m_stack.size(); ++i)
        setItemStatus(m_stack.at(i).item, i == m_stack.size() - 1 ? StackViewItemStatus::Active : StackViewItemStatus::Inactive);
    layoutStackItems();
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);
}

void StackView::onItemDestroyed(QObject* object)
{
    if (m_destroying || !object)
        return;

    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    for (int index = m_stack.size() - 1; index >= 0; --index) {
        if (m_stack.at(index).rawItem == object)
            removeEntryAt(index);
    }
    if (QWidget* current = currentItem()) {
        m_internalStackChange = true;
        QStackedWidget::setCurrentWidget(current);
        m_internalStackChange = false;
        setItemStatus(current, StackViewItemStatus::Active);
    }
    layoutStackItems();
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);
}

void StackView::pruneRemovedWidgets()
{
    QWidget* oldCurrent = currentItem();
    QWidget* oldInitial = initialItem();
    const int oldDepth = depth();
    bool removedAny = false;

    for (int index = m_stack.size() - 1; index >= 0; --index) {
        QWidget* item = m_stack.at(index).item.data();
        if (!item || QStackedWidget::indexOf(item) >= 0)
            continue;
        QObject::disconnect(m_stack.at(index).destroyedConnection);
        item->hide();
        emitItemStatus(item, StackViewItemStatus::Inactive);
        m_stack.removeAt(index);
        removedAny = true;
    }

    if (!removedAny)
        return;

    if (QWidget* current = currentItem()) {
        if (QStackedWidget::indexOf(current) >= 0) {
            m_internalStackChange = true;
            QStackedWidget::setCurrentWidget(current);
            m_internalStackChange = false;
            setItemStatus(current, StackViewItemStatus::Active);
        }
    }
    layoutStackItems();
    emitDepthIfChanged(oldDepth);
    emitInitialIfChanged(oldInitial);
    emitCurrentIfChanged(oldCurrent);
}

void StackView::removeEntryAt(int index)
{
    if (index < 0 || index >= m_stack.size())
        return;
    m_stack.removeAt(index);
}

} // namespace fluent::collections
