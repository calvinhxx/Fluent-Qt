"""Interactive and snapshot acceptance example for Popup."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QPoint, QTimer, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt Popup example."
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
    window.setWindowTitle("FluentQt PySide6 Popup")
    window.resize(820, 540)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 32, 36, 32)
    root.setSpacing(16)

    title = fluentqt.Label("Popup from Python", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    subtitle = fluentqt.Label(
        "The surface stays inside the owning Window while native FluentQt "
        "handles placement, focus, close policy, scrim, and painting.",
        page,
    )
    subtitle.setWordWrap(True)
    root.addWidget(subtitle)

    summary = fluentqt.Card(page)
    summary_layout = QVBoxLayout(summary)
    summary_layout.setContentsMargins(24, 22, 24, 22)
    summary_layout.setSpacing(8)
    summary_title = fluentqt.Label("Same-window overlay", summary)
    summary_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    summary_text = fluentqt.Label(
        "The anchor, local theme source, and toolbar passthrough remain "
        "caller-owned. The Python facade retains only their wrappers.",
        summary,
    )
    summary_text.setWordWrap(True)
    summary_layout.addWidget(summary_title)
    summary_layout.addWidget(summary_text)
    root.addWidget(summary)
    root.addStretch()

    actions = QHBoxLayout()
    actions.setSpacing(12)
    status = fluentqt.Label("Popup ready", page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    toolbar_button = fluentqt.Button("Toolbar passthrough", page)
    open_button = fluentqt.Button("Open popup", page)
    open_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addWidget(status)
    actions.addStretch()
    actions.addWidget(toolbar_button)
    actions.addWidget(open_button)
    root.addLayout(actions)

    window.setContentWidget(page)

    popup = fluentqt.Popup(page)
    popup.setFixedSize(360, 230)
    popup.setAnimationEnabled(not snapshot_mode)
    popup.setExitAnimationEnabled(not snapshot_mode)
    popup.setModal(True)
    popup.setDim(True)
    popup.setLightDismissConsumesPress(True)
    popup.setClosePolicy(
        fluentqt.Popup.CloseFlag.CloseOnPressOutside
        | fluentqt.Popup.CloseFlag.CloseOnEscape
    )
    popup.setPosition(
        open_button,
        QPoint(
            open_button.width() - popup.width(),
            -popup.height() - 8,
        ),
    )
    popup.setThemeSource(open_button)
    popup.addLightDismissPassthrough(toolbar_button)

    popup_layout = QVBoxLayout(popup)
    popup_layout.setContentsMargins(32, 28, 32, 28)
    popup_layout.setSpacing(10)
    popup_title = fluentqt.Label("Workspace notifications", popup)
    popup_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    popup_detail = fluentqt.Label(
        "Python supplied this content and the anchor. The Popup itself, "
        "including the shadow and dismissal behavior, is native C++.",
        popup,
    )
    popup_detail.setWordWrap(True)
    popup_layout.addWidget(popup_title)
    popup_layout.addWidget(popup_detail)
    popup_layout.addStretch()
    dismiss_button = fluentqt.Button("Done", popup)
    dismiss_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    popup_layout.addWidget(dismiss_button, 0, Qt.AlignRight)

    open_button.clicked.connect(popup.open)
    dismiss_button.clicked.connect(popup.close)
    popup.opened.connect(lambda: status.setText("Popup opened"))
    popup.closed.connect(lambda: status.setText("Focus returned"))
    toolbar_button.clicked.connect(
        lambda: status.setText("Passthrough activated")
    )

    window._popup = popup
    if snapshot_mode:
        QTimer.singleShot(0, popup.open)
    return window


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = build_showcase(args.snapshot is not None)
    app.aboutToQuit.connect(window._popup.close)
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
