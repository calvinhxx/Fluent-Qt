"""Interactive and snapshot acceptance example for the native CalendarView."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QDate, QTimer, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget

from fluentqt.basicinput import Button
from fluentqt.date_time import CalendarView
from fluentqt.layout import Divider
from fluentqt.textfields import Label
from fluentqt.windowing import Window


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt CalendarView example."
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Save the rendered window as a PNG and exit.",
    )
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Automatically close the interactive window after this delay.",
    )
    return parser.parse_args()


def build_showcase() -> Window:
    window = Window()
    window.setWindowTitle("FluentQt PySide6 CalendarView")
    window.resize(470, 680 if sys.platform == "win32" else 650)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = Label("Native CalendarView", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = Label(
        "Python supplies QDate values and signal handlers; navigation, "
        "selection, keyboard input, animation, and painting remain native "
        "FluentQt.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(Divider(content))

    calendar = CalendarView(content)
    calendar.setDateRange(QDate(2025, 1, 1), QDate(2027, 12, 31))
    calendar.setVisibleMonth(QDate(2026, 5, 1))
    calendar.setSelectedDate(QDate(2026, 5, 21))
    calendar.setFixedSize(calendar.sizeHint())
    root.addWidget(calendar, 0, Qt.AlignHCenter)

    levels = (
        ("Day", CalendarView.CalendarContentLevel.Day),
        ("Month", CalendarView.CalendarContentLevel.Month),
        ("Year", CalendarView.CalendarContentLevel.Year),
    )
    level_names = {level: name for name, level in levels}
    level_row = QHBoxLayout()
    level_row.addStretch()
    for name, level in levels:
        button = Button(name, content)
        button.clicked.connect(
            lambda _checked=False, target=level: calendar.setContentLevel(
                target
            )
        )
        level_row.addWidget(button)
    level_row.addStretch()
    root.addLayout(level_row)

    status = Label("", content)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    status.setWordWrap(True)
    root.addWidget(status)

    def update_status(*_args: object) -> None:
        selected = calendar.selectedDate().toString("yyyy-MM-dd")
        level = level_names[calendar.contentLevel()]
        status.setText(
            "Selected: {0}  |  Level: {1}".format(selected, level)
        )

    calendar.selectedDateChanged.connect(update_status)
    calendar.contentLevelChanged.connect(update_status)
    update_status()

    root.addStretch()
    window.setContentWidget(content)
    return window


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = build_showcase()
    window.show()

    if args.snapshot is not None:
        snapshot_path = args.snapshot.expanduser().resolve()

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

        QTimer.singleShot(250, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
