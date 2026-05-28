#include "CalendarDatePicker.h"

#include <QEvent>
#include <QKeyEvent>
#include <QSizePolicy>

#include "CalendarView.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "view/dialogs_flyouts/Flyout.h"

namespace view::date_time {

namespace {

constexpr int kButtonMinWidth = 120;
constexpr int kCalendarShadowMargin = ::Spacing::Standard;
constexpr int kCalendarContentInset = ::Spacing::XSmall;
constexpr int kCalendarWindowMargin = 4;

QDate firstOfMonth(const QDate& date)
{
    if (!date.isValid())
        return QDate();
    return QDate(date.year(), date.month(), 1);
}

QDate todayMonth()
{
    return firstOfMonth(QDate::currentDate());
}

Qt::DayOfWeek normalizeDayOfWeek(Qt::DayOfWeek day)
{
    if (day < Qt::Monday || day > Qt::Sunday)
        return QLocale().firstDayOfWeek();
    return day;
}

QDate boundedMonthForRange(const QDate& month, const QDate& minDate, const QDate& maxDate)
{
    QDate visible = firstOfMonth(month.isValid() ? month : QDate::currentDate());
    if (!visible.isValid())
        visible = todayMonth();

    if (minDate.isValid()) {
        const QDate minMonth = firstOfMonth(minDate);
        if (visible.daysInMonth() > 0 && visible.addDays(visible.daysInMonth() - 1) < minMonth)
            visible = minMonth;
    }
    if (maxDate.isValid()) {
        const QDate maxMonth = firstOfMonth(maxDate);
        if (visible > maxMonth)
            visible = maxMonth;
    }
    return visible;
}

} // namespace

class CalendarDatePickerPopup : public view::dialogs_flyouts::Flyout {
public:
    explicit CalendarDatePickerPopup(CalendarDatePicker* picker)
        : Flyout(picker)
        , m_picker(picker)
        , m_calendarView(new CalendarView(this))
    {
        setObjectName(QStringLiteral("CalendarDatePickerPopup"));
        setAnimationEnabled(false);
        setPlacement(view::dialogs_flyouts::Flyout::Auto);
        setAnchorOffset(::Spacing::XSmall);
        setModal(false);
        setDim(false);
        setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

        m_calendarView->setObjectName(QStringLiteral("CalendarDatePickerCalendarView"));
        m_calendarView->setFrameVisible(false);
        connect(m_calendarView, &CalendarView::dateActivated,
                m_picker, &CalendarDatePicker::selectDateFromCalendar);

        layoutCalendarView();
    }

    CalendarView* calendarView() const { return m_calendarView; }

    void showForPicker()
    {
        if (!m_picker)
            return;

        setAnchor(m_picker);
        layoutCalendarView();
        updateFromPicker(true);

        if (isOpen() || isVisible()) {
            move(computePosition());
            show();
            raise();
        } else {
            showAt(m_picker);
        }
        m_calendarView->setFocus(Qt::PopupFocusReason);
    }

    void updateFromPicker(bool resetMonth)
    {
        if (!m_picker)
            return;

        const QDate month = resetMonth ? m_picker->defaultVisibleMonth() : m_calendarView->visibleMonth();
        m_calendarView->setDateRange(m_picker->minDate(), m_picker->maxDate());
        m_calendarView->setFirstDayOfWeek(m_picker->firstDayOfWeek());
        m_calendarView->setSelectedDate(m_picker->date());
        m_calendarView->setVisibleMonth(month);
    }

    void onThemeUpdated() override
    {
        Flyout::onThemeUpdated();
        if (m_calendarView)
            m_calendarView->update();
    }

protected:
    QPoint computePosition() const override
    {
        if (!m_picker || !m_picker->window())
            return Flyout::computePosition();

        QWidget* top = m_picker->window();
        const QRect anchor(m_picker->mapTo(top, QPoint(0, 0)), m_picker->size());
        const int cardW = width() - kCalendarShadowMargin * 2;
        const int cardH = height() - kCalendarShadowMargin * 2;

        const int spaceBelow = top->height() - anchor.bottom();
        const int spaceAbove = anchor.top();
        int y = anchor.bottom() + anchorOffset();
        if (spaceBelow < cardH + anchorOffset() && spaceAbove > spaceBelow)
            y = anchor.top() - anchorOffset() - cardH;

        int x = anchor.left();
        const int maxX = qMax(kCalendarWindowMargin, top->width() - cardW - kCalendarWindowMargin);
        const int maxY = qMax(kCalendarWindowMargin, top->height() - cardH - kCalendarWindowMargin);
        x = qBound(kCalendarWindowMargin, x, maxX);
        y = qBound(kCalendarWindowMargin, y, maxY);
        return QPoint(x, y) - QPoint(kCalendarShadowMargin, kCalendarShadowMargin);
    }

private:
    void layoutCalendarView()
    {
        const QSize calendarSize = m_calendarView->sizeHint();
        const int chromeInset = kCalendarShadowMargin + kCalendarContentInset;
        setFixedSize(calendarSize.width() + chromeInset * 2,
                     calendarSize.height() + chromeInset * 2);
        m_calendarView->setGeometry(QRect(QPoint(chromeInset, chromeInset), calendarSize));
    }

    CalendarDatePicker* m_picker = nullptr;
    CalendarView* m_calendarView = nullptr;
};

CalendarDatePicker::CalendarDatePicker(QWidget* parent)
    : view::basicinput::Button(parent)
{
    setObjectName(QStringLiteral("CalendarDatePicker"));
    setFocusPolicy(Qt::TabFocus);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setFluentStyle(view::basicinput::Button::Standard);
    setFluentSize(view::basicinput::Button::StandardSize);
    setFluentLayout(view::basicinput::Button::IconAfter);
    setIconGlyph(Typography::Icons::Calendar, 13);
    setMinimumWidth(kButtonMinWidth);
    refreshButtonText();

    connect(this, &view::basicinput::Button::clicked, this, [this] {
        if (isCalendarOpen())
            closeCalendar();
        else
            openCalendar();
    });
}

CalendarDatePicker::~CalendarDatePicker()
{
    if (m_popup) {
        m_popup->close();
        delete m_popup;
        m_popup = nullptr;
    }
}

void CalendarDatePicker::setPlaceholderText(const QString& text)
{
    if (m_placeholderText == text)
        return;
    m_placeholderText = text;
    refreshButtonText();
    emit placeholderTextChanged(m_placeholderText);
}

void CalendarDatePicker::setDate(const QDate& date)
{
    const QDate normalized = normalizeDateForRange(date);
    if (m_date == normalized)
        return;

    m_date = normalized;
    if (m_popup)
        m_popup->updateFromPicker(false);
    refreshButtonText();
    emit dateChanged(m_date);
}

void CalendarDatePicker::setMinDate(const QDate& date)
{
    if (m_minDate == date)
        return;

    m_minDate = date;
    bool maxAdjusted = false;
    if (m_minDate.isValid() && m_maxDate.isValid() && m_maxDate < m_minDate) {
        m_maxDate = m_minDate;
        maxAdjusted = true;
    }

    enforceSelectedDateInRange();
    if (m_popup)
        m_popup->updateFromPicker(false);
    emit minDateChanged(m_minDate);
    if (maxAdjusted)
        emit maxDateChanged(m_maxDate);
}

void CalendarDatePicker::setMaxDate(const QDate& date)
{
    if (m_maxDate == date)
        return;

    m_maxDate = date;
    bool minAdjusted = false;
    if (m_minDate.isValid() && m_maxDate.isValid() && m_minDate > m_maxDate) {
        m_minDate = m_maxDate;
        minAdjusted = true;
    }

    enforceSelectedDateInRange();
    if (m_popup)
        m_popup->updateFromPicker(false);
    if (minAdjusted)
        emit minDateChanged(m_minDate);
    emit maxDateChanged(m_maxDate);
}

void CalendarDatePicker::setDisplayFormat(const QString& format)
{
    if (m_displayFormat == format)
        return;
    m_displayFormat = format;
    refreshButtonText();
    emit displayFormatChanged(m_displayFormat);
}

void CalendarDatePicker::setFirstDayOfWeek(Qt::DayOfWeek day)
{
    const Qt::DayOfWeek normalized = normalizeDayOfWeek(day);
    if (m_firstDayOfWeek == normalized)
        return;
    m_firstDayOfWeek = normalized;
    if (m_popup)
        m_popup->updateFromPicker(false);
    emit firstDayOfWeekChanged(m_firstDayOfWeek);
}

void CalendarDatePicker::setCalendarOpen(bool open)
{
    if (open)
        openCalendar();
    else
        closeCalendar();
}

QString CalendarDatePicker::displayText() const
{
    if (!m_date.isValid())
        return m_placeholderText;
    if (!m_displayFormat.isEmpty())
        return m_date.toString(m_displayFormat);
    return QLocale().toString(m_date, QLocale::ShortFormat);
}

QDate CalendarDatePicker::visibleMonth() const
{
    if (m_popup && m_popup->calendarView())
        return m_popup->calendarView()->visibleMonth();
    return defaultVisibleMonth();
}

CalendarView* CalendarDatePicker::calendarView() const
{
    if (!m_popup)
        return nullptr;
    return m_popup->calendarView();
}

bool CalendarDatePicker::isDateSelectable(const QDate& date) const
{
    if (!date.isValid())
        return false;
    if (m_minDate.isValid() && date < m_minDate)
        return false;
    if (m_maxDate.isValid() && date > m_maxDate)
        return false;
    return true;
}

QSize CalendarDatePicker::sizeHint() const
{
    QSize size = view::basicinput::Button::sizeHint();
    size.setWidth(qMax(size.width(), kButtonMinWidth));
    return size;
}

QSize CalendarDatePicker::minimumSizeHint() const
{
    return sizeHint();
}

void CalendarDatePicker::openCalendar()
{
    if (!isEnabled())
        return;
    ensurePopup();
    m_popup->showForPicker();
}

void CalendarDatePicker::closeCalendar()
{
    if (m_popup)
        m_popup->close();
}

void CalendarDatePicker::clearDate()
{
    setDate(QDate());
}

void CalendarDatePicker::changeEvent(QEvent* event)
{
    view::basicinput::Button::changeEvent(event);
    if (event->type() == QEvent::EnabledChange && !isEnabled())
        closeCalendar();
}

void CalendarDatePicker::keyPressEvent(QKeyEvent* event)
{
    if (!isEnabled()) {
        view::basicinput::Button::keyPressEvent(event);
        return;
    }

    switch (event->key()) {
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Down:
        openCalendar();
        event->accept();
        return;
    case Qt::Key_Escape:
        if (isCalendarOpen()) {
            closeCalendar();
            event->accept();
            return;
        }
        break;
    default:
        break;
    }
    view::basicinput::Button::keyPressEvent(event);
}

void CalendarDatePicker::onThemeUpdated()
{
    view::basicinput::Button::onThemeUpdated();
    if (m_popup)
        m_popup->onThemeUpdated();
}

QDate CalendarDatePicker::normalizeDateForRange(const QDate& date) const
{
    if (!date.isValid())
        return QDate();
    if (m_minDate.isValid() && date < m_minDate)
        return m_minDate;
    if (m_maxDate.isValid() && date > m_maxDate)
        return m_maxDate;
    return date;
}

void CalendarDatePicker::enforceSelectedDateInRange()
{
    if (!m_date.isValid())
        return;
    const QDate normalized = normalizeDateForRange(m_date);
    if (normalized == m_date)
        return;
    m_date = normalized;
    refreshButtonText();
    emit dateChanged(m_date);
}

void CalendarDatePicker::ensurePopup()
{
    if (m_popup)
        return;

    m_popup = new CalendarDatePickerPopup(this);
    connect(m_popup, &view::dialogs_flyouts::Popup::isOpenChanged,
            this, &CalendarDatePicker::handlePopupOpenChanged);
    connect(m_popup, &view::dialogs_flyouts::Popup::closed, this, [this]() {
        handlePopupOpenChanged(false);
    });
}

void CalendarDatePicker::handlePopupOpenChanged(bool open)
{
    if (open)
        clearFocus();
    if (m_calendarOpen == open)
        return;
    m_calendarOpen = open;
    emit calendarOpenChanged(m_calendarOpen);
}

void CalendarDatePicker::selectDateFromCalendar(const QDate& date)
{
    if (!isDateSelectable(date))
        return;
    setDate(date);
    if (m_popup && m_popup->calendarView())
        m_popup->calendarView()->setVisibleMonth(firstOfMonth(date));
    closeCalendar();
}

QDate CalendarDatePicker::defaultVisibleMonth() const
{
    if (m_date.isValid())
        return boundedMonthForRange(firstOfMonth(m_date), m_minDate, m_maxDate);
    return boundedMonthForRange(QDate::currentDate(), m_minDate, m_maxDate);
}

void CalendarDatePicker::refreshButtonText()
{
    setText(displayText());
    setAccessibleName(displayText());
    updateGeometry();
    update();
}

} // namespace view::date_time
