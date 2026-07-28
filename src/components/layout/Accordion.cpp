#include "components/layout/Accordion.h"

#include <QEvent>
#include <QKeyEvent>
#include <QSizePolicy>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/layout/Expander.h"

namespace fluent::layout {

Accordion::Accordion(QWidget* parent)
    : QWidget(parent)
    , m_layout(new QVBoxLayout(this))
{
    setObjectName(QStringLiteral("fluentAccordion"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(themeSpacing().gap.tight);
    m_layout->setSizeConstraint(QLayout::SetMinimumSize);
}

Accordion::~Accordion()
{
    for (int index = m_items.size() - 1; index >= 0; --index)
        releaseItem(index, false, true, false);
}

void Accordion::setExpansionMode(ExpansionMode mode)
{
    if (m_expansionMode == mode)
        return;

    m_expansionMode = mode;
    if (m_expansionMode == ExpansionMode::Single)
        enforceSingleExpansion(nullptr);
    emit expansionModeChanged(m_expansionMode);
}

Expander* Accordion::itemAt(int index) const
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
    return m_items.at(index).item.data();
}

int Accordion::indexOf(const Expander* item) const
{
    if (!item)
        return -1;

    for (int index = 0; index < m_items.size(); ++index) {
        if (m_items.at(index).identity == item)
            return index;
    }
    return -1;
}

WidgetOwnership Accordion::itemOwnershipAt(int index) const
{
    if (index < 0 || index >= m_items.size())
        return WidgetOwnership::Borrowed;
    return m_items.at(index).ownership;
}

bool Accordion::addItem(Expander* item)
{
    return addItem(item, WidgetOwnership::Borrowed);
}

bool Accordion::addItem(Expander* item, WidgetOwnership ownership)
{
    return insertItem(m_items.size(), item, ownership);
}

bool Accordion::insertItem(int index, Expander* item)
{
    return insertItem(index, item, WidgetOwnership::Borrowed);
}

bool Accordion::insertItem(int index,
                           Expander* item,
                           WidgetOwnership ownership)
{
    if (!item || item->isAncestorOf(this) || indexOf(item) >= 0) {
        return false;
    }

    const int normalizedIndex = qBound(0, index, m_items.size());
    ItemRecord record;
    record.identity = item;
    record.item = item;
    record.originalParent = item->parentWidget();
    record.ownership = ownership;

    item->setParent(this);
    m_layout->insertWidget(normalizedIndex, item);
    item->headerButton()->installEventFilter(this);

    record.expandedConnection = connect(
        item, &Expander::expandedChanged, this,
        [this, item](bool expanded) {
            handleExpandedChanged(item, expanded);
        });
    record.destroyedConnection = connect(
        item, &QObject::destroyed, this,
        [this, item]() {
            handleItemDestroyed(item);
        });

    m_items.insert(normalizedIndex, record);
    item->show();
    if (m_expansionMode == ExpansionMode::Single && item->isExpanded())
        enforceSingleExpansion(item);

    updateGeometry();
    emit countChanged(m_items.size());
    emit itemAdded(normalizedIndex, item);
    return true;
}

bool Accordion::removeItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return false;
    releaseItem(index, true, true, true);
    return true;
}

Expander* Accordion::takeItem(int index)
{
    if (index < 0 || index >= m_items.size())
        return nullptr;
    m_items[index].ownership = WidgetOwnership::Borrowed;
    return releaseItem(index, false, false, true);
}

QSize Accordion::sizeHint() const
{
    return m_layout ? m_layout->sizeHint() : QWidget::sizeHint();
}

QSize Accordion::minimumSizeHint() const
{
    return m_layout ? m_layout->minimumSize() : QWidget::minimumSizeHint();
}

void Accordion::onThemeUpdated()
{
    if (m_layout)
        m_layout->setSpacing(themeSpacing().gap.tight);
    updateGeometry();
    update();
}

bool Accordion::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(watched, event);

    int currentIndex = -1;
    for (int index = 0; index < m_items.size(); ++index) {
        Expander* item = m_items.at(index).item.data();
        if (item && item->headerButton() == watched) {
            currentIndex = index;
            break;
        }
    }
    if (currentIndex < 0)
        return QWidget::eventFilter(watched, event);

    auto* keyEvent = static_cast<QKeyEvent*>(event);
    bool handled = false;
    switch (keyEvent->key()) {
    case Qt::Key_Up:
        handled = focusRelativeHeader(currentIndex, -1);
        break;
    case Qt::Key_Down:
        handled = focusRelativeHeader(currentIndex, 1);
        break;
    case Qt::Key_Home:
        handled = focusHeaderAt(0);
        break;
    case Qt::Key_End:
        handled = focusHeaderAt(m_items.size() - 1);
        break;
    default:
        break;
    }

    if (handled) {
        keyEvent->accept();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void Accordion::handleExpandedChanged(Expander* item, bool expanded)
{
    const int index = indexOf(item);
    if (index < 0)
        return;

    if (expanded && m_expansionMode == ExpansionMode::Single
        && !m_enforcingExpansion) {
        enforceSingleExpansion(item);
    }
    emit itemExpansionChanged(index, expanded);
}

void Accordion::handleItemDestroyed(Expander* identity)
{
    const int index = indexOf(identity);
    if (index < 0)
        return;

    ItemRecord record = m_items.takeAt(index);
    QObject::disconnect(record.expandedConnection);
    QObject::disconnect(record.destroyedConnection);
    updateGeometry();
    emit countChanged(m_items.size());
}

void Accordion::enforceSingleExpansion(Expander* preferredItem)
{
    if (m_enforcingExpansion || m_expansionMode != ExpansionMode::Single)
        return;

    Expander* keeper = preferredItem;
    if (!keeper) {
        for (const ItemRecord& record : m_items) {
            if (record.item && record.item->isExpanded()) {
                keeper = record.item.data();
                break;
            }
        }
    }

    m_enforcingExpansion = true;
    for (const ItemRecord& record : m_items) {
        if (record.item && record.item != keeper
            && record.item->isExpanded()) {
            record.item->setExpanded(false);
        }
    }
    m_enforcingExpansion = false;
}

bool Accordion::focusHeaderAt(int index)
{
    if (m_items.isEmpty())
        return false;

    const int direction = index >= m_items.size() - 1 ? -1 : 1;
    int candidate = qBound(0, index, m_items.size() - 1);
    for (int visited = 0; visited < m_items.size(); ++visited) {
        Expander* item = m_items.at(candidate).item.data();
        if (item && item->isVisible() && item->isEnabled()
            && item->headerButton()->isEnabled()) {
            item->headerButton()->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
        candidate += direction;
        if (candidate < 0 || candidate >= m_items.size())
            break;
    }
    return false;
}

bool Accordion::focusRelativeHeader(int currentIndex, int direction)
{
    if (m_items.size() < 2)
        return false;

    int candidate = currentIndex;
    for (int visited = 0; visited < m_items.size() - 1; ++visited) {
        candidate = (candidate + direction + m_items.size()) % m_items.size();
        Expander* item = m_items.at(candidate).item.data();
        if (item && item->isVisible() && item->isEnabled()
            && item->headerButton()->isEnabled()) {
            item->headerButton()->setFocus(Qt::ShortcutFocusReason);
            return true;
        }
    }
    return false;
}

Expander* Accordion::releaseItem(int index,
                                 bool deleteOwned,
                                 bool restoreParent,
                                 bool emitSignals)
{
    ItemRecord record = m_items.takeAt(index);
    Expander* item = record.item.data();
    QObject::disconnect(record.expandedConnection);
    QObject::disconnect(record.destroyedConnection);

    if (item) {
        item->headerButton()->removeEventFilter(this);
        m_layout->removeWidget(item);
        item->hide();
    }

    updateGeometry();
    if (emitSignals) {
        QPointer<Expander> guard(item);
        emit countChanged(m_items.size());
        emit itemRemoved(index, item);
        item = guard.data();
    }

    if (item) {
        if (record.ownership == WidgetOwnership::Owned) {
            if (deleteOwned)
                delete item;
        } else if (restoreParent
                   && record.ownership == WidgetOwnership::Reparented) {
            item->setParent(record.originalParent.data());
        } else {
            item->setParent(nullptr);
        }
    }
    return item;
}

} // namespace fluent::layout
