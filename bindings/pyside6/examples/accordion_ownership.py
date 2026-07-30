"""Interactive and snapshot acceptance example for Accordion ownership."""

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
        description="Open or snapshot the FluentQt Accordion example."
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


def create_item(title: str, detail: str) -> fluentqt.Expander:
    item = fluentqt.Expander()
    item.setHeaderText(title)
    item.setAnimationEnabled(False)

    content = QWidget()
    content_layout = QVBoxLayout(content)
    content_layout.setContentsMargins(18, 12, 18, 16)
    description = fluentqt.Label(detail, content)
    description.setWordWrap(True)
    content_layout.addWidget(description)
    item.setOwnedContentWidget(content)
    return item


def build_showcase() -> fluentqt.Window:
    window = fluentqt.Window()
    window.setWindowTitle("FluentQt PySide6 Accordion")
    window.resize(650, 520)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(28, 24, 28, 24)
    root.setSpacing(12)

    title = fluentqt.Label("Accordion ownership facade", page)
    title.setFluentTypography(fluentqt.FontRole.Title)
    root.addWidget(title)

    summary = fluentqt.Label(
        "Each Expander uses a fixed Python lifetime method while the native "
        "Accordion coordinates expansion and keyboard focus.",
        page,
    )
    summary.setWordWrap(True)
    root.addWidget(summary)
    root.addWidget(fluentqt.Divider(page))

    accordion = fluentqt.Accordion(page)
    borrowed = create_item(
        "Borrowed item",
        "Detaches as a parentless widget when the Accordion releases it.",
    )
    owned = create_item(
        "Owned item",
        "Is deleted with the Accordion unless Python takes it first.",
    )
    restore_parent = QWidget(page)
    reparented = create_item(
        "Reparented item",
        "Returns to the QWidget parent it had when it was adopted.",
    )
    reparented.setParent(restore_parent)

    accordion.addBorrowedItem(borrowed)
    accordion.addOwnedItem(owned)
    accordion.addReparentedItem(reparented)
    owned.setExpanded(True)
    root.addWidget(accordion)
    root.addStretch()

    status = fluentqt.Label(
        "Single mode · Owned item expanded",
        page,
    )
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)
    root.addWidget(status)

    actions = QHBoxLayout()
    mode_button = fluentqt.Button("Allow multiple", page)
    mode_button.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
    actions.addStretch()
    actions.addWidget(mode_button, 0, Qt.AlignRight)
    root.addLayout(actions)

    def toggle_mode() -> None:
        multiple = (
            accordion.expansionMode()
            == fluentqt.Accordion.ExpansionMode.Multiple
        )
        if multiple:
            accordion.setExpansionMode(
                fluentqt.Accordion.ExpansionMode.Single
            )
            mode_button.setText("Allow multiple")
            status.setText("Single mode · only one item remains expanded")
        else:
            accordion.setExpansionMode(
                fluentqt.Accordion.ExpansionMode.Multiple
            )
            mode_button.setText("Use single mode")
            status.setText("Multiple mode · expand any combination")

    mode_button.clicked.connect(toggle_mode)
    window.setContentWidget(page)
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
