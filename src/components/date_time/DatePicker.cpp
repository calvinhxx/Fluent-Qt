#include "DatePicker.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDate>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLocale>
#include <QPainter>
#include <QtMath>

#include "components/basicinput/Button.h"
#include "components/date_time/private/PickerAccessibility_p.h"
#include "components/date_time/private/PickerFlyoutGeometry_p.h"
#include "components/date_time/private/PickerWheel_p.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "design/Spacing.h"

namespace fluent::date_time {

namespace {
constexpr int kDatePickerThemeMinWidth = 296;
constexpr int kSegmentHPadding = 12;
constexpr int kPopupShadowMargin = ::Spacing::Standard;
constexpr int kDividerWidth = 1;
constexpr int kMonthColumnBaseWidth = 134;
constexpr int kDayColumnBaseWidth = 80;
constexpr int kYearColumnBaseWidth = 80;

int dateFieldBaseWidth(DatePicker::DateField field)
{
    switch (field) {
    case DatePicker::DateField::Month:
        return kMonthColumnBaseWidth;
    case DatePicker::DateField::Day:
        return kDayColumnBaseWidth;
    case DatePicker::DateField::Year:
        return kYearColumnBaseWidth;
    }
    return kDayColumnBaseWidth;
}

QDate dateWithClampedDay(int year, int month, int day)
{
    year = qBound(1, year, 9999);
    month = qBound(1, month, 12);

    const QDate first(year, month, 1);
    const int clampedDay = qBound(1, day, first.daysInMonth());
    return QDate(year, month, clampedDay);
}
} // namespace

class DatePickerFlyout;

class PickerColumn : public detail::PickerWheelColumn {
public:
    PickerColumn(DatePickerFlyout* flyout, DatePicker::DateField field, QWidget* parent = nullptr);

    DatePicker::DateField field() const { return m_field; }

    QString pickerColumnName() const override;
    QString pickerColumnValueText() const override;
    QVariant pickerColumnCurrentValue() const override;
    QVariant pickerColumnMinimumValue() const override;
    QVariant pickerColumnMaximumValue() const override;
    QVariant pickerColumnStepSize() const override { return 1; }
    void pickerColumnSetValue(const QVariant& value) override;

protected:
    bool canShiftBy(int offset) const override;
    void shiftBy(int offset) override;
    void commitPickerValue() override;
    void cancelPickerValue() override;
    QString displayTextForOffset(int offset) const override;
    bool isRowSelectable(int offset) const override;
    bool isRowTextEnabled(int offset) const override;
    Qt::Alignment columnTextAlignment() const override;
    bool isFirstVisibleColumn() const override;
    bool isLastVisibleColumn() const override;

private:
    DatePickerFlyout* m_flyout = nullptr;
    DatePicker::DateField m_field;
};

class DatePickerFlyout : public fluent::dialogs_flyouts::Flyout {
public:
    explicit DatePickerFlyout(DatePicker* owner);

    DatePicker* owner() const { return m_owner; }
    QDate pendingDate() const { return m_pendingDate; }

    void showForPicker();
    void refreshLayout();
    void setPendingDate(const QDate& date);
    QDate shifted(DatePicker::DateField field, int offset) const;
    bool canShift(DatePicker::DateField field, int offset) const;
    bool isDateSelectable(const QDate& date) const;
    Qt::Alignment textAlignment(DatePicker::DateField field) const;
    QString displayText(DatePicker::DateField field, const QDate& date) const;
    bool isFirstVisibleField(DatePicker::DateField field) const;
    bool isLastVisibleField(DatePicker::DateField field) const;
    void shiftField(DatePicker::DateField field, int offset);
    void commit();
    void cancel();
    void refreshActionAccessibility();

    void onThemeUpdated() override;

protected:
    QPoint computePosition() const override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QVector<DatePicker::DateField> visibleFields() const;
    int preferredColumnWidth(DatePicker::DateField field) const;
    void notifyColumnValueChanges(const QDate& before, const QDate& after);
    void updateColumns();

    DatePicker* m_owner = nullptr;
    QDate m_pendingDate;
    detail::PickerWheelPanel* m_panel = nullptr;
    PickerColumn* m_monthColumn = nullptr;
    PickerColumn* m_dayColumn = nullptr;
    PickerColumn* m_yearColumn = nullptr;
};

PickerColumn::PickerColumn(DatePickerFlyout* flyout, DatePicker::DateField field, QWidget* parent)
    : detail::PickerWheelColumn(100, parent), m_flyout(flyout), m_field(field)
{
    refreshColumnProperties();
}

QString PickerColumn::pickerColumnName() const
{
    switch (m_field) {
    case DatePicker::DateField::Month:
        return QCoreApplication::translate("PickerAccessibility", "Month");
    case DatePicker::DateField::Day:
        return QCoreApplication::translate("PickerAccessibility", "Day");
    case DatePicker::DateField::Year:
        return QCoreApplication::translate("PickerAccessibility", "Year");
    }
    return {};
}

QString PickerColumn::pickerColumnValueText() const
{
    return m_flyout ? m_flyout->displayText(m_field, m_flyout->pendingDate()) : QString();
}

QVariant PickerColumn::pickerColumnCurrentValue() const
{
    const QDate value = m_flyout ? m_flyout->pendingDate() : QDate();
    if (!value.isValid())
        return {};
    switch (m_field) {
    case DatePicker::DateField::Month:
        return value.month();
    case DatePicker::DateField::Day:
        return value.day();
    case DatePicker::DateField::Year:
        return value.year();
    }
    return {};
}

QVariant PickerColumn::pickerColumnMinimumValue() const
{
    if (!m_flyout || !m_flyout->owner())
        return {};
    const QDate pending = m_flyout->pendingDate();
    const QDate minimum = m_flyout->owner()->minimumDate();
    if (m_field == DatePicker::DateField::Year)
        return minimum.year();
    if (m_field == DatePicker::DateField::Month && pending.year() == minimum.year()) {
        return minimum.month();
    }
    if (m_field == DatePicker::DateField::Day && pending.year() == minimum.year() &&
        pending.month() == minimum.month()) {
        return minimum.day();
    }
    return 1;
}

QVariant PickerColumn::pickerColumnMaximumValue() const
{
    if (!m_flyout || !m_flyout->owner())
        return {};
    const QDate pending = m_flyout->pendingDate();
    const QDate maximum = m_flyout->owner()->maximumDate();
    if (m_field == DatePicker::DateField::Year)
        return maximum.year();
    if (m_field == DatePicker::DateField::Month)
        return pending.year() == maximum.year() ? maximum.month() : 12;
    if (pending.year() == maximum.year() && pending.month() == maximum.month()) {
        return maximum.day();
    }
    return pending.isValid() ? QVariant(pending.daysInMonth()) : QVariant();
}

void PickerColumn::pickerColumnSetValue(const QVariant& value)
{
    if (!m_flyout)
        return;
    bool ok = false;
    int requested = value.toInt(&ok);
    const int current = pickerColumnCurrentValue().toInt();
    if (ok) {
        requested = qBound(pickerColumnMinimumValue().toInt(), requested,
                           pickerColumnMaximumValue().toInt());
    }
    if (ok && requested != current)
        m_flyout->shiftField(m_field, requested - current);
}

bool PickerColumn::canShiftBy(int offset) const
{
    return m_flyout && m_flyout->canShift(m_field, offset);
}

void PickerColumn::shiftBy(int offset)
{
    if (m_flyout)
        m_flyout->shiftField(m_field, offset);
}

void PickerColumn::commitPickerValue()
{
    if (m_flyout)
        m_flyout->commit();
}

void PickerColumn::cancelPickerValue()
{
    if (m_flyout)
        m_flyout->cancel();
}

QString PickerColumn::displayTextForOffset(int offset) const
{
    if (!m_flyout)
        return {};
    return m_flyout->displayText(m_field, m_flyout->shifted(m_field, offset));
}

bool PickerColumn::isRowSelectable(int offset) const
{
    return m_flyout && m_flyout->isDateSelectable(m_flyout->shifted(m_field, offset));
}

bool PickerColumn::isRowTextEnabled(int offset) const
{
    return isRowSelectable(offset);
}

Qt::Alignment PickerColumn::columnTextAlignment() const
{
    return m_flyout ? m_flyout->textAlignment(m_field) : Qt::AlignLeft;
}

bool PickerColumn::isFirstVisibleColumn() const
{
    return m_flyout && m_flyout->isFirstVisibleField(m_field);
}

bool PickerColumn::isLastVisibleColumn() const
{
    return m_flyout && m_flyout->isLastVisibleField(m_field);
}

DatePickerFlyout::DatePickerFlyout(DatePicker* owner)
    : fluent::dialogs_flyouts::Flyout(owner), m_owner(owner)
{
    setObjectName(QStringLiteral("DatePickerFlyout"));
    setAnimationEnabled(false);
    setPlacement(fluent::dialogs_flyouts::Flyout::Auto);
    setAnchorOffset(::Spacing::XSmall);
    setModal(false);
    setDim(false);
    setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

    m_panel = new detail::PickerWheelPanel(QStringLiteral("DatePickerFlyoutPanel"),
                                           kDatePickerThemeMinWidth, this);
    m_monthColumn = new PickerColumn(this, DatePicker::DateField::Month, m_panel);
    m_monthColumn->setObjectName(QStringLiteral("DatePickerMonthColumn"));
    m_dayColumn = new PickerColumn(this, DatePicker::DateField::Day, m_panel);
    m_dayColumn->setObjectName(QStringLiteral("DatePickerDayColumn"));
    m_yearColumn = new PickerColumn(this, DatePicker::DateField::Year, m_panel);
    m_yearColumn->setObjectName(QStringLiteral("DatePickerYearColumn"));
    m_panel->setColumns({m_monthColumn, m_dayColumn, m_yearColumn});
    m_panel->initializeActions(
        QStringLiteral("DatePickerConfirmButton"), QStringLiteral("DatePickerCancelButton"),
        [this] { commit(); }, [this] { cancel(); });
    refreshActionAccessibility();
    connect(this, &DatePickerFlyout::closed, this, [this] {
        if (m_owner)
            m_owner->handleFlyoutClosed();
    });
}

QPoint DatePickerFlyout::computePosition() const
{
    if (!m_owner || !m_panel || !m_owner->window())
        return fluent::dialogs_flyouts::Flyout::computePosition();

    return detail::alignedWheelFlyoutPosition(m_owner, size(), kPopupShadowMargin,
                                              m_panel->selectedRowCenterY(), clampToWindow());
}

QVector<DatePicker::DateField> DatePickerFlyout::visibleFields() const
{
    QVector<DatePicker::DateField> fields;
    if (!m_owner)
        return fields;
    if (m_owner->monthVisible())
        fields.append(DatePicker::DateField::Month);
    if (m_owner->dayVisible())
        fields.append(DatePicker::DateField::Day);
    if (m_owner->yearVisible())
        fields.append(DatePicker::DateField::Year);
    return fields;
}

int DatePickerFlyout::preferredColumnWidth(DatePicker::DateField field) const
{
    if (!m_owner)
        return dateFieldBaseWidth(field);
    return m_owner->preferredFieldWidth(field);
}

bool DatePickerFlyout::isFirstVisibleField(DatePicker::DateField field) const
{
    const auto fields = visibleFields();
    return !fields.isEmpty() && fields.first() == field;
}

bool DatePickerFlyout::isLastVisibleField(DatePicker::DateField field) const
{
    const auto fields = visibleFields();
    return !fields.isEmpty() && fields.last() == field;
}

void DatePickerFlyout::showForPicker()
{
    if (!m_owner)
        return;

    const QDate selected = m_owner->selectedDate();
    setPendingDate(selected.isValid() ? selected : m_owner->date());
    refreshLayout();

    if (isOpen() || isVisible()) {
        show();
        raise();
        setFocus(Qt::PopupFocusReason);
    } else {
        showAt(m_owner);
    }

    if (auto* column = m_panel->firstVisibleColumn())
        column->setFocus(Qt::PopupFocusReason);
}

void DatePickerFlyout::refreshLayout()
{
    if (!m_owner)
        return;

    const auto fields = visibleFields();
    m_panel->configureColumns(m_owner->font(),
                              {fields.contains(DatePicker::DateField::Month),
                               fields.contains(DatePicker::DateField::Day),
                               fields.contains(DatePicker::DateField::Year)},
                              {preferredColumnWidth(DatePicker::DateField::Month),
                               preferredColumnWidth(DatePicker::DateField::Day),
                               preferredColumnWidth(DatePicker::DateField::Year)});

    const QSize cardSize = m_panel->sizeHint();
    const int cardW = cardSize.width();
    const int cardH = cardSize.height();
    setFixedSize(cardW + kPopupShadowMargin * 2, cardH + kPopupShadowMargin * 2);
    m_panel->setGeometry(kPopupShadowMargin, kPopupShadowMargin, cardW, cardH);
    setAnchor(m_owner);

    if (isOpen() || isVisible())
        move(computePosition());
}

void DatePickerFlyout::setPendingDate(const QDate& date)
{
    if (!m_owner)
        return;
    const QDate normalized = m_owner->clampDate(date.isValid() ? date : m_owner->date());
    if (m_pendingDate == normalized)
        return;
    const QDate before = m_pendingDate;
    m_pendingDate = normalized;
    updateColumns();
    notifyColumnValueChanges(before, m_pendingDate);
}

QDate DatePickerFlyout::shifted(DatePicker::DateField field, int offset) const
{
    if (!m_owner)
        return QDate();
    return m_owner->shiftedDate(m_pendingDate, field, offset);
}

bool DatePickerFlyout::canShift(DatePicker::DateField field, int offset) const
{
    const QDate candidate = shifted(field, offset);
    return isDateSelectable(candidate) && candidate != m_pendingDate;
}

bool DatePickerFlyout::isDateSelectable(const QDate& date) const
{
    if (!m_owner || !date.isValid())
        return false;
    return date >= m_owner->minimumDate() && date <= m_owner->maximumDate();
}

Qt::Alignment DatePickerFlyout::textAlignment(DatePicker::DateField field) const
{
    return m_owner ? m_owner->fieldTextAlignment(field) : Qt::AlignLeft;
}

QString DatePickerFlyout::displayText(DatePicker::DateField field, const QDate& date) const
{
    return m_owner ? m_owner->formatField(field, date) : QString();
}

void DatePickerFlyout::shiftField(DatePicker::DateField field, int offset)
{
    if (!m_owner || offset == 0)
        return;
    const QDate next = shifted(field, offset);
    if (!isDateSelectable(next) || next == m_pendingDate)
        return;
    const QDate before = m_pendingDate;
    m_pendingDate = next;
    updateColumns();
    notifyColumnValueChanges(before, m_pendingDate);
}

void DatePickerFlyout::commit()
{
    if (m_owner && isDateSelectable(m_pendingDate))
        m_owner->applyPendingDate(m_pendingDate);
    close();
}

void DatePickerFlyout::cancel()
{
    close();
}

void DatePickerFlyout::refreshActionAccessibility()
{
    if (!m_panel)
        return;
    const QString confirmOverride = m_owner ? m_owner->confirmButtonAccessibleName() : QString();
    const QString cancelOverride = m_owner ? m_owner->cancelButtonAccessibleName() : QString();
    m_panel->setActionAccessibleNames(
        confirmOverride.isEmpty()
            ? QCoreApplication::translate("PickerAccessibility", "Confirm date")
            : confirmOverride,
        cancelOverride.isEmpty() ? QCoreApplication::translate("PickerAccessibility", "Cancel")
                                 : cancelOverride);
}

void DatePickerFlyout::onThemeUpdated()
{
    fluent::dialogs_flyouts::Flyout::onThemeUpdated();
    if (m_panel)
        m_panel->refreshTheme();
}

void DatePickerFlyout::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        commit();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape) {
        cancel();
        event->accept();
        return;
    }
    fluent::dialogs_flyouts::Flyout::keyPressEvent(event);
}

void DatePickerFlyout::updateColumns()
{
    if (m_panel)
        m_panel->updateColumns();
    update();
}

void DatePickerFlyout::notifyColumnValueChanges(const QDate& before, const QDate& after)
{
    if (!m_panel || before == after)
        return;
    if (before.month() != after.month())
        detail::notifyPickerColumnValueChanged(m_monthColumn);
    if (before.day() != after.day())
        detail::notifyPickerColumnValueChanged(m_dayColumn);
    if (before.year() != after.year())
        detail::notifyPickerColumnValueChanged(m_yearColumn);
}

DatePicker::DatePicker(QWidget* parent) : fluent::basicinput::Button(parent)
{
    detail::ensurePickerAccessibilityFactory();
    m_observedLocale = QWidget::locale();
    const QDate today = QDate::currentDate();
    m_minimumDate = today.addYears(-100);
    m_maximumDate = today.addYears(100);
    m_date = clampDate(today);

    setObjectName(QStringLiteral("DatePicker"));
    setFocusPolicy(Qt::StrongFocus);
    setFluentStyle(fluent::basicinput::Button::Standard);
    setFluentSize(fluent::basicinput::Button::StandardSize);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(this, &fluent::basicinput::Button::clicked, this, &DatePicker::openPicker);
    onThemeUpdated();
}

DatePicker::~DatePicker()
{
    if (m_flyout) {
        DatePickerFlyout* flyout = m_flyout.data();
        m_flyout = nullptr;
        flyout->setAnimationEnabled(false);
        flyout->close();
        delete flyout;
    }
}

void DatePicker::setDate(const QDate& date)
{
    if (!date.isValid())
        return;
    setSelectedDate(date);
}

void DatePicker::setSelectedDate(const QDate& date)
{
    if (!date.isValid()) {
        clearSelectedDate();
        return;
    }

    const QDate normalized = clampDate(date);
    const QDate oldDate = this->date();
    const QDate oldSelected = m_selectedDate;

    m_date = normalized;
    m_selectedDate = normalized;

    if (oldSelected != m_selectedDate)
        detail::notifyPickerRootValueChanged(this);

    if (oldSelected != m_selectedDate)
        emit selectedDateChanged(m_selectedDate);
    if (oldDate != this->date())
        emit dateChanged(this->date());
    update();
}

void DatePicker::clearSelectedDate()
{
    if (!m_selectedDate.isValid())
        return;
    m_selectedDate = QDate();
    detail::notifyPickerRootValueChanged(this);
    emit selectedDateChanged(m_selectedDate);
    update();
}

void DatePicker::setMinimumDate(const QDate& date)
{
    setDateRange(date, m_maximumDate);
}

void DatePicker::setMaximumDate(const QDate& date)
{
    setDateRange(m_minimumDate, date);
}

void DatePicker::setDateRange(const QDate& minimumDate, const QDate& maximumDate)
{
    QDate nextMin = minimumDate.isValid() ? minimumDate : m_minimumDate;
    QDate nextMax = maximumDate.isValid() ? maximumDate : m_maximumDate;
    if (nextMin > nextMax)
        nextMax = nextMin;

    const bool minChanged = m_minimumDate != nextMin;
    const bool maxChanged = m_maximumDate != nextMax;
    if (!minChanged && !maxChanged)
        return;

    m_minimumDate = nextMin;
    m_maximumDate = nextMax;

    if (minChanged)
        emit minimumDateChanged(m_minimumDate);
    if (maxChanged)
        emit maximumDateChanged(m_maximumDate);

    const QDate oldDate = date();
    if (m_selectedDate.isValid()) {
        const QDate clamped = clampDate(m_selectedDate);
        if (clamped != m_selectedDate) {
            m_selectedDate = clamped;
            m_date = clamped;
            detail::notifyPickerRootValueChanged(this);
            emit selectedDateChanged(m_selectedDate);
        }
    } else {
        m_date = clampDate(m_date);
    }
    if (oldDate != date())
        emit dateChanged(date());

    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
}

void DatePicker::setMonthVisible(bool visible)
{
    if (setFieldVisible(DateField::Month, visible)) {
        if (m_selectedDate.isValid())
            detail::notifyPickerRootValueChanged(this);
        emit monthVisibleChanged(m_monthVisible);
    }
}

void DatePicker::setDayVisible(bool visible)
{
    if (setFieldVisible(DateField::Day, visible)) {
        if (m_selectedDate.isValid())
            detail::notifyPickerRootValueChanged(this);
        emit dayVisibleChanged(m_dayVisible);
    }
}

void DatePicker::setYearVisible(bool visible)
{
    if (setFieldVisible(DateField::Year, visible)) {
        if (m_selectedDate.isValid())
            detail::notifyPickerRootValueChanged(this);
        emit yearVisibleChanged(m_yearVisible);
    }
}

void DatePicker::setMonthFormat(MonthFormat format)
{
    if (m_monthFormat == format)
        return;
    m_monthFormat = format;
    updateGeometry();
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    if (m_selectedDate.isValid())
        detail::notifyPickerRootValueChanged(this);
    emit monthFormatChanged(m_monthFormat);
}

void DatePicker::setDayFormat(DayFormat format)
{
    if (m_dayFormat == format)
        return;
    m_dayFormat = format;
    updateGeometry();
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    if (m_selectedDate.isValid())
        detail::notifyPickerRootValueChanged(this);
    emit dayFormatChanged(m_dayFormat);
}

void DatePicker::setYearFormat(YearFormat format)
{
    if (m_yearFormat == format)
        return;
    m_yearFormat = format;
    updateGeometry();
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
    if (m_selectedDate.isValid())
        detail::notifyPickerRootValueChanged(this);
    emit yearFormatChanged(m_yearFormat);
}

void DatePicker::setLocale(const QLocale& locale)
{
    if (QWidget::locale() == locale)
        return;
    QWidget::setLocale(locale);
}

void DatePicker::setPlaceholderText(DateField field, const QString& text)
{
    QString* target = nullptr;
    switch (field) {
    case DateField::Month:
        target = &m_monthPlaceholderText;
        break;
    case DateField::Day:
        target = &m_dayPlaceholderText;
        break;
    case DateField::Year:
        target = &m_yearPlaceholderText;
        break;
    }
    if (!target || *target == text)
        return;

    *target = text;
    updateGeometry();
    update();
    emit placeholderTextChanged(field, text);
}

void DatePicker::setConfirmButtonAccessibleName(const QString& name)
{
    if (m_confirmButtonAccessibleName == name)
        return;
    m_confirmButtonAccessibleName = name;
    if (m_flyout)
        m_flyout->refreshActionAccessibility();
    emit confirmButtonAccessibleNameChanged(m_confirmButtonAccessibleName);
}

void DatePicker::setCancelButtonAccessibleName(const QString& name)
{
    if (m_cancelButtonAccessibleName == name)
        return;
    m_cancelButtonAccessibleName = name;
    if (m_flyout)
        m_flyout->refreshActionAccessibility();
    emit cancelButtonAccessibleNameChanged(m_cancelButtonAccessibleName);
}

Qt::Alignment DatePicker::fieldTextAlignment(DateField field) const
{
    switch (field) {
    case DateField::Month:
        return m_monthTextAlignment;
    case DateField::Day:
        return m_dayTextAlignment;
    case DateField::Year:
        return m_yearTextAlignment;
    }
    return Qt::AlignLeft;
}

void DatePicker::setFieldTextAlignment(DateField field, Qt::Alignment alignment)
{
    Qt::Alignment* target = nullptr;
    switch (field) {
    case DateField::Month:
        target = &m_monthTextAlignment;
        break;
    case DateField::Day:
        target = &m_dayTextAlignment;
        break;
    case DateField::Year:
        target = &m_yearTextAlignment;
        break;
    }
    if (!target)
        return;

    const Qt::Alignment normalized =
        detail::normalizedPickerHorizontalAlignment(alignment, *target);
    if (*target == normalized)
        return;

    *target = normalized;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
}

void DatePicker::openPicker()
{
    if (!isEnabled() || visibleFieldCount() <= 0)
        return;

    QPointer<DatePicker> guard(this);
    if (!m_flyout) {
        setDropDownOpen(false);
        if (!guard)
            return;
        m_flyout = new DatePickerFlyout(this);
    }

    m_flyout->showForPicker();
    if (!guard)
        return;
    guard->setDropDownOpen(true);
    if (guard)
        guard->update();
}

void DatePicker::closePicker()
{
    if (m_flyout)
        m_flyout->cancel();
}

QString DatePicker::fieldDisplayText(DateField field) const
{
    if (!isFieldVisible(field))
        return QString();
    if (!m_selectedDate.isValid())
        return placeholderText(field);
    return formatField(field, m_selectedDate);
}

QString DatePicker::placeholderText(DateField field) const
{
    switch (field) {
    case DateField::Month:
        return m_monthPlaceholderText;
    case DateField::Day:
        return m_dayPlaceholderText;
    case DateField::Year:
        return m_yearPlaceholderText;
    }
    return QString();
}

int DatePicker::preferredFieldWidth(DateField field) const
{
    const QFontMetrics metrics(font());
    int textWidth = metrics.horizontalAdvance(placeholderText(field));
    auto includeDate = [this, field, &metrics, &textWidth](const QDate& date) {
        textWidth = qMax(textWidth, metrics.horizontalAdvance(formatField(field, date)));
    };

    switch (field) {
    case DateField::Month:
        for (int month = 1; month <= 12; ++month)
            includeDate(QDate(2026, month, 1));
        break;
    case DateField::Day:
        if (m_dayFormat == DayFormat::DayIntegerWithAbbreviatedWeekday) {
            for (int dayOfWeek = 1; dayOfWeek <= 7; ++dayOfWeek) {
                const QString text = QStringLiteral("31 (%1)").arg(
                    locale().dayName(dayOfWeek, QLocale::ShortFormat));
                textWidth = qMax(textWidth, metrics.horizontalAdvance(text));
            }
        } else {
            includeDate(QDate(2026, 1, 31));
        }
        break;
    case DateField::Year:
        includeDate(QDate(1, 1, 1));
        includeDate(QDate(8888, 1, 1));
        includeDate(QDate(9999, 1, 1));
        break;
    }

    return qMax(dateFieldBaseWidth(field), textWidth + kSegmentHPadding * 2);
}

QSize DatePicker::sizeHint() const
{
    int width = 0;
    if (m_monthVisible)
        width += preferredFieldWidth(DateField::Month);
    if (m_dayVisible)
        width += preferredFieldWidth(DateField::Day);
    if (m_yearVisible)
        width += preferredFieldWidth(DateField::Year);
    width += qMax(0, visibleFieldCount() - 1) * kDividerWidth;
    width = qMax(width, kDatePickerThemeMinWidth);

    return QSize(width, detail::pickerEntryHeight(font()));
}

QSize DatePicker::minimumSizeHint() const
{
    return QSize(kDatePickerThemeMinWidth, detail::pickerEntryHeight(font()));
}

void DatePicker::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto radius = themeRadius();
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty())
        return;

    const bool active = isEnabled() && (underMouse() || isDown());
    const QRectF sr(surface);

    // Fluent treatment. zh_CN: Fluent 样式。
    QColor bg = colors.controlDefault;
    if (!isEnabled())
        bg = colors.controlDisabled;
    else if (isDown())
        bg = colors.subtleTertiary;
    else if (underMouse())
        bg = colors.subtleSecondary;

    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRoundedRect(QRectF(surface), radius.control, radius.control);

    const auto segments = fieldSegments();
    painter.setFont(font());
    for (int i = 0; i < segments.size(); ++i) {
        const auto& segment = segments.at(i);

        if (i > 0) {
            painter.setPen(colors.strokeDivider);
            painter.drawLine(segment.rect.left(), surface.top() + 1, segment.rect.left(),
                             surface.bottom() - 1);
        }

        QColor segmentTextColor = isEnabled() ? (active ? colors.textPrimary : colors.textSecondary)
                                              : colors.textDisabled;
        painter.setPen(segmentTextColor);
        const QString text = fieldDisplayText(segment.field);
        QRect textRect = segment.rect.adjusted(kSegmentHPadding, 0, -kSegmentHPadding, 0);
        const Qt::Alignment alignment = Qt::AlignVCenter | fieldTextAlignment(segment.field);
        painter.drawText(textRect, alignment,
                         painter.fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
    }
}

void DatePicker::keyPressEvent(QKeyEvent* event)
{
    if (!m_dropDownOpen && isEnabled() &&
        (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
         event->key() == Qt::Key_Enter || event->key() == Qt::Key_F4 ||
         (event->key() == Qt::Key_Down && event->modifiers().testFlag(Qt::AltModifier)))) {
        openPicker();
        event->accept();
        return;
    }
    fluent::basicinput::Button::keyPressEvent(event);
}

void DatePicker::changeEvent(QEvent* event)
{
    fluent::basicinput::Button::changeEvent(event);
    if (event->type() == QEvent::FontChange) {
        updateGeometry();
        if (m_flyout && m_flyout->isOpen())
            m_flyout->refreshLayout();
        update();
    }
    if (event->type() == QEvent::LocaleChange && m_observedLocale != QWidget::locale()) {
        m_observedLocale = QWidget::locale();
        if (m_flyout && m_flyout->isOpen())
            m_flyout->showForPicker();
        updateGeometry();
        if (m_selectedDate.isValid())
            detail::notifyPickerRootValueChanged(this);
        update();
        emit localeChanged(m_observedLocale);
    }
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            closePicker();
        update();
    }
}

void DatePicker::onThemeUpdated()
{
    fluent::basicinput::Button::onThemeUpdated();
    if (m_flyout)
        m_flyout->onThemeUpdated();
}

int DatePicker::visibleFieldCount() const
{
    return (m_monthVisible ? 1 : 0) + (m_dayVisible ? 1 : 0) + (m_yearVisible ? 1 : 0);
}

bool DatePicker::isFieldVisible(DateField field) const
{
    switch (field) {
    case DateField::Month:
        return m_monthVisible;
    case DateField::Day:
        return m_dayVisible;
    case DateField::Year:
        return m_yearVisible;
    }
    return false;
}

bool DatePicker::setFieldVisible(DateField field, bool visible)
{
    bool* target = nullptr;
    switch (field) {
    case DateField::Month:
        target = &m_monthVisible;
        break;
    case DateField::Day:
        target = &m_dayVisible;
        break;
    case DateField::Year:
        target = &m_yearVisible;
        break;
    }

    if (!target || *target == visible)
        return false;
    if (!visible && visibleFieldCount() <= 1)
        return false;

    *target = visible;
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    updateGeometry();
    update();
    return true;
}

QVector<DatePicker::FieldSegment> DatePicker::fieldSegments() const
{
    QVector<DateField> fields;
    if (m_monthVisible)
        fields.append(DateField::Month);
    if (m_dayVisible)
        fields.append(DateField::Day);
    if (m_yearVisible)
        fields.append(DateField::Year);

    QVector<FieldSegment> result;
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty() || fields.isEmpty())
        return result;

    int totalWeight = 0;
    auto weightFor = [this](DateField field) { return preferredFieldWidth(field); };
    for (DateField field : fields)
        totalWeight += weightFor(field);

    int x = surface.left();
    int remainingW = surface.width();
    int remainingWeight = totalWeight;
    for (int i = 0; i < fields.size(); ++i) {
        const int weight = weightFor(fields.at(i));
        int w =
            i == fields.size() - 1
                ? remainingW
                : qMax(32, qRound(double(surface.width()) * double(weight) / double(totalWeight)));
        w = qMin(w, remainingW);
        result.append({fields.at(i), QRect(x, surface.top(), w, surface.height())});
        x += w;
        remainingW -= w;
        remainingWeight -= weight;
        Q_UNUSED(remainingWeight);
    }
    return result;
}

QRect DatePicker::fieldSurfaceRect() const
{
    return QRect(0, 0, width(), qMin(height(), detail::pickerEntryHeight(font())));
}

QString DatePicker::formatField(DateField field, const QDate& date) const
{
    if (!date.isValid())
        return QString();

    switch (field) {
    case DateField::Month:
        switch (m_monthFormat) {
        case MonthFormat::FullMonthName:
            return locale().monthName(date.month(), QLocale::LongFormat);
        case MonthFormat::AbbreviatedMonthName:
            return locale().monthName(date.month(), QLocale::ShortFormat);
        case MonthFormat::NumericMonth:
            return QString::number(date.month());
        case MonthFormat::TwoDigitMonth:
            return QStringLiteral("%1").arg(date.month(), 2, 10, QLatin1Char('0'));
        }
        break;
    case DateField::Day:
        switch (m_dayFormat) {
        case DayFormat::DayInteger:
            return QString::number(date.day());
        case DayFormat::TwoDigitDay:
            return QStringLiteral("%1").arg(date.day(), 2, 10, QLatin1Char('0'));
        case DayFormat::DayIntegerWithAbbreviatedWeekday:
            return QStringLiteral("%1 (%2)")
                .arg(date.day())
                .arg(locale().dayName(date.dayOfWeek(), QLocale::ShortFormat));
        }
        break;
    case DateField::Year:
        switch (m_yearFormat) {
        case YearFormat::FullYear:
            return QString::number(date.year());
        case YearFormat::TwoDigitYear:
            return QStringLiteral("%1").arg(date.year() % 100, 2, 10, QLatin1Char('0'));
        }
        break;
    }
    return QString();
}

QDate DatePicker::normalizeDate(int year, int month, int day) const
{
    return clampDate(dateWithClampedDay(year, month, day));
}

QDate DatePicker::clampDate(const QDate& date) const
{
    QDate result = date.isValid() ? date : QDate::currentDate();
    if (m_minimumDate.isValid() && result < m_minimumDate)
        result = m_minimumDate;
    if (m_maximumDate.isValid() && result > m_maximumDate)
        result = m_maximumDate;
    return result;
}

QDate DatePicker::shiftedDate(const QDate& date, DateField field, int offset) const
{
    const QDate base = date.isValid() ? date : clampDate(QDate::currentDate());
    if (offset == 0)
        return base;

    switch (field) {
    case DateField::Month: {
        int month = (base.month() - 1 + offset) % 12;
        if (month < 0)
            month += 12;
        return dateWithClampedDay(base.year(), month + 1, base.day());
    }
    case DateField::Day: {
        const int daysInMonth = QDate(base.year(), base.month(), 1).daysInMonth();
        const int day = detail::wrappedPickerValue(base.day() + offset, 1, daysInMonth);
        return dateWithClampedDay(base.year(), base.month(), day);
    }
    case DateField::Year:
        return dateWithClampedDay(base.year() + offset, base.month(), base.day());
    }
    return base;
}

void DatePicker::setDropDownOpen(bool open)
{
    if (m_dropDownOpen == open)
        return;
    m_dropDownOpen = open;
    detail::notifyPickerRootPopupChanged(this);
    QPointer<DatePicker> guard(this);
    emit dropDownOpenChanged(m_dropDownOpen);
    if (guard)
        guard->update();
}

void DatePicker::applyPendingDate(const QDate& date)
{
    setSelectedDate(date);
}

void DatePicker::handleFlyoutClosed()
{
    setDropDownOpen(false);
}

} // namespace fluent::date_time
