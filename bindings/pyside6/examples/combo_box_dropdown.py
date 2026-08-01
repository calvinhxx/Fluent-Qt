"""Interactive and snapshot acceptance example for ComboBox."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

import fluentqt

fluentqt.prepare_high_dpi_application()

from PySide6.QtCore import QAbstractListModel, QModelIndex, QTimer, Qt
from PySide6.QtWidgets import QApplication, QHBoxLayout, QVBoxLayout, QWidget


class EnvironmentModel(QAbstractListModel):
    """Small Python model consumed directly by the native ComboBox."""

    def __init__(self, rows, parent=None):
        super().__init__(parent)
        self._rows = list(rows)

    def rowCount(self, parent=QModelIndex()):
        return 0 if parent.isValid() else len(self._rows)

    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid():
            return None
        label, identifier = self._rows[index.row()]
        if role in (Qt.DisplayRole, Qt.EditRole):
            return label
        if role == Qt.UserRole:
            return identifier
        return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Open or snapshot the FluentQt ComboBox example."
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
    window.setWindowTitle("FluentQt PySide6 ComboBox")
    window.resize(760, 500)

    page = QWidget()
    root = QVBoxLayout(page)
    root.setContentsMargins(36, 32, 36, 32)
    root.setSpacing(16)

    title = fluentqt.Label("ComboBox from Python", page)
    title.setFluentTypography(fluentqt.FontRole.TitleLarge)
    root.addWidget(title)

    subtitle = fluentqt.Label(
        "A Python QAbstractListModel drives the native same-window Fluent "
        "dropdown. Selection, keyboard input, Escape, and light dismiss stay "
        "inside the C++ component.",
        page,
    )
    subtitle.setWordWrap(True)
    root.addWidget(subtitle)

    card = fluentqt.Card(page)
    card_layout = QVBoxLayout(card)
    card_layout.setContentsMargins(24, 22, 24, 22)
    card_layout.setSpacing(12)

    model = EnvironmentModel(
        [
            ("Production", "prod"),
            ("Staging", "staging"),
            ("Development", "dev"),
            ("Local preview", "local"),
        ],
        card,
    )
    combo = fluentqt.ComboBox(card)
    combo.setModel(model)
    combo.setCurrentIndex(1)
    combo.setMinimumWidth(260)

    editable = fluentqt.ComboBox(card)
    editable.addItems(["System default", "English", "简体中文", "العربية"])
    editable.setEditable(True)
    editable.setCurrentIndex(0)
    editable.setMinimumWidth(260)

    first_row = QHBoxLayout()
    first_row.addWidget(fluentqt.Label("Deployment target", card))
    first_row.addStretch()
    first_row.addWidget(combo)
    card_layout.addLayout(first_row)

    second_row = QHBoxLayout()
    second_row.addWidget(fluentqt.Label("Editable language", card))
    second_row.addStretch()
    second_row.addWidget(editable)
    card_layout.addLayout(second_row)
    root.addWidget(card)

    status = fluentqt.Label(page)
    status.setFluentTypography(fluentqt.FontRole.BodyStrong)

    def update_status(index: int) -> None:
        status.setText(
            "Selected: {0} ({1})".format(
                combo.itemText(index),
                combo.itemData(index, Qt.UserRole),
            )
        )

    combo.currentIndexChanged.connect(update_status)
    update_status(combo.currentIndex())
    root.addWidget(status)
    root.addStretch()
    window.setContentWidget(page)

    window._combo_model = model
    window._combo = combo
    window._editable_combo = editable
    window._snapshot_open = combo.showPopup
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
            window._combo.hidePopup()
            app.quit()

        QTimer.singleShot(300, capture)
    elif args.auto_close_ms > 0:
        QTimer.singleShot(args.auto_close_ms, app.quit)

    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
