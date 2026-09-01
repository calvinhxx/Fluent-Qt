#include "TimePicker.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QTime>
#include <QtMath>

#include "components/basicinput/Button.h"
#include "components/date_time/private/PickerAccessibility_p.h"
#include "components/date_time/private/PickerFlyoutGeometry_p.h"
#include "components/date_time/private/PickerWheel_p.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "design/Spacing.h"

namespace fluent::date_time {

namespace {
constexpr int kTimePickerThemeMinWidth = 242;
constexpr int kSegmentHPadding = 12;
constexpr int kPopupShadowMargin = ::Spacing::Standard;
constexpr int kDividerWidth = 1;
constexpr int kColumnBaseWidth = 80;

int timeFieldBaseWidth(TimePicker::TimeField field)
{
    switch (field) {
    case TimePicker::TimeField::Hour:
    case TimePicker::TimeField::Minute:
    case TimePicker::TimeField::Period:
        return kColumnBaseWidth;
    }
    return kColumnBaseWidth;
}

int clampMinuteIncrement(int increment)
{
    return qBound(1, increment, 59);
}

bool isPm(const QTime& time)
{
    return time.hour() >= 12;
}

int displayHour12(int hour)
{
    const int value = hour % 12;
    return value == 0 ? 12 : value;
}

int hourFromDisplay12(int displayHour, bool pm)
{
    displayHour = qBound(1, displayHour, 12);
    if (displayHour == 12)
        return pm ? 12 : 0;
    return pm ? displayHour + 12 : displayHour;
}
} // namespace

class TimePickerFlyout;

class TimePickerColumn : public detail::PickerWheelColumn {
public:
    TimePickerColumn(TimePickerFlyout* flyout, TimePicker::TimeField field,
                     QWidget* parent = nullptr);

    TimePicker::TimeField field() const { return m_field; }

    QString pickerColumnName() const override;
    QString pickerColumnValueText() const override;
    QVariant pickerColumnCurrentValue() const override;
    QVariant pickerColumnMinimumValue() const override;
    QVariant pickerColumnMaximumValue() const override;
    QVariant pickerColumnStepSize() const override;
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
    int visibleItemCountProperty() const override;
    bool refreshPropertiesOnFocus() const override { return true; }

private:
    TimePickerFlyout* m_flyout = nullptr;
    TimePicker::TimeField m_field;
};

class TimePickerFlyout : public fluent::dialogs_flyouts::Flyout {
public:
    explicit TimePickerFlyout(TimePicker* owner);

    TimePicker* owner() const { return m_owner; }
    QTime pendingTime() const { return m_pendingTime; }

    void showForPicker();
    void refreshLayout();
    void setPendingTime(const QTime& time);
    QTime shifted(TimePicker::TimeField field, int offset) const;
    bool canShift(TimePicker::TimeField field, int offset) const;
    Qt::Alignment textAlignment(TimePicker::TimeField field) const;
    QString displayText(TimePicker::TimeField field, const QTime& time) const;
    bool isFirstVisibleField(TimePicker::TimeField field) const;
    bool isLastVisibleField(TimePicker::TimeField field) const;
    void shiftField(TimePicker::TimeField field, int offset);
    void commit();
    void cancel();
    void refreshActionAccessibility();

    void onThemeUpdated() override;

protected:
    QPoint computePosition() const override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    QVector<TimePicker::TimeField> visibleFields() const;
    int preferredColumnWidth(TimePicker::TimeField field) const;
    void notifyColumnValueChanges(const QTime& before, const QTime& after);
    void updateColumns();

    TimePicker* m_owner = nullptr;
    QTime m_pendingTime;
    detail::PickerWheelPanel* m_panel = nullptr;
    TimePickerColumn* m_hourColumn = nullptr;
    TimePickerColumn* m_minuteColumn = nullptr;
    TimePickerColumn* m_periodColumn = nullptr;
};

TimePickerColumn::TimePickerColumn(TimePickerFlyout* flyout, TimePicker::TimeField field,
                                   QWidget* parent)
    : detail::PickerWheelColumn(82, parent), m_flyout(flyout), m_field(field)
{
    refreshColumnProperties();
}

QString TimePickerColumn::pickerColumnName() const
{
    switch (m_field) {
    case TimePicker::TimeField::Hour:
        return QCoreApplication::translate("PickerAccessibility", "Hour");
    case TimePicker::TimeField::Minute:
        return QCoreApplication::translate("PickerAccessibility", "Minute");
    case TimePicker::TimeField::Period:
        return QCoreApplication::translate("PickerAccessibility", "Period");
    }
    return {};
}

QString TimePickerColumn::pickerColumnValueText() const
{
    return m_flyout ? m_flyout->displayText(m_field, m_flyout->pendingTime()) : QString();
}

QVariant TimePickerColumn::pickerColumnCurrentValue() const
{
    const QTime value = m_flyout ? m_flyout->pendingTime() : QTime();
    if (!value.isValid())
        return {};
    switch (m_field) {
    case TimePicker::TimeField::Hour:
        if (m_flyout->owner()->clockIdentifier() ==
            TimePicker::ClockIdentifier::TwentyFourHourClock) {
            return value.hour();
        }
        return displayHour12(value.hour());
    case TimePicker::TimeField::Minute:
        return value.minute();
    case TimePicker::TimeField::Period:
        return isPm(value) ? 1 : 0;
    }
    return {};
}

QVariant TimePickerColumn::pickerColumnMinimumValue() const
{
    if (m_field == TimePicker::TimeField::Hour && m_flyout &&
        m_flyout->owner()->clockIdentifier() == TimePicker::ClockIdentifier::TwentyFourHourClock) {
        return 0;
    }
    if (m_field == TimePicker::TimeField::Period)
        return 0;
    return 1;
}

QVariant TimePickerColumn::pickerColumnMaximumValue() const
{
    if (m_field == TimePicker::TimeField::Period)
        return 1;
    if (m_field == TimePicker::TimeField::Hour && m_flyout &&
        m_flyout->owner()->clockIdentifier() == TimePicker::ClockIdentifier::TwentyFourHourClock) {
        return 23;
    }
    if (m_field == TimePicker::TimeField::Minute && m_flyout) {
        const int increment = m_flyout->owner()->minuteIncrement();
        return (59 / increment) * increment;
    }
    return 12;
}

QVariant TimePickerColumn::pickerColumnStepSize() const
{
    return m_field == TimePicker::TimeField::Minute && m_flyout
               ? QVariant(m_flyout->owner()->minuteIncrement())
               : QVariant(1);
}

void TimePickerColumn::pickerColumnSetValue(const QVariant& value)
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
    if (!ok || requested == current)
        return;

    int offset = requested - current;
    if (m_field == TimePicker::TimeField::Minute) {
        const int increment = m_flyout->owner()->minuteIncrement();
        requested = qBound(0, qRound(static_cast<qreal>(requested) / increment) * increment,
                           pickerColumnMaximumValue().toInt());
        offset = requested / increment - current / increment;
    }
    m_flyout->shiftField(m_field, offset);
}

bool TimePickerColumn::canShiftBy(int offset) const
{
    return m_flyout && m_flyout->canShift(m_field, offset);
}

void TimePickerColumn::shiftBy(int offset)
{
    if (m_flyout)
        m_flyout->shiftField(m_field, offset);
}

void TimePickerColumn::commitPickerValue()
{
    if (m_flyout)
        m_flyout->commit();
}

void TimePickerColumn::cancelPickerValue()
{
    if (m_flyout)
        m_flyout->cancel();
}

QString TimePickerColumn::displayTextForOffset(int offset) const
{
    if (!m_flyout)
        return {};
    return m_flyout->displayText(m_field, m_flyout->shifted(m_field, offset));
}

bool TimePickerColumn::isRowSelectable(int offset) const
{
    return m_flyout && m_flyout->canShift(m_field, offset);
}

bool TimePickerColumn::isRowTextEnabled(int offset) const
{
    return offset == 0 || isRowSelectable(offset);
}

Qt::Alignment TimePickerColumn::columnTextAlignment() const
{
    return m_flyout ? m_flyout->textAlignment(m_field) : Qt::AlignLeft;
}

bool TimePickerColumn::isFirstVisibleColumn() const
{
    return m_flyout && m_flyout->isFirstVisibleField(m_field);
}

bool TimePickerColumn::isLastVisibleColumn() const
{
    return m_flyout && m_flyout->isLastVisibleField(m_field);
}

int TimePickerColumn::visibleItemCountProperty() const
{
    return m_field == TimePicker::TimeField::Period ? 2 : 7;
}

TimePickerFlyout::TimePickerFlyout(TimePicker* owner)
    : fluent::dialogs_flyouts::Flyout(owner), m_owner(owner)
{
    setObjectName(QStringLiteral("TimePickerFlyout"));
    setAnimationEnabled(false);
    setPlacement(fluent::dialogs_flyouts::Flyout::Auto);
    setAnchorOffset(::Spacing::XSmall);
    setModal(false);
    setDim(false);
    setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

    m_panel = new detail::PickerWheelPanel(QStringLiteral("TimePickerFlyoutPanel"),
                                           kTimePickerThemeMinWidth, this);
    m_hourColumn = new TimePickerColumn(this, TimePicker::TimeField::Hour, m_panel);
    m_hourColumn->setObjectName(QStringLiteral("TimePickerHourColumn"));
    m_minuteColumn = new TimePickerColumn(this, TimePicker::TimeField::Minute, m_panel);
    m_minuteColumn->setObjectName(QStringLiteral("TimePickerMinuteColumn"));
    m_periodColumn = new TimePickerColumn(this, TimePicker::TimeField::Period, m_panel);
    m_periodColumn->setObjectName(QStringLiteral("TimePickerPeriodColumn"));
    m_panel->setColumns({m_hourColumn, m_minuteColumn, m_periodColumn});
    m_panel->initializeActions(
        QStringLiteral("TimePickerConfirmButton"), QStringLiteral("TimePickerCancelButton"),
        [this] { commit(); }, [this] { cancel(); });
    refreshActionAccessibility();
    connect(this, &TimePickerFlyout::closed, this, [this] {
        if (m_owner)
            m_owner->handleFlyoutClosed();
    });
}

QPoint TimePickerFlyout::computePosition() const
{
    if (!m_owner || !m_panel || !m_owner->window())
        return fluent::dialogs_flyouts::Flyout::computePosition();

    return detail::alignedWheelFlyoutPosition(m_owner, size(), kPopupShadowMargin,
                                              m_panel->selectedRowCenterY(), clampToWindow());
}

QVector<TimePicker::TimeField> TimePickerFlyout::visibleFields() const
{
    return m_owner ? m_owner->visibleFields() : QVector<TimePicker::TimeField>();
}

int TimePickerFlyout::preferredColumnWidth(TimePicker::TimeField field) const
{
    if (!m_owner)
        return timeFieldBaseWidth(field);
    return m_owner->preferredFieldWidth(field);
}

bool TimePickerFlyout::isFirstVisibleField(TimePicker::TimeField field) const
{
    const auto fields = visibleFields();
    return !fields.isEmpty() && fields.first() == field;
}

bool TimePickerFlyout::isLastVisibleField(TimePicker::TimeField field) const
{
    const auto fields = visibleFields();
    return !fields.isEmpty() && fields.last() == field;
}

void TimePickerFlyout::showForPicker()
{
    if (!m_owner)
        return;

    setPendingTime(m_owner->time());
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

void TimePickerFlyout::refreshLayout()
{
    if (!m_owner)
        return;

    const auto fields = visibleFields();
    m_panel->configureColumns(m_owner->font(),
                              {fields.contains(TimePicker::TimeField::Hour),
                               fields.contains(TimePicker::TimeField::Minute),
                               fields.contains(TimePicker::TimeField::Period)},
                              {preferredColumnWidth(TimePicker::TimeField::Hour),
                               preferredColumnWidth(TimePicker::TimeField::Minute),
                               preferredColumnWidth(TimePicker::TimeField::Period)});

    const QSize cardSize = m_panel->sizeHint();
    const int cardW = cardSize.width();
    const int cardH = cardSize.height();
    setFixedSize(cardW + kPopupShadowMargin * 2, cardH + kPopupShadowMargin * 2);
    m_panel->setGeometry(kPopupShadowMargin, kPopupShadowMargin, cardW, cardH);
    setAnchor(m_owner);

    if (isOpen() || isVisible())
        move(computePosition());
}

void TimePickerFlyout::setPendingTime(const QTime& time)
{
    if (!m_owner)
        return;
    const QTime normalized = m_owner->normalizeTime(time.isValid() ? time : m_owner->time());
    if (m_pendingTime == normalized)
        return;
    const QTime before = m_pendingTime;
    m_pendingTime = normalized;
    updateColumns();
    notifyColumnValueChanges(before, m_pendingTime);
}

QTime TimePickerFlyout::shifted(TimePicker::TimeField field, int offset) const
{
    if (!m_owner)
        return QTime();
    return m_owner->shiftedTime(m_pendingTime, field, offset);
}

bool TimePickerFlyout::canShift(TimePicker::TimeField field, int offset) const
{
    const QTime candidate = shifted(field, offset);
    return candidate.isValid() && candidate != m_pendingTime;
}

Qt::Alignment TimePickerFlyout::textAlignment(TimePicker::TimeField field) const
{
    return m_owner ? m_owner->fieldTextAlignment(field) : Qt::AlignLeft;
}

QString TimePickerFlyout::displayText(TimePicker::TimeField field, const QTime& time) const
{
    return m_owner ? m_owner->formatField(field, time) : QString();
}

void TimePickerFlyout::shiftField(TimePicker::TimeField field, int offset)
{
    if (!m_owner || offset == 0)
        return;
    const QTime next = shifted(field, offset);
    if (!next.isValid() || next == m_pendingTime)
        return;
    const QTime before = m_pendingTime;
    m_pendingTime = next;
    updateColumns();
    notifyColumnValueChanges(before, m_pendingTime);
}

void TimePickerFlyout::commit()
{
    if (m_owner && m_pendingTime.isValid())
        m_owner->applyPendingTime(m_pendingTime);
    close();
}

void TimePickerFlyout::cancel()
{
    close();
}

void TimePickerFlyout::refreshActionAccessibility()
{
    if (!m_panel)
        return;
    const QString confirmOverride = m_owner ? m_owner->confirmButtonAccessibleName() : QString();
    const QString cancelOverride = m_owner ? m_owner->cancelButtonAccessibleName() : QString();
    m_panel->setActionAccessibleNames(
        confirmOverride.isEmpty()
            ? QCoreApplication::translate("PickerAccessibility", "Confirm time")
            : confirmOverride,
        cancelOverride.isEmpty() ? QCoreApplication::translate("PickerAccessibility", "Cancel")
                                 : cancelOverride);
}

void TimePickerFlyout::onThemeUpdated()
{
    fluent::dialogs_flyouts::Flyout::onThemeUpdated();
    if (m_panel)
        m_panel->refreshTheme();
}

void TimePickerFlyout::keyPressEvent(QKeyEvent* event)
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

void TimePickerFlyout::updateColumns()
{
    if (m_panel)
        m_panel->updateColumns();
    update();
}

void TimePickerFlyout::notifyColumnValueChanges(const QTime& before, const QTime& after)
{
    if (!m_panel || before == after)
        return;
    if (before.hour() != after.hour())
        detail::notifyPickerColumnValueChanged(m_hourColumn);
    if (before.minute() != after.minute())
        detail::notifyPickerColumnValueChanged(m_minuteColumn);
    if (isPm(before) != isPm(after))
        detail::notifyPickerColumnValueChanged(m_periodColumn);
}

TimePicker::TimePicker(QWidget* parent) : fluent::basicinput::Button(parent)
{
    detail::ensurePickerAccessibilityFactory();
    m_observedLocale = QWidget::locale();
    m_time = normalizeTime(QTime::currentTime());

    setObjectName(QStringLiteral("TimePicker"));
    setFocusPolicy(Qt::StrongFocus);
    setFluentStyle(fluent::basicinput::Button::Standard);
    setFluentSize(fluent::basicinput::Button::StandardSize);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    connect(this, &fluent::basicinput::Button::clicked, this, &TimePicker::openPicker);
    onThemeUpdated();
}

TimePicker::~TimePicker()
{
    if (m_flyout) {
        TimePickerFlyout* flyout = m_flyout.data();
        m_flyout = nullptr;
        flyout->setAnimationEnabled(false);
        flyout->close();
        delete flyout;
    }
}

void TimePicker::setTime(const QTime& time)
{
    if (!time.isValid())
        return;
    const QTime normalized = normalizeTime(time);
    if (m_time == normalized)
        return;
    m_time = normalized;
    if (!m_selectedTime.isValid())
        emit timeChanged(this->time());
    if (m_flyout && m_flyout->isOpen())
        m_flyout->setPendingTime(m_time);
    update();
}

void TimePicker::setSelectedTime(const QTime& time)
{
    if (!time.isValid()) {
        clearSelectedTime();
        return;
    }

    const QTime normalized = normalizeTime(time);
    const QTime oldTime = this->time();
    const QTime oldSelected = m_selectedTime;

    m_time = normalized;
    m_selectedTime = normalized;

    if (oldSelected != m_selectedTime)
        detail::notifyPickerRootValueChanged(this);

    if (oldSelected != m_selectedTime)
        emit selectedTimeChanged(m_selectedTime);
    if (oldTime != this->time())
        emit timeChanged(this->time());
    update();
}

void TimePicker::clearSelectedTime()
{
    if (!m_selectedTime.isValid())
        return;
    m_selectedTime = QTime();
    detail::notifyPickerRootValueChanged(this);
    emit selectedTimeChanged(m_selectedTime);
    update();
}

void TimePicker::setMinuteIncrement(int increment)
{
    const int normalizedIncrement = clampMinuteIncrement(increment);
    if (m_minuteIncrement == normalizedIncrement)
        return;

    const QTime oldTime = time();
    const QTime oldSelected = m_selectedTime;
    m_minuteIncrement = normalizedIncrement;
    m_time = normalizeTime(m_time);
    if (m_selectedTime.isValid())
        m_selectedTime = normalizeTime(m_selectedTime);

    if (oldSelected != m_selectedTime)
        detail::notifyPickerRootValueChanged(this);
    emit minuteIncrementChanged(m_minuteIncrement);
    if (oldSelected != m_selectedTime)
        emit selectedTimeChanged(m_selectedTime);
    if (oldTime != time())
        emit timeChanged(time());

    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    update();
}

void TimePicker::setClockIdentifier(ClockIdentifier identifier)
{
    if (m_clockIdentifier == identifier)
        return;
    m_clockIdentifier = identifier;
    if (m_selectedTime.isValid())
        detail::notifyPickerRootValueChanged(this);
    if (m_flyout && m_flyout->isOpen())
        m_flyout->showForPicker();
    updateGeometry();
    update();
    emit clockIdentifierChanged(m_clockIdentifier);
}

void TimePicker::setLocale(const QLocale& locale)
{
    if (QWidget::locale() == locale)
        return;
    QWidget::setLocale(locale);
}

void TimePicker::setPlaceholderText(TimeField field, const QString& text)
{
    QString* target = nullptr;
    switch (field) {
    case TimeField::Hour:
        target = &m_hourPlaceholderText;
        break;
    case TimeField::Minute:
        target = &m_minutePlaceholderText;
        break;
    case TimeField::Period:
        target = &m_periodPlaceholderText;
        break;
    }
    if (!target || *target == text)
        return;

    *target = text;
    updateGeometry();
    update();
    emit placeholderTextChanged(field, text);
}

void TimePicker::setConfirmButtonAccessibleName(const QString& name)
{
    if (m_confirmButtonAccessibleName == name)
        return;
    m_confirmButtonAccessibleName = name;
    if (m_flyout)
        m_flyout->refreshActionAccessibility();
    emit confirmButtonAccessibleNameChanged(m_confirmButtonAccessibleName);
}

void TimePicker::setCancelButtonAccessibleName(const QString& name)
{
    if (m_cancelButtonAccessibleName == name)
        return;
    m_cancelButtonAccessibleName = name;
    if (m_flyout)
        m_flyout->refreshActionAccessibility();
    emit cancelButtonAccessibleNameChanged(m_cancelButtonAccessibleName);
}

void TimePicker::openPicker()
{
    if (!isEnabled())
        return;

    QPointer<TimePicker> guard(this);
    if (!m_flyout) {
        setDropDownOpen(false);
        if (!guard)
            return;
        m_flyout = new TimePickerFlyout(this);
    }

    m_flyout->showForPicker();
    if (!guard)
        return;
    guard->setDropDownOpen(true);
    if (guard)
        guard->update();
}

void TimePicker::closePicker()
{
    if (m_flyout)
        m_flyout->cancel();
}

QString TimePicker::fieldDisplayText(TimeField field) const
{
    if (!isFieldVisible(field))
        return QString();
    if (!m_selectedTime.isValid())
        return placeholderText(field);
    return formatField(field, m_selectedTime);
}

QString TimePicker::placeholderText(TimeField field) const
{
    switch (field) {
    case TimeField::Hour:
        return m_hourPlaceholderText;
    case TimeField::Minute:
        return m_minutePlaceholderText;
    case TimeField::Period:
        return m_periodPlaceholderText;
    }
    return QString();
}

int TimePicker::preferredFieldWidth(TimeField field) const
{
    const QFontMetrics metrics(font());
    int textWidth = metrics.horizontalAdvance(placeholderText(field));
    auto includeTime = [this, field, &metrics, &textWidth](const QTime& time) {
        textWidth = qMax(textWidth, metrics.horizontalAdvance(formatField(field, time)));
    };

    switch (field) {
    case TimeField::Hour:
        for (int hour = 0; hour < 24; ++hour)
            includeTime(QTime(hour, 0));
        break;
    case TimeField::Minute:
        for (int minute : minuteValues())
            includeTime(QTime(0, minute));
        break;
    case TimeField::Period:
        includeTime(QTime(0, 0));
        includeTime(QTime(13, 0));
        break;
    }

    return qMax(timeFieldBaseWidth(field), textWidth + kSegmentHPadding * 2);
}

Qt::Alignment TimePicker::fieldTextAlignment(TimeField field) const
{
    switch (field) {
    case TimeField::Hour:
        return m_hourTextAlignment;
    case TimeField::Minute:
        return m_minuteTextAlignment;
    case TimeField::Period:
        return m_periodTextAlignment;
    }
    return Qt::AlignLeft;
}

void TimePicker::setFieldTextAlignment(TimeField field, Qt::Alignment alignment)
{
    Qt::Alignment* target = nullptr;
    switch (field) {
    case TimeField::Hour:
        target = &m_hourTextAlignment;
        break;
    case TimeField::Minute:
        target = &m_minuteTextAlignment;
        break;
    case TimeField::Period:
        target = &m_periodTextAlignment;
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

QSize TimePicker::sizeHint() const
{
    int width = 0;
    for (TimeField field : visibleFields())
        width += preferredFieldWidth(field);
    width += qMax(0, visibleFields().size() - 1) * kDividerWidth;
    width = qMax(width, kTimePickerThemeMinWidth);

    return QSize(width, detail::pickerEntryHeight(font()));
}

QSize TimePicker::minimumSizeHint() const
{
    return QSize(kTimePickerThemeMinWidth, detail::pickerEntryHeight(font()));
}

void TimePicker::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColorsRef();
    const auto radius = themeRadius();
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty())
        return;

    const bool active = isEnabled() && (underMouse() || isDown());
    const QRectF r(surface);

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

        QColor textColor = isEnabled() ? (active ? colors.textPrimary : colors.textSecondary)
                                       : colors.textDisabled;
        painter.setPen(textColor);
        const QString text = fieldDisplayText(segment.field);
        QRect textRect = segment.rect.adjusted(kSegmentHPadding, 0, -kSegmentHPadding, 0);
        painter.drawText(textRect, Qt::AlignVCenter | fieldTextAlignment(segment.field),
                         painter.fontMetrics().elidedText(text, Qt::ElideRight, textRect.width()));
    }
}

void TimePicker::resizeEvent(QResizeEvent* event)
{
    QPushButton::resizeEvent(event);
}

void TimePicker::keyPressEvent(QKeyEvent* event)
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

void TimePicker::changeEvent(QEvent* event)
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
        update();
        if (m_selectedTime.isValid())
            detail::notifyPickerRootValueChanged(this);
        emit localeChanged(m_observedLocale);
    }
    if (event->type() == QEvent::EnabledChange) {
        if (!isEnabled())
            closePicker();
        update();
    }
}

void TimePicker::onThemeUpdated()
{
    fluent::basicinput::Button::onThemeUpdated();
    if (m_flyout)
        m_flyout->onThemeUpdated();
}

QVector<TimePicker::TimeField> TimePicker::visibleFields() const
{
    QVector<TimeField> fields;
    fields.append(TimeField::Hour);
    fields.append(TimeField::Minute);
    if (m_clockIdentifier == ClockIdentifier::TwelveHourClock)
        fields.append(TimeField::Period);
    return fields;
}

bool TimePicker::isFieldVisible(TimeField field) const
{
    return field != TimeField::Period || m_clockIdentifier == ClockIdentifier::TwelveHourClock;
}

QVector<TimePicker::FieldSegment> TimePicker::fieldSegments() const
{
    const QVector<TimeField> fields = visibleFields();
    QVector<FieldSegment> result;
    const QRect surface = fieldSurfaceRect();
    if (surface.isEmpty() || fields.isEmpty())
        return result;

    int totalWeight = 0;
    for (TimeField field : fields)
        totalWeight += preferredFieldWidth(field);

    int x = surface.left();
    int remainingW = surface.width();
    for (int i = 0; i < fields.size(); ++i) {
        const int weight = preferredFieldWidth(fields.at(i));
        int w =
            i == fields.size() - 1
                ? remainingW
                : qMax(32, qRound(double(surface.width()) * double(weight) / double(totalWeight)));
        w = qMin(w, remainingW);
        result.append({fields.at(i), QRect(x, surface.top(), w, surface.height())});
        x += w;
        remainingW -= w;
    }
    return result;
}

QRect TimePicker::fieldSurfaceRect() const
{
    return QRect(0, 0, width(), qMin(height(), detail::pickerEntryHeight(font())));
}

QString TimePicker::formatField(TimeField field, const QTime& time) const
{
    if (!time.isValid())
        return QString();

    switch (field) {
    case TimeField::Hour:
        if (m_clockIdentifier == ClockIdentifier::TwentyFourHourClock)
            return QStringLiteral("%1").arg(time.hour(), 2, 10, QLatin1Char('0'));
        return QString::number(displayHour12(time.hour()));
    case TimeField::Minute:
        return QStringLiteral("%1").arg(time.minute(), 2, 10, QLatin1Char('0'));
    case TimeField::Period:
        return isPm(time) ? locale().pmText() : locale().amText();
    }
    return QString();
}

QTime TimePicker::normalizeTime(const QTime& time) const
{
    const QTime base = time.isValid() ? time : QTime::currentTime();
    return QTime(base.hour(), snappedMinute(base.minute()));
}

QTime TimePicker::shiftedTime(const QTime& time, TimeField field, int offset) const
{
    const QTime base = normalizeTime(time.isValid() ? time : this->time());
    if (offset == 0)
        return base;

    switch (field) {
    case TimeField::Hour: {
        if (m_clockIdentifier == ClockIdentifier::TwentyFourHourClock)
            return QTime(detail::wrappedPickerValue(base.hour() + offset, 0, 23), base.minute());
        const int nextDisplayHour =
            detail::wrappedPickerValue(displayHour12(base.hour()) + offset, 1, 12);
        return QTime(hourFromDisplay12(nextDisplayHour, isPm(base)), base.minute());
    }
    case TimeField::Minute: {
        const QVector<int> values = minuteValues();
        const int current = snappedMinute(base.minute());
        int index = values.indexOf(current);
        if (index < 0)
            index = 0;
        const int nextIndex = detail::wrappedPickerValue(index + offset, 0, values.size() - 1);
        return QTime(base.hour(), values.at(nextIndex));
    }
    case TimeField::Period:
        if (!isPm(base) && offset == 1)
            return QTime(base.hour() + 12, base.minute());
        if (isPm(base) && offset == -1)
            return QTime(base.hour() - 12, base.minute());
        return QTime();
    }
    return base;
}

QVector<int> TimePicker::minuteValues() const
{
    QVector<int> values;
    const int increment = clampMinuteIncrement(m_minuteIncrement);
    for (int minute = 0; minute < 60; minute += increment)
        values.append(minute);
    if (values.isEmpty())
        values.append(0);
    return values;
}

int TimePicker::snappedMinute(int minute) const
{
    minute = qBound(0, minute, 59);
    const QVector<int> values = minuteValues();
    int best = values.first();
    int bestDistance = qAbs(minute - best);
    for (int value : values) {
        const int distance = qAbs(minute - value);
        if (distance < bestDistance || (distance == bestDistance && value > best)) {
            best = value;
            bestDistance = distance;
        }
    }
    return best;
}

void TimePicker::setDropDownOpen(bool open)
{
    if (m_dropDownOpen == open)
        return;
    m_dropDownOpen = open;
    detail::notifyPickerRootPopupChanged(this);
    QPointer<TimePicker> guard(this);
    emit dropDownOpenChanged(m_dropDownOpen);
    if (guard)
        guard->update();
}

void TimePicker::applyPendingTime(const QTime& time)
{
    setSelectedTime(time);
}

void TimePicker::handleFlyoutClosed()
{
    setDropDownOpen(false);
}

} // namespace fluent::date_time
