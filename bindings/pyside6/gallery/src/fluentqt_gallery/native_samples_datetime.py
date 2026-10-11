"""Source-driven date/time range cards matching the native Gallery."""

from .native_samples import register_source_samples


_BASE = '''import fluentqt
from PySide6.QtCore import QDate, QDateTime, QTime

picker = fluentqt.DateTimeRangePicker()
picker.setStartLabelText("Start")
picker.setEndLabelText("End")
'''

register_source_samples(
    "date-time-range-picker",
    ("DateTimeRangePicker",),
    {
        "date-time-range-dates": (
            "picker",
            _BASE + '''picker.setTimeEnabled(False)
picker.setRange(
    QDateTime(QDate(2025, 10, 1), QTime(0, 0)),
    QDateTime(QDate(2025, 11, 1), QTime(0, 0)),
)
''',
        ),
        "date-time-range-time": (
            "picker",
            _BASE + '''picker.setRange(
    QDateTime(QDate(2025, 10, 1), QTime(10, 10)),
    QDateTime(QDate(2025, 10, 3), QTime(15, 10)),
)
''',
        ),
        "date-time-range-open-ended": (
            "picker",
            '''import fluentqt
from PySide6.QtCore import QDate, QDateTime, QTime

picker = fluentqt.DateTimeRangePicker()
picker.setStartLabelText("Since")
picker.setEndLabelText("Until")
picker.startDatePicker().setPlaceholderText("No lower bound")
picker.endDatePicker().setPlaceholderText("No upper bound")
picker.endTimePicker().setPlaceholderText(fluentqt.TimePicker.TimeField.Hour, "hh")
picker.endTimePicker().setPlaceholderText(fluentqt.TimePicker.TimeField.Minute, "mm")
picker.setStartDateTime(QDateTime(QDate(2025, 10, 1), QTime(10, 10)))
picker.rangeChanged.connect(lambda start, end: None)  # Build the query here.
''',
        ),
    },
)
