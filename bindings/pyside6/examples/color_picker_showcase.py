"""Interactive and snapshot acceptance example for the native ColorPicker."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget

from fluentqt.basicinput import Button, ColorPicker
from fluentqt.layout import Divider
from fluentqt.textfields import Label
from fluentqt.windowing import Window


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt ColorPicker example."
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


def color_text(color: QColor) -> str:
    return "#{0:02X}{1:02X}{2:02X}{3:02X}".format(
        color.alpha(),
        color.red(),
        color.green(),
        color.blue(),
    )


def build_showcase() -> Window:
    window = Window()
    window.setWindowTitle("FluentQt PySide6 ColorPicker")
    window.resize(560, 760 if sys.platform == "win32" else 720)

    content = QWidget()
    root = QVBoxLayout(content)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = Label("Native ColorPicker", content)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = Label(
        "Python supplies QColor values and signal handlers; spectrum, "
        "channels, alpha, painting, and layout remain native FluentQt.",
        content,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(Divider(content))

    picker = ColorPicker(content)
    picker.setColor(QColor(0, 120, 212, 180))
    picker.setMinimumSize(420, 480)
    root.addWidget(picker, 1)

    status_row = QHBoxLayout()
    status = Label("Color: {0}".format(color_text(picker.color())), content)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    alpha_button = Button("Disable alpha", content)
    status_row.addWidget(status)
    status_row.addStretch()
    status_row.addWidget(alpha_button)
    root.addLayout(status_row)

    def update_status(color: QColor) -> None:
        status.setText("Color: {0}".format(color_text(color)))

    def toggle_alpha() -> None:
        enabled = not picker.alphaEnabled()
        picker.setAlphaEnabled(enabled)
        alpha_button.setText(
            "Disable alpha" if enabled else "Enable alpha"
        )

    picker.colorChanged.connect(update_status)
    alpha_button.clicked.connect(toggle_alpha)
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
