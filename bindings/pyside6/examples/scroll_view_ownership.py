"""Demonstrate ScrollView's Python-safe owned and take contracts."""

import argparse
from pathlib import Path

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def create_scroll_content():
    content = QWidget()
    layout = QVBoxLayout(content)
    layout.setContentsMargins(20, 20, 20, 20)
    layout.setSpacing(12)
    for index in range(1, 21):
        layout.addWidget(
            fluentqt.Label(
                "Owned FluentQt ScrollView row {0}".format(index),
                content,
            )
        )
    layout.addStretch()
    return content


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--auto-close-ms",
        type=int,
        default=0,
        help="Close automatically after this delay.",
    )
    parser.add_argument(
        "--snapshot",
        type=Path,
        help="Save the initial ownership example as a PNG and exit.",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    app = QApplication([])
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = fluentqt.Window()
    window.setWindowTitle("FluentQt ScrollView ownership")
    window.resize(560, 520)

    page = QWidget()
    page_layout = QVBoxLayout(page)
    page_layout.setContentsMargins(24, 24, 24, 24)
    page_layout.setSpacing(12)

    title = fluentqt.Label("Owned content and explicit take", page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    page_layout.addWidget(title)

    status = fluentqt.Label(
        "ScrollView owns the hosted widget.",
        page,
    )
    page_layout.addWidget(status)

    scroll_view = fluentqt.ScrollView(page)
    scroll_view.setVerticalScrollBarVisibility(
        fluentqt.ScrollView.ScrollBarVisibility.Auto
    )
    scroll_view.setContentWidget(create_scroll_content())
    page_layout.addWidget(scroll_view, 1)

    actions = QHBoxLayout()
    toggle = fluentqt.Button("Take content", page)
    toggle.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(toggle, 0, Qt.AlignRight)
    page_layout.addLayout(actions)

    detached = {"widget": None}

    def toggle_content():
        if detached["widget"] is None:
            detached["widget"] = scroll_view.takeContentWidget()
            status.setText(
                "Content is parentless and owned by Python."
            )
            toggle.setText("Reattach content")
        else:
            scroll_view.setContentWidget(detached["widget"])
            detached["widget"] = None
            status.setText("ScrollView owns the hosted widget.")
            toggle.setText("Take content")

    toggle.clicked.connect(toggle_content)
    window.setContentWidget(page)
    window.show()

    def save_snapshot():
        args.snapshot.parent.mkdir(parents=True, exist_ok=True)
        if not window.grab().save(str(args.snapshot)):
            raise RuntimeError(
                "Could not save ScrollView snapshot: {0}".format(
                    args.snapshot
                )
            )
        app.quit()

    if args.snapshot:
        QTimer.singleShot(100, save_snapshot)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
