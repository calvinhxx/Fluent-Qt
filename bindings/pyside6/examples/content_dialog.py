"""Interactive and snapshot acceptance example for ContentDialog."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt ContentDialog example."
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
    window.setWindowTitle("FluentQt PySide6 ContentDialog")
    window.resize(820, 560)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 32, 36, 32)
    root.setSpacing(16)

    title = fluentqt.Label("ContentDialog from Python", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    subtitle = fluentqt.Label(
        "The dialog remains inside the owning Window. Native FluentQt "
        "provides the smoke scrim, focus, command results, and rendering.",
        page,
    )
    subtitle.setWordWrap(True)
    root.addWidget(subtitle)

    summary = fluentqt.Card(page)
    summary_layout = QVBoxLayout(summary)
    summary_layout.setContentsMargins(24, 22, 24, 22)
    summary_layout.setSpacing(8)
    summary_title = fluentqt.Label("Explicit content ownership", summary)
    summary_title.setFluentTypography(fluentqt.FontRole.Subtitle)
    summary_text = fluentqt.Label(
        "Python supplies the content widget and retains its wrapper. While "
        "installed, it is a child of ContentDialog and is destroyed with it; "
        "takeContent() returns it parentless to Python.",
        summary,
    )
    summary_text.setWordWrap(True)
    summary_layout.addWidget(summary_title)
    summary_layout.addWidget(summary_text)
    root.addWidget(summary)
    root.addStretch()

    actions = QHBoxLayout()
    status = fluentqt.Label("No choice yet", page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    open_button = fluentqt.Button("Review changes", page)
    open_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addWidget(status)
    actions.addStretch()
    actions.addWidget(open_button)
    root.addLayout(actions)
    window.setContentWidget(page)

    dialog_content = QWidget()
    content_layout = QVBoxLayout(dialog_content)
    content_layout.setContentsMargins(0, 0, 0, 0)
    content_layout.setSpacing(12)
    explanation = fluentqt.Label(
        "Three local files have unsaved changes. Saving keeps the changes; "
        "discarding cannot be undone.",
        dialog_content,
    )
    explanation.setWordWrap(True)
    remember = fluentqt.CheckBox(
        "Use this choice for the remaining files",
        dialog_content,
    )
    content_layout.addWidget(explanation)
    content_layout.addWidget(remember)

    dialog = fluentqt.ContentDialog(page)
    dialog.setFixedSize(520, 320)
    dialog.setAnimationEnabled(not snapshot_mode)
    dialog.setTitle("Save changes before closing?")
    dialog.setPrimaryButtonText("Save")
    dialog.setSecondaryButtonText("Discard")
    dialog.setCloseButtonText("Cancel")
    dialog.setDefaultButton(fluentqt.ContentDialogButton.Primary)
    dialog.setContent(dialog_content)

    open_button.clicked.connect(dialog.open)
    dialog.primaryButtonClicked.connect(
        lambda: status.setText("Changes saved")
    )
    dialog.secondaryButtonClicked.connect(
        lambda: status.setText("Changes discarded")
    )
    dialog.closeButtonClicked.connect(lambda: status.setText("Cancelled"))

    window._content_dialog = dialog
    if snapshot_mode:
        QTimer.singleShot(0, dialog.open)
    return window


def main() -> int:
    args = parse_args()
    app = QApplication.instance() or QApplication(sys.argv)
    if not fluentqt.initialize_resources():
        raise RuntimeError("FluentQt resources could not be initialized")
    app.setFont(fluentqt.font_for_role(fluentqt.FontRole.Body))

    window = build_showcase(args.snapshot is not None)
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
