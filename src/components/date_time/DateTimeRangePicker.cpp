#include "compatibility/InputEventCompat.h"
#include "compatibility/private/DateTimeCompat_p.h"
#include "DateTimeRangePicker.h"

#include <QBoxLayout>
#include <QAccessibleWidget>
#include <QEvent>
#include <QGridLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QStringList>

#include "CalendarDatePicker.h"
#include "TimePicker.h"
#include "CalendarView.h"
#include "components/textfields/LineEdit.h"
#include "components/dialogs_flyouts/Flyout.h"
#include "components/scrolling/ScrollView.h"
#include "components/foundation/overlay/OverlayPresentation_p.h"
#include "components/basicinput/Button.h"
#include "components/layout/Field.h"
#include "private/DateTimeRangePickerAccessibility_p.h"
#include "private/DateTimeRangePickerText_p.h"

namespace fluent::date_time {
namespace {

bool sameDateTime(const QDateTime& left, const QDateTime& right)
{
    if (!left.isValid() || !right.isValid())
        return left.isValid() == right.isValid();
    if (left != right || left.date() != right.date() || left.time() != right.time()
        || left.timeSpec() != right.timeSpec())
        return false;
    return ::compatibility::detail::dateTimeZonesEqual(left, right);
}

} // namespace

namespace {

QStringList presetLabels(const QLocale& locale)
{
    return {detail::dateTimeRangeText(locale, QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "Today")),
        detail::dateTimeRangeText(locale, QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "Yesterday")),
        detail::dateTimeRangeText(locale, QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "Past week")),
        detail::dateTimeRangeText(locale, QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "Past month")),
        detail::dateTimeRangeText(locale,
            QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "Past six months")),
        detail::dateTimeRangeText(locale, QT_TRANSLATE_NOOP("fluent::date_time::DateTimeRangePicker", "Past year"))};
}

// Reserve a trailing action inside the existing Fluent button surface.
// zh_CN: 在现有 Fluent 按钮表面内为尾部清除操作预留空间。
class RangeDateInput final : public CalendarDatePicker {
public:
    explicit RangeDateInput(QWidget* parent) : CalendarDatePicker(parent)
    {
        m_clear = new basicinput::Button(this);
        m_clear->setIconGlyph(Typography::Icons::Cancel, Typography::IconSize::Compact);
        m_clear->setFluentStyle(basicinput::Button::Subtle);
        m_clear->setFixedSize(28, 28);
        setMinimumHeight(::Spacing::ControlHeight::Standard);
        connect(this, &CalendarDatePicker::calendarOpenChanged, this, [this](bool open) {
            setInteractionState(open ? basicinput::Button::Hover : basicinput::Button::Rest);
        });
    }

    basicinput::Button* clearButton() const { return m_clear; }
    QSize sizeHint() const override
    {
        auto size = CalendarDatePicker::sizeHint();
        size.rwidth() += actionWidth();
        size.setHeight(qMax(size.height(), ::Spacing::ControlHeight::Standard));
        return size;
    }

protected:
    QRectF contentPaintRect(const QRectF& surface) const override
    {
        // Keep date text and the chevron stationary across press and popup states.
        // zh_CN: 按压与弹层开合期间固定日期文字及箭头位置。
        QRectF content = surface;
        content.moveTop(rect().top());
        if (m_clear->isHidden())
            return content;
        return layoutDirection() == Qt::RightToLeft
            ? content.adjusted(actionWidth(), 0, 0, 0)
            : content.adjusted(0, 0, -actionWidth(), 0);
    }
    void resizeEvent(QResizeEvent* event) override
    {
        CalendarDatePicker::resizeEvent(event);
        positionClearButton();
    }
    void changeEvent(QEvent* event) override
    {
        CalendarDatePicker::changeEvent(event);
        if (m_clear && event->type() == QEvent::LayoutDirectionChange)
            positionClearButton();
    }

private:
    int actionWidth() const { return m_clear->width() + ::Spacing::Gap::Tight; }
    void positionClearButton()
    {
        const int inset = 2;
        m_clear->move(layoutDirection() == Qt::RightToLeft ? inset : width() - m_clear->width() - inset,
            (height() - m_clear->height() + 1) / 2);
    }
    basicinput::Button* m_clear = nullptr;
};

// The range popup edits time in place; the public TimePicker value API remains shared.
// zh_CN: 范围弹层内直接编辑时间，继续复用 TimePicker 的值与格式接口。
void ensureRangeTimeAccessibility();

class RangeTimeInput final : public TimePicker {
public:
    explicit RangeTimeInput(QWidget* parent) : TimePicker(parent)
    {
        m_edit = new textfields::LineEdit(this);
        m_edit->setClearButtonEnabled(false);
        m_edit->setAlignment(Qt::AlignCenter);
        m_edit->setToolTip(DateTimeRangePicker::tr("Type a time, or use Up and Down to adjust it"));
        m_edit->installEventFilter(this);
        setFocusProxy(m_edit);
        ensureRangeTimeAccessibility();
        disconnect(this, &basicinput::Button::clicked, this, &TimePicker::openPicker);
        connect(this, &basicinput::Button::clicked, m_edit, [this] { m_edit->setFocus(Qt::OtherFocusReason); });
        connect(m_edit, &QLineEdit::editingFinished, this, [this] { commit(); });
        connect(this, &TimePicker::selectedTimeChanged, this, [this] { refresh(); });
        connect(this, &TimePicker::clockIdentifierChanged, this, [this] { refresh(); });
        connect(this, &TimePicker::placeholderTextChanged, this, [this] { refresh(); });
    }

    void refreshLabels()
    {
        m_edit->setObjectName(objectName() + QStringLiteral("Edit"));
        m_edit->setAccessibleName(accessibleName());
        m_edit->setPlaceholderText(
            placeholderText(TimeField::Hour) + QStringLiteral(":") + placeholderText(TimeField::Minute));
    }

    void refresh()
    {
        refreshLabels();
        const QString text = selectedTime().isValid() ? locale().toString(selectedTime(), format()) : QString();
        if (m_edit->text() != text)
            m_edit->setText(text);
    }

    QSize sizeHint() const override
    {
        return QSize(120, qMax(m_edit->sizeHint().height(), ::Spacing::ControlHeight::Standard));
    }
    QSize minimumSizeHint() const override { return QSize(92, sizeHint().height()); }

    void commit()
    {
        if (!isEnabled() || !isVisible())
            return;
        const QTime value = locale().toTime(m_edit->text().trimmed(), format());
        const QPointer<RangeTimeInput> guard(this);
        if (value.isValid())
            setSelectedTime(value);
        if (guard)
            refresh();
    }

protected:
    void paintEvent(QPaintEvent*) override {}
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_F4 || event->key() == Qt::Key_Down || event->key() == Qt::Key_Return
            || event->key() == Qt::Key_Enter) {
            m_edit->setFocus(Qt::OtherFocusReason);
            event->accept();
            return;
        }
        basicinput::Button::keyPressEvent(event);
    }
    void changeEvent(QEvent* event) override
    {
        TimePicker::changeEvent(event);
        if (m_edit && event->type() == QEvent::LocaleChange)
            refresh();
    }
    void resizeEvent(QResizeEvent* event) override
    {
        TimePicker::resizeEvent(event);
        m_edit->setGeometry(rect());
    }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_edit && event->type() == QEvent::KeyPress) {
            auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                commit();
                return true;
            }
            if ((key->key() == Qt::Key_Up || key->key() == Qt::Key_Down) && selectedTime().isValid()) {
                const int position = m_edit->cursorPosition();
                const int step = position <= m_edit->text().indexOf(QLatin1Char(':')) ? 3600 : minuteIncrement() * 60;
                const QPointer<RangeTimeInput> guard(this);
                setSelectedTime(selectedTime().addSecs(key->key() == Qt::Key_Up ? step : -step));
                if (guard)
                    m_edit->setCursorPosition(position);
                return true;
            }
        }
        return TimePicker::eventFilter(watched, event);
    }

private:
    QString format() const
    {
        return clockIdentifier() == ClockIdentifier::TwentyFourHourClock
            ? QStringLiteral("HH:mm")
            : QStringLiteral("h:mm AP");
    }
    textfields::LineEdit* m_edit = nullptr;
};

void ensureRangeTimeAccessibility()
{
#if QT_CONFIG(accessibility)
    static const bool installed = [] {
        QAccessible::installFactory([](const QString&, QObject* object) -> QAccessibleInterface* {
            if (auto* input = dynamic_cast<RangeTimeInput*>(object))
                return new QAccessibleWidget(input, QAccessible::Grouping);
            return nullptr;
        });
        return true;
    }();
    Q_UNUSED(installed);
#endif
}

void refreshLayoutHints(QLayout* layout)
{
    if (!layout)
        return;
    for (int index = 0; index < layout->count(); ++index) {
        auto* item = layout->itemAt(index);
        if (auto* widget = item->widget()) {
            refreshLayoutHints(widget->layout());
            widget->updateGeometry();
        } else {
            refreshLayoutHints(item->layout());
        }
    }
    layout->invalidate();
}

} // namespace

class DateTimeRangePickerPopup final : public dialogs_flyouts::Flyout {
public:
    explicit DateTimeRangePickerPopup(DateTimeRangePicker* picker) : Flyout(picker), m_picker(picker)
    {
        setObjectName(QStringLiteral("DateTimeRangePickerPopup"));
        setAnimationEnabled(false);
        setPlacement(Auto);
        setAnchorOffset(4);
        setModal(false);
        setDim(false);
        setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));
        auto* outer = new QVBoxLayout(this);
        outer->setSizeConstraint(QLayout::SetNoConstraint);
        outer->setContentsMargins(12, 12, 12, 12);
        outer->setSpacing(12);
        m_scroll = new scrolling::ScrollView(this);
        m_scroll->setObjectName(QStringLiteral("rangePopupScroll"));
        m_scroll->setWidgetResizable(true);
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_scroll->setHorizontalScrollBarVisibility(scrolling::ScrollView::ScrollBarVisibility::Disabled);
        m_body = new QWidget;
        m_bodyLayout = new QVBoxLayout(m_body);
        m_bodyLayout->setContentsMargins(0, 0, 0, 0);
        m_bodyLayout->setSpacing(12);
        m_bodyLayout->setSizeConstraint(QLayout::SetNoConstraint);
        m_presets = new QWidget(m_body);
        m_presets->setObjectName(QStringLiteral("rangePresets"));
        m_presetsLayout = new QGridLayout(m_presets);
        m_presetsLayout->setContentsMargins(0, 0, 0, 0);
        m_presetsLayout->setSpacing(4);
        const auto labels = presetLabels(picker->locale());
        for (int index = 0; index < labels.size(); ++index) {
            auto* button = new basicinput::Button(labels.at(index), m_presets);
            button->setObjectName(QStringLiteral("rangePreset%1").arg(index));
            button->setFluentStyle(basicinput::Button::Subtle);
            button->setFluentSize(basicinput::Button::Small);
            button->setFontRole(Typography::FontRole::Caption);
            button->setMinimumHeight(28);
            m_presetButtons.append(button);
            m_presetsLayout->addWidget(button, index / 3, index % 3);
            QObject::connect(button, &basicinput::Button::clicked, picker, [picker, index] {
                picker->requestPreset(static_cast<DateTimeRangePicker::Preset>(index));
            });
        }
        m_bodyLayout->addWidget(m_presets);
        m_main = new QWidget(m_body);
        auto* mainLayout = new QVBoxLayout(m_main);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(12);
        mainLayout->setSizeConstraint(QLayout::SetNoConstraint);
        m_endpointField = new layout::Field(m_main);
        m_endpointField->setObjectName(QStringLiteral("rangeEndpointField"));
        m_endpointField->setAccessibleName(DateTimeRangePicker::tr("Range bounds"));
        m_inputs = new QWidget(m_endpointField);
        m_inputs->setObjectName(QStringLiteral("rangeEndpointInputs"));
        m_inputLayout = new QBoxLayout(QBoxLayout::LeftToRight, m_inputs);
        m_inputLayout->setContentsMargins(0, 0, 0, 0);
        m_inputLayout->setSpacing(12);
        m_inputLayout->setSizeConstraint(QLayout::SetNoConstraint);
        m_endpointField->setEditor(m_inputs, WidgetOwnership::Owned);
        mainLayout->addWidget(m_endpointField);
        m_calendar = new CalendarView(m_main);
        m_calendar->setObjectName(QStringLiteral("rangeCalendar"));
        m_calendar->setAccessibleName(DateTimeRangePicker::tr("Date range calendar"));
        m_calendar->setAccessibleDescription(
            DateTimeRangePicker::tr("Choose a start date, then an end date. Either endpoint can be cleared."));
        m_calendar->setFrameVisible(false);
        m_calendar->setMinimumWidth(0);
        m_calendar->setMaximumWidth(QWIDGETSIZE_MAX);
        m_calendar->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        mainLayout->addWidget(m_calendar);
        m_bodyLayout->addWidget(m_main);
        m_scroll->setWidget(m_body);
        m_body->setAutoFillBackground(false);
        outer->addWidget(m_scroll, 1);
        auto* footer = new QHBoxLayout;
        auto* clear = new basicinput::Button(DateTimeRangePicker::tr("Clear all"), this);
        clear->setObjectName(QStringLiteral("rangeClearAll"));
        clear->setFluentStyle(basicinput::Button::Subtle);
        footer->addWidget(clear);
        addLocalizedText(clear, "text", "Clear all");
        footer->addStretch();
        auto* done = new basicinput::Button(DateTimeRangePicker::tr("Done"), this);
        done->setObjectName(QStringLiteral("rangeDone"));
        done->setFluentStyle(basicinput::Button::Accent);
        footer->addWidget(done);
        addLocalizedText(done, "text", "Done");
        addLocalizedText(m_endpointField, "accessibleName", "Range bounds");
        addLocalizedText(m_calendar, "accessibleName", "Date range calendar");
        outer->addLayout(footer);
        QObject::connect(clear, &basicinput::Button::clicked, picker, &DateTimeRangePicker::clear);
        QObject::connect(done, &basicinput::Button::clicked, picker, &DateTimeRangePicker::closePicker);
        QObject::connect(m_calendar, &CalendarView::dateActivated, picker, &DateTimeRangePicker::selectCalendarDate);
    }

    QWidget* inputHost() const { return m_inputs; }
    CalendarView* calendar() const { return m_calendar; }
    void addInput(QWidget* input) { m_inputLayout->addWidget(input, 1, Qt::AlignBottom); }
    void addLocalizedText(QObject* object, const char* property, const char* source)
    {
        m_localizedTexts.append({object, property, source, object->property(property).toString()});
    }
    bool refreshLanguage()
    {
        const QPointer<DateTimeRangePickerPopup> guard(this);
        const auto labels = presetLabels(m_picker->locale());
        for (int index = 0; index < m_presetButtons.size(); ++index) {
            m_presetButtons.at(index)->setText(labels.at(index));
            if (!guard)
                return false;
        }
        for (auto& text : m_localizedTexts) {
            if (!text.object)
                continue;
            // Refresh defaults without replacing application-authored text.
            // zh_CN: 刷新默认文案，保留应用自行设置的文字。
            const bool usesDefault = text.followsLocale
                && text.object->property(text.property).toString() == text.previous;
            text.followsLocale = usesDefault;
            const QString next = detail::dateTimeRangeText(m_picker->locale(), text.source);
            text.previous = next;
            if (usesDefault)
                text.object->setProperty(text.property, next);
            if (!guard)
                return false;
        }
        return true;
    }
    void setValidationMessage(const QString& message)
    {
        m_endpointField->setValidationState(
            message.isEmpty() ? layout::Field::ValidationState::None : layout::Field::ValidationState::Error);
        m_endpointField->setValidationMessage(message);
    }

    void layoutForHost()
    {
        m_scroll->viewport()->setAutoFillBackground(false);
        QWidget* host = overlay::presentedTopLevel(m_picker);
        const int availableWidth = host ? qMax(240, host->width() - 16) : 440;
        const int availableHeight = host ? qMax(180, host->height() - 16) : 620;
        m_presets->setVisible(m_picker->isPresetsVisible());
        const int popupWidth = qMin(440, availableWidth);
        const int mainWidth = popupWidth - 56;
        const int columns = mainWidth >= 340 ? 3 : 2;
        if (columns != m_presetColumns) {
            QList<QWidget*> buttons;
            while (auto* item = m_presetsLayout->takeAt(0)) {
                buttons.append(item->widget());
                delete item;
            }
            for (int index = 0; index < buttons.size(); ++index)
                m_presetsLayout->addWidget(buttons.at(index), index / columns, index % columns);
            m_presetColumns = columns;
        }
        const int inputMinimum = m_picker->m_startField->minimumSizeHint().width()
            + m_picker->m_endField->minimumSizeHint().width() + 12;
        m_inputLayout->setDirection(mainWidth >= inputMinimum ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
        const int controlHeight =
            qMax(qMax(m_picker->m_startDatePicker->sizeHint().height(), m_picker->m_endDatePicker->sizeHint().height()),
                qMax(m_picker->m_startTimePicker->sizeHint().height(), m_picker->m_endTimePicker->sizeHint().height()));
        m_picker->m_startDatePicker->setFixedHeight(controlHeight);
        m_picker->m_endDatePicker->setFixedHeight(controlHeight);
        m_picker->m_startTimePicker->setFixedHeight(controlHeight);
        m_picker->m_endTimePicker->setFixedHeight(controlHeight);
        // Validation and time rows can disappear before their layout requests are delivered.
        // zh_CN: 校验提示与时间行隐藏后，立即刷新父布局的尺寸缓存。
        refreshLayoutHints(m_bodyLayout);
        const int bodyHeight = m_bodyLayout->hasHeightForWidth()
            ? m_bodyLayout->heightForWidth(mainWidth)
            : m_bodyLayout->sizeHint().height();
        m_body->resize(mainWidth, bodyHeight);
        m_bodyLayout->activate();
        const QMargins margins = layout()->contentsMargins();
        const QMargins shadow = contentsMargins();
        const int chrome = margins.top() + margins.bottom() + shadow.top() + shadow.bottom() + layout()->spacing()
            + layout()->itemAt(1)->layout()->sizeHint().height() + 2 * m_scroll->frameWidth();
        const int height = bodyHeight + chrome;
        setFixedSize(popupWidth, qMin(height, availableHeight));
        layout()->activate();
    }

    void showForPicker()
    {
        setAnchor(m_picker->entryButton());
        layoutForHost();
        const QPointer<DateTimeRangePickerPopup> guard(this);
        showAt(m_picker->entryButton());
        if (guard) {
            layoutForHost();
            move(computePosition());
            m_calendar->setFocus(Qt::PopupFocusReason);
        }
    }

protected:
    bool event(QEvent* event) override
    {
        const bool syncStack = event && event->type() == QEvent::ZOrderChange;
        const QPointer<DateTimeRangePickerPopup> guard(this);
        const bool handled = Flyout::event(event);
        if (guard && syncStack && isOpen() && !m_syncingChildStack) {
            m_syncingChildStack = true;
            for (const auto& child : childFlyouts()) {
                if (child && child->isVisible())
                    child->raise();
                if (!guard)
                    return handled;
            }
            m_syncingChildStack = false;
        }
        return handled;
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        QWidget* host = overlay::presentedTopLevel(m_picker);
        if (isOpen() && host && event) {
            if (watched == host && event->type() == QEvent::Resize) {
                layoutForHost();
                move(computePosition());
            }
            if (event->type() == QEvent::MouseButtonPress) {
                const QPoint global = fluentMouseGlobalPos(static_cast<QMouseEvent*>(event));
                if (watched != this && anchor()
                    && anchor()->rect().contains(overlay::localPointFromPresentedGlobal(anchor(), global)))
                    return false;
                for (const auto& flyout : childFlyouts()) {
                    if (flyout && overlay::visibleCardContains(flyout->rect(), flyout->mapFromGlobal(global)))
                        return false;
                }
            }
        }
        return Flyout::eventFilter(watched, event);
    }

private:
    QList<QPointer<dialogs_flyouts::Flyout>> childFlyouts() const
    {
        QList<QPointer<dialogs_flyouts::Flyout>> children;
        QWidget* host = overlay::presentedTopLevel(m_picker);
        if (!host)
            return children;
        const auto flyouts = host->findChildren<dialogs_flyouts::Flyout*>();
        for (auto* flyout : flyouts) {
            if (flyout != this && flyout->isOpen() && flyout->anchor() && isAncestorOf(flyout->anchor()))
                children.append(flyout);
        }
        return children;
    }

    DateTimeRangePicker* m_picker;
    struct LocalizedText {
        QPointer<QObject> object;
        const char* property;
        const char* source;
        QString previous;
        bool followsLocale = true;
    };
    QList<LocalizedText> m_localizedTexts;
    QList<basicinput::Button*> m_presetButtons;
    bool m_syncingChildStack = false;
    scrolling::ScrollView* m_scroll = nullptr;
    QWidget* m_body = nullptr;
    QVBoxLayout* m_bodyLayout = nullptr;
    QWidget* m_presets = nullptr;
    QWidget* m_main = nullptr;
    QGridLayout* m_presetsLayout = nullptr;
    int m_presetColumns = 3;
    CalendarView* m_calendar = nullptr;
    QWidget* m_inputs = nullptr;
    layout::Field* m_endpointField = nullptr;
    QBoxLayout* m_inputLayout = nullptr;
};

DateTimeRangePicker::DateTimeRangePicker(QWidget* parent) : QWidget(parent), m_observedLocale(QWidget::locale())
{
    detail::ensureDateTimeRangePickerAccessibilityFactory();
    setObjectName(QStringLiteral("DateTimeRangePicker"));
    setAccessibleName(tr("Date range"));
    setFocusPolicy(Qt::StrongFocus);
    auto policy = QSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
    m_invalidRangeText = tr("End must not precede start");
    m_placeholderText = tr("Select date range");
    m_layout = new QVBoxLayout(this);
    m_layout->setSizeConstraint(QLayout::SetNoConstraint);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_summaryField = new layout::Field(this);
    m_summaryField->setObjectName(QStringLiteral("rangeSummaryField"));
    m_entryButton = new basicinput::Button(this);
    m_entryButton->setObjectName(QStringLiteral("rangeEntry"));
    m_entryButton->setIconGlyph(Typography::Icons::Calendar, Typography::IconSize::Standard);
    m_entryButton->setFluentLayout(basicinput::Button::IconAfter);
    m_entryButton->setAccessibleName(tr("Choose date range"));
    m_entryButton->setMinimumWidth(240);
    m_entryButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_entryButton->installEventFilter(this);
    m_summaryField->setEditor(m_entryButton, WidgetOwnership::Owned);
    m_layout->addWidget(m_summaryField);
    m_popup = new DateTimeRangePickerPopup(this);
    m_popup->addLocalizedText(this, "accessibleName", "Date range");
    m_popup->addLocalizedText(this, "placeholderText", "Select date range");
    m_popup->addLocalizedText(this, "invalidRangeText", "End must not precede start");
    m_popup->addLocalizedText(m_entryButton, "accessibleName", "Choose date range");

    for (bool start : {true, false}) {
        const QString prefix = start ? QStringLiteral("rangeStart") : QStringLiteral("rangeEnd");
        auto* field = new layout::Field(m_popup->inputHost());
        field->setObjectName(prefix + QStringLiteral("Field"));
        auto* editor = new QWidget(field);
        auto* stack = new QVBoxLayout(editor);
        stack->setContentsMargins(0, 0, 0, 0);
        stack->setSpacing(themeSpacing().gap.normal);
        auto* datePicker = new RangeDateInput(editor);
        datePicker->m_compactPopup = true;
        datePicker->setIconGlyph(Typography::Icons::ChevronDown, Typography::IconSize::Compact);
        datePicker->setObjectName(prefix + QStringLiteral("Date"));
        datePicker->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        datePicker->setAccessibleName(start ? tr("Start date") : tr("End date"));
        datePicker->setPlaceholderText(start ? tr("Start date") : tr("End date"));
        auto* timePicker = new RangeTimeInput(editor);
        timePicker->m_allowDayEnd = !start;
        timePicker->setObjectName(prefix + QStringLiteral("Time"));
        timePicker->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        timePicker->setMinimumWidth(92);
        timePicker->setClockIdentifier(TimePicker::ClockIdentifier::TwentyFourHourClock);
        timePicker->setPlaceholderText(TimePicker::TimeField::Hour, QStringLiteral("hh"));
        timePicker->setPlaceholderText(TimePicker::TimeField::Minute, QStringLiteral("mm"));
        timePicker->setAccessibleName(start ? tr("Start time") : tr("End time"));
        timePicker->setConfirmButtonAccessibleName(tr("Accept time"));
        timePicker->setCancelButtonAccessibleName(tr("Cancel time"));
        auto* clearButton = datePicker->clearButton();
        clearButton->setObjectName(prefix + QStringLiteral("Clear"));
        clearButton->setAccessibleName(start ? tr("Clear start") : tr("Clear end"));
        clearButton->setToolTip(clearButton->accessibleName());
        const char* dateLabel = start ? "Start date" : "End date";
        m_popup->addLocalizedText(datePicker, "accessibleName", dateLabel);
        m_popup->addLocalizedText(datePicker, "placeholderText", dateLabel);
        m_popup->addLocalizedText(timePicker, "accessibleName", start ? "Start time" : "End time");
        m_popup->addLocalizedText(timePicker, "confirmButtonAccessibleName", "Accept time");
        m_popup->addLocalizedText(timePicker, "cancelButtonAccessibleName", "Cancel time");
        m_popup->addLocalizedText(clearButton, "accessibleName", start ? "Clear start" : "Clear end");
        m_popup->addLocalizedText(clearButton, "toolTip", start ? "Clear start" : "Clear end");
        m_popup->addLocalizedText(timePicker->findChild<QLineEdit*>(), "toolTip",
            "Type a time, or use Up and Down to adjust it");
        stack->addWidget(datePicker);
        stack->addWidget(timePicker);
        field->setEditor(editor, WidgetOwnership::Owned);
        m_popup->addInput(field);
        if (start) {
            m_startField = field;
            m_startDatePicker = datePicker;
            m_startTimePicker = timePicker;
            m_startClearButton = clearButton;
        } else {
            m_endField = field;
            m_endDatePicker = datePicker;
            m_endTimePicker = timePicker;
            m_endClearButton = clearButton;
        }
        connect(datePicker, &CalendarDatePicker::dateChanged, this, [this, start](const QDate& date) {
            const QPointer<DateTimeRangePicker> guard(this);
            updateDate(date, start);
            if (guard && isPickerOpen() && date.isValid()) {
                calendarView()->setSelectedDate(date);
                calendarView()->setVisibleMonth(QDate(date.year(), date.month(), 1));
            }
        });
        connect(timePicker, &TimePicker::selectedTimeChanged, this, [this, start](const QTime& time) {
            updateTime(time, start);
        });
        connect(clearButton, &basicinput::Button::clicked, this, [this, start, datePicker] {
            const QPointer<DateTimeRangePicker> guard(this);
            datePicker->closeCalendar();
            if (guard)
                start ? clearStart() : clearEnd();
        });
        connect(datePicker, &CalendarDatePicker::calendarOpenChanged, this,
            [this, start, datePicker, prefix](bool open) {
                if (open) {
                    const QPointer<DateTimeRangePicker> guard(this);
                    closePickers(datePicker);
                    if (!guard)
                        return;
                    activateEndpoint(start);
                    if (guard && datePicker->calendarView())
                        datePicker->calendarView()->setObjectName(prefix + QStringLiteral("Calendar"));
                } else {
                    synchronizeInputs();
                }
            });
        connect(timePicker, &TimePicker::dropDownOpenChanged, this, [this, timePicker](bool open) {
            if (open)
                closePickers(timePicker);
        });
        const auto boundsChanged = [this] { setRange(m_startDateTime, m_endDateTime); };
        connect(datePicker, &CalendarDatePicker::minDateChanged, this, boundsChanged);
        connect(datePicker, &CalendarDatePicker::maxDateChanged, this, boundsChanged);
        connect(timePicker, &TimePicker::minuteIncrementChanged, this, boundsChanged);
        connect(datePicker, &CalendarDatePicker::displayFormatChanged, this, [this] { refreshEntry(); });
        connect(timePicker, &TimePicker::clockIdentifierChanged, this, [this] { refreshEntry(); });
    }
    setFocusProxy(m_entryButton);
    connect(m_entryButton, &basicinput::Button::clicked, this, [this] { setPickerOpen(!isPickerOpen()); });
    connect(m_popup, &dialogs_flyouts::Popup::isOpenChanged, this, [this](bool open) {
        const QPointer<DateTimeRangePicker> guard(this);
        if (!open)
            closePickers();
        if (!guard)
            return;
        detail::notifyDateTimeRangePickerOpenChanged(this);
        if (guard)
            emit pickerOpenChanged(open);
    });
    QWidget::setTabOrder(m_startDatePicker, m_startClearButton);
    QWidget::setTabOrder(m_startClearButton, m_startTimePicker);
    QWidget::setTabOrder(m_startTimePicker, m_endDatePicker);
    QWidget::setTabOrder(m_endDatePicker, m_endClearButton);
    QWidget::setTabOrder(m_endClearButton, m_endTimePicker);
    m_popup->refreshLanguage();
    synchronizeInputs();
}

DateTimeRangePicker::~DateTimeRangePicker()
{
    if (m_popup) {
        QObject::disconnect(m_popup, nullptr, this, nullptr);
        closePickers();
        m_popup->close();
        delete m_popup;
    }
}

bool DateTimeRangePicker::isRangeValid() const
{
    return !m_startDateTime.isValid() || !m_endDateTime.isValid() || m_startDateTime <= m_endDateTime;
}

QString DateTimeRangePicker::startLabelText() const
{
    return m_startField->labelText();
}
QString DateTimeRangePicker::endLabelText() const
{
    return m_endField->labelText();
}

QDateTime DateTimeRangePicker::normalizedDateTime(const QDateTime& value, bool start) const
{
    if (!value.isValid())
        return QDateTime();
    const auto* datePicker = start ? m_startDatePicker : m_endDatePicker;
    const auto* timePicker = start ? m_startTimePicker : m_endTimePicker;
    QDateTime result = value;
    QDate date = value.date();
    if (datePicker->minDate().isValid() && date < datePicker->minDate())
        date = datePicker->minDate();
    if (datePicker->maxDate().isValid() && date > datePicker->maxDate())
        date = datePicker->maxDate();
    result.setDate(date);
    result.setTime(m_timeEnabled ? timePicker->normalizeTime(value.time()) : QTime(0, 0));
    return result.isValid() ? result : QDateTime();
}

void DateTimeRangePicker::setRange(const QDateTime& start, const QDateTime& end)
{
    const QDateTime nextStart = normalizedDateTime(start, true);
    const QDateTime nextEnd = normalizedDateTime(end, false);
    const bool startChanged = !sameDateTime(m_startDateTime, nextStart);
    const bool endChanged = !sameDateTime(m_endDateTime, nextEnd);
    if (!startChanged && !endChanged) {
        synchronizeInputs();
        return;
    }
    const bool previousValidity = isRangeValid();
    m_startDateTime = nextStart;
    m_endDateTime = nextEnd;
    synchronizeInputs();
    const QPointer<DateTimeRangePicker> guard(this);
    const auto current = [&] {
        return guard && sameDateTime(m_startDateTime, nextStart) && sameDateTime(m_endDateTime, nextEnd);
    };
    if (startChanged) {
        emit startDateTimeChanged(nextStart);
        if (!current())
            return;
    }
    if (endChanged) {
        emit endDateTimeChanged(nextEnd);
        if (!current())
            return;
    }
    if (previousValidity != isRangeValid()) {
        emit rangeValidityChanged(isRangeValid());
        if (!current())
            return;
    }
    detail::notifyDateTimeRangePickerValueChanged(this, previousValidity != isRangeValid());
    if (current())
        emit rangeChanged(nextStart, nextEnd);
}

void DateTimeRangePicker::setStartDateTime(const QDateTime& value)
{
    setRange(value, m_endDateTime);
}
void DateTimeRangePicker::setEndDateTime(const QDateTime& value)
{
    setRange(m_startDateTime, value);
}
void DateTimeRangePicker::clearStart()
{
    m_editingStart = true;
    m_completingRange = false;
    setStartDateTime(QDateTime());
}
void DateTimeRangePicker::clearEnd()
{
    m_editingStart = false;
    m_completingRange = false;
    setEndDateTime(QDateTime());
}
void DateTimeRangePicker::clear()
{
    m_editingStart = true;
    m_completingRange = false;
    setRange(QDateTime(), QDateTime());
}

void DateTimeRangePicker::updateDate(const QDate& date, bool start)
{
    QDateTime value = start ? m_startDateTime : m_endDateTime;
    if (!date.isValid()) {
        value = QDateTime();
    } else if (value.isValid()) {
        value.setDate(date);
    } else {
        value = QDateTime(date, QTime(0, 0));
    }
    start ? setStartDateTime(value) : setEndDateTime(value);
}

void DateTimeRangePicker::updateTime(const QTime& time, bool start)
{
    QDateTime value = start ? m_startDateTime : m_endDateTime;
    if (!m_timeEnabled || !value.isValid()) {
        synchronizeInputs();
        return;
    }
    value.setTime(time.isValid() ? time : QTime(0, 0));
    start ? setStartDateTime(value) : setEndDateTime(value);
}

void DateTimeRangePicker::synchronizeInputs()
{

    const QSignalBlocker startDate(m_startDatePicker);
    const QSignalBlocker endDate(m_endDatePicker);
    const QSignalBlocker startTime(m_startTimePicker);
    const QSignalBlocker endTime(m_endTimePicker);
    m_startDatePicker->setDate(m_startDateTime.date());
    m_endDatePicker->setDate(m_endDateTime.date());
    m_startTimePicker->setSelectedTime(m_startDateTime.time());
    m_endTimePicker->setSelectedTime(m_endDateTime.time());
    static_cast<RangeTimeInput*>(m_startTimePicker)->refresh();
    static_cast<RangeTimeInput*>(m_endTimePicker)->refresh();
    const auto* activeDate = m_editingStart ? m_startDatePicker : m_endDatePicker;
    calendarView()->setDateRange(activeDate->minDate(), activeDate->maxDate());
    m_startTimePicker->setVisible(m_timeEnabled);
    m_endTimePicker->setVisible(m_timeEnabled);
    m_startTimePicker->setEnabled(m_timeEnabled && m_startDateTime.isValid());
    m_endTimePicker->setEnabled(m_timeEnabled && m_endDateTime.isValid());
    m_startClearButton->setEnabled(m_startDateTime.isValid());
    m_endClearButton->setEnabled(m_endDateTime.isValid());
    m_startClearButton->setVisible(m_startDateTime.isValid());
    m_endClearButton->setVisible(m_endDateTime.isValid());
    calendarView()->setHighlightedDateRange(m_startDateTime.date(), m_endDateTime.date());
    refreshPresentationText();
}

void DateTimeRangePicker::refreshPresentationText()
{
    static_cast<RangeTimeInput*>(m_startTimePicker)->refreshLabels();
    static_cast<RangeTimeInput*>(m_endTimePicker)->refreshLabels();
    calendarView()->setAccessibleDescription(detail::dateTimeRangeText(locale(),
        m_editingStart ? QT_TR_NOOP("Choose the start date") : QT_TR_NOOP("Choose the end date")));
    m_popup->setValidationMessage(isRangeValid() ? QString() : m_invalidRangeText);
    m_summaryField->setValidationState(
        isRangeValid() ? layout::Field::ValidationState::None : layout::Field::ValidationState::Error);
    m_summaryField->setValidationMessage(isRangeValid() ? QString() : m_invalidRangeText);

    refreshEntry();
    updateLayoutDirection();
    updateGeometry();
}

void DateTimeRangePicker::setTimeEnabled(bool enabled)
{
    if (m_timeEnabled == enabled)
        return;
    m_timeEnabled = enabled;
    if (!enabled) {
        m_startTimePicker->closePicker();
        m_endTimePicker->closePicker();
    }
    const QPointer<DateTimeRangePicker> guard(this);
    setRange(m_startDateTime, m_endDateTime);
    if (guard)
        emit timeEnabledChanged(m_timeEnabled);
}

void DateTimeRangePicker::setStartLabelText(const QString& text)
{
    if (startLabelText() == text)
        return;
    m_startField->setLabelText(text);
    updateLayoutDirection();
    updateGeometry();
    emit startLabelTextChanged(text);
}

void DateTimeRangePicker::setEndLabelText(const QString& text)
{
    if (endLabelText() == text)
        return;
    m_endField->setLabelText(text);
    updateLayoutDirection();
    updateGeometry();
    emit endLabelTextChanged(text);
}

void DateTimeRangePicker::setInvalidRangeText(const QString& text)
{
    if (m_invalidRangeText == text)
        return;
    m_invalidRangeText = text;
    refreshPresentationText();
    if (!isRangeValid())
        detail::notifyDateTimeRangePickerDescriptionChanged(this);
    emit invalidRangeTextChanged(text);
}

void DateTimeRangePicker::setLocale(const QLocale& locale)
{
    QWidget::setLocale(locale);
}

void DateTimeRangePicker::closePickers(QWidget* except)
{
    const QPointer<DateTimeRangePicker> guard(this);
    if (m_startDatePicker != except)
        m_startDatePicker->closeCalendar();
    if (!guard)
        return;
    if (m_endDatePicker != except)
        m_endDatePicker->closeCalendar();
    if (!guard)
        return;
    if (m_startTimePicker != except)
        m_startTimePicker->closePicker();
    if (!guard)
        return;
    if (m_endTimePicker != except)
        m_endTimePicker->closePicker();
}

int DateTimeRangePicker::horizontalWidth() const
{
    return qBound(260, m_entryButton->sizeHint().width(), 520);
}

QSize DateTimeRangePicker::sizeHint() const
{
    return QSize(horizontalWidth(), m_summaryField->sizeHint().height());
}

QSize DateTimeRangePicker::minimumSizeHint() const
{
    return QSize(240, m_summaryField->minimumSizeHint().height());
}

int DateTimeRangePicker::heightForWidth(int width) const
{
    return qMax(m_summaryField->sizeHint().height(), m_summaryField->heightForWidth(width));
}

void DateTimeRangePicker::updateLayoutDirection()
{
    if (m_popup && isPickerOpen())
        m_popup->layoutForHost();
}

QString DateTimeRangePicker::displayText() const
{
    const auto text = [this](const QDateTime& value, CalendarDatePicker* datePicker, TimePicker* timePicker) {
        QString result = datePicker->displayText();
        if (m_timeEnabled) {
            const auto format = timePicker->clockIdentifier() == TimePicker::ClockIdentifier::TwentyFourHourClock
                ? QStringLiteral("HH:mm")
                : QStringLiteral("h:mm AP");
            result += QStringLiteral(" ") + locale().toString(value.time(), format);
        }
        return result;
    };
    if (!m_startDateTime.isValid() && !m_endDateTime.isValid())
        return m_placeholderText;
    if (!m_startDateTime.isValid())
        return detail::dateTimeRangeText(locale(), QT_TR_NOOP("Until %1"))
            .arg(text(m_endDateTime, m_endDatePicker, m_endTimePicker));
    if (!m_endDateTime.isValid())
        return detail::dateTimeRangeText(locale(), QT_TR_NOOP("From %1"))
            .arg(text(m_startDateTime, m_startDatePicker, m_startTimePicker));
    return tr("%1 – %2").arg(text(m_startDateTime, m_startDatePicker, m_startTimePicker),
        text(m_endDateTime, m_endDatePicker, m_endTimePicker));
}

void DateTimeRangePicker::refreshEntry()
{
    m_entryButton->setText(displayText());
    m_entryButton->setToolTip(displayText());
    updateGeometry();
}

CalendarView* DateTimeRangePicker::calendarView() const
{
    return m_popup ? m_popup->calendar() : nullptr;
}

bool DateTimeRangePicker::isPickerOpen() const
{
    return m_popup && m_popup->isOpen();
}

void DateTimeRangePicker::setPickerOpen(bool open)
{
    open ? openPicker() : closePicker();
}

void DateTimeRangePicker::openPicker()
{
    if (!isEnabled() || !isVisible() || !m_popup || isPickerOpen())
        return;
    // The popup is reparented to the overlay host, which can have another locale.
    // zh_CN: 弹窗会重挂到可能使用其他区域设置的浮层宿主，保留范围控件的区域设置。
    m_popup->setLocale(locale());
    m_editingStart = !m_startDateTime.isValid() || m_endDateTime.isValid();
    m_completingRange = !m_endDateTime.isValid();
    synchronizeInputs();
    const QDate date = m_startDateTime.isValid() ? m_startDateTime.date()
        : m_endDateTime.isValid()
        ? m_endDateTime.date()
        : QDate::currentDate();
    calendarView()->setLocale(locale());
    calendarView()->resetScroll();
    calendarView()->setSelectedDate(date);
    calendarView()->setContentLevel(CalendarView::CalendarContentLevel::Day);
    calendarView()->setVisibleMonth(QDate(date.year(), date.month(), 1));
    calendarView()->refreshProperties();
    calendarView()->update();
    m_popup->showForPicker();
}

void DateTimeRangePicker::closePicker()
{
    const QPointer<DateTimeRangePicker> guard(this);
    if (isPickerOpen()) {
        static_cast<RangeTimeInput*>(m_startTimePicker)->commit();
        if (!guard)
            return;
        static_cast<RangeTimeInput*>(m_endTimePicker)->commit();
        if (!guard)
            return;
    }
    closePickers();
    if (guard && m_popup)
        m_popup->close();
}

void DateTimeRangePicker::setPlaceholderText(const QString& text)
{
    if (m_placeholderText == text)
        return;
    m_placeholderText = text;
    refreshEntry();
    emit placeholderTextChanged(text);
}

void DateTimeRangePicker::setPresetsVisible(bool visible)
{
    if (m_presetsVisible == visible)
        return;
    m_presetsVisible = visible;
    updateLayoutDirection();
    emit presetsVisibleChanged(visible);
}

void DateTimeRangePicker::setAutoApplyPresetsEnabled(bool enabled)
{
    if (m_autoApplyPresetsEnabled == enabled)
        return;
    m_autoApplyPresetsEnabled = enabled;
    emit autoApplyPresetsEnabledChanged(enabled);
}

void DateTimeRangePicker::requestPreset(Preset preset)
{
    const QPointer<DateTimeRangePicker> guard(this);
    emit presetRequested(preset);
    if (guard && m_autoApplyPresetsEnabled)
        applyPreset(preset);
}

void DateTimeRangePicker::applyPreset(Preset preset)
{
    applyPreset(preset, QDateTime::currentDateTime());
}

void DateTimeRangePicker::applyPreset(Preset preset, const QDateTime& reference)
{
    if (!reference.isValid())
        return;
    QDate end = reference.date();
    QDate start = end;
    switch (preset) {
    case Preset::Today:
        break;
    case Preset::Yesterday:
        start = end = end.addDays(-1);
        break;
    case Preset::PastWeek:
        start = end.addDays(-6);
        break;
    case Preset::PastMonth:
        start = end.addMonths(-1).addDays(1);
        break;
    case Preset::PastSixMonths:
        start = end.addMonths(-6).addDays(1);
        break;
    case Preset::PastYear:
        start = end.addYears(-1).addDays(1);
        break;
    default:
        return;
    }
    QDateTime beginning = reference;
    QDateTime ending = reference;
    beginning.setDate(start);
    beginning.setTime(QTime(0, 0));
    ending.setDate(end);
    ending.setTime(m_timeEnabled ? QTime(23, 59) : QTime(0, 0));
    m_editingStart = true;
    m_completingRange = false;
    const QPointer<DateTimeRangePicker> guard(this);
    setRange(beginning, ending);
    if (guard && isPickerOpen())
        calendarView()->setVisibleMonth(QDate(end.year(), end.month(), 1));
}

void DateTimeRangePicker::activateEndpoint(bool start)
{
    m_editingStart = start;
    m_completingRange = false;
    synchronizeInputs();
}

void DateTimeRangePicker::selectCalendarDate(const QDate& date)
{
    const auto* input = m_editingStart ? m_startDatePicker : m_endDatePicker;
    if (!input->isDateSelectable(date))
        return;
    const bool advance = m_editingStart;
    const bool complete = !m_endDateTime.isValid();
    const QPointer<DateTimeRangePicker> guard(this);
    if (!m_editingStart && m_completingRange && m_startDateTime.isValid() && date < m_startDateTime.date()) {
        auto start = m_startDateTime;
        start.setDate(date);
        setRange(start, m_startDateTime);
    } else {
        updateDate(date, m_editingStart);
    }
    if (!guard)
        return;
    if (advance) {
        m_editingStart = false;
        m_completingRange = complete;
        synchronizeInputs();
    } else {
        m_completingRange = false;
    }
}

bool DateTimeRangePicker::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_entryButton && event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter || key->key() == Qt::Key_F4
            || (key->key() == Qt::Key_Down && key->modifiers().testFlag(Qt::AltModifier))) {
            openPicker();
            event->accept();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void DateTimeRangePicker::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayoutDirection();
}

void DateTimeRangePicker::hideEvent(QHideEvent* event)
{
    closePicker();
    QWidget::hideEvent(event);
}

void DateTimeRangePicker::changeEvent(QEvent* event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::EnabledChange && !isEnabled())
        closePicker();
    if (event->type() == QEvent::LocaleChange && m_observedLocale != QWidget::locale()) {
        m_observedLocale = QWidget::locale();
        m_popup->setLocale(m_observedLocale);
        calendarView()->setLocale(m_observedLocale);
        m_startDatePicker->setLocale(m_observedLocale);
        m_endDatePicker->setLocale(m_observedLocale);
        m_startTimePicker->setLocale(m_observedLocale);
        m_endTimePicker->setLocale(m_observedLocale);
        if (!m_popup->refreshLanguage())
            return;
        synchronizeInputs();
        emit localeChanged(m_observedLocale);
    } else if (event->type() == QEvent::LanguageChange && m_popup) {
        if (m_popup->refreshLanguage())
            refreshPresentationText();
    }
}

void DateTimeRangePicker::onThemeUpdated()
{
    if (!m_layout)
        return;
    m_layout->setSpacing(themeSpacing().gap.normal);
    refreshEntry();
    updateLayoutDirection();
    updateGeometry();
}

} // namespace fluent::date_time
