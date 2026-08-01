"""Interactive and snapshot acceptance example for Flyout."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt Flyout example."
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


def build_showcase(snapshot_mode: bool) -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 Flyout")
    window.resize(820, 560)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 32, 36, 32)
    root.setSpacing(16)

    title = fluentqt.Label("Flyout from Python", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    subtitle = fluentqt.Label(
        "Choose an anchor placement below. Native FluentQt keeps the flyout "
        "inside this Window and handles light dismiss, focus, and painting.",
        page,
    )
    subtitle.setWordWrap(True)
    root.addWidget(subtitle)

    summary = fluentqt.Card(page)
    summary_layout = QVBoxLayout(summary)
    summary_layout.setContentsMargins(24, 22, 24, 22)
    summary_layout.setSpacing(8)
    summary_title = fluentqt.Label("Anchor-aware overlay", summary)
    summary_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    summary_text = fluentqt.Label(
        "The anchor remains caller-owned. The Python facade retains its "
        "wrapper while native C++ resolves Top, Bottom, Left, Right, Full, "
        "or Auto placement.",
        summary,
    )
    summary_text.setWordWrap(True)
    summary_layout.addWidget(summary_title)
    summary_layout.addWidget(summary_text)
    root.addWidget(summary)
    root.addStretch()

    status = fluentqt.Label("Flyout ready", page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    actions.setSpacing(10)
    placement_buttons = []
    for text, placement in (
        ("Top", fluentqt.Flyout.Placement.Top),
        ("Bottom", fluentqt.Flyout.Placement.Bottom),
        ("Left", fluentqt.Flyout.Placement.Left),
        ("Right", fluentqt.Flyout.Placement.Right),
        ("Auto", fluentqt.Flyout.Placement.Auto),
    ):
        button = fluentqt.Button(text, page)
        if placement == fluentqt.Flyout.Placement.Auto:
            button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
        actions.addWidget(button)
        placement_buttons.append((button, placement))
    root.addLayout(actions)

    window.setContentWidget(page)

    flyout = fluentqt.Flyout(page)
    flyout.setFixedSize(340, 210)
    flyout.setAnimationEnabled(not snapshot_mode)
    flyout.setExitAnimationEnabled(False)
    flyout.setAnchorOffset(10)
    flyout.setClampToWindow(True)

    flyout_layout = QVBoxLayout(flyout)
    flyout_layout.setContentsMargins(32, 28, 32, 28)
    flyout_layout.setSpacing(10)
    flyout_title = fluentqt.Label("Context actions", flyout)
    flyout_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    flyout_detail = fluentqt.Label(
        "This is a native, non-modal Flyout anchored by a Python QWidget.",
        flyout,
    )
    flyout_detail.setWordWrap(True)
    flyout_layout.addWidget(flyout_title)
    flyout_layout.addWidget(flyout_detail)
    flyout_layout.addStretch()
    dismiss_button = fluentqt.Button("Done", flyout)
    dismiss_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    flyout_layout.addWidget(dismiss_button, 0, Qt.AlignRight)

    def show_at(anchor, placement) -> None:
        if flyout.isOpen():
            flyout.close()
        flyout.setPlacement(placement)
        flyout.showAt(anchor)
        status.setText("Placement: {0}".format(anchor.text()))

    for button, placement in placement_buttons:
        button.clicked.connect(
            lambda _checked=False, anchor=button, value=placement: show_at(
                anchor,
                value,
            )
        )

    dismiss_button.clicked.connect(flyout.close)
    flyout.closed.connect(lambda: status.setText("Flyout closed"))

    window._flyout = flyout
    window._snapshot_open = lambda: show_at(
        placement_buttons[-1][0],
        fluentqt.Flyout.Placement.Auto,
    )
    if snapshot_mode:
        QTimer.singleShot(0, window._snapshot_open)
    return window


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = build_showcase(args.snapshot is not None)
    app.aboutToQuit.connect(window._flyout.close)
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

        QTimer.singleShot(300, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
