#include "CalendarViewAccessibility_p.h"

#include <QAccessible>
#include <QAccessibleWidget>
#include <QCoreApplication>
#include <QHash>
#include <QPointer>
#include <QStringList>

#include "components/date_time/CalendarView.h"

namespace fluent::date_time::detail {

#if QT_CONFIG(accessibility)

namespace {

constexpr int kHeaderChildCount = 3;

QString calendarText(const char* source)
{
    return QCoreApplication::translate("CalendarView", source);
}

class CalendarViewAccessibleChild;

} // namespace

class CalendarViewAccessible final : public QAccessibleWidget,
                                     public QAccessibleTableInterface {
public:
    explicit CalendarViewAccessible(CalendarView* view)
        : QAccessibleWidget(view, QAccessible::Table)
    {
    }

    ~CalendarViewAccessible() override { clearChildCache(); }

    QString text(QAccessible::Text type) const override;
    QAccessible::State state() const override;
    QAccessibleInterface* childAt(int x, int y) const override;
    QAccessibleInterface* focusChild() const override;
    int childCount() const override;
    int indexOfChild(const QAccessibleInterface* child) const override;
    QAccessibleInterface* child(int logicalIndex) const override;
    void* interface_cast(QAccessible::InterfaceType type) override;

    QAccessibleInterface* caption() const override { return nullptr; }
    QAccessibleInterface* summary() const override { return nullptr; }
    QAccessibleInterface* cellAt(int row, int column) const override;
    int selectedCellCount() const override;
    QList<QAccessibleInterface*> selectedCells() const override;
    QString columnDescription(int column) const override;
    QString rowDescription(int) const override { return {}; }
    int selectedColumnCount() const override { return 0; }
    int selectedRowCount() const override { return 0; }
    int columnCount() const override;
    int rowCount() const override;
    QList<int> selectedColumns() const override { return {}; }
    QList<int> selectedRows() const override { return {}; }
    bool isColumnSelected(int) const override { return false; }
    bool isRowSelected(int) const override { return false; }
    bool selectRow(int) override { return false; }
    bool selectColumn(int) override { return false; }
    bool unselectRow(int) override { return false; }
    bool unselectColumn(int) override { return false; }
    void modelChange(QAccessibleTableModelChangeEvent*) override {}

    CalendarView* view() const
    {
        return static_cast<CalendarView*>(widget());
    }

    int cellCount() const;
    int cellIndexForLogicalChild(int logicalIndex) const
    {
        return logicalIndex - kHeaderChildCount;
    }
    int logicalChildForCell(int cellIndex) const
    {
        return cellIndex + kHeaderChildCount;
    }
    bool childIsEnabled(int logicalIndex) const;
    bool childIsSelected(int logicalIndex) const;
    bool childIsFocused(int logicalIndex) const;
    QString childText(int logicalIndex, QAccessible::Text type) const;
    QRect childRect(int logicalIndex) const;
    void activateChild(int logicalIndex);

private:
    QDate dayForCell(int cellIndex) const;
    int monthForCell(int cellIndex) const;
    int yearForCell(int cellIndex) const;
    int selectedCellIndex() const;
    int focusedCellIndex() const;
    void clearChildCache() const;

    mutable QHash<int, QAccessible::Id> m_childToId;
};

namespace {

class CalendarViewAccessibleChild final
    : public QAccessibleInterface,
      public QAccessibleTableCellInterface,
      public QAccessibleActionInterface {
public:
    CalendarViewAccessibleChild(CalendarView* view, int logicalIndex)
        : m_view(view)
        , m_logicalIndex(logicalIndex)
    {
    }

    bool isValid() const override
    {
        CalendarViewAccessible* root = accessibleRoot();
        return root && m_logicalIndex >= 0
            && m_logicalIndex < root->childCount();
    }
    QObject* object() const override { return nullptr; }
    QAccessibleInterface* childAt(int, int) const override { return nullptr; }
    QAccessibleInterface* parent() const override
    {
        return m_view
            ? QAccessible::queryAccessibleInterface(m_view)
            : nullptr;
    }
    QAccessibleInterface* child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface*) const override { return -1; }
    QString text(QAccessible::Text type) const override
    {
        CalendarViewAccessible* root = accessibleRoot();
        return root ? root->childText(m_logicalIndex, type) : QString();
    }
    void setText(QAccessible::Text, const QString&) override {}
    QRect rect() const override
    {
        CalendarViewAccessible* root = accessibleRoot();
        return root ? root->childRect(m_logicalIndex) : QRect();
    }
    QAccessible::Role role() const override
    {
        return isCell() ? QAccessible::Cell : QAccessible::Button;
    }
    QAccessible::State state() const override
    {
        QAccessible::State result;
        CalendarViewAccessible* root = accessibleRoot();
        if (!root || !isValid()) {
            result.invalid = true;
            result.invisible = true;
            return result;
        }

        const bool enabled = root->childIsEnabled(m_logicalIndex);
        result.disabled = !enabled;
        result.focusable = enabled;
        result.selectable = isCell() && enabled;
        result.selected = isCell()
            && root->childIsSelected(m_logicalIndex);
        result.focused = isCell()
            && root->childIsFocused(m_logicalIndex);
        const QRect globalRect = rect();
        result.invisible = !m_view->isVisible() || globalRect.isEmpty();
        result.offscreen = !globalRect.isEmpty()
            && !QRect(m_view->mapToGlobal(QPoint(0, 0)), m_view->size())
                    .intersects(globalRect);
        return result;
    }
    void* interface_cast(QAccessible::InterfaceType type) override
    {
        if (type == QAccessible::ActionInterface)
            return static_cast<QAccessibleActionInterface*>(this);
        if (type == QAccessible::TableCellInterface && isCell())
            return static_cast<QAccessibleTableCellInterface*>(this);
        return nullptr;
    }

    bool isSelected() const override
    {
        CalendarViewAccessible* root = accessibleRoot();
        return isCell() && root
            && root->childIsSelected(m_logicalIndex);
    }
    QList<QAccessibleInterface*> columnHeaderCells() const override
    {
        return {};
    }
    QList<QAccessibleInterface*> rowHeaderCells() const override
    {
        return {};
    }
    int columnIndex() const override
    {
        CalendarViewAccessible* root = accessibleRoot();
        return root && isCell()
            ? root->cellIndexForLogicalChild(m_logicalIndex)
                  % root->columnCount()
            : -1;
    }
    int rowIndex() const override
    {
        CalendarViewAccessible* root = accessibleRoot();
        return root && isCell()
            ? root->cellIndexForLogicalChild(m_logicalIndex)
                  / root->columnCount()
            : -1;
    }
    int columnExtent() const override { return isCell() ? 1 : 0; }
    int rowExtent() const override { return isCell() ? 1 : 0; }
    QAccessibleInterface* table() const override { return parent(); }

    QStringList actionNames() const override
    {
        CalendarViewAccessible* root = accessibleRoot();
        return root && root->childIsEnabled(m_logicalIndex)
            ? QStringList{QAccessibleActionInterface::pressAction()}
            : QStringList{};
    }
    void doAction(const QString& actionName) override
    {
        CalendarViewAccessible* root = accessibleRoot();
        if (root && actionName == QAccessibleActionInterface::pressAction()
            && root->childIsEnabled(m_logicalIndex)) {
            root->activateChild(m_logicalIndex);
        }
    }
    QStringList keyBindingsForAction(const QString& actionName) const override
    {
        if (actionName != QAccessibleActionInterface::pressAction())
            return {};
        if (m_logicalIndex == 0)
            return {QStringLiteral("PageUp")};
        if (m_logicalIndex == 2)
            return {QStringLiteral("PageDown")};
        if (isCell())
            return {QStringLiteral("Enter"), QStringLiteral("Space")};
        return {};
    }

    int logicalIndex() const { return m_logicalIndex; }
    CalendarView* calendarView() const { return m_view; }

private:
    bool isCell() const { return m_logicalIndex >= kHeaderChildCount; }
    CalendarViewAccessible* accessibleRoot() const
    {
        return dynamic_cast<CalendarViewAccessible*>(parent());
    }

    QPointer<CalendarView> m_view;
    int m_logicalIndex = -1;
};

} // namespace

QString CalendarViewAccessible::text(QAccessible::Text type) const
{
    const CalendarView* calendar = view();
    if (!calendar)
        return {};

    if (type == QAccessible::Name) {
        return calendar->accessibleName().isEmpty()
            ? calendarText("Calendar")
            : calendar->accessibleName();
    }
    if (type == QAccessible::Description)
        return calendar->accessibleDescription();
    if (type == QAccessible::Value) {
        return calendar->selectedDate().isValid()
            ? calendar->locale().toString(
                  calendar->selectedDate(), QLocale::LongFormat)
            : calendar->property("titleText").toString();
    }
    return QAccessibleWidget::text(type);
}

QAccessible::State CalendarViewAccessible::state() const
{
    QAccessible::State result = QAccessibleWidget::state();
    const CalendarView* calendar = view();
    result.focusable = calendar && calendar->isEnabled();
    result.focused = calendar && calendar->hasFocus();
    result.selectable = result.focusable;
    return result;
}

QAccessibleInterface* CalendarViewAccessible::childAt(int x, int y) const
{
    CalendarView* calendar = view();
    if (!calendar)
        return nullptr;
    const QPoint globalPoint(x, y);
    for (int logicalIndex = 0; logicalIndex < childCount(); ++logicalIndex) {
        if (childRect(logicalIndex).contains(globalPoint))
            return child(logicalIndex);
    }
    return nullptr;
}

QAccessibleInterface* CalendarViewAccessible::focusChild() const
{
    const int cellIndex = focusedCellIndex();
    return cellIndex >= 0 ? child(logicalChildForCell(cellIndex)) : nullptr;
}

int CalendarViewAccessible::childCount() const
{
    return view() ? kHeaderChildCount + cellCount() : 0;
}

int CalendarViewAccessible::indexOfChild(
    const QAccessibleInterface* accessibleChild) const
{
    const auto* childInterface =
        dynamic_cast<const CalendarViewAccessibleChild*>(accessibleChild);
    return childInterface && childInterface->calendarView() == view()
        ? childInterface->logicalIndex()
        : -1;
}

QAccessibleInterface* CalendarViewAccessible::child(int logicalIndex) const
{
    CalendarView* calendar = view();
    if (!calendar || logicalIndex < 0 || logicalIndex >= childCount())
        return nullptr;

    auto cached = m_childToId.constFind(logicalIndex);
    if (cached != m_childToId.constEnd()) {
        if (QAccessibleInterface* interface =
                QAccessible::accessibleInterface(cached.value())) {
            return interface;
        }
        m_childToId.remove(logicalIndex);
    }

    auto* interface = new CalendarViewAccessibleChild(
        calendar, logicalIndex);
    const QAccessible::Id id =
        QAccessible::registerAccessibleInterface(interface);
    m_childToId.insert(logicalIndex, id);
    return interface;
}

void* CalendarViewAccessible::interface_cast(
    QAccessible::InterfaceType type)
{
    if (type == QAccessible::TableInterface)
        return static_cast<QAccessibleTableInterface*>(this);
    return QAccessibleWidget::interface_cast(type);
}

QAccessibleInterface* CalendarViewAccessible::cellAt(
    int row, int column) const
{
    if (row < 0 || column < 0 || row >= rowCount()
        || column >= columnCount()) {
        return nullptr;
    }
    return child(logicalChildForCell(row * columnCount() + column));
}

int CalendarViewAccessible::selectedCellCount() const
{
    return selectedCellIndex() >= 0 ? 1 : 0;
}

QList<QAccessibleInterface*> CalendarViewAccessible::selectedCells() const
{
    const int selected = selectedCellIndex();
    QAccessibleInterface* interface = selected >= 0
        ? child(logicalChildForCell(selected))
        : nullptr;
    return interface
        ? QList<QAccessibleInterface*>{interface}
        : QList<QAccessibleInterface*>{};
}

QString CalendarViewAccessible::columnDescription(int column) const
{
    const CalendarView* calendar = view();
    if (!calendar || calendar->contentLevel()
            != CalendarView::CalendarContentLevel::Day
        || column < 0 || column >= columnCount()) {
        return {};
    }
    const int first = static_cast<int>(calendar->firstDayOfWeek());
    const int day = ((first - 1 + column) % 7) + 1;
    return calendar->locale().standaloneDayName(
        static_cast<Qt::DayOfWeek>(day), QLocale::LongFormat);
}

int CalendarViewAccessible::columnCount() const
{
    const CalendarView* calendar = view();
    return calendar && calendar->contentLevel()
            == CalendarView::CalendarContentLevel::Day
        ? 7
        : 3;
}

int CalendarViewAccessible::rowCount() const
{
    const CalendarView* calendar = view();
    return calendar && calendar->contentLevel()
            == CalendarView::CalendarContentLevel::Day
        ? 6
        : 4;
}

int CalendarViewAccessible::cellCount() const
{
    return rowCount() * columnCount();
}

QDate CalendarViewAccessible::dayForCell(int cellIndex) const
{
    CalendarView* calendar = view();
    return calendar && cellIndex >= 0 && cellIndex < 42
        ? calendar->gridStartDate().addDays(cellIndex)
        : QDate();
}

int CalendarViewAccessible::monthForCell(int cellIndex) const
{
    return cellIndex >= 0 && cellIndex < 12 ? cellIndex + 1 : -1;
}

int CalendarViewAccessible::yearForCell(int cellIndex) const
{
    CalendarView* calendar = view();
    if (!calendar || cellIndex < 0 || cellIndex >= 12)
        return 0;
    const int visibleYear = calendar->visibleMonth().year();
    return visibleYear - visibleYear % 12 + cellIndex;
}

bool CalendarViewAccessible::childIsEnabled(int logicalIndex) const
{
    CalendarView* calendar = view();
    if (!calendar || !calendar->isEnabled() || logicalIndex < 0
        || logicalIndex >= childCount()) {
        return false;
    }
    if (logicalIndex == 0)
        return calendar->shiftedVisibleMonth(-1) != calendar->visibleMonth();
    if (logicalIndex == 2)
        return calendar->shiftedVisibleMonth(1) != calendar->visibleMonth();
    if (logicalIndex == 1)
        return true;

    const int cellIndex = cellIndexForLogicalChild(logicalIndex);
    switch (calendar->contentLevel()) {
    case CalendarView::CalendarContentLevel::Day:
        return calendar->isDateSelectable(dayForCell(cellIndex));
    case CalendarView::CalendarContentLevel::Month:
        return calendar->isMonthSelectable(
            calendar->visibleMonth().year(), monthForCell(cellIndex));
    case CalendarView::CalendarContentLevel::Year:
        return calendar->isYearSelectable(yearForCell(cellIndex));
    }
    return false;
}

int CalendarViewAccessible::selectedCellIndex() const
{
    CalendarView* calendar = view();
    if (!calendar)
        return -1;
    switch (calendar->contentLevel()) {
    case CalendarView::CalendarContentLevel::Day: {
        const QDate selected = calendar->selectedDate();
        const int offset = calendar->gridStartDate().daysTo(selected);
        return selected.isValid() && offset >= 0 && offset < 42
            ? offset
            : -1;
    }
    case CalendarView::CalendarContentLevel::Month:
        return calendar->visibleMonth().month() - 1;
    case CalendarView::CalendarContentLevel::Year: {
        const int visibleYear = calendar->visibleMonth().year();
        return visibleYear - (visibleYear - visibleYear % 12);
    }
    }
    return -1;
}

int CalendarViewAccessible::focusedCellIndex() const
{
    CalendarView* calendar = view();
    if (!calendar || !calendar->hasFocus())
        return -1;
    if (calendar->contentLevel() != CalendarView::CalendarContentLevel::Day)
        return selectedCellIndex();
    const QDate focused = calendar->property("focusedDate").toDate();
    const int offset = calendar->gridStartDate().daysTo(focused);
    return focused.isValid() && offset >= 0 && offset < 42
        ? offset
        : -1;
}

bool CalendarViewAccessible::childIsSelected(int logicalIndex) const
{
    return logicalIndex >= kHeaderChildCount
        && cellIndexForLogicalChild(logicalIndex) == selectedCellIndex();
}

bool CalendarViewAccessible::childIsFocused(int logicalIndex) const
{
    return logicalIndex >= kHeaderChildCount
        && cellIndexForLogicalChild(logicalIndex) == focusedCellIndex();
}

QString CalendarViewAccessible::childText(
    int logicalIndex, QAccessible::Text type) const
{
    CalendarView* calendar = view();
    if (!calendar || logicalIndex < 0 || logicalIndex >= childCount())
        return {};
    if (type != QAccessible::Name
        && type != QAccessible::Description
        && type != QAccessible::Value) {
        return {};
    }

    if (logicalIndex == 0)
        return type == QAccessible::Name
            ? calendarText("Previous page") : QString();
    if (logicalIndex == 2)
        return type == QAccessible::Name
            ? calendarText("Next page") : QString();
    if (logicalIndex == 1) {
        if (type == QAccessible::Name || type == QAccessible::Value)
            return calendar->property("titleText").toString();
        return calendarText("Change calendar view");
    }

    const int cellIndex = cellIndexForLogicalChild(logicalIndex);
    switch (calendar->contentLevel()) {
    case CalendarView::CalendarContentLevel::Day: {
        const QDate date = dayForCell(cellIndex);
        if (type == QAccessible::Name || type == QAccessible::Value)
            return calendar->locale().toString(date, QLocale::LongFormat);
        QStringList descriptions;
        if (date == QDate::currentDate())
            descriptions.append(calendarText("Today"));
        if (date.month() != calendar->visibleMonth().month()
            || date.year() != calendar->visibleMonth().year()) {
            descriptions.append(calendarText("Outside current month"));
        }
        return descriptions.join(QStringLiteral(", "));
    }
    case CalendarView::CalendarContentLevel::Month:
        return type == QAccessible::Name || type == QAccessible::Value
            ? calendar->locale().standaloneMonthName(
                  monthForCell(cellIndex), QLocale::LongFormat)
            : QString();
    case CalendarView::CalendarContentLevel::Year:
        return type == QAccessible::Name || type == QAccessible::Value
            ? QString::number(yearForCell(cellIndex))
            : QString();
    }
    return {};
}

QRect CalendarViewAccessible::childRect(int logicalIndex) const
{
    CalendarView* calendar = view();
    if (!calendar || logicalIndex < 0 || logicalIndex >= childCount())
        return {};

    QRect localRect;
    if (logicalIndex == 0)
        localRect = calendar->previousButtonRect();
    else if (logicalIndex == 1)
        localRect = calendar->titleButtonRect();
    else if (logicalIndex == 2)
        localRect = calendar->nextButtonRect();
    else {
        const int cellIndex = cellIndexForLogicalChild(logicalIndex);
        localRect = calendar->contentLevel()
                == CalendarView::CalendarContentLevel::Day
            ? calendar->cellRect(cellIndex / 7, cellIndex % 7)
            : calendar->contentCellRect(cellIndex / 3, cellIndex % 3);
    }
    return QRect(calendar->mapToGlobal(localRect.topLeft()),
                 localRect.size());
}

void CalendarViewAccessible::activateChild(int logicalIndex)
{
    CalendarView* calendar = view();
    if (!calendar || !childIsEnabled(logicalIndex))
        return;

    if (logicalIndex == 0) {
        calendar->setFocus(Qt::OtherFocusReason);
        calendar->navigatePage(-1, true);
        return;
    }
    if (logicalIndex == 2) {
        calendar->setFocus(Qt::OtherFocusReason);
        calendar->navigatePage(1, true);
        return;
    }
    if (logicalIndex == 1) {
        calendar->setFocus(Qt::OtherFocusReason);
        calendar->switchToParentContentLevel();
        return;
    }

    const int cellIndex = cellIndexForLogicalChild(logicalIndex);
    switch (calendar->contentLevel()) {
    case CalendarView::CalendarContentLevel::Day: {
        const QDate date = dayForCell(cellIndex);
        const bool alreadyFocused = calendar->hasFocus();
        calendar->m_focusedDate = date;
        calendar->m_focusIndicatorVisible = true;
        calendar->refreshProperties();
        calendar->setFocus(Qt::OtherFocusReason);
        if (alreadyFocused)
            notifyCalendarViewAccessibilityFocus(calendar);
        calendar->activateDate(date);
        return;
    }
    case CalendarView::CalendarContentLevel::Month:
        calendar->setFocus(Qt::OtherFocusReason);
        calendar->setVisibleMonth(QDate(
            calendar->visibleMonth().year(), monthForCell(cellIndex), 1));
        calendar->setContentLevel(CalendarView::CalendarContentLevel::Day);
        return;
    case CalendarView::CalendarContentLevel::Year:
        calendar->setFocus(Qt::OtherFocusReason);
        calendar->setVisibleMonth(QDate(
            yearForCell(cellIndex), calendar->visibleMonth().month(), 1));
        calendar->setContentLevel(CalendarView::CalendarContentLevel::Month);
        return;
    }
}

void CalendarViewAccessible::clearChildCache() const
{
    const QList<QAccessible::Id> ids = m_childToId.values();
    m_childToId.clear();
    for (QAccessible::Id id : ids)
        QAccessible::deleteAccessibleInterface(id);
}

QAccessibleInterface* calendarViewAccessibilityFactory(
    const QString&, QObject* object)
{
    auto* view = dynamic_cast<CalendarView*>(object);
    return view ? new CalendarViewAccessible(view) : nullptr;
}

void ensureCalendarViewAccessibilityFactory()
{
    static const bool installed = [] {
        QAccessible::installFactory(calendarViewAccessibilityFactory);
        return true;
    }();
    Q_UNUSED(installed)
}

void notifyCalendarViewAccessibilityReset(CalendarView* view)
{
    if (!view)
        return;
    QAccessibleTableModelChangeEvent event(
        view, QAccessibleTableModelChangeEvent::ModelReset);
    QAccessible::updateAccessibility(&event);
}

void notifyCalendarViewAccessibilitySelection(CalendarView* view)
{
    if (!view)
        return;
    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(view);
    auto* calendar = dynamic_cast<CalendarViewAccessible*>(root);
    QAccessibleInterface* selected = calendar && calendar->selectedCellCount() > 0
        ? calendar->selectedCells().first()
        : root;
    if (selected) {
        QAccessibleEvent selectionEvent(selected, QAccessible::Selection);
        QAccessible::updateAccessibility(&selectionEvent);
    }
    QAccessibleValueChangeEvent valueEvent(
        view, view->selectedDate().isValid()
            ? QVariant(view->locale().toString(
                  view->selectedDate(), QLocale::LongFormat))
            : QVariant(QString()));
    QAccessible::updateAccessibility(&valueEvent);
}

void notifyCalendarViewAccessibilityFocus(CalendarView* view)
{
    if (!view)
        return;
    QAccessibleInterface* root =
        QAccessible::queryAccessibleInterface(view);
    QAccessibleInterface* focused = root ? root->focusChild() : nullptr;
    if (!focused)
        return;
    QAccessibleEvent event(focused, QAccessible::Focus);
    QAccessible::updateAccessibility(&event);
}

#else

void ensureCalendarViewAccessibilityFactory() {}
void notifyCalendarViewAccessibilityReset(CalendarView*) {}
void notifyCalendarViewAccessibilitySelection(CalendarView*) {}
void notifyCalendarViewAccessibilityFocus(CalendarView*) {}

#endif // QT_CONFIG(accessibility)

} // namespace fluent::date_time::detail
