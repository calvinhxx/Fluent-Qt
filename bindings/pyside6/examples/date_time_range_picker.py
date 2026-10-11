"""Run a nullable order-time filter using the public DateTimeRangePicker."""

import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QDate, QDateTime, QTime, Qt
from PySide6.QtWidgets import QApplication, QVBoxLayout


def main():
    app = QApplication(sys.argv)
    fluentqt.initialize_resources()
    window = fluentqt.Window()
    window.setWindowTitle("Order time filter")
    window.resize(820, 700)
    layout = QVBoxLayout(window.contentHost())
    layout.setContentsMargins(24, 24, 24, 24)
    layout.setSpacing(16)
    picker = fluentqt.DateTimeRangePicker(window)
    picker.setAccessibleName("Order time filter")
    picker.setStartLabelText("Start")
    picker.setEndLabelText("End")
    picker.startDatePicker().setPlaceholderText("No lower bound")
    picker.endDatePicker().setPlaceholderText("No upper bound")
    for time_picker in (picker.startTimePicker(), picker.endTimePicker()):
        time_picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Hour, "hh")
        time_picker.setPlaceholderText(fluentqt.TimePicker.TimeField.Minute, "mm")
    picker.setRange(QDateTime(QDate(2025, 10, 1), QTime(10, 10)),
                    QDateTime(QDate(2025, 10, 3), QTime(15, 10)))
    date_only = fluentqt.CheckBox("Dates only", window)
    date_only.toggled.connect(lambda checked: picker.setTimeEnabled(not checked))
    summary = fluentqt.Label(window)
    summary.setWordWrap(True)

    def show_bounds(*_):
        if not picker.isRangeValid():
            summary.setText("Fix the range before applying the query.")
            return
        start, end = picker.startDateTime(), picker.endDateTime()
        clauses = []
        if start.isValid():
            clauses.append("Start: " + start.toString(Qt.DateFormat.ISODate))
        if end.isValid():
            clauses.append("End: " + end.toString(Qt.DateFormat.ISODate))
        summary.setText(" · ".join(clauses) or "No date/time filter")

    picker.rangeChanged.connect(show_bounds)
    show_bounds()
    layout.addWidget(picker)
    layout.addWidget(date_only)
    layout.addWidget(summary)
    layout.addStretch()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
