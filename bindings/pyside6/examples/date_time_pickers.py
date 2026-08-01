"""Interactive and snapshot acceptance example for native date/time pickers."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QDate, QLocale, QTime, QTimer, Qt
from PySide6.QtWidgets import QApplication, QVBoxLayout, QWidget

from fluentqt.date_time import CalendarDatePicker, DatePicker, TimePicker
from fluentqt.layout import Divider
from fluentqt.textfields import Label
from fluentqt.windowing import Window


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt date/time picker example."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Open the calendar popup, save the window as PNG, and exit.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def build_showcase() -> tuple[Window, CalendarDatePicker]:
    window = Window()
    window.setWindowTitle("FluentQt PySide6 Date & Time Pickers")
    window.resize(920, 620)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = Label("Native date and time pickers", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = Label(
        "Python supplies QDate, QTime, locale, enum, and signal values. "
        "The entry surfaces, same-window popups, keyboard interaction, and "
        "painting remain native FluentQt.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(Divider(content))

    locale = QLocale(QLocale.English, QLocale.UnitedStates)

    calendar_label = Label("Calendar date picker", content)
    calendar_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(calendar_label)
    calendar_picker = CalendarDatePicker(content)
    calendar_picker.setLocale(locale)
    calendar_picker.setPlaceholderText("Choose a delivery date")
    calendar_picker.setDateRange(QDate(2026, 5, 1), QDate(2026, 5, 31))
    calendar_picker.setDate(QDate(2026, 5, 21))
    calendar_picker.setFixedWidth(280)
    root.addWidget(calendar_picker, 0, Qt.AlignRight)

    date_label = Label("Segmented date picker", content)
    date_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(date_label)
    date_picker = DatePicker(content)
    date_picker.setLocale(locale)
    date_picker.setDateRange(QDate(2025, 1, 1), QDate(2027, 12, 31))
    date_picker.setSelectedDate(QDate(2026, 7, 21))
    date_picker.setDayFormat(
        DatePicker.DayFormat.DayIntegerWithAbbreviatedWeekday
    )
    date_picker.setFixedWidth(360)
    root.addWidget(date_picker, 0, Qt.AlignLeft)

    time_label = Label("Time picker", content)
    time_label.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(time_label)
    time_picker = TimePicker(content)
    time_picker.setLocale(locale)
    time_picker.setMinuteIncrement(15)
    time_picker.setSelectedTime(QTime(13, 45))
    time_picker.setFixedWidth(280)
    root.addWidget(time_picker, 0, Qt.AlignLeft)

    root.addWidget(Divider(content))
    status = Label("", content)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    status.setWordWrap(True)
    root.addWidget(status)

    def update_status(*_args: object) -> None:
        calendar_text = calendar_picker.date().toString("yyyy-MM-dd")
        date_text = date_picker.selectedDate().toString("yyyy-MM-dd")
        time_text = time_picker.selectedTime().toString("HH:mm")
        status.setText(
            "Calendar: {0}  |  Date: {1}  |  Time: {2}".format(
                calendar_text,
                date_text,
                time_text,
            )
        )

    calendar_picker.dateChanged.connect(update_status)
    date_picker.selectedDateChanged.connect(update_status)
    time_picker.selectedTimeChanged.connect(update_status)
    update_status()

    hint = Label(
        "Activate any picker to inspect its native same-window popup; "
        "Escape and outside press dismiss without committing pending values.",
        content,
    )
    hint.setMaximumWidth(500)
    hint.setWordWrap(True)
    root.addWidget(hint)
    root.addStretch()

    window.setContentWidget(content)
    return window, calendar_picker


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window, calendar_picker = build_showcase()
    window.show()

    if args.snapshot is not None:
        snapshot_path = args.snapshot.expanduser().resolve()

        def open_calendar() -> None:
            calendar_picker.openCalendar()

        def capture() -> None:
            snapshot_path.parent.mkdir(parents=True, exist_ok=True)
            image = window.grab()
            if image.isNull() or not image.save(str(snapshot_path), "PNG"):
                print(
                    "Unable to save snapshot: {0}".format(snapshot_path),
                    file=sys.stderr,
                )
                app.exit(2)
                return
            print("snapshot: {0}".format(snapshot_path))
            app.quit()

        QTimer.singleShot(80, open_calendar)
        QTimer.singleShot(400, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
